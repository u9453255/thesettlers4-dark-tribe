#pragma once
#include <stdint.h>
#include <algorithm>
#include <cstdlib>
namespace dt {
struct UnitRef {uint16_t id=0,kind=0;uint32_t identity=0;};
inline bool sameEntity(UnitRef a,UnitRef b){return a.id&&a.identity&&a.id==b.id&&a.identity==b.identity;}
inline bool affordable(uint32_t bank,uint32_t cost,uint32_t jobs,uint32_t limit){return cost&&bank>=cost&&jobs<limit;}
inline bool manaTarget(uint32_t before,int amount,uint32_t&after){
 int64_t target=int64_t(before)+amount;
 if(target<0||target>UINT32_MAX)return false;
 after=(uint32_t)target;return true;
}
struct SpriteHit {UnitRef unit{};int left=0,top=0,right=0,bottom=0;uint32_t stamp=0,frame=0;};
inline bool spriteContains(const SpriteHit&r,int x,int y,uint32_t now){return r.unit.id&&now-r.stamp<=250&&x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}
inline bool nearBoarding(int x,int y,int a,int b){return abs(x-a)<=6&&abs(y-b)<=6;}
// A move order whose map target lies on/next to a landed aircraft means
// "board it". Map coordinates work with every renderer (HD has no sprite blits).
inline bool boardingTarget(int tx,int ty,int ax,int ay){return abs(tx-ax)<=4&&abs(ty-ay)<=4;}
// Right-button click versus camera drag, mirroring the native right-drag pan
// of CStateGame (input handler RVA 0x1172F0; window procedure RVA 0x5A1A0):
// - the press (event 9, RVA 0x11808D) sets +0xD1, captures the mouse and
//   stores the press point; only the release (event 10, RVA 0x1182DD) clears
//   +0xD1, releases capture and restores the cursor, so the game must get
//   both halves of a right press or neither;
// - a move more than 4 px from the press on either axis starts the pan
//   (+0xD2, RVA 0x117E32/0x117E48 and 0x117EFE/0x117F14); a release after a
//   pan issues no order, otherwise it orders a move to the press point;
// - with GAMESETTINGS/LegacyMouse the pan hides the cursor and parks it at
//   the render centre (width/2, height/2 from RVA 0x2598F0/0x259900) on every
//   move (SetCursorPos at RVA 0x117EE6/0x11800C).
// The pinned S4ModApi hooks only the button path of that window procedure
// (RVA 0x5A3D9) and the wheel (0x5A409); WM_MOUSEMOVE goes 0x5A313 -> 0x5A39F
// and never reaches the mouse listener. The release point is therefore
// usually the only sample after the press. A legacy pan always ends near the
// render centre, so a press near the centre released near the centre is
// ambiguous: there only a short hold counts as a click.
constexpr int NativeDragSlop=4,WarpZone=12;constexpr uint32_t WarpClickMs=250;
inline bool beyond(int ax,int ay,int bx,int by,int slop){return abs(ax-bx)>slop||abs(ay-by)>slop;}
struct RightGesture {
 enum Route:uint8_t {None=0,Passed=1,Consumed=2}; // None: no press seen; Passed: the game got the press.
 Route route=None;bool aircraftOnly=false;int x=0,y=0,reach=0;uint32_t time=0;
 // A new press always starts over, whatever happened to the previous one.
 void press(int px,int py,uint32_t t,Route r,bool aircraft){*this=RightGesture{};route=r;aircraftOnly=aircraft;x=px;y=py;time=t;}
 // Largest per-axis distance from the press seen while the button is held.
 void sample(int px,int py){if(route!=None)reach=std::max(reach,std::max(abs(px-x),abs(py-y)));}
 // Hands back the press that this release ends and forgets it.
 RightGesture release(){RightGesture g=*this;*this=RightGesture{};return g;}
};
// True when the native game would treat this release as a click (no pan).
// cx/cy: render centre in mouse coordinates, or 0 when unknown.
inline bool rightClick(const RightGesture&g,int rx,int ry,uint32_t now,int cx,int cy){
 if(g.route==RightGesture::None||g.reach>NativeDragSlop||beyond(rx,ry,g.x,g.y,NativeDragSlop))return false;
 if(cx>0&&cy>0&&!beyond(g.x,g.y,cx,cy,WarpZone)&&now-g.time>WarpClickMs)return false;
 return true;
}
}
