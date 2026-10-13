#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "gfx-assets.hpp"
#include "queue-model.hpp"
namespace dtui {
struct Data {bool ready=false,buildMenu=false,site=false,recruitmentReady=false,isAircraft=false,flightActive=false,landed=false,coordinateMode=false;int aircraftX=0,aircraftY=0,targetX=0,targetY=0,draftX=0,draftY=0;uint32_t passengers=0,boarding=0,group=0,groupPassengers=0,groupUnload=0;
 // Selected-aircraft markers in game render pixels (tick cached); renderW/H = render surface size.
 int16_t markX[12]{},markY[12]{};uint8_t markCount=0;uint16_t renderW=0,renderH=0;
 bool unloadTargeting=false; /* Unload pressed: waiting for the map click. */
 uint8_t markHp[12]{}; /* Health percent 0..100 of each marked aircraft. */
 /* Dark unit selection panel: native selection of own Dark settlers. */
 bool unitPanel=false;uint8_t unitTypes=0;uint16_t unitKind[6]{},unitCount[6]{};uint8_t unitHealth[6]{};uint16_t unitTotal=0;bool unitMilitary=false;
 uint16_t passengerKind[6]{}; /* Settler type of each shown passenger (0 = unavailable). */
 uint8_t aircraftHp=0; /* Health percent of the selected aircraft. */
 uint8_t recruitPick=0; /* Temple/Fortress: selected recruitment unit 0..3 (RecruitUnits). UI state set by panel input, not by the tick. */
 /* Manacopter panel (tick cached): the first airShown (<= 6) aircraft of the pick with their entity/identity (for the keep
    command), health percent, passengers and landed state; groupHp = average health of the whole pick; its passengers by type
    (most numerous first, riderTypes counts every type). */
 uint8_t airShown=0,airHp[6]{},airLoad[6]{},groupHp=0,riderTypes=0;bool airLanded[6]{};uint16_t airId[6]{},riderKind[4]{},riderCount[4]{};uint32_t airIdentity[6]{};
char passengerLines[6][32]{};uint16_t selected=0,kind=0;uint32_t selectedIdentity=0,mana=0,queues[6]{},aircraftCount=0,aircraftSeconds=0,progress=0;char message[160]{};
 uint8_t buildLimit[4]{}; /* Construction tiles (BuildTypes order): 0 free, TileBuilt, TileBuilding. One Dark Temple and one Dark Fortress per player; tick cached. */
};
struct Action {int type=0,arg=0,amount=0;};
inline COLORREF healthColor(int percent){return percent>=66?RGB(64,200,72):percent>=33?RGB(232,196,48):RGB(214,58,40);}
// ---- Native layout -----------------------------------------------------------
// Canvas coordinates shared by the HD owned window and the classic GUI pillar
// (x relative to the pillar). They follow the game's own GUI definition
// (Menu/GUISetRomans.DAT): the content panel 9.gfx #589 (210x390) at y 210,
// used by every Dark page including construction, the construction tab of
// the minimap frame at (9,176), and the resource panel (menu 1) at y 600.
constexpr int PanelW=210,PanelY=210,PanelH=390;
// Covered in every mode: the HD canvas is 200 wide; columns 200..209 are the
// native frame, identical in every native menu.
constexpr int BoxX=0,BoxY=210,BoxW=200,BoxH=390;
// Inner (moss) areas inside the gold frames, measured from the sprites. #589
// has the gold frame at x 6..8 and 196..198 with a dark inset at x 9..10 and
// 194..195; its content box has gold at y 250..252 and 591..593 with the
// inset at 253..254 and 589..590. The title box of #589 has gold at y
// 218..220 and 242..244 with the inset at 221..222 and 240..241.
// Bounds are half-open [0,1). Every text box lies inside one of these areas;
// glyphs keep TextPad px clear of the inset on the left and top, and their
// 1-px shadow (right and below) may reach it. The drawn fallback draws the
// same frames.
constexpr int InnerX0=11,InnerX1=194,InnerY0=255,InnerY1=589,TitleY0=223,TitleY1=240,TextPad=1;
// Full-width text lines align with the separator rule (x 17..192).
constexpr int TextX=17,TextW=176;
constexpr int TabX=9,TabY=176,TabW=43,TabH=34;
// Bottom section: the native resource panel (menu 1, 9.gfx #587, 210x168 at
// y 600) is always covered by the Dark mana section, directly below the Dark
// menus. #587 has the side frame of #589 (same InnerX0/InnerX1) and gold at
// y 603..605 and 759..761 with the inset at 606..607 and 757..758.
constexpr int ManaY=600,ManaH=168,ManaInnerY0=608,ManaInnerY1=757;
// The mana section's area: shown and click-swallowing in every mode (no commands).
inline bool manaArea(int x,int y){return x>=0&&x<PanelW&&y>=ManaY&&y<ManaY+ManaH;}
// Four large building tiles (2x2) fill the page: the picture (IconW x IconH,
// bottom-aligned), the name on two lines and the mana cost below it. Each
// tile is clickable as a whole.
constexpr int CellX[2]={InnerX0+1,InnerX0+92},CellW=91,CellY[2]={260,426},CellH=152,IconW=88,IconH=92,CellRuleY=418;
constexpr int BuildTypes[4]={49,50,51,80};constexpr uint32_t BuildCosts[4]={25,100,150,120};
// A tile whose building may exist only once (Temple, Fortress) and already
// exists is disabled like an unaffordable one, with its state instead of the cost.
constexpr uint8_t TileBuilt=1,TileBuilding=2;
inline bool buildable(const struct Data&v,int c){return v.mana>=BuildCosts[c]&&!v.buildLimit[c];}
// Building panels (menus 14/16): title text box (glyphs bottom-aligned in the
// 17-px title box), picture, right-hand buttons. A picture with a vertical bar
// beside it (Hall, Manacopter) moves left; the info column (mana, counts)
// fills the space between the frame and the picture.
constexpr int TitleX=13,TitleY=223,TitleW=179,TitleH=16,PictureY=259,RightX=148,RightY=258,RightW=45,RightH=24,SecondX=143,SecondY=288,SecondW=52,SecondH=22;
constexpr int PictureX=56,BarPictureX=46,BarX=131,BarY=261,BarW=10,BarH=65,InfoX=InnerX0+1;
inline constexpr int infoWidth(int pictureX){return pictureX-1-InfoX;}
// Hall queue count under the demolish button, beside the progress bar.
constexpr int QueueX=147,QueueW=46;
constexpr int MessageY=528,MessageH=60; // notices of building and aircraft panels: four whole lines
constexpr int SeparatorY=340;
// Temple/Fortress recruitment works like the native barracks recruitment menu
// (GUISetRomans.DAT menu 46), inside the building panel: the four unit types
// are radio cells (2x2; the selected one gets the native gold corner brackets)
// and one native control cluster acts on the selected type. Cluster order and
// spacing follow menu 46: infinite, up 5, up 1 on top; clear, down 5, down 1
// below; 26x28 buttons, 1 px apart vertically, columns 53/54 px apart.
constexpr int RecruitUnits[4]={29,32,52,54},RecruitCosts[4]={15,15,5,10},RecruitQueue[4]={4,5,2,3};
constexpr const char*RecruitNames[4]={"Warrior","Thrower","Gardener","Shaman"};
constexpr int RecruitX[2]={InnerX0+1,InnerX0+92},RecruitY[2]={345,401},RecruitW=90,RecruitH=54,RecruitTextDX=39,RecruitTextW[2]={55,52}; /* text column right of the selection bracket (portrait +2..+38) and its antialiasing; the left one may run over the cell gap, the right one stops at the frame */
constexpr int ClusterX[3]={39,92,146},ClusterY[2]={475,504},ClusterW=26,ClusterH=28,RecruitCaptionY=457,RecruitMessageY=542,RecruitMessageH=45;
constexpr int ClusterAmount[2][3]={{dt::OrderInfinite,5,1},{dt::OrderClear,-5,-1}};
// Commands: RecruitSelect (arg = row) only changes UI state in panel input and
// is never queued; RecruitOrder (arg = settler type, amount = native order)
// is validated and applied on the tick.
constexpr int RecruitSelect=21,RecruitOrder=22;
// Action rows: a native icon button (46x30 cell) with its label, ending at the text column.
constexpr int ActionX=14,ActionW=TextX+TextW-ActionX,ActionH=30,ActionIconW=52,ActionLabelX=68;
// Manacopter Hall: the aircraft-panel grid shows the build queue (first cell in
// production with its progress, the others waiting), the summary row the queue,
// and the action rows queue/cancel/select/demolish. Each takes HallBuildSeconds.
constexpr int HallCost=300,HallBuildSeconds=30,HallActions=5;
constexpr int HallRows[HallActions]={3,9,10,11,5};
// Manacopter panel: the Dark unit panel's layout (unit-panel.hpp; panel-tests
// checks they match). Aircraft cells on the 60-px grid of the native selection
// menu: the Manacopter picture (sitting on the cell floor when landed, raised
// over a shadow when flying), the native 6x41 health bar and the passengers
// aboard below. The second row holds aircraft 4-6, or else the passengers of
// the pick by type (portrait and count on the same grid; 45 px apart for four
// types); a notice replaces it while shown. Then the summary row and five
// labelled action rows.
constexpr int AirX0=19,AirDX=60,AirRowY[2]={262,338},AirCellW=56,AirCellH=67,AirRuleY[2]={331,407},AirSummaryY=414;
constexpr int AirPictureDX=-8,AirPictureW=44,AirPictureH=46; // picture box relative to the portrait origin, ending at the count (y+47)
constexpr int RiderX0=19,RiderDX=45,RiderShown=4;
constexpr int AirActionY=440,AirActionStep=29,AirActions=5;
// Commands: keep only the clicked aircraft of the pick (UI sends its entity and
// identity; validated on the tick).
constexpr int AircraftKeep=37;
inline bool inside(int x,int y,int rx,int ry,int w,int h){return x>=rx&&x<rx+w&&y>=ry&&y<ry+h;}
inline int actionRow(int x,int y,int top,int step,int rows){if(x<ActionX||x>=ActionX+ActionW||y<top)return -1;int r=(y-top)/step;return r<rows&&(y-top)%step<ActionH?r:-1;}
inline int buildCell(int x,int y){
 int col=-1,row=-1;for(int c=0;c<2;c++){if(x>=CellX[c]&&x<CellX[c]+CellW)col=c;if(y>=CellY[c]&&y<CellY[c]+CellH)row=c;}
 return col<0||row<0?-1:row*2+col;
}
inline int recruitCell(int x,int y){for(int r=0;r<4;r++)if(inside(x,y,RecruitX[r%2],RecruitY[r/2],RecruitW,RecruitH))return r;return -1;}
inline int clusterButton(int x,int y){for(int b=0;b<6;b++)if(inside(x,y,ClusterX[b%3],ClusterY[b/3],ClusterW,ClusterH))return b;return -1;}
inline int recruitRow(const struct Data&v){return v.recruitPick<4?v.recruitPick:0;}
inline uint32_t manualOrders(const struct Data&v){uint32_t n=0;for(int r=0;r<4;r++)n+=v.queues[RecruitQueue[r]];return n;}
// Manacopter panel: portrait origin of aircraft cell i (as unitCell), the cells
// shown (a notice replaces the second row) and whether anything can unload.
inline void airCell(int i,int&x,int&y){x=AirX0+(i%3)*AirDX;y=AirRowY[i/3];}
inline bool airNotice(const struct Data&v){return v.message[0]!=0;}
inline int airCells(const struct Data&v){int n=std::min<int>(v.airShown,6);return airNotice(v)?std::min(n,3):n;}
inline bool airUnload(const struct Data&v){return v.passengers||v.groupPassengers||v.groupUnload;}
// Dark unit selection panel (native/unit-panel.hpp).
inline void renderUnits(HDC dc,int left,const struct Data&v);
inline Action hitUnits(const struct Data&v,int x,int y);
inline Action hit(const Data&v,bool build,int x,int y){
 if(x<BoxX||x>=BoxX+BoxW||y<BoxY||y>=BoxY+BoxH)return {};
 if(build){int c=buildCell(x,y);return c>=0&&buildable(v,c)?Action{1,BuildTypes[c]}:Action{};}
 if(v.unitPanel&&!v.isAircraft)return hitUnits(v,x,y);
 if(v.site)return {};
 if(v.isAircraft){
  /* An aircraft cell keeps only that aircraft (like the unit portraits; nothing to filter with one). */
  for(int i=0;v.group>1&&i<airCells(v);i++){int cx,cy;airCell(i,cx,cy);if(inside(x,y,cx+AirPictureDX,cy,AirCellW,AirCellH))return {AircraftKeep,i};}
  /* Unload (only with passengers), load nearby units, return to base, next Manacopter, deselect. */
  static const int rows[AirActions]={16,36,12,11,13};int r=actionRow(x,y,AirActionY,AirActionStep,AirActions);
  return r<0||(r==0&&!airUnload(v))?Action{}:Action{rows[r],0};
 }
 if(v.kind==80){
  /* Queue (enough mana), cancel last / all (something queued), select a ready Manacopter, demolish the Hall. */
  int r=actionRow(x,y,AirActionY,AirActionStep,HallActions);if(r<0)return {};bool queued=v.aircraftCount>0;
  const bool on[HallActions]={v.mana>=(uint32_t)HallCost,queued,queued,true,true};return on[r]?Action{HallRows[r],0}:Action{};
 }
 if(inside(x,y,RightX,RightY,RightW,RightH))return {5,0};
 if((v.kind==50||v.kind==51)&&v.recruitmentReady){
  if(inside(x,y,SecondX,SecondY,SecondW,SecondH))return manualOrders(v)?Action{8,0}:Action{};
  int r=recruitCell(x,y);if(r>=0)return {RecruitSelect,r};
  int b=clusterButton(x,y),pick=recruitRow(v);
  if(b>=0){int amount=ClusterAmount[b/3][b%3];return dt::canOrder(v.queues[RecruitQueue[pick]],amount)?Action{RecruitOrder,RecruitUnits[pick],amount}:Action{};}
 }
 return {};
}
// ---- Palette and fonts: the game's own GUI font table -------------------------
// Fonts are the native Arial entries (S4_Main.exe font table, created with
// ANTIALIASED_QUALITY): f1 20 bold, f3 18 bold, f4 18 medium, f9 15 regular,
// f11 14 bold, f14 14 bold light blue. White ink, 1-px dark drop shadow.
// No visible pixel may be pure black: under HD RGB(0,0,0) is the colour key.
constexpr COLORREF Ink=RGB(254,254,254),Red=RGB(254,0,0),Blue=RGB(117,193,219),Grey=RGB(150,150,138),Shade=RGB(16,14,8),
 Gold=RGB(232,196,112),GoldDark=RGB(108,78,34),Edge=RGB(34,26,10);
enum Font {F1,F3,F4,F9,F11,F14,FontCount};
inline HFONT font(Font f){
 static HFONT fonts[FontCount]{};static const int spec[FontCount][2]={{20,700},{18,700},{18,500},{15,400},{14,700},{14,700}};
 if(!fonts[f])fonts[f]=CreateFontA(spec[f][0],0,0,0,spec[f][1],0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,"Arial");return fonts[f];
}
enum Align {AlignLeft,AlignCenter,AlignRight};
// ---- Layout test hooks ----------------------------------------------------------
// nullptr in the DLL (one pointer test per call). panel-tests.cpp sets them to
// collect every text box with its measured extent and every drawn part, then
// checks them against the frame bounds above.
struct LabelProbe {const char*text;Font font;RECT box;bool wrap;UINT format;SIZE extent;}; // extent: single line = text size, wrapped = DT_CALCRECT at the box width
enum Part {PartIcon=-1,PartButton=-2,PartBar=-3,PartSlot=-4,PartRule=-5,PartTab=-6}; // drawn stand-ins; sprites report their dtgfx::Art
inline void(*labelProbe)(HDC,const LabelProbe&)=nullptr;
inline void(*partProbe)(const RECT&,int part)=nullptr;
inline void part(int x,int y,int w,int h,int what){if(partProbe)partProbe(RECT{x,y,x+w,y+h},what);}
// Left-aligned text whose first glyph overhangs to the left (negative A width)
// starts that many pixels to the right so no ink leaves its box. The font
// must be selected. (One column of antialiasing may still reach the box edge.)
inline int leadIn(HDC dc,const char*s,UINT fmt){
 if((fmt&(DT_CENTER|DT_RIGHT))||!s||!s[0])return 0;ABC abc{};return GetCharABCWidthsA(dc,(BYTE)s[0],(BYTE)s[0],&abc)&&abc.abcA<0?-abc.abcA:0;
}
// Text in box (x,y,w,h) with the native 1-px shadow; fmt is the DrawText format.
inline void text(HDC dc,int x,int y,int w,int h,const char*s,Font f,COLORREF color,UINT fmt){
 SelectObject(dc,font(f));int dx=leadIn(dc,s,fmt);
 if(labelProbe){bool wrap=(fmt&DT_WORDBREAK)!=0;SIZE e{};if(wrap){RECT c{x,y,x+w,y};DrawTextA(dc,s,-1,&c,(fmt&(DT_NOPREFIX|DT_CENTER|DT_RIGHT))|DT_WORDBREAK|DT_CALCRECT);e={c.right-c.left,c.bottom-c.top};}else{GetTextExtentPoint32A(dc,s,(int)strlen(s),&e);e.cx+=dx;}labelProbe(dc,{s,f,{x,y,x+w,y+h},wrap,fmt,e});}

 RECT o{x+dx+1,y+1,x+w+1,y+h+1};SetTextColor(dc,Shade);DrawTextA(dc,s,-1,&o,fmt);RECT r{x+dx,y,x+w,y+h};SetTextColor(dc,f==F14&&color==Ink?Blue:color);DrawTextA(dc,s,-1,&r,fmt);
}
// Single lines are centred vertically and end in an ellipsis when too long;
// wrapped text shows whole lines only (DT_EDITCONTROL), the last one ending
// in an ellipsis when the text does not fit.
inline void label(HDC dc,int x,int y,int w,int h,const char*s,Font f,COLORREF color,Align a=AlignLeft,bool wrap=false){
 UINT align=a==AlignCenter?DT_CENTER:a==AlignRight?DT_RIGHT:DT_LEFT;
 text(dc,x,y,w,h,s,f,color,DT_NOPREFIX|align|(wrap?DT_WORDBREAK|DT_EDITCONTROL|DT_END_ELLIPSIS:DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS));
}
inline int textWidth(HDC dc,const char*s,Font f){SelectObject(dc,font(f));SIZE z{};GetTextExtentPoint32A(dc,s,(int)strlen(s),&z);return z.cx;}
// ---- Drawing primitives -------------------------------------------------------
inline COLORREF mix(COLORREF a,COLORREF b,int t){t=std::max(0,std::min(256,t));return RGB((GetRValue(a)*(256-t)+GetRValue(b)*t)>>8,(GetGValue(a)*(256-t)+GetGValue(b)*t)>>8,(GetBValue(a)*(256-t)+GetBValue(b)*t)>>8);}
inline void box(HDC dc,int x,int y,int w,int h,COLORREF color){RECT r{x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
inline void fill(HDC dc,int x,int y,int w,int h,COLORREF color){if(w<=0||h<=0)return;RECT r{x,y,x+w,y+h};COLORREF old=SetBkColor(dc,color);ExtTextOutA(dc,0,0,ETO_OPAQUE,&r,nullptr,0,nullptr);SetBkColor(dc,old);}
inline void polygon(HDC dc,const POINT*pts,int n,COLORREF fill,COLORREF edge){HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,edge);auto ob=SelectObject(dc,b);auto op=SelectObject(dc,p);Polygon(dc,pts,n);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);}
inline void oval(HDC dc,int x,int y,int w,int h,COLORREF fill){HBRUSH b=CreateSolidBrush(fill);auto ob=SelectObject(dc,b);auto op=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,x,y,x+w,y+h);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);}
inline void line(HDC dc,int x,int y,int a,int b,COLORREF color,int width=2){HPEN p=CreatePen(PS_SOLID,width,color);auto old=SelectObject(dc,p);MoveToEx(dc,x,y,nullptr);LineTo(dc,a,b);SelectObject(dc,old);DeleteObject(p);}
inline dtgfx::Library&art(){return dtgfx::library();}
inline bool sprite(HDC dc,int x,int y,int a,BYTE alpha=255){if(!art().ok||!art().draw(dc,x,y,(dtgfx::Art)a,alpha))return false;const auto&m=art()[(dtgfx::Art)a];part(x,y,m.w,m.h,a);return true;}
// ---- Drawn fallback (game files differ from the pins) -------------------------
// Vector icons on a 32x32 grid. muted=true draws the greyed (unaffordable) look.
inline void icon(HDC dc,int x,int y,int size,int kind,bool muted=false){part(x,y,size,size,PartIcon);POINT org{};GetViewportOrgEx(dc,&org);int save=SaveDC(dc);SetMapMode(dc,MM_ANISOTROPIC);SetWindowExtEx(dc,32,32,nullptr);SetViewportExtEx(dc,size,size,nullptr);SetViewportOrgEx(dc,org.x+x,org.y+y,nullptr);
 auto tone=[&](COLORREF c){if(!muted)return c;int l=(GetRValue(c)*3+GetGValue(c)*5+GetBValue(c)*2)/10;l=48+l*5/8;return RGB(l,l,l*9/10);};
 COLORREF gold=tone(RGB(222,186,110)),silver=tone(RGB(224,236,232)),purple=tone(RGB(150,98,200)),green=tone(RGB(92,218,178)),dark=tone(RGB(34,26,44)),cap=tone(RGB(186,72,116));
 if(kind==49){box(dc,13,15,7,14,gold);box(dc,12,28,9,2,tone(RGB(120,92,48)));oval(dc,3,4,27,17,cap);oval(dc,7,8,4,4,silver);oval(dc,19,6,4,4,silver);oval(dc,23,12,3,3,silver);}
 else if(kind==50){POINT roof[]={{2,13},{16,2},{30,13}};polygon(dc,roof,3,purple,gold);box(dc,5,13,22,3,gold);for(int a=6;a<=24;a+=9)box(dc,a,16,3,12,gold);box(dc,3,28,27,3,gold);}
 else if(kind==51){box(dc,3,10,27,21,purple);for(int a=3;a<30;a+=10)box(dc,a,3,6,10,gold);box(dc,3,12,27,3,gold);box(dc,14,21,7,11,dark);line(dc,6,20,10,20,gold,1);line(dc,23,20,27,20,gold,1);}
 else if(kind==80){oval(dc,8,13,17,11,purple);line(dc,16,4,16,14,gold);line(dc,2,5,30,5,silver);line(dc,3,12,9,17,gold);line(dc,10,29,26,29,silver);line(dc,13,23,13,29,gold);line(dc,22,23,22,29,gold);}
 else if(kind==29){POINT blade[]={{10,23},{24,2},{28,3},{17,26}};polygon(dc,blade,4,silver,gold);line(dc,5,21,20,30,gold,3);line(dc,11,26,7,31,purple,4);}
 else if(kind==32){line(dc,3,6,18,15,gold);line(dc,18,15,10,28,gold);oval(dc,18,6,11,11,silver);line(dc,4,7,12,28,purple,3);}
 else if(kind==52){POINT leaf[]={{6,27},{5,10},{22,3},{28,12},{17,23}};polygon(dc,leaf,5,green,gold);line(dc,5,29,23,8,gold,2);}
 else if(kind==54){line(dc,10,7,10,30,gold,3);oval(dc,2,0,16,16,purple);oval(dc,6,4,8,8,green);POINT star[]={{25,5},{27,11},{31,13},{27,15},{25,21},{23,15},{19,13},{23,11}};polygon(dc,star,8,gold,gold);}
 else {POINT crystal[]={{16,1},{28,13},{19,31},{7,20},{5,9}};polygon(dc,crystal,5,green,silver);POINT facet[]={{16,1},{15,25},{7,20},{5,9}};polygon(dc,facet,4,tone(RGB(56,170,150)),silver);line(dc,16,2,15,25,silver,1);}
 RestoreDC(dc,save);}
