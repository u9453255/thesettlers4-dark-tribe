#pragma once
#include "panel.hpp"
#include <windowsx.h>
namespace dthd {
// The canvas spans the native GUI column in its logical coordinates down to
// the end of the resource panel (menu 1, y 600..767), which the mana section
// covers. The scale is set by the first ScaleHeight logical px exactly as
// before that section existed: dpi/96 (96..240 DPI), reduced only so those
// 600 px fit the client; a client shorter than 600 px shows nothing.
constexpr int CanvasWidth=200,CanvasHeight=dtui::ManaY+dtui::ManaH,ScaleHeight=600;
struct Geometry {
 // width x height: the window (client px), the part of the scaled canvas
 // that fits the client; full: the whole canvas height scaled (>= height).
 int width=0,height=0,full=0;
 static Geometry fit(int clientHeight,unsigned dpi){
  if(clientHeight<ScaleHeight)return {};
  int span=std::min(clientHeight,(int)((ScaleHeight*std::max(96u,std::min(dpi,240u)))/96));
  int full=CanvasHeight*span/ScaleHeight;
  return {CanvasWidth*span/ScaleHeight,std::min(clientHeight,full),full};
 }
 int logicalX(int x)const{return width?x*CanvasWidth/width:-1;}
 int logicalY(int y)const{return full?y*CanvasHeight/full:-1;}
};
struct Snapshot {dtui::Data data{};bool active=false,build=false,disabled=false;bool hdBlocked=false,scene=false,running=false,ingame=true;};
inline bool detail(const dtui::Data&v){return v.isAircraft||v.unitPanel||v.kind==49||v.kind==50||v.kind==51||v.kind==80;}
// The construction tab (native position), while a Dark page shows the whole
// native content panel, and always the mana section (its clicks are
// swallowed so the native resource panel beneath never reacts).
inline bool interactive(const Snapshot&s,int x,int y){
 if(!s.active)return false;
 if(dtui::inside(x,y,dtui::TabX,dtui::TabY,dtui::TabW,dtui::TabH)||dtui::manaArea(x,y))return true;
 return (s.build||detail(s.data)||s.disabled)&&dtui::inside(x,y,dtui::BoxX,dtui::BoxY,dtui::BoxW,dtui::BoxH);
}
inline void paint(HDC dc,const Snapshot&s){
 dtui::box(dc,0,0,CanvasWidth,CanvasHeight,RGB(0,0,0));
 if(!s.active)return;
 if(s.build&&!s.disabled)dtui::houseTab(dc,0);
 if(s.disabled)dtui::failure(dc,0);
 else if(s.build||detail(s.data))dtui::render(dc,0,s.data,s.build);
 dtui::manaSection(dc,0,s.disabled?dtui::Data{}:s.data); /* every mode; empty when the plugin stopped */
}
// Click-through owned windows that show selection brackets above picked
// Manacopters (positions from the cached tick snapshot, scaled to the client).
// Each window owns a small premultiplied bitmap, re-rendered only when that
// marker's health percent changes; one memory DC selects them in turn.
class Markers {
 HWND windows[12]{};bool shown[12]{};POINT placed[12]{};HDC dc=nullptr;HBITMAP bitmaps[12]{},old=nullptr;uint32_t*pixels[12]{};uint8_t drawn[12]{}; // drawn = percent+1, 0 = none
 HWND badge=nullptr;bool badgeShown=false;POINT badgeAt{};HDC badgeDc=nullptr;HBITMAP badgeBitmap=nullptr,badgeOld=nullptr;
 static void premultiply(uint32_t*px,int n){for(int i=0;i<n;i++){uint32_t c=px[i],a=c>>24;px[i]=(a<<24)|((((c>>16)&255)*a/255)<<16)|((((c>>8)&255)*a/255)<<8)|((c&255)*a/255);}}
public:
 ~Markers(){destroy();}
 void destroy(){for(auto&w:windows){if(w)DestroyWindow(w);w=nullptr;}for(auto&s:shown)s=false;if(badge)DestroyWindow(badge);badge=nullptr;badgeShown=false;
  if(dc){if(old)SelectObject(dc,old);DeleteDC(dc);dc=nullptr;}old=nullptr;for(int i=0;i<12;i++){if(bitmaps[i])DeleteObject(bitmaps[i]);bitmaps[i]=nullptr;pixels[i]=nullptr;drawn[i]=0;}
  if(badgeDc){SelectObject(badgeDc,badgeOld);DeleteObject(badgeBitmap);DeleteDC(badgeDc);badgeDc=nullptr;}}
 bool create(HINSTANCE module,HWND owner){
  WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=module;cls.lpszClassName=L"DarkTribeHDMarker0215";
  if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=dtui::MarkW;info.bmiHeader.biHeight=-dtui::MarkH;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  dc=CreateCompatibleDC(nullptr);if(!dc)return false;
  for(int i=0;i<12;i++){void*bits=nullptr;bitmaps[i]=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmaps[i]||!bits)return false;pixels[i]=(uint32_t*)bits;drawn[i]=0;}
  old=(HBITMAP)SelectObject(dc,bitmaps[0]);
  for(auto&w:windows){w=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,L"",WS_POPUP,0,0,dtui::MarkW,dtui::MarkH,owner,nullptr,module,nullptr);if(!w)return false;}
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=dtui::BadgeW;bi.bmiHeader.biHeight=-dtui::BadgeH;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*bits=nullptr;
  badgeDc=CreateCompatibleDC(nullptr);badgeBitmap=CreateDIBSection(badgeDc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!badgeDc||!badgeBitmap)return false;badgeOld=(HBITMAP)SelectObject(badgeDc,badgeBitmap);
  dtui::badgePixels((uint32_t*)bits);premultiply((uint32_t*)bits,dtui::BadgeW*dtui::BadgeH);
  badge=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,L"",WS_POPUP,0,0,dtui::BadgeW,dtui::BadgeH,owner,nullptr,module,nullptr);
  return badge!=nullptr;
 }
 void hide(){for(int i=0;i<12;i++)if(shown[i]){ShowWindow(windows[i],SW_HIDE);shown[i]=false;}}
 void update(HWND owner,const dtui::Data&d,bool visible){
  RECT client{};GetClientRect(owner,&client);
  // Unload targeting: badge attached to the real cursor while it is over the game.
  {POINT c{};bool want=visible&&d.unloadTargeting&&badge&&GetCursorPos(&c);if(want){POINT local=c;ScreenToClient(owner,&local);want=local.x>=0&&local.y>=0&&local.x<client.right&&local.y<client.bottom;}
   if(!want){if(badgeShown){ShowWindow(badge,SW_HIDE);badgeShown=false;}}
   else{POINT at={c.x+14,c.y+14};if(!badgeShown||at.x!=badgeAt.x||at.y!=badgeAt.y){POINT origin={0,0};SIZE size={dtui::BadgeW,dtui::BadgeH};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};UpdateLayeredWindow(badge,nullptr,&at,&size,badgeDc,&origin,0,&blend,ULW_ALPHA);badgeAt=at;if(!badgeShown){ShowWindow(badge,SW_SHOWNOACTIVATE);badgeShown=true;}}}}
  for(int i=0;i<12;i++){
   bool want=visible&&i<d.markCount&&d.renderW&&d.renderH&&windows[i];POINT p{};
   if(want){p.x=d.markX[i]*client.right/d.renderW-dtui::MarkW/2;p.y=d.markY[i]*client.bottom/d.renderH-dtui::MarkH/2;want=p.x>-dtui::MarkW&&p.y>-dtui::MarkH&&p.x<client.right&&p.y<client.bottom;}
   if(!want){if(shown[i]){ShowWindow(windows[i],SW_HIDE);shown[i]=false;}continue;}
   ClientToScreen(owner,&p);
   int percent=std::min<int>(100,d.markHp[i]);bool repaint=drawn[i]!=percent+1&&pixels[i];
   if(repaint){GdiFlush();dtui::markerPixels(pixels[i],percent);premultiply(pixels[i],dtui::MarkW*dtui::MarkH);drawn[i]=(uint8_t)(percent+1);}
   if(!shown[i]||repaint||p.x!=placed[i].x||p.y!=placed[i].y){
    POINT origin={0,0};SIZE size={dtui::MarkW,dtui::MarkH};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    SelectObject(dc,bitmaps[i]);UpdateLayeredWindow(windows[i],nullptr,&p,&size,dc,&origin,0,&blend,ULW_ALPHA);placed[i]=p;
    if(!shown[i]){ShowWindow(windows[i],SW_SHOWNOACTIVATE);shown[i]=true;}
   }
  }
 }
};
// An owned, non-activating window composes above HD's windowed DX11 swapchain.
// It uses only cached snapshots and queued input; it never reads game entities/Lua.
class Host {
public:
 using Read=bool(*)(Snapshot&);using Input=void(*)(const Snapshot&,int,int,UINT);using Log=void(*)(const char*);using Ready=void(*)(bool);
 Read read;Input input;Log log;Ready prepared;HINSTANCE module;HWND owner=nullptr,window=nullptr;Snapshot cached{};Geometry geometry{};Markers markers;
 HDC source=nullptr,destination=nullptr;HBITMAP bitmap=nullptr,scaled=nullptr,oldSource=nullptr,oldDestination=nullptr;
 Host(HINSTANCE m,Read r,Input i,Log l,Ready ready=nullptr):read(r),input(i),log(l),prepared(ready),module(m){}
 ~Host(){teardown();}
 void teardown(){markers.destroy();if(window)DestroyWindow(window);window=nullptr;releaseScaled();if(source){SelectObject(source,oldSource);DeleteObject(bitmap);DeleteDC(source);source=nullptr;bitmap=nullptr;}geometry={};}
 static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM w,LPARAM l){
  Host*p=(Host*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
  if(message==WM_NCCREATE){p=(Host*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)p);}
  if(!p)return DefWindowProcW(hwnd,message,w,l);
  if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
  if(message==WM_NCHITTEST){POINT point={GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd,&point);return interactive(p->cached,p->geometry.logicalX(point.x),p->geometry.logicalY(point.y))?HTCLIENT:HTTRANSPARENT;}
  if(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP){
   int x=p->geometry.logicalX(GET_X_LPARAM(l)),y=p->geometry.logicalY(GET_Y_LPARAM(l));
   if(interactive(p->cached,x,y)&&!p->cached.disabled)p->input(p->cached,x,y,message);return 0;
  }
  if(message==WM_MOUSEWHEEL)return 0;
  if(message==WM_CLOSE)return 0;
  return DefWindowProcW(hwnd,message,w,l);
 }
 bool create(HWND parent){
  owner=parent;WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=module;cls.lpszClassName=L"DarkTribeHDPanel0215";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
  if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
  window=CreateWindowExW(WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Dark Tribe",WS_POPUP,0,0,CanvasWidth,CanvasHeight,parent,nullptr,module,this);
  source=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=CanvasWidth;info.bmiHeader.biHeight=-CanvasHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  void*pixels=nullptr;bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!window||!source||!bitmap)return false;
  oldSource=(HBITMAP)SelectObject(source,bitmap);return true;
 }
 void releaseScaled(){if(destination){SelectObject(destination,oldDestination);DeleteObject(scaled);DeleteDC(destination);destination=nullptr;scaled=nullptr;}}
 // The whole canvas is scaled (width x full); the window shows its top
 // `height` rows, so a short client keeps the same scale and loses only the
 // bottom of the mana section.
 bool draw(Geometry next,POINT point){
  if(!next.width||!next.height||!next.full)return false;
  if(!destination||geometry.width!=next.width||geometry.full!=next.full){
   releaseScaled();destination=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=next.width;info.bmiHeader.biHeight=-next.full;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
   void*pixels=nullptr;scaled=CreateDIBSection(destination,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!scaled)return false;oldDestination=(HBITMAP)SelectObject(destination,scaled);
  }
  geometry=next;paint(source,cached);SetStretchBltMode(destination,COLORONCOLOR);StretchBlt(destination,0,0,next.width,next.full,source,0,0,CanvasWidth,CanvasHeight,SRCCOPY);GdiFlush();
  POINT origin={0,0};SIZE size={next.width,next.height};
  return UpdateLayeredWindow(window,nullptr,&point,&size,destination,&origin,RGB(0,0,0),nullptr,ULW_COLORKEY)!=FALSE;
 }
 static BOOL CALLBACK find(HWND hwnd,LPARAM context){
  Host*p=(Host*)context;DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);RECT r{};
  if(pid==GetCurrentProcessId()&&hwnd!=p->window&&!GetWindow(hwnd,GW_OWNER)&&IsWindowVisible(hwnd)&&GetClientRect(hwnd,&r)&&r.right>=640&&r.bottom>=400){p->owner=hwnd;return FALSE;}return TRUE;
 }
 void run(){
  // The game/HD can replace its window (mode switch, device reset). Re-attach
  // instead of ending the thread, which previously removed the panel for good.
  bool first=true;
  for(;;){
   owner=nullptr;while(!owner){EnumWindows(find,(LPARAM)this);if(!owner)Sleep(100);}
   if(!create(owner)){if(first&&prepared)prepared(false);log("HD panel window creation failed");teardown();Sleep(1000);if(first)return;continue;}
   if(first&&prepared)prepared(true);
   if(!markers.create(module,owner))log("HD selection marker windows unavailable");
   log(first?"HD panel owned window initialized":"HD panel re-attached to a new game window");first=false;
   attached();teardown();
   log("HD panel owner window lost; re-attaching");
  }
 }
 static void reason(char*out,size_t n,unsigned bits,const Snapshot&s){
  snprintf(out,n,"HD_PANEL %s reason=%s%s%s%s%s%s%s%s%s%s",bits?"hidden":"shown",bits&1&&!s.ingame?"not-in-game,":"",bits&1?"inactive,":"",bits&2?"unfocused,":"",bits&4?"owner-invisible,":"",bits&8?"minimised,":"",bits&16?"window-too-small,":"",
   bits&1&&s.hdBlocked?"hd-blocked,":"",bits&1&&!s.scene?"scene-mismatch,":"",bits&1&&!s.running&&!s.disabled?"plugin-not-running,":"",bits&1&&!s.data.ready?"no-data,":"");
 }
 void attached(){
  bool visible=false,reported=false;POINT previous={-1,-1};Snapshot shown{};unsigned lastBits=0xffffffff,drawFailures=0;
  while(IsWindow(owner)){
   MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
   Snapshot fresh;if(read(fresh))cached=fresh;
   RECT client{};GetClientRect(owner,&client);HWND foreground=GetForegroundWindow();bool focused=foreground==owner||GetAncestor(foreground,GA_ROOTOWNER)==owner;
   unsigned dpi=96;using GetDpi=UINT(WINAPI*)(HWND);auto getDpi=(GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");if(getDpi)dpi=getDpi(owner);
   Geometry next=Geometry::fit(client.bottom,dpi);POINT point={0,0};ClientToScreen(owner,&point);
   unsigned bits=(cached.active?0:1)|(focused?0:2)|(IsWindowVisible(owner)?0:4)|(IsIconic(owner)?8:0)|(next.width?0:16);
   // Log every visibility change with its reason (focus changes excluded to keep the log short).
   if((bits&~2u)!=(lastBits&~2u)&&lastBits!=0xffffffff){char line[220];reason(line,sizeof(line),bits&~2u,cached);log(line);}lastBits=bits;
   bool show=!bits;
   if(show){
    Snapshot a=shown,b=cached;a.data.markCount=b.data.markCount=0;memset(a.data.markX,0,sizeof(a.data.markX));memset(b.data.markX,0,sizeof(b.data.markX));memset(a.data.markY,0,sizeof(a.data.markY));memset(b.data.markY,0,sizeof(b.data.markY));
    if(!visible||memcmp(&a,&b,sizeof(a))||next.width!=geometry.width||next.height!=geometry.height||next.full!=geometry.full||point.x!=previous.x||point.y!=previous.y){
     if(draw(next,point)){shown=cached;previous=point;if(!visible)ShowWindow(window,SW_SHOWNOACTIVATE);visible=true;drawFailures=0;if(!reported){log("HD panel composed from cached snapshot; input scaled with panel");reported=true;}}
     else if(drawFailures++%200==0){char line[120];snprintf(line,sizeof(line),"HD panel composition failed (error %lu, %u attempts)",GetLastError(),drawFailures);log(line);}
    }
   }else if(visible){ShowWindow(window,SW_HIDE);visible=false;}
   markers.update(owner,cached.data,focused&&IsWindowVisible(owner)&&!IsIconic(owner)&&cached.active&&!cached.disabled);
   MsgWaitForMultipleObjects(0,nullptr,FALSE,30,QS_ALLINPUT);
  }
 }
};
}
