#include "hd-overlay.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int clicks=0,types=0;static dtui::Data data;
static uint32_t clickedIdentity=0;
static void input(const dthd::Snapshot&shown,int x,int y,UINT message){if(message==WM_LBUTTONUP){clicks++;types=dtui::hit(shown.data,shown.build,x,y).type;clickedIdentity=shown.data.selectedIdentity;}}
static bool read(dthd::Snapshot&){return false;}
static void log(const char*){}
int main(){
 data.ready=true;data.buildMenu=true;data.mana=150;
 dthd::Snapshot snapshot;snapshot.active=true;snapshot.build=true;snapshot.data=data;
 assert(dthd::CanvasHeight==768&&dthd::CanvasHeight==dtui::ManaY+dtui::ManaH&&dthd::ScaleHeight==600);
 for(unsigned dpi:{96u,120u,144u,192u}){
  // The whole canvas (down to the end of the mana section) at dpi/96.
  auto geometry=dthd::Geometry::fit(1600,dpi);assert(geometry.width==200*(int)dpi/96&&geometry.full==768*(int)dpi/96&&geometry.height==geometry.full);
  for(int row=0;row<4;row++){
   int px=(dtui::CellX[row%2]+20)*geometry.width/200,py=(dtui::CellY[row/2]+20)*geometry.full/768;
   assert(dthd::interactive(snapshot,geometry.logicalX(px),geometry.logicalY(py)));
   assert(dtui::hit(data,true,geometry.logicalX(px),geometry.logicalY(py)).type==1);
  }
  int mx=100*geometry.width/200,my=700*geometry.full/768;assert(dtui::manaArea(geometry.logicalX(mx),geometry.logicalY(my))&&dthd::interactive(snapshot,geometry.logicalX(mx),geometry.logicalY(my)));
  assert(!dthd::interactive(snapshot,geometry.logicalX(0),geometry.logicalY(0)));
 }
 // The scale of the first 600 logical px is unchanged: dpi/96, so for every
 // DPI step of 4 the click mapping is exactly the former y*600/height.
 for(unsigned dpi=96;dpi<=240;dpi+=4){auto g=dthd::Geometry::fit(4000,dpi);int span=600*(int)dpi/96;assert(g.width==200*span/600&&g.full==768*span/600);
  for(int y=0;y<g.full;y++)assert(g.logicalY(y)==y*600/span);}
 // Client shorter than the scaled canvas: same scale, the window shows the top
 // part that fits (the bottom of the mana section is cut off); the former
 // reduced scale for clients shorter than 600*dpi/96 is kept, within 1 px.
 {auto g=dthd::Geometry::fit(700,96);assert(g.width==200&&g.height==700&&g.full==768&&g.logicalY(650)==650&&g.logicalY(699)==699);
  g=dthd::Geometry::fit(768,96);assert(g.height==768&&g.full==768);g=dthd::Geometry::fit(600,96);assert(g.width==200&&g.height==600&&g.full==768);
  for(int client=600;client<=1300;client+=7){g=dthd::Geometry::fit(client,192);int span=std::min(client,1200);assert(g.height==std::min(client,g.full)&&g.width==200*span/600);
   for(int y=0;y<span;y++)assert(abs(g.logicalY(y)-y*600/span)<=1);}}
 auto compact=dthd::Geometry::fit(720,192);assert(compact.height==720&&compact.width==240&&compact.full==921);
 assert(!dthd::Geometry::fit(599,120).width&&!dthd::Geometry::fit(599,96).full);
 snapshot.active=false;assert(!dthd::interactive(snapshot,30,320)&&!dthd::interactive(snapshot,100,700));snapshot.active=true;
 snapshot.build=false;snapshot.data.kind=0;assert(!dthd::interactive(snapshot,30,320));
 assert(dthd::interactive(snapshot,20,195)&&!dthd::interactive(snapshot,65,195)&&!dthd::interactive(snapshot,20,170)); // only the native construction tab
 // The mana section in every mode: no Dark page, a native menu above it, or the plugin stopped.
 for(int x=0;x<200;x+=9)for(int y=dtui::ManaY;y<dtui::ManaY+dtui::ManaH;y+=9)assert(dthd::interactive(snapshot,x,y));
 assert(!dthd::interactive(snapshot,100,dtui::ManaY-1)&&!dthd::interactive(snapshot,100,dtui::ManaY+dtui::ManaH));
 snapshot.disabled=true;assert(dthd::interactive(snapshot,100,700));snapshot.disabled=false;
 snapshot.build=true;snapshot.data.kind=50;snapshot.data.selectedIdentity=1234;
 HINSTANCE module=GetModuleHandleW(nullptr);HWND owner=CreateWindowExW(0,L"STATIC",L"Original offline fixture",WS_POPUP,0,0,1000,900,nullptr,nullptr,module,nullptr);assert(owner);
 {
  dthd::Host host(module,read,input,log);assert(host.create(owner));host.cached=snapshot;
  POINT point={0,0};auto geometry=dthd::Geometry::fit(900,120);assert(geometry.full==960&&geometry.height==900);assert(host.draw(geometry,point));
  {RECT r{};GetWindowRect(host.window,&r);assert(r.right-r.left==geometry.width&&r.bottom-r.top==900);} // a short client shows the part that fits
  assert((GetWindowLongPtrW(host.window,GWL_EXSTYLE)&(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED))==(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED));
  assert(GetWindow(host.window,GW_OWNER)==owner&&!IsWindowVisible(host.window));
  assert(SendMessageW(host.window,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);
  assert(GetPixel(host.source,10,220)!=RGB(0,0,0)&&GetPixel(host.source,5,100)==RGB(0,0,0)&&GetPixel(host.source,10,700)!=RGB(0,0,0)&&GetPixel(host.source,100,767)!=RGB(0,0,0));
  assert(GetPixel(host.destination,10,880)!=RGB(0,0,0)&&GetPixel(host.destination,100,959)!=RGB(0,0,0)); // the scaled canvas holds the whole mana section
  int px=30*host.geometry.width/200,py=320*host.geometry.full/768;
  data.selectedIdentity=5678; // Selection changes before the next display refresh.
  SendMessageW(host.window,WM_LBUTTONUP,0,MAKELPARAM(px,py));assert(clicks==1&&types==1&&clickedIdentity==1234);
  SendMessageW(host.window,WM_LBUTTONUP,0,MAKELPARAM(0,0));assert(clicks==1);
  // Mana section: hit-tested as ours (the game never sees the click), no command.
  int mx=100*host.geometry.width/200,my=700*host.geometry.full/768;
  assert(SendMessageW(host.window,WM_NCHITTEST,0,MAKELPARAM(mx,my))==HTCLIENT&&SendMessageW(host.window,WM_NCHITTEST,0,MAKELPARAM(5,100))==HTTRANSPARENT);
  SendMessageW(host.window,WM_LBUTTONUP,0,MAKELPARAM(mx,my));assert(clicks==2&&types==0);
  host.cached.build=false;host.cached.data.kind=0;assert(SendMessageW(host.window,WM_NCHITTEST,0,MAKELPARAM(mx,my))==HTCLIENT);
  assert(host.draw(dthd::Geometry::fit(1600,96),point));assert(GetPixel(host.source,10,300)==RGB(0,0,0)&&GetPixel(host.source,10,700)!=RGB(0,0,0)); // no Dark page: only the mana section
  host.cached.build=true;host.cached.data.kind=50;
  host.cached.disabled=true;SendMessageW(host.window,WM_LBUTTONUP,0,MAKELPARAM(px,py));SendMessageW(host.window,WM_LBUTTONUP,0,MAKELPARAM(mx,my));assert(clicks==2);
  assert(host.draw(dthd::Geometry::fit(1600,96),point));assert(GetPixel(host.source,10,300)!=RGB(0,0,0)&&GetPixel(host.source,10,700)!=RGB(0,0,0)); // failure panel and the empty mana section
 }
 DestroyWindow(owner);
 puts("PASS: HD panel drawing in a hidden original window, owned/non-activating styles, DPI/resize hit targets, mana section in every mode down to y 768 (same scale, short clients show the part that fits), transparency and disabled input; no game or HD DLL loaded");
}
