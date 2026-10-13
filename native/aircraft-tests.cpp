#include "aircraft-model.hpp"
#include "panel.hpp"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include <vector>
#include <math.h>
int main(){
 assert(dt::flightAllowed(4,4,1,574));assert(!dt::flightAllowed(4,5,20,20));assert(!dt::flightAllowed(0,0,20,20));
 for(int edge:{0,575,65535,-1}){assert(!dt::flightAllowed(4,4,edge,20));assert(!dt::flightAllowed(4,4,20,edge));}
 assert(dt::boardable(29));assert(dt::boardable(32));assert(dt::boardable(54));assert(dt::boardable(52));assert(!dt::boardable(53));uint32_t last=20;{dt::ScopedAircraft a(last,10);assert(last==10);{dt::ScopedAircraft b(last,30);assert(last==30);}assert(last==10);}assert(last==20);
 // Skewed isometric-like projection: map x=(sx+sy/2)/32, y=sy/16, camera offset 100,50.
 auto proj=[](int sx,int sy,int out[2]){out[0]=100+(int)lround((sx+sy*0.5)/32.0);out[1]=50+(int)lround(sy/16.0);};
 int m0[2],m1[2],m2[2],m3[2];proj(256,192,m0);proj(768,192,m1);proj(256,576,m2);proj(768,576,m3);
 auto t=dt::fitAffine(256,192,768,192,256,576,m0,m1,m2);assert(t.ok&&dt::affineAgrees(t,768,576,m3));
 int bogus[2]={m3[0]+9,m3[1]};assert(!dt::affineAgrees(t,768,576,bogus));
 double sx,sy;assert(dt::toScreen(t,m0[0],m0[1],sx,sy)&&fabs(sx-256)<40&&fabs(sy-192)<20);
 auto box=dt::spriteBox(t,m0[0],m0[1]);assert(box.right>box.left&&box.bottom>box.top);
 int cx=(int)sx,cy=(int)sy;
 assert(dt::pickHit(box,cx,cy-20,cx,cy-20));            // click on the hovering sprite
 assert(!dt::pickHit(box,cx+400,cy,cx+400,cy));          // click elsewhere
 assert(dt::pickHit(box,cx-150,cy-150,cx+150,cy+150));   // drag box around it
 assert(!dt::pickHit(box,cx+200,cy+200,cx+400,cy+400));  // drag box elsewhere
 assert(!dt::boxSelect(10,10,14,15)&&dt::boxSelect(10,10,30,10));
 int seen[13][2];for(int i=0;i<13;i++){dt::groupTarget(i,300,300,seen[i][0],seen[i][1]);for(int j=0;j<i;j++)assert(seen[i][0]!=seen[j][0]||seen[i][1]!=seen[j][1]);}
 int ex,ey;dt::groupTarget(1,573,1,ex,ey);assert(ex==573&&ey==2);
 {static uint8_t land[64*64*4];for(int i=0;i<64*64;i++){land[4*i+3]=0xff;land[4*i+2]=0x11;}
  int n=dt::revealCircle(land,64,2,2,dt::AircraftSight);assert(n>0);
  assert(land[4*(2*64+2)+3]==0xc0&&land[4*(2*64+2)+2]==0x11);           // centre revealed, attributes kept
  assert(land[4*(2*64+(2+15))+3]==0xc0&&land[4*(2*64+(2+26))+3]==0xff);  // radius edge
  assert(land[4*(40*64+40)+3]==0xff&&dt::revealCircle(land,64,2,2,15)==0); // far cell untouched; idempotent
  assert(dt::revealCircle(land,64,63,63,15)>0);}                            // clipped at map edge
 {// Inverse projection solver against a skewed projection with "height" bumps.
  auto proj=[](int sx,int sy,int out[2]){double h=3.0*sin(sx/90.0)*cos(sy/70.0);out[0]=100+(int)floor((sx+(sy+h)*0.5)/32.0);out[1]=50+(int)floor((sy+h)/16.0);};
  for(int tx=105;tx<=130;tx+=5)for(int ty=55;ty<=90;ty+=7){int sx=-1,sy=-1;assert(dt::solveScreen(proj,1024,768,tx,ty,sx,sy));int m[2];proj(sx,sy,m);assert(m[0]==tx&&m[1]==ty);}
  int sx=-1,sy=-1;assert(!dt::solveScreen(proj,1024,768,5000,5000,sx,sy));}
 {// Nearest valid unload spot: water/blocked area around the click.
  auto free=[](int x,int y){return !(x>=95&&x<=110&&y>=95&&y<=110)&&!(x==120&&y==100);};
  int ox,oy;assert(dt::nearestUnloadSpot(free,50,50,20,nullptr,0,ox,oy)&&ox==50&&oy==50);       // valid click kept
  assert(dt::nearestUnloadSpot(free,100,100,20,nullptr,0,ox,oy));                               // inside blocked area
  assert(dt::unloadSpotValid(free,ox,oy)&&(abs(ox-100)>=6||abs(oy-100)>=6));
  int alt=-1,best=1<<30;for(int y=80;y<=120;y++)for(int x=80;x<=120;x++)if(dt::unloadSpotValid(free,x,y)){int d=(x-100)*(x-100)+(y-100)*(y-100);if(d<best){best=d;alt=x;}}
  assert((ox-100)*(ox-100)+(oy-100)*(oy-100)==best);                                            // truly the closest
  int taken[1][2]={{ox,oy}};int px,py;assert(dt::nearestUnloadSpot(free,100,100,20,taken,1,px,py)&&(abs(px-ox)>=5||abs(py-oy)>=5));
  auto none=[](int,int){return false;};assert(!dt::nearestUnloadSpot(none,100,100,5,nullptr,0,ox,oy));(void)alt;}
 {// Load nearby: closest first, capacity respected, radius respected, groups share.
  std::vector<dt::LoadCraft> crafts={{100,100,2},{120,100,3}};
  std::vector<dt::LoadUnit> units={{101,100},{103,100},{102,101},{119,100},{121,101},{110,100},{100,130},{125,104}};
  auto a=dt::assignNearby(crafts,units);int count[2]={0,0};std::vector<int> seen(units.size(),0);
  for(auto&p:a){count[p.second]++;assert(!seen[p.first]);seen[p.first]=1;}
  assert(count[0]==2&&count[1]==3&&!seen[6]);                       // capacities; unit 30 tiles away ignored
  assert(seen[0]&&seen[2]&&!seen[1]);                                 // craft 0 takes its two closest
  assert(seen[3]&&seen[4]&&seen[7]);                                  // craft 1 takes its three closest
  auto none=dt::assignNearby({{100,100,0}},units);assert(none.empty()); // full aircraft takes nobody
  assert(dt::LoadRadius==24&&dt::assignNearby({{200,200,6}},{{220,200},{217,212},{225,200}}).size()==2);} // ~20 tiles in, 25 out
 {// Manacopter health: native maximum, percent edge cases, bar fill and art.
  assert(dt::aircraftMaxHp(250)==250&&dt::aircraftMaxHp(255)==255&&dt::aircraftMaxHp(1)==1&&dt::aircraftMaxHp(100)==100);
  assert(dt::aircraftMaxHp(256)==250&&dt::aircraftMaxHp(1000)==250&&dt::aircraftMaxHp(0)==250&&dt::aircraftMaxHp(-5)==250); // ctor keeps 0xFA
  assert(dt::healthPercent(250,250)==100&&dt::healthPercent(255,250)==100&&dt::healthPercent(0,250)==0&&dt::healthPercent(-3,250)==0);
  assert(dt::healthPercent(125,250)==50&&dt::healthPercent(1,250)==1&&dt::healthPercent(249,250)==99&&dt::healthPercent(1,255)==1);
  assert(dt::healthPercent(50,0)==20&&dt::healthPercent(255,0)==100&&dt::healthPercent(3,4)==75&&dt::healthPercent(1,1)==100);
  for(int m=1;m<=255;m++){int last=0;for(int hp=1;hp<=m;hp++){int p=dt::healthPercent(hp,m);assert(p>=1&&p<=100&&p>=last&&(p==100)==(hp==m));last=p;}}
  assert(dtui::markerFill(0,18)==0&&dtui::markerFill(-4,18)==0&&dtui::markerFill(1,18)==1&&dtui::markerFill(50,18)==9&&dtui::markerFill(99,18)==17&&dtui::markerFill(100,18)==18&&dtui::markerFill(140,18)==18&&dtui::markerFill(50,0)==0);
  for(int p=1;p<100;p++)assert(dtui::markerFill(p,18)>=dtui::markerFill(p-1,18)&&dtui::markerFill(p,18)<18);
  assert(dtui::MarkInner==18&&dtui::MarkBarX+dtui::MarkBarW<=dtui::MarkW-12);
  static uint32_t px[dtui::MarkW*dtui::MarkH];int row=(dtui::MarkBarY+2)*dtui::MarkW+dtui::MarkBarX+1;
  auto argb=[](COLORREF c){return 0xFF000000u|((uint32_t)GetRValue(c)<<16)|((uint32_t)GetGValue(c)<<8)|GetBValue(c);};
  for(int p:{100,60,25,5,0}){dtui::markerPixels(px,p);uint32_t body=argb(dtui::healthColor(p));int filled=0;for(int x=0;x<dtui::MarkInner;x++)if(px[row+x]==body)filled++;
   assert(filled==dtui::markerFill(p,dtui::MarkInner));
   for(int x=filled;x<dtui::MarkInner;x++)assert(px[row+x]!=body&&(px[row+x]>>24)>=0xC0&&(px[row+x]&0xFFFFFF)<0x404040); // dark remainder
   assert(px[1*dtui::MarkW+1]==0xFFF0F0F0&&px[1*dtui::MarkW+dtui::MarkW-2]==0xFFF0F0F0&&px[7*dtui::MarkW+1]==0xFFF0F0F0);}  // white corner brackets
  assert(dtui::healthColor(100)!=dtui::healthColor(50)&&dtui::healthColor(50)!=dtui::healthColor(10));}
 puts("PASS: flight bounds, recycled identity refusal, scoped per-aircraft routing/restoration, projection fit/inverse, click and box aircraft picking, distinct group landing tiles, aircraft sight circle, inverse projection for selection brackets, nearest valid unload spot, load-nearby assignment, Manacopter health percent and marker bar");
}
