#pragma once
// Dark unit selection panel (agent C). Shown instead of the native Roman
// specialist/military menus when own Dark settlers are selected.
#include "panel.hpp"
#include <stdlib.h>
#include <vector>
#include <utility>
// ---- Pure logic (unit-tested by unit-panel-tests.cpp) ----------------------
namespace dtunits {
constexpr int MaxTypes=6;
// Settler types (S4_SETTLER_ENUM): 29-31 Warrior (SWORDSMAN_01..03), 32-34
// Thrower (BOWMAN_01..03), 52 Dark Gardener, 53 Mushroom Farmer, 54 Shaman,
// 55 slaved settler, 56 temple servant, 66 Manacopter master, 1 carrier.
inline bool military(uint16_t k){return k>=29&&k<=34;}
// Same set as dt::boardable (aircraft-model.hpp); checked by the tests.
inline bool boardable(uint16_t k){return k==29||k==32||k==52||k==54;}
// Free units the panel may order around. Farm workers (53) and other job
// holders stay under native control.
inline bool mobile(uint16_t k){return military(k)||k==52||k==54;}
inline uint16_t portraitKind(uint16_t k){return k==30||k==31?29:k==33||k==34?32:k;}
inline const char* name(uint16_t k){
 switch(k){case 29:case 30:case 31:return "Warrior";case 32:case 33:case 34:return "Thrower";case 52:return "Gardener";case 53:return "Farmer";
  case 54:return "Shaman";case 55:return "Slave";case 56:return "Servant";case 66:return "Pilot";case 1:return "Carrier";default:return "Unit";}
}
// Native selection bracket: bar = 7*hp/max, nothing when max is 0 (RVA 0x181398).
inline int healthPercent(int hp,int max){if(max<=0)return 100;if(hp<=0)return 0;int p=hp*100/max;return p<1?1:p>100?100:p;}
struct Member {uint16_t kind=0;uint8_t health=100;};
struct Summary {uint8_t types=0;uint16_t kind[MaxTypes]{},count[MaxTypes]{};uint8_t health[MaxTypes]{};uint16_t total=0;bool military=false;};
// Group by exact type, most numerous first (ties: lower type id); average
// health percent per type, rounded. `types` counts every distinct type, even
// those beyond the six shown.
inline Summary summarize(const Member*m,size_t n){
 struct Group {uint16_t kind;uint32_t count,health;};std::vector<Group> g;Summary s;
 for(size_t i=0;i<n;i++){auto it=std::find_if(g.begin(),g.end(),[&](const Group&o){return o.kind==m[i].kind;});
  if(it==g.end())g.push_back({m[i].kind,1,m[i].health});else{it->count++;it->health+=m[i].health;}
  if(military(m[i].kind))s.military=true;}
 std::stable_sort(g.begin(),g.end(),[](const Group&a,const Group&b){return a.count!=b.count?a.count>b.count:a.kind<b.kind;});
 s.types=(uint8_t)std::min<size_t>(g.size(),255);s.total=(uint16_t)std::min<size_t>(n,65535);
 for(size_t i=0;i<g.size()&&i<(size_t)MaxTypes;i++){s.kind[i]=g[i].kind;s.count[i]=(uint16_t)std::min<uint32_t>(g[i].count,65535);s.health[i]=(uint8_t)((g[i].health+g[i].count/2)/g[i].count);}
 return s;
}
// Count-weighted health of the shown types.
inline int overallHealth(const uint16_t*count,const uint8_t*health,int shown){uint32_t n=0,sum=0;for(int i=0;i<shown;i++){n+=count[i];sum+=count[i]*health[i];}return n?(int)((sum+n/2)/n):100;}
}
namespace dtui {
// Layout of the native settler selection menu (GUISet menu 44): 30x47
// portraits on a 60-px grid with a vertical health bar (#76) and the count
// below, then summary and action rows.
constexpr int UnitX0=19,UnitDX=60,UnitRowY[2]={262,338},UnitCellW=52,UnitCellH=67;
constexpr int UnitSummaryY=414,UnitActionY=440,UnitActionStep=29;
// Commands (tick: unitCommand in dark-tribe.cpp).
enum UnitAction {UnitKeepType=40,UnitBoard=41};
struct UnitFlags {int shown=0;bool board=false,move=false,mixed=false;};
inline UnitFlags unitFlags(const Data&v){
 UnitFlags u;u.shown=std::min<int>(v.unitTypes,dtunits::MaxTypes);bool workers=v.unitTypes>dtunits::MaxTypes;
 for(int i=0;i<u.shown;i++){uint16_t k=v.unitKind[i];if(dtunits::boardable(k))u.board=true;if(dtunits::mobile(k))u.move=true;if(!dtunits::military(k))workers=true;}
 u.mixed=v.unitMilitary&&workers;return u;
}
inline void unitCell(int i,int&x,int&y){x=UnitX0+(i%3)*UnitDX;y=UnitRowY[i/3];}
inline Action hitUnits(const Data&v,int x,int y){
 auto u=unitFlags(v);
 for(int i=0;u.shown>1&&i<u.shown;i++){int cx,cy;unitCell(i,cx,cy);if(inside(x,y,cx-4,cy,UnitCellW,UnitCellH))return {UnitKeepType,v.unitKind[i]};}
 return actionRow(x,y,UnitActionY,UnitActionStep,1)==0&&u.board?Action{UnitBoard,0}:Action{};
}
inline void renderUnits(HDC dc,int left,const Data&v){
 auto u=unitFlags(v);char s[64];
 background(dc,left,false);title(dc,left,u.mixed?"Dark Forces":v.unitMilitary?"Dark Army":"Dark Specialists");
 for(int i=0;i<u.shown;i++){int x,y;unitCell(i,x,y);x+=left;
  portrait(dc,x,y,dtunits::portraitKind(v.unitKind[i]));vbar(dc,x+41,y,6,41,dtgfx::HealthFrame,v.unitHealth[i],healthColor(v.unitHealth[i]));
  snprintf(s,sizeof(s),"%u",(unsigned)v.unitCount[i]);label(dc,x-5,y+47,40,20,s,F3,Ink,AlignCenter);}
 separator(dc,left,331);separator(dc,left,407);
 // Summary row: unit count (and hidden types) left, overall health right, on
 // the text column. When the row is too narrow, the word "Health" goes
 // first, then the hidden-types note.
 char count[24],pct[8];snprintf(count,sizeof(count),"%u unit%s",(unsigned)v.unitTotal,v.unitTotal==1?"":"s");
 if(v.unitTypes>dtunits::MaxTypes)snprintf(s,sizeof(s),"%s, +%u types",count,(unsigned)(v.unitTypes-dtunits::MaxTypes));else snprintf(s,sizeof(s),"%s",count);
 int hp=dtunits::overallHealth(v.unitCount,v.unitHealth,u.shown);snprintf(pct,sizeof(pct),"%d%%",hp);
 const int gap=6,right=TextX+TextW,pw=textWidth(dc,pct,F11),hw=textWidth(dc,"Health ",F11);bool word=true;
 if(textWidth(dc,s,F11)+gap+hw+pw>TextW)word=false;if(textWidth(dc,s,F11)+gap+pw>TextW)snprintf(s,sizeof(s),"%s",count);
 int healthX=right-pw-(word?hw:0);label(dc,left+TextX,UnitSummaryY,healthX-gap-TextX,18,s,F11,Ink);
 if(word)label(dc,left+healthX,UnitSummaryY,hw,18,"Health",F11,Ink);label(dc,left+right-pw,UnitSummaryY,pw,18,pct,F11,healthColor(hp));
 action(dc,left,UnitActionY,dtgfx::ManacopterIcon,"^","Board Manacopter",u.board);
}
}
