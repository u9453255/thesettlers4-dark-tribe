#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <utility>
namespace dt {
inline bool boardable(uint16_t kind){return kind==29||kind==32||kind==52||kind==54;}
inline bool flightAllowed(uint32_t expected,uint32_t actual,int x,int y){return expected&&expected==actual&&x>0&&y>0&&x<575&&y<575;}
// Restore even when a native request reports failure. Never retain a selected
// aircraft in the engine's script-global last-created slot.
struct ScopedAircraft {
 uint32_t&slot;uint32_t previous;
 ScopedAircraft(uint32_t&s,uint32_t id):slot(s),previous(s){slot=id;}
 ~ScopedAircraft(){slot=previous;}
 ScopedAircraft(const ScopedAircraft&)=delete;
 ScopedAircraft&operator=(const ScopedAircraft&)=delete;
};
// Screen<->map mapping for aircraft picking under any renderer. The native
// projection (RVA 0x259910) is affine on a flat map; it is sampled on ticks
// and inverted here. map = (a*sx+b*sy+c, d*sx+e*sy+f).
struct Affine {double a=0,b=0,c=0,d=0,e=0,f=0;bool ok=false;};
inline Affine fitAffine(int x0,int y0,int x1,int y1,int x2,int y2,const int m0[2],const int m1[2],const int m2[2]){
 // Points: (x0,y0) origin, (x1,y0) horizontal, (x0,y2) vertical (y1 unused for symmetry).
 (void)y1;(void)x2;Affine t;double w=x1-x0,h=y2-y0;if(w<=0||h<=0)return t;
 t.a=(m1[0]-m0[0])/w;t.d=(m1[1]-m0[1])/w;t.b=(m2[0]-m0[0])/h;t.e=(m2[1]-m0[1])/h;
 t.c=m0[0]-t.a*x0-t.b*y0;t.f=m0[1]-t.d*x0-t.e*y0;t.ok=fabs(t.a*t.e-t.b*t.d)>1e-9;return t;
}
inline bool affineAgrees(const Affine&t,int sx,int sy,const int m[2]){return t.ok&&fabs(t.a*sx+t.b*sy+t.c-m[0])<=2.5&&fabs(t.d*sx+t.e*sy+t.f-m[1])<=2.5;}
inline bool toScreen(const Affine&t,double mx,double my,double&sx,double&sy){
 double det=t.a*t.e-t.b*t.d;if(!t.ok||fabs(det)<1e-9)return false;mx-=t.c;my-=t.f;
 sx=(t.e*mx-t.b*my)/det;sy=(-t.d*mx+t.a*my)/det;return true;
}
struct PickBox {int left=0,top=0,right=0,bottom=0;};
// Aircraft sprites hover above their map tile: accept a generous box around
// and above it, scaled with zoom (pixels per map tile).
inline PickBox spriteBox(const Affine&t,int mx,int my){
 PickBox b;double sx,sy,ux,uy,vx,vy;if(!toScreen(t,mx,my,sx,sy)||!toScreen(t,mx+1,my,ux,uy)||!toScreen(t,mx,my+1,vx,vy))return b;
 double tile=fmax(4.0,fmax(hypot(ux-sx,uy-sy),hypot(vx-sx,vy-sy)));
 b.left=(int)(sx-2.5*tile);b.right=(int)(sx+2.5*tile);b.top=(int)(sy-6.0*tile);b.bottom=(int)(sy+1.5*tile);return b;
}
inline bool boxSelect(int x0,int y0,int x1,int y1){return abs(x1-x0)>=8||abs(y1-y0)>=8;}
inline bool pickHit(const PickBox&b,int x0,int y0,int x1,int y1){
 if(b.right<=b.left)return false;
 if(!boxSelect(x0,y0,x1,y1))return x1>=b.left&&x1<=b.right&&y1>=b.top&&y1<=b.bottom;
 int l=x0<x1?x0:x1,r=x0<x1?x1:x0,u=y0<y1?y0:y1,d=y0<y1?y1:y0;
 int cx=(b.left+b.right)/2,cy=b.bottom-(b.bottom-b.top)/4; // sprite body centre
 return cx>=l&&cx<=r&&cy>=u&&cy<=d;
}
// Find the screen pixel whose native screen->map projection is the given tile.
// Native projection includes terrain height, so refine with local fits.
template<class P> bool solveScreen(P project,int w,int h,int mx,int my,int&sx,int&sy){
 double gx=sx,gy=sy;if(gx<0||gy<0||gx>=w||gy>=h){gx=w/2.0;gy=h/2.0;}
 const int step=64; // tiles are ~32x16 px at normal zoom; integer tiles need a wide fit
 for(int iter=0;iter<10;iter++){
  int x=(int)lround(gx),y=(int)lround(gy),a[2],b[2],c[2];project(x,y,a);project(x+step,y,b);project(x,y+step,c);
  if(a[0]==mx&&a[1]==my)break;
  double m00=(b[0]-a[0])/(double)step,m10=(b[1]-a[1])/(double)step,m01=(c[0]-a[0])/(double)step,m11=(c[1]-a[1])/(double)step,det=m00*m11-m01*m10;
  if(fabs(det)<1e-9)return false;
  double ex=mx-a[0],ey=my-a[1];gx+=(m11*ex-m01*ey)/det;gy+=(-m10*ex+m00*ey)/det;
  if(gx<-w||gy<-h||gx>2.0*w||gy>2.0*h)return false;
 }
 // Integer tiles: finish with a small local search for the exact tile.
 int cx=(int)lround(gx),cy=(int)lround(gy);
 for(int r=0;r<=48;r+=4)for(int dy=-r;dy<=r;dy+=4)for(int dx=-r;dx<=r;dx+=4){
  if(r&&abs(dx)!=r&&abs(dy)!=r)continue;int a[2];project(cx+dx,cy+dy,a);
  if(a[0]==mx&&a[1]==my){sx=cx+dx;sy=cy+dy;return true;}
 }
 return false;
}
// Unload spot: the aircraft tile plus the six native exit tiles must be free
// (same offsets the transport check uses). Search outward from the clicked
// tile and return the closest valid spot, avoiding spots already assigned.
constexpr int UnloadExitX[6]={2,2,2,1,0,-1},UnloadExitY[6]={0,1,2,2,2,1};
template<class F> bool unloadSpotValid(F free,int x,int y){if(!free(x,y))return false;for(int i=0;i<6;i++)if(!free(x+UnloadExitX[i],y+UnloadExitY[i]))return false;return true;}
template<class F> bool nearestUnloadSpot(F free,int tx,int ty,int radius,const int(*taken)[2],int takenCount,int&ox,int&oy){
 long best=-1;
 for(int r=0;r<=radius;r++){
  for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){
   if(abs(dx)!=r&&abs(dy)!=r)continue;int x=tx+dx,y=ty+dy;if(x<2||y<2||x>571||y>571)continue;
   bool clash=false;for(int i=0;i<takenCount;i++)if(abs(taken[i][0]-x)<5&&abs(taken[i][1]-y)<5){clash=true;break;}
   if(clash||!unloadSpotValid(free,x,y))continue;long d=(long)dx*dx+(long)dy*dy;if(best<0||d<best){best=d;ox=x;oy=y;}
  }
  if(best>=0&&(long)r*r>=best)return true; // no closer spot can exist further out
 }
 return best>=0;
}
// Load nearby: assign the closest eligible units to selected landed aircraft,
// globally nearest pair first, never exceeding each aircraft's free seats.
struct LoadCraft {int x=0,y=0,free=0;};struct LoadUnit {int x=0,y=0;};
constexpr int LoadRadius=24; // Doubled on user request (was 12).
inline std::vector<std::pair<int,int>> assignNearby(std::vector<LoadCraft> crafts,const std::vector<LoadUnit>&units,int radius=LoadRadius){
 struct Pair{long d;int u,c;};std::vector<Pair> pairs;
 for(int u=0;u<(int)units.size();u++)for(int c=0;c<(int)crafts.size();c++){long dx=units[u].x-crafts[c].x,dy=units[u].y-crafts[c].y,d=dx*dx+dy*dy;if(d<=(long)radius*radius&&crafts[c].free>0)pairs.push_back({d,u,c});}
 std::stable_sort(pairs.begin(),pairs.end(),[](const Pair&a,const Pair&b){return a.d<b.d;});
 std::vector<char> used(units.size(),0);std::vector<std::pair<int,int>> out;
 for(auto&p:pairs){if(used[p.u]||crafts[p.c].free<=0)continue;used[p.u]=1;crafts[p.c].free--;out.push_back({p.u,p.c});}
 return out;
}
// Group destinations: distinct landing tiles around the clicked target.
inline void groupTarget(int index,int x,int y,int&ox,int&oy){
 static const int dx[]={0,5,-5,0,0,5,-5,5,-5,10,-10,0,0},dy[]={0,0,0,5,-5,5,-5,-5,5,0,0,10,-10};
 int i=index%13;ox=x+dx[i];oy=y+dy[i];if(ox<2)ox=2;if(oy<2)oy=2;if(ox>573)ox=573;if(oy>573)oy=573;
}
// Clear the low six fog bits (upper attribute bits kept) inside a circle,
// clipped to the map. Same encoding as the Dark-land visibility pass.
constexpr int AircraftSight=25; // Native settler vision radius (RVA 0x1AE306).
inline int revealCircle(uint8_t*land,int size,int cx,int cy,int radius){
 int changed=0;if(!land||radius<0)return 0;
 for(int y=cy-radius;y<=cy+radius;y++){if(y<0||y>=size)continue;for(int x=cx-radius;x<=cx+radius;x++){if(x<0||x>=size)continue;int dx=x-cx,dy=y-cy;if(dx*dx+dy*dy>radius*radius)continue;uint8_t&fog=land[4*(y*size+x)+3];if(fog&0x3f){fog&=0xc0;changed++;}}}
 return changed;
}
// Manacopter health. CManakopter ctor (RVA 0x16AF34..0x16AF74): ecx=0xFA; if the
// int setting DTManakopterHitpoints (RVA 0xDCDB24, static default 0xFA written at
// RVA 0x24571) is signed-below 0x100 it is used instead; BYTE +0x21 = cl. Healing
// cells (RVA 0x16B58F) restore the low byte of the setting.
constexpr uintptr_t ManakopterHitpointsSetting=0xDCDB24;
constexpr int ManakopterDefaultHitpoints=0xFA;
inline int aircraftMaxHp(int32_t setting){return setting>=1&&setting<0x100?setting:ManakopterDefaultHitpoints;}
// Rounded percent 0..100; any remaining hitpoint shows at least 1 percent.
inline int healthPercent(int hp,int maxHp){
 if(hp<=0)return 0;if(maxHp<=0)maxHp=ManakopterDefaultHitpoints;if(hp>=maxHp)return 100;
 int p=(hp*200+maxHp)/(2*maxHp);return p<1?1:p>99?99:p;
}
}