// Procedural moss parchment for the fallback background (generated once).
struct Texture {
 uint32_t light[BoxW*BoxH];BITMAPINFO info{};
 static uint32_t hash(int x,int y,uint32_t seed){uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u+seed*2246822519u;h=(h^(h>>13))*1274126177u;return h^(h>>16);}
 static float noise(int x,int y,int cell,uint32_t seed){
  int cx=x/cell,cy=y/cell;float fx=(float)(x%cell)/cell,fy=(float)(y%cell)/cell;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
  auto at=[&](int a,int b){return (hash(a,b,seed)&0xffff)/65535.0f;};
  float top=at(cx,cy)+(at(cx+1,cy)-at(cx,cy))*fx,bottom=at(cx,cy+1)+(at(cx+1,cy+1)-at(cx,cy+1))*fx;return top+(bottom-top)*fy;
 }
 Texture(){
  info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=BoxW;info.bmiHeader.biHeight=-BoxH;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  for(int y=0;y<BoxH;y++)for(int x=0;x<BoxW;x++){
   float n=0.5f*noise(x,y,23,1)+0.32f*noise(x,y,9,2)+0.18f*noise(x,y,4,3);n=std::max(0.0f,std::min(1.0f,(n-0.5f)*1.7f+0.5f));int grain=(int)(hash(x,y,7)&15)-7;
   auto c=[](float v,int lo){return (uint32_t)std::max(lo,std::min(255,(int)v));};
   light[y*BoxW+x]=(c(52+n*44+grain*0.6f,10)<<16)|(c(64+n*46+grain*0.7f,12)<<8)|c(28+n*22+grain*0.4f,6);
  }
 }
};
inline const Texture&texture(){static const Texture*t=new Texture();return *t;}
inline void bevel(HDC dc,int x,int y,int w,int h,bool raised){
 COLORREF a=raised?Gold:GoldDark,b=raised?GoldDark:Gold;
 fill(dc,x,y,w,1,Edge);fill(dc,x,y+h-1,w,1,Edge);fill(dc,x,y,1,h,Edge);fill(dc,x+w-1,y,1,h,Edge);
 fill(dc,x+1,y+1,w-2,1,a);fill(dc,x+1,y+1,1,h-2,a);fill(dc,x+1,y+h-2,w-2,1,b);fill(dc,x+w-2,y+1,1,h-2,b);
}
// Fallback stand-in for the native gold frames: 3-px gold rim and a 2-px dark
// inset around the inner area [InnerX0,InnerX1) x [y0+5,y1-5).
inline void goldFrame(HDC dc,int left,int y0,int y1){
 const COLORREF ring[5]={RGB(197,145,65),RGB(246,206,65),RGB(148,85,8),RGB(49,32,8),RGB(16,20,16)};
 for(int i=0;i<5;i++){int x0=left+InnerX0-5+i,x1=left+InnerX1+4-i,a=y0+i,b=y1-1-i;fill(dc,x0,a,x1-x0+1,1,ring[i]);fill(dc,x0,b,x1-x0+1,1,ring[i]);fill(dc,x0,a,1,b-a+1,ring[i]);fill(dc,x1,a,1,b-a+1,ring[i]);}
}
// Fallback stand-in for a native icon button: a small bronze bevel with a glyph.
inline void glyphButton(HDC dc,int x,int y,int w,int h,const char*glyph,bool enabled){
 part(x,y,w,h,PartButton);
 for(int i=1;i<h-1;i++)fill(dc,x+1,y+i,w-2,1,mix(enabled?RGB(156,126,70):RGB(86,88,66),enabled?RGB(90,68,32):RGB(62,64,48),(i-1)*256/std::max(1,h-3)));
 bevel(dc,x,y,w,h,true);label(dc,x,y,w,h,glyph,F11,enabled?Ink:Grey,AlignCenter);
}
inline void tabButton(HDC dc,int x,int y,int w,int h,int kind){
 part(x,y,w,h,PartTab);
 for(int i=1;i<h-1;i++)fill(dc,x+1,y+i,w-2,1,mix(RGB(118,98,56),RGB(64,50,24),(i-1)*256/std::max(1,h-3)));bevel(dc,x,y,w,h,true);
 int s=std::min(w,h)-6;icon(dc,x+(w-s)/2,y+(h-s)/2,s,kind);
}
// ---- Native parts (sprite first, drawn fallback second) -----------------------
// Every Dark page, construction included, uses the native content panel #589
// with its title box (the former construction strip #592 and page #590 are
// no longer drawn). The third argument is ignored; kept for existing callers.
inline void background(HDC dc,int left,bool=false){
 if(sprite(dc,left,PanelY,dtgfx::Panel))return;
 const Texture&t=texture();StretchDIBits(dc,left+BoxX,BoxY,BoxW,BoxH,0,0,BoxW,BoxH,t.light,&t.info,DIB_RGB_COLORS,SRCCOPY);
 goldFrame(dc,left,TitleY0-5,TitleY1+5);goldFrame(dc,left,InnerY0-5,InnerY1+5);
}
inline void title(HDC dc,int left,const char*s){text(dc,left+TitleX,TitleY,TitleW,TitleH,s,F4,Ink,DT_NOPREFIX|DT_CENTER|DT_SINGLELINE|DT_BOTTOM|DT_END_ELLIPSIS);}
inline void separator(HDC dc,int left,int y){if(sprite(dc,left+17,y,dtgfx::Separator))return;part(left+17,y,176,5,PartRule);fill(dc,left+17,y+1,176,1,Gold);fill(dc,left+17,y+2,176,1,GoldDark);fill(dc,left+17,y+3,176,1,Edge);}
inline void crystal(HDC dc,int x,int y,bool large=false){if(!sprite(dc,x,y,large?dtgfx::CrystalLarge:dtgfx::Crystal))icon(dc,x,y,large?24:14,0);}
// A native 45x24 / 52x22 icon button; disabled ones are faded like native.
inline void iconButton(HDC dc,int x,int y,int w,int h,int a,const char*glyph,bool enabled){if(!sprite(dc,x,y,a,enabled?255:110))glyphButton(dc,x,y,w,h,glyph,enabled);}
inline void picture(HDC dc,int x,int y,int kind,bool site){
 int a=kind==49?dtgfx::FarmPicture:kind==50?dtgfx::TemplePicture:kind==51?dtgfx::FortressPicture:kind==80?dtgfx::HallPicture:-1;
 if(a>=0&&site)a+=dtgfx::SitePictureFarm-dtgfx::FarmPicture;
 if(a<0||!sprite(dc,x,y,a))icon(dc,x+10,y+5,64,kind,site);
}
// Settler type -> portrait (sprites resolved from the engine's animation table by tools/gfx_pins.py).
inline int portraitArt(uint16_t kind){return kind>=29&&kind<=31?dtgfx::WarriorPortrait:kind>=32&&kind<=34?dtgfx::ThrowerPortrait:kind==52?dtgfx::GardenerPortrait:kind==54?dtgfx::ShamanPortrait:kind==53?dtgfx::FarmerPortrait:kind==55?dtgfx::SlavePortrait:kind==66?dtgfx::PilotPortrait:-1;}
// Native settler portrait cell (30x47, like the selection menu).
inline void portrait(HDC dc,int x,int y,uint16_t kind){int a=portraitArt(kind);if(a<0||!sprite(dc,x,y,a))icon(dc,x,y+8,30,kind==30||kind==31?29:kind==33||kind==34?32:kind);}
inline void emptySlot(HDC dc,int x,int y){part(x,y,30,47,PartSlot);fill(dc,x+1,y,28,47,RGB(98,80,36));fill(dc,x+2,y+1,26,45,RGB(46,54,24));}
// Vertical native bar (frame sprite) filled from the bottom.
inline void vbar(HDC dc,int x,int y,int w,int h,int frame,int percent,COLORREF color){
 if(!sprite(dc,x,y,frame)){part(x,y,w,h,PartBar);fill(dc,x,y,w,h,Edge);fill(dc,x+1,y+1,w-2,h-2,RGB(30,28,16));}
 int inner=h-2,f=std::max(0,std::min(100,percent))*inner/100;fill(dc,x+1,y+1+inner-f,w-2,f,color);if(f)fill(dc,x+1,y+1+inner-f,1,f,mix(color,RGB(255,255,230),110));
}
inline void healthBar(HDC dc,int x,int y,int w,int h,int percent){
 part(x,y,w,h,PartBar);int p=std::max(0,std::min(100,percent));COLORREF c=healthColor(p);fill(dc,x,y,w,h,Edge);fill(dc,x+1,y+1,w-2,h-2,RGB(42,36,20));
 if(h>w){int f=p*(h-2)/100;fill(dc,x+1,y+h-1-f,w-2,f,c);if(f)fill(dc,x+1,y+h-1-f,1,f,mix(c,RGB(255,255,230),110));}
 else{int f=p*(w-2)/100;fill(dc,x+1,y+1,f,h-2,c);if(f)fill(dc,x+1,y+1,f,1,mix(c,RGB(255,255,230),110));}
}
// Icon button cell plus label: the native panels' icon-and-number pattern.
inline void action(HDC dc,int left,int y,int a,const char*glyph,const char*caption,bool enabled){
 int x=left+ActionX;const auto&im=art()[(dtgfx::Art)a];
 if(art().ok&&!im.empty())sprite(dc,x+(ActionIconW-im.w)/2,y+(ActionH-im.h)/2,a,enabled?255:110);else glyphButton(dc,x+3,y+3,ActionIconW-6,ActionH-6,glyph,enabled);
 int room=ActionX+ActionW-ActionLabelX;Font f=textWidth(dc,caption,F11)<=room?F11:F9;label(dc,left+ActionLabelX,y,room,ActionH,caption,f,enabled?Ink:Grey);
}
// The native construction tab, pressed, while the Dark construction page shows.
inline void houseTab(HDC dc,int left){if(!sprite(dc,left+TabX,TabY,dtgfx::HouseTabOn))tabButton(dc,left+TabX,TabY+3,TabW-6,TabH-5,51);}
// The first font of `fonts` in which s fits `room` px (the last one otherwise).
inline Font fitting(HDC dc,const char*s,int room,std::initializer_list<Font> fonts){Font f=F11;for(Font c:fonts){f=c;if(textWidth(dc,s,c)<=room)break;}return f;}
// Info column left of the picture: a large number over a small word.
inline void info(HDC dc,int left,int pictureX,int y,const char*value,const char*word,COLORREF color=Ink){
 int w=infoWidth(pictureX);label(dc,left+InfoX,y,w,20,value,fitting(dc,value,w,{F1,F3,F11}),color,AlignCenter);label(dc,left+InfoX,y+20,w,16,word,F9,color,AlignCenter);
}
// ---- Pages ----------------------------------------------------------------------
inline void renderBuild(HDC dc,int left,const Data&v){
 background(dc,left);title(dc,left,"Dark Buildings");char s[16];
 static const char*names[4][2]={{"Mushroom","Farm"},{"Dark","Temple"},{"Dark","Fortress"},{"Manacopter","Hall"}};
 for(int c=0;c<4;c++){int x=left+CellX[c%2],y=CellY[c/2];bool ok=buildable(v,c);COLORREF ink=ok?Ink:Grey;
  int px=x+(CellW-IconW)/2,py=y+2;if(!sprite(dc,px,py,(ok?dtgfx::FarmIcon:dtgfx::FarmIconGrey)+c))icon(dc,px+14,py+16,60,BuildTypes[c],!ok);
  label(dc,x,y+96,CellW,16,names[c][0],F11,ink,AlignCenter);label(dc,x,y+111,CellW,16,names[c][1],F11,ink,AlignCenter);
  /* Only one Temple and one Fortress: while one exists its state replaces the cost. */
  if(v.buildLimit[c]){label(dc,x,y+129,CellW,20,v.buildLimit[c]==TileBuilt?"Built":"Building",F14,Ink,AlignCenter);continue;}
  /* Cost: crystal and number centred together under the name, red when unaffordable. */
  snprintf(s,sizeof(s),"%u",BuildCosts[c]);int w=textWidth(dc,s,F1),cx=x+(CellW-(14+w))/2;crystal(dc,cx,y+131);label(dc,cx+14,y+129,w+2,20,s,F1,ok?Ink:Red);
 }
 separator(dc,left,CellRuleY);
}
// "Infinity" (U+221E) is outside the ANSI code page of label(): drawn as Unicode
// in bold Arial, same ink and shadow. Arial's sign is small for its size, so a
// `px`-high font draws it about as tall as digits of a font two thirds that size.
inline void infinity(HDC dc,int x,int y,int w,int h,int px,COLORREF color,Align a=AlignCenter){
 static HFONT fonts[4]{};static int sizes[4]{};HFONT f=nullptr;for(int i=0;i<4&&!f;i++){if(!sizes[i]){sizes[i]=px;fonts[i]=CreateFontA(px,0,0,0,700,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,"Arial");}if(sizes[i]==px)f=fonts[i];}
 SelectObject(dc,f?f:font(F1));UINT fmt=DT_NOPREFIX|DT_SINGLELINE|DT_VCENTER|(a==AlignCenter?DT_CENTER:a==AlignRight?DT_RIGHT:DT_LEFT);
 static const wchar_t sign[2]={0x221E,0};
 RECT o{x+1,y+1,x+w+1,y+h+1};SetTextColor(dc,Shade);DrawTextW(dc,sign,1,&o,fmt);RECT r{x,y,x+w,y+h};SetTextColor(dc,color);DrawTextW(dc,sign,1,&r,fmt);
}
// Native selection bracket around a w x h cell: the gold corners of a pressed native tab.
inline void bracket(HDC dc,int x,int y,int w,int h){
 if(art().ok&&!art()[dtgfx::BracketTL].empty()){sprite(dc,x,y,dtgfx::BracketTL);sprite(dc,x+w-10,y,dtgfx::BracketTR);sprite(dc,x,y+h-10,dtgfx::BracketBL);sprite(dc,x+w-10,y+h-10,dtgfx::BracketBR);return;}
 for(int c=0;c<4;c++){int cx=c&1?x+w-10:x,cy=c&2?y+h-10:y,ax=c&1?x+w-3:x,ay=c&2?y+h-3:y;
  fill(dc,cx,ay,10,3,Edge);fill(dc,ax,cy,3,10,Edge);fill(dc,cx+1,ay+1,8,1,Gold);fill(dc,ax+1,cy+1,1,8,Gold);}
}
// One native recruitment control (26x28). Disabled controls are faded like
// native ones; `on` shows the pressed sprite (the infinite order in force).
inline void clusterControl(HDC dc,int x,int y,int b,bool enabled,bool on){
 static const int arts[6]={dtgfx::Infinite,dtgfx::Up5,dtgfx::Up,dtgfx::Zero,dtgfx::Down5,dtgfx::Down};static const char*glyphs[6]={"","+5","+1","0","-5","-1"};
 if(sprite(dc,x,y,on&&b==0?dtgfx::InfiniteOn:arts[b],enabled||on?255:110))return;
 glyphButton(dc,x,y,ClusterW,ClusterH,glyphs[b],enabled||on);if(on)bevel(dc,x,y,ClusterW,ClusterH,false);if(b==0)infinity(dc,x,y-1,ClusterW,ClusterH,28,enabled||on?Ink:Grey);
}
// Temple/Fortress recruitment below the separator: unit cells, the control
// cluster for the selected unit, then the latest message.
inline void recruitment(HDC dc,int left,const Data&v){
 char s[32];int pick=recruitRow(v);
 for(int r=0;r<4;r++){int x=left+RecruitX[r%2],y=RecruitY[r/2];uint32_t n=v.queues[RecruitQueue[r]];bool afford=v.mana>=(uint32_t)RecruitCosts[r];
  portrait(dc,x+5,y+4,(uint16_t)RecruitUnits[r]);if(r==pick)bracket(dc,x+2,y+2,36,51);
  label(dc,x+RecruitTextDX,y+4,RecruitTextW[r%2],16,RecruitNames[r],F11,r==pick?Gold:Ink);
  crystal(dc,x+RecruitTextDX,y+21);snprintf(s,sizeof(s),"%d",RecruitCosts[r]);label(dc,x+RecruitTextDX+14,y+21,24,16,s,F9,afford?Ink:Red);
  if(dt::infiniteQueue(n))infinity(dc,x+RecruitTextDX,y+33,RecruitTextW[r%2],20,30,Ink,AlignLeft);else{snprintf(s,sizeof(s),"%u",n);label(dc,x+RecruitTextDX,y+34,RecruitTextW[r%2],20,s,F1,n?Ink:Grey);}
 }
 uint32_t n=v.queues[RecruitQueue[pick]];snprintf(s,sizeof(s),"%ss to train",RecruitNames[pick]);label(dc,left+TextX,RecruitCaptionY,TextW,16,s,F14,Blue,AlignCenter);
 for(int b=0;b<6;b++)clusterControl(dc,left+ClusterX[b%3],ClusterY[b/3],b,dt::canOrder(n,ClusterAmount[b/3][b%3]),b==0&&dt::infiniteQueue(n));
 separator(dc,left,RecruitMessageY-8);label(dc,left+TextX,RecruitMessageY,TextW,RecruitMessageH,v.message,F9,Ink,AlignLeft,true);
}
inline const char*buildingName(uint16_t kind){return kind==49?"Mushroom Farm":kind==80?"Manacopter Hall":kind==51?"Dark Fortress":"Dark Temple";}
inline void renderBuilding(HDC dc,int left,const Data&v){
 background(dc,left,false);title(dc,left,buildingName(v.kind));
 if(v.site){
  picture(dc,left+PictureX,PictureY,v.kind,true);separator(dc,left,SeparatorY);
  label(dc,left+TextX,350,TextW,16,"Under construction",F14,Blue,AlignCenter);
  label(dc,left+TextX,MessageY,TextW,MessageH,v.message,F9,Ink,AlignLeft,true);return;
 }
 picture(dc,left+(v.kind==80?BarPictureX:PictureX),PictureY,v.kind,false);iconButton(dc,left+RightX,RightY,RightW,RightH,dtgfx::Demolish,"X",true);
 if(v.kind==50||v.kind==51){
  if(!v.recruitmentReady){separator(dc,left,SeparatorY);label(dc,left+TextX,360,TextW,20,"Recruitment unavailable",F11,Ink,AlignCenter);label(dc,left+TextX,MessageY,TextW,MessageH,v.message,F9,Ink,AlignLeft,true);return;}
  iconButton(dc,left+SecondX,SecondY,SecondW,SecondH,dtgfx::Cancel,"Clear",manualOrders(v)>0);
  separator(dc,left,SeparatorY);recruitment(dc,left,v);
  return;
 }
 separator(dc,left,SeparatorY);
 label(dc,left+TextX,350,TextW,16,"Mushroom Farmers",F14,Blue,AlignCenter);
 label(dc,left+TextX,372,TextW,110,"Farmers carry mushrooms to your Temple or Fortress. Each delivery adds mana.",F9,Ink,AlignCenter,true);
 label(dc,left+TextX,MessageY,TextW,MessageH,v.message,F9,Ink,AlignLeft,true);
}
// One aircraft cell of the Manacopter panel: picture (raised over its shadow
// while flying), native health bar, passengers aboard below.
inline void aircraftCell(HDC dc,int left,const Data&v,int i){
 int x,y;airCell(i,x,y);x+=left;char s[16];bool flying=!v.airLanded[i];int px=x+AirPictureDX,py=y+47-AirPictureH;
 if(!sprite(dc,px,py,flying?dtgfx::ManacopterFlying:dtgfx::ManacopterPortrait)){
  /* Drawn fallback: the same picture box, raised over a dark ellipse when flying. */
  if(flying){part(px+9,py+41,AirPictureW-18,5,PartIcon);oval(dc,px+9,py+41,AirPictureW-18,5,RGB(44,48,22));}icon(dc,px+5,py+(flying?0:10),34,80);}
 vbar(dc,x+41,y,6,41,dtgfx::HealthFrame,v.airHp[i],healthColor(v.airHp[i]));
 snprintf(s,sizeof(s),"%u/6",(unsigned)v.airLoad[i]);label(dc,x-5,y+47,40,20,s,F3,Ink,AlignCenter);
}
// Summary row like the unit panel: aircraft count (or the state of a single
// one) left, overall health right; the word "Health" goes first when the row
// is too narrow. While unloading waits for its map click, the row says how to cancel.
inline void aircraftSummary(HDC dc,int left,const Data&v){
 if(v.unloadTargeting){const char*s="Right-click cancels unloading";label(dc,left+TextX,AirSummaryY,TextW,18,s,fitting(dc,s,TextW,{F11,F9}),Blue,AlignCenter);return;}
 char s[32],pct[8];if(v.group>1)snprintf(s,sizeof(s),"%u aircraft",(unsigned)v.group);else snprintf(s,sizeof(s),"%s",v.landed?"Landed":"Flying");
 int hp=v.groupHp;snprintf(pct,sizeof(pct),"%d%%",hp);
 const int gap=6,right=TextX+TextW,pw=textWidth(dc,pct,F11),hw=textWidth(dc,"Health ",F11);bool word=textWidth(dc,s,F11)+gap+hw+pw<=TextW;
 int healthX=right-pw-(word?hw:0);label(dc,left+TextX,AirSummaryY,healthX-gap-TextX,18,s,F11,Ink);
 if(word)label(dc,left+healthX,AirSummaryY,hw,18,"Health",F11,Ink);label(dc,left+right-pw,AirSummaryY,pw,18,pct,F11,healthColor(hp));
}
inline void renderAircraft(HDC dc,int left,const Data&v){
 background(dc,left,false);title(dc,left,v.group>1?"Manacopters":"Manacopter");char s[16];
 int cells=airCells(v);for(int i=0;i<cells;i++)aircraftCell(dc,left,v,i);
 /* Second row: aircraft 4-6, or else the passengers by type; a notice replaces it while shown. */
 if(airNotice(v))label(dc,left+TextX,AirRowY[1]+3,TextW,60,v.message,F9,Ink,AlignLeft,true);
 else if(v.airShown<=3){
  if(!v.groupPassengers)label(dc,left+TextX,AirRowY[1]+24,TextW,18,"No passengers",F11,Grey,AlignCenter);
  for(int i=0,n=std::min<int>(v.riderTypes,RiderShown);i<n;i++){int x=left+RiderX0+i*(n>3?RiderDX:AirDX),y=AirRowY[1];portrait(dc,x,y,v.riderKind[i]);snprintf(s,sizeof(s),"%u",(unsigned)v.riderCount[i]);label(dc,x-5,y+47,40,20,s,F3,Ink,AlignCenter);}
 }
 separator(dc,left,AirRuleY[0]);separator(dc,left,AirRuleY[1]);aircraftSummary(dc,left,v);
 action(dc,left,AirActionY,dtgfx::Unload,"v",v.unloadTargeting?"Click map to unload":"Unload passengers",airUnload(v));
 action(dc,left,AirActionY+AirActionStep,dtgfx::ManacopterIcon,"+","Load nearby units",true);
 action(dc,left,AirActionY+2*AirActionStep,dtgfx::Back,"<","Return to base",true);
 action(dc,left,AirActionY+3*AirActionStep,dtgfx::Next,">","Next Manacopter",true);
 action(dc,left,AirActionY+4*AirActionStep,dtgfx::Cancel,"X","Deselect",true);
}
// Shown instead of the menus if the plugin had to stop (no internal details).
inline void failure(HDC dc,int left){
 int save=SaveDC(dc);IntersectClipRect(dc,left+BoxX,BoxY,left+PanelW,BoxY+BoxH);SetBkMode(dc,TRANSPARENT);
 background(dc,left,false);title(dc,left,"Dark Tribe");label(dc,left+TextX,300,TextW,60,"Dark Tribe could not start. Please restart the game.",F11,Ink,AlignCenter,true);RestoreDC(dc,save);
}
// One queue cell of the Hall: the Manacopter picture (full while in production,
// faded while waiting), the native bar with the build progress, and the time
// until that aircraft is ready.
inline void hallCell(HDC dc,int left,const Data&v,int i){
 int x,y;airCell(i,x,y);x+=left;char s[16];int px=x+AirPictureDX,py=y+47-AirPictureH;
 if(!sprite(dc,px,py,dtgfx::ManacopterPortrait,i?120:255))icon(dc,px+5,py+10,34,80,i>0);
 vbar(dc,x+41,y,6,41,dtgfx::HealthFrame,i?0:(int)std::min<uint32_t>(v.progress,100),RGB(70,190,168));
 snprintf(s,sizeof(s),"%us",v.aircraftSeconds+(uint32_t)(HallBuildSeconds*i));label(dc,x-5,y+47,40,20,s,F3,i?Grey:Ink,AlignCenter);
}
inline void renderHall(HDC dc,int left,const Data&v){
 background(dc,left,false);title(dc,left,"Manacopter Hall");char s[48];
 bool queued=v.aircraftCount>0,notice=v.message[0]!=0;int cells=std::min<int>((int)v.aircraftCount,notice?3:6);
 for(int i=0;i<cells;i++)hallCell(dc,left,v,i);
 if(!queued)label(dc,left+TextX,AirRowY[0]+24,TextW,18,"No Manacopters queued",F11,Grey,AlignCenter);
 /* A notice replaces the second row for its few seconds, as in the Manacopter panel. */
 if(notice)label(dc,left+TextX,AirRowY[1]+3,TextW,60,v.message,F9,Ink,AlignLeft,true);
 separator(dc,left,AirRuleY[0]);separator(dc,left,AirRuleY[1]);
 if(queued){snprintf(s,sizeof(s),"%u queued",v.aircraftCount);label(dc,left+TextX,AirSummaryY,80,18,s,F11,Ink);
  snprintf(s,sizeof(s),"Next in %u s",v.aircraftSeconds);label(dc,left+TextX+80,AirSummaryY,TextW-80,18,s,F11,Ink,AlignRight);}
 else label(dc,left+TextX,AirSummaryY,TextW,18,"300 mana and 30 s each",F11,Ink,AlignCenter);
 action(dc,left,AirActionY,dtgfx::ManacopterIcon,"+","Build Manacopter",v.mana>=(uint32_t)HallCost);
 action(dc,left,AirActionY+AirActionStep,dtgfx::Cancel,"-","Cancel last",queued);
 action(dc,left,AirActionY+2*AirActionStep,dtgfx::Stop,"X","Cancel all",queued);
 action(dc,left,AirActionY+3*AirActionStep,dtgfx::Next,">","Select Manacopter",true);
 action(dc,left,AirActionY+4*AirActionStep,dtgfx::Demolish,"X","Demolish Hall",true);
}
inline void render(HDC dc,int left,const Data&v,bool build){
 int save=SaveDC(dc);IntersectClipRect(dc,left+BoxX,BoxY,left+PanelW,BoxY+BoxH);SetBkMode(dc,TRANSPARENT);
 if(build)renderBuild(dc,left,v);else if(v.unitPanel&&!v.isAircraft)renderUnits(dc,left,v);else if(v.isAircraft)renderAircraft(dc,left,v);else if(v.kind==80&&!v.site)renderHall(dc,left,v);else renderBuilding(dc,left,v);
 RestoreDC(dc,save);
}
// ---- Bottom section: Dark mana ------------------------------------------------------
// Covers the native resource panel (menu 1: Roman goods, settlers and
// percentages, all meaningless for the Dark Tribe) in every mode, whatever
// native tab or selection is shown, with that panel's own empty background
// #587: the large mana crystal over the current mana in the largest native
// font, captioned like the info column. Drawn by the HD canvas and the
// classic frame after the page (or the failure panel); without data (not
// ready, plugin stopped) the panel stays empty. The block (crystal, value,
// caption) is centred in the inner area.
constexpr int ManaCrystalW=40,ManaCrystalH=58,ManaValueH=24,ManaCaptionH=18;
constexpr int ManaCrystalY=ManaInnerY0+(ManaInnerY1-ManaInnerY0-(ManaCrystalH+3+ManaValueH+ManaCaptionH))/2,ManaValueY=ManaCrystalY+ManaCrystalH+3,ManaCaptionY=ManaValueY+ManaValueH;
inline void manaSection(HDC dc,int left,const Data&v){
 int save=SaveDC(dc);IntersectClipRect(dc,left,ManaY,left+PanelW,ManaY+ManaH);SetBkMode(dc,TRANSPARENT);
 if(!sprite(dc,left,ManaY,dtgfx::ResourcePanel)){const Texture&t=texture();StretchDIBits(dc,left+BoxX,ManaY,BoxW,ManaH,0,0,BoxW,ManaH,t.light,&t.info,DIB_RGB_COLORS,SRCCOPY);goldFrame(dc,left,ManaInnerY0-5,ManaInnerY1+5);}
 if(v.ready){
  char s[16];snprintf(s,sizeof(s),"%u",v.mana); /* any 32-bit value fits the text column exactly */
  int x=left+TextX+(TextW-ManaCrystalW)/2;if(!sprite(dc,x,ManaCrystalY,dtgfx::ManaCrystal))icon(dc,x,ManaCrystalY+(ManaCrystalH-ManaCrystalW)/2,ManaCrystalW,0);
  label(dc,left+TextX,ManaValueY,TextW,ManaValueH,s,F1,Ink,AlignCenter);
  label(dc,left+TextX,ManaCaptionY,TextW,ManaCaptionH,"Mana",F11,Ink,AlignCenter);
 }
 RestoreDC(dc,save);
}
// Native-style selection bracket for picked Manacopters: white corner brackets
// with a dark outline and a health bar between them: the filled part in
// healthColor(percent), the rest dark, as drawn above selected native units.
constexpr int MarkW=44,MarkH=14;
constexpr int MarkBarX=12,MarkBarY=3,MarkBarW=MarkW-24,MarkBarH=5; // bar incl. 1px outline
constexpr int MarkInner=MarkBarW-2;
// Filled pixels of an `inner`-pixel bar: proportional, never empty while alive
// (percent>0), never full below 100 percent.
inline int markerFill(int percent,int inner){
 if(inner<=0||percent<=0)return 0;if(percent>=100)return inner;
 int f=(percent*inner+50)/100;return f<1?1:f>=inner?inner-1:f;
}
// Opaque ARGB of c; mix>0 lightens towards white, mix<0 darkens (256 = full).
inline uint32_t markerArgb(COLORREF c,int mix){
 auto ch=[&](int v){v=mix>=0?v+(255-v)*mix/256:v*(256+mix)/256;return (uint32_t)std::min(255,std::max(0,v));};
 return 0xFF000000u|(ch(GetRValue(c))<<16)|(ch(GetGValue(c))<<8)|ch(GetBValue(c));
}
inline void markerPixels(uint32_t*px,int percent){
 auto put=[&](int x,int y,uint32_t argb){if(x>=0&&y>=0&&x<MarkW&&y<MarkH)px[y*MarkW+x]=argb;};
 for(int i=0;i<MarkW*MarkH;i++)px[i]=0;
 const uint32_t edge=0xC0101010,white=0xFFF0F0F0,barEdge=0xE0101010,empty=0xF0262626,emptyLow=0xF01A1A1A;
 auto corner=[&](int x0,int dir){
  for(int i=-1;i<=10;i++)for(int t=-1;t<=2;t++)put(x0+dir*i,t+1,edge);
  for(int j=-1;j<=7;j++)for(int t=-1;t<=2;t++)put(x0+dir*t,j+1,edge);
  for(int i=0;i<=9;i++)for(int t=0;t<2;t++)put(x0+dir*i,t+1,white);
  for(int j=0;j<=6;j++)for(int t=0;t<2;t++)put(x0+dir*t,j+1,white);
 };
 corner(1,1);corner(MarkW-2,-1);
 percent=std::max(0,std::min(100,percent));COLORREF c=healthColor(percent);int fill=markerFill(percent,MarkInner);
 const uint32_t shade[3]={markerArgb(c,72),markerArgb(c,0),markerArgb(c,-80)}; // lit top, body, shadow
 for(int x=MarkBarX;x<MarkBarX+MarkBarW;x++)for(int y=MarkBarY;y<MarkBarY+MarkBarH;y++){
  bool border=y==MarkBarY||y==MarkBarY+MarkBarH-1||x==MarkBarX||x==MarkBarX+MarkBarW-1;int row=y-MarkBarY-1;
  put(x,y,border?barEdge:(x-MarkBarX-1)<fill?shade[row]:row==2?emptyLow:empty);
 }
}
inline void markerGdi(HDC dc,int cx,int cy,int percent);
inline void markersGdi(HDC dc,const Data&v){for(int i=0;i<v.markCount&&i<12;i++)markerGdi(dc,v.markX[i],v.markY[i],v.markHp[i]);}
inline void markerGdi(HDC dc,int cx,int cy,int percent){
 // Classic DirectDraw surface: same shape, drawn with GDI (no alpha).
 percent=std::max(0,std::min(100,percent));int filled=markerFill(percent,MarkInner);
 int x=cx-MarkW/2,y=cy-MarkH/2;HBRUSH dark=CreateSolidBrush(RGB(16,16,16)),white=CreateSolidBrush(RGB(240,240,240)),bar=CreateSolidBrush(healthColor(percent)),empty=CreateSolidBrush(RGB(38,38,38));
 auto fill=[&](int a,int b,int w,int h,HBRUSH br){if(w<=0||h<=0)return;RECT r{x+a,y+b,x+a+w,y+b+h};FillRect(dc,&r,br);};
 fill(0,0,12,4,dark);fill(0,0,4,9,dark);fill(MarkW-12,0,12,4,dark);fill(MarkW-4,0,4,9,dark);
 fill(1,1,10,2,white);fill(1,1,2,7,white);fill(MarkW-11,1,10,2,white);fill(MarkW-3,1,2,7,white);
 fill(MarkBarX,MarkBarY,MarkBarW,MarkBarH,dark);fill(MarkBarX+1,MarkBarY+1,filled,MarkBarH-2,bar);fill(MarkBarX+1+filled,MarkBarY+1,MarkInner-filled,MarkBarH-2,empty);
 DeleteObject(dark);DeleteObject(white);DeleteObject(bar);DeleteObject(empty);
}
// Unload-targeting cursor badge: a green down arrow onto a ground bar.
constexpr int BadgeW=26,BadgeH=26;
inline void badgePixels(uint32_t*px){
 for(int i=0;i<BadgeW*BadgeH;i++)px[i]=0;
 auto put=[&](int x,int y,uint32_t c){if(x>=0&&y>=0&&x<BadgeW&&y<BadgeH)px[y*BadgeW+x]=c;};
 const uint32_t edge=0xD0101010,green=0xFF2FD06A,ground=0xFFE8D9A0;
 for(int pass=0;pass<2;pass++){uint32_t c=pass?green:edge;int g=pass?0:1;
  for(int y=2-g;y<=12+g;y++)for(int x=10-g;x<=15+g;x++)put(x,y,c);                 // shaft
  for(int y=0;y<=8+g;y++)for(int x=-(9-y)-g;x<=(9-y)+g;x++)put(13+x,12+y,c);         // head
 }
 for(int y=22;y<=25;y++)for(int x=1;x<=24;x++)put(x,y,(y==22||y==25||x==1||x==24)?edge:ground);
}
inline void badgeGdi(HDC dc,int x,int y){
 HBRUSH dark=CreateSolidBrush(RGB(16,16,16)),green=CreateSolidBrush(RGB(47,208,106)),ground=CreateSolidBrush(RGB(232,217,160));
 RECT shaft{x+9,y+1,x+17,y+14},inner{x+10,y+2,x+16,y+13},bar{x+1,y+22,x+25,y+26},barIn{x+2,y+23,x+24,y+25};
 FillRect(dc,&shaft,dark);FillRect(dc,&inner,green);
 POINT outer[]={{x+2,y+12},{x+24,y+12},{x+13,y+22}},head[]={{x+4,y+13},{x+22,y+13},{x+13,y+20}};
 auto oldPen=SelectObject(dc,GetStockObject(NULL_PEN));auto oldBrush=SelectObject(dc,dark);Polygon(dc,outer,3);SelectObject(dc,green);Polygon(dc,head,3);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);
 FillRect(dc,&bar,dark);FillRect(dc,&barIn,ground);DeleteObject(dark);DeleteObject(green);DeleteObject(ground);
}
}
#include "unit-panel.hpp"
