#pragma once
#include <windows.h>
#include <vector>
#include <algorithm>
#include "fast-memory.hpp"
#include "map-size.hpp"
#include "offence-model.hpp"
#include "footprints.hpp"
#include "offence-signatures.hpp" // Private, generated from the pinned file.
namespace dtoffence {
// Native computer AI, pinned executable only (research/AI_OFFENCE_0215.md,
// re-checked by tools/inspect-offence.py). All addresses are RVAs.
// CAIMain (static, vtable 0xC2A8A4): +8 bit p = player p has an AI,
// +0xC+4p = player AI. CAINormalPlayerAI (vtable 0xC2AA0C): byte +8 = 0
// (1 = Dark AI), +0xC player, +0x48 sector count (<= 8), +0x4C sectors.
// CAINormalSectorAI (vtable 0xC2B218): +0x1C+4g free-soldier reservoir of
// entity group g (vtable 0xC2A950), +0x58 task-force manager (+4 player),
// +0xC8 squad list (CAITaskForceSquad, vtable 0xC2A91C, next +0x24), +0xF4/
// +0xF8 home, +0x100 land region, +0xB1C fighters F and +0xB20 strength Str
// (written by CAIAgentSquads 0x342BA/0x342C6). Task force: +4 type (2 squad),
// +8 state (5 guard, 6 attack, 3 home, 4 suicide), +0xC members, +0x10 first
// member record (+0 entity id, +8 task force, +0x10 next), command (+0x38>>4)&0xFF.
// Entity +0x10 = its member record.
class Native {
 uintptr_t image=0;
 using Create=uintptr_t(__thiscall*)(void*,int);            // 0x45020(manager, state), ret 4
 using Command=char(__thiscall*)(void*,int,int,int);        // squad slot 8 = 0x46880(cmd, param, flags), ret 0xC
 using AddMember=int(__thiscall*)(void*,void*,int);         // squad slot 5 = 0x46770(record, mode), ret 8, 3 = added
 using Delete=void*(__thiscall*)(void*,int);                // squad slot 2 = 0x398B0(1 = free), ret 4
 using Dissolve=void(__thiscall*)(void*,void*);             // 0x3FA50(sector, squad): members back to the reservoirs, ret 4
 using Destroy=void(__thiscall*)(void*,uint32_t);           // 0x136A80(building, destroying player), ret 4
 Create create=nullptr;Dissolve dissolveSquad=nullptr;Destroy destroyBuilding=nullptr;
 static bool readable(uintptr_t p,size_t bytes){return dtmem::readable(p,bytes);}
 template<class T> static bool read(uintptr_t p,T&v){if(!readable(p,sizeof(T)))return false;memcpy(&v,(void*)p,sizeof(T));return true;}
 template<class T> static T value(uintptr_t p){T v{};read(p,v);return v;}
 bool cell(int x,int y,unsigned&at)const{int n=dtmap::size(image);if(!n||x<1||y<1||x>=n-1||y>=n-1)return false;at=(unsigned)(y*n+x);return true;}
 static bool foundation(uint32_t uid){return uid&&!(uid&0x60000000);}
 // A source a soldier was found in (reservoir or guard squad) when listed.
 struct Source {uint16_t id=0;uintptr_t record=0,owner=0;};
 std::vector<Source> sources;
public:
 const char*reason="";
 bool initialize(uintptr_t module){
  image=module;create=(Create)(module+0x45020);dissolveSquad=(Dissolve)(module+0x3fa50);destroyBuilding=(Destroy)(module+0x136a80);
  return intact(module);
 }
 uintptr_t entity(uint16_t id)const{uintptr_t p=0;uint16_t slot=0;if(!id||!read(image+0xe9bc38+id*4,p)||!p||!read(p+8,slot)||slot!=id||!readable(p,0x58))return 0;return p;}
 // Player p's normal (non-Dark) AI, or 0.
 uintptr_t playerAI(uint8_t p)const{
  uintptr_t main=image+0x106b148,pa=0,vt=0;uint32_t bits=0,owner=0;BYTE dark=1;
  if(p<1||p>8||!read(main,vt)||vt!=image+0xc2a8a4||!read(main+8,bits)||!(bits&(1u<<p)))return 0; /* bts [CAIMain+8],p at 0x3856A */
  if(!read(main+0xc+4*p,pa)||!pa||!read(pa,vt)||vt!=image+0xc2aa0c||!read(pa+8,dark)||dark||!read(pa+0xc,owner)||owner!=p)return 0;
  return pa;
 }
 int sectorCount(uint8_t p)const{uintptr_t pa=playerAI(p);int n=0;return pa&&read(pa+0x48,n)&&n>=1&&n<=8?n:0;}
 uintptr_t sector(uint8_t p,int i)const{
  uintptr_t pa=playerAI(p),s=0,vt=0;int n=0;uint32_t owner=0;
  if(!pa||!read(pa+0x48,n)||n<1||n>8||i<0||i>=n||!read(pa+0x4c+4*i,s)||!s||!read(s,vt)||vt!=image+0xc2b218||!readable(s,0xb24)||!read(s+0x58+4,owner)||owner!=p)return 0;
  return s;
 }
 // Player p is an enemy of the Dark player 1 (team masks, bit q-1 = player q).
 bool enemyOfDark(uint8_t p)const{uint32_t mask=0;return p>=2&&p<=8&&read(image+0xedcd54+4*p,mask)&&(mask&1);}
 Config config(uint8_t p)const{
  Config c;uintptr_t record=image+0xe92648+p*0x17c,column=0;
  if(p<1||p>8||!read(record,column))return c;bool valid=false;for(int k=0;k<4;k++)if(column==record+4+k*0x5c)valid=true; /* active difficulty column */
  int v[5]{};if(!valid||!readable(column,sizeof(v)))return c;memcpy(v,(void*)column,sizeof(v));
  c.mode=v[0];c.delay=v[1];c.chancePartial=v[2];c.thresholdPartial=v[3];c.thresholdTotal=v[4];
  c.squadMin=value<int>(image+0xd7d10c);c.squadExtra=value<int>(image+0xd7d118);c.ok=c.squadMin>=0&&c.squadMin<=100&&c.squadExtra>=0&&c.squadExtra<=100;
  return c;
 }
 Strength strength(uint8_t p,int i)const{
  Strength s;uintptr_t sec=sector(p,i);if(!sec)return s;
  s.fighters=value<int>(sec+0xb1c);s.strength=value<int>(sec+0xb20);s.region=value<int>(sec+0x100);s.homeX=value<int>(sec+0xf4);s.homeY=value<int>(sec+0xf8);
  s.darkValue=value<int>(image+0xe66e28+DarkPlayer*0x58+0x38); /* E(1)+0x38: 2 per Dark Warrior/Thrower */
  int n=dtmap::size(image);s.ok=n&&s.region>0&&s.homeX>0&&s.homeY>0&&s.homeX<n&&s.homeY<n;
  return s;
 }
 // Land region of a cell (EcoSectorMap word -> EcoSectorPool+4), as the AI compares it.
 int region(int x,int y)const{unsigned at=0;uintptr_t map=0;uint16_t tile=0,id=0;if(!cell(x,y,at)||!read(image+0xef1a18,map)||!read(map+2*at,tile)||tile>=32768||!read(image+0xef1a24+tile*20,id))return 0;return id;}
 BYTE flags(int x,int y)const{unsigned at=0;uintptr_t map=0;BYTE f=0xff;if(cell(x,y,at)&&read(image+0x11630d8,map))read(map+at,f);return f;}
 uint16_t ground(int x,int y)const{unsigned at=0;uintptr_t map=0;uint16_t id=0xffff;if(cell(x,y,at)&&read(image+0x11630dc,map))read(map+2*at,id);return id;}
 // Walkable, unoccupied, not a building cell (flags 0x01 blocked, 0x08 building body). Flag 0x80 is
 // military influence over the whole tower territory (0x20D1AC), not a footprint: it does not matter here.
 bool free(int x,int y)const{BYTE f=flags(x,y);return !(f&0x09)&&!ground(x,y);}
 bool stand(int x,int y,int pass,int wanted)const{int n=dtmap::size(image);if(!n||x<3||y<3||x>=n-3||y>=n-3||!free(x,y))return false;if(pass==0&&(flags(x,y)&0x04))return false;return region(x,y)==wanted;}
 // Completed Dark building of player 1 (role vtable of the finished kind).
 bool darkBuilding(uintptr_t e,uint16_t kind)const{
  uint32_t uid=0,bits=0;uint16_t type=0;BYTE cls=0,owner=0;uintptr_t role=0,vt=0;
  if(!e||!read(e+4,uid)||!foundation(uid)||!read(e+10,cls)||cls!=8||!read(e+12,type)||type!=kind||!read(e+32,owner)||owner!=0x13||!read(e+0x14,bits)||!(bits&0x2000000)||(bits&0x4000000)||!read(e+0x50,role)||!read(role,vt))return false;
  return vt==image+(kind==49?0xc58ae8:kind==80?0xc593d0:0xc59284);
 }
 void targets(int wanted,std::vector<Target>&out)const{
  out.clear();static const uint16_t kinds[]={Farm,Temple,Fortress,Hall};std::vector<std::pair<int,int>> dark;fighters(dark);
  for(uint16_t kind:kinds){uint32_t head=0;if(!read(image+0x10eea74+(DarkPlayer*0x53+kind)*4,head))continue; /* per-player building list, next word +0x2A */
   uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<2048;guard++){uintptr_t e=entity(id);if(!e)break;uint16_t next=value<uint16_t>(e+0x2a);
    if(darkBuilding(e,kind)){Target t;t.id=id;t.kind=kind;t.uid=value<uint32_t>(e+4);t.x=value<uint16_t>(e+24);t.y=value<uint16_t>(e+26);uint32_t door=value<uint32_t>(e+0x48);t.doorX=door&0xffff;t.doorY=door>>16;
     int r=region(t.doorX,t.doorY);t.reachable=r&&r==wanted;for(auto&d:dark)if(distance(d.first,d.second,t.doorX,t.doorY)<=RaidRadius)t.defence+=2;out.push_back(t);}
    id=next;}}
 }
 bool alive(const Target&t)const{uintptr_t e=entity(t.id);return e&&value<uint32_t>(e+4)==t.uid&&darkBuilding(e,t.kind);}
 // Dark Warriors, Throwers and Shamans standing (visible, alive) within radius.
 int defenders(int x,int y,int radius,bool weighted)const{
  std::vector<std::pair<int,int>> dark;fighters(dark);int n=0;for(auto&d:dark)if(distance(x,y,d.first,d.second)<=radius)n+=weighted?2:1;return n;
 }
 // Positions of visible, living Dark Warriors, Throwers and Shamans of player 1.
 void fighters(std::vector<std::pair<int,int>>&out)const{
  out.clear();static const uint16_t types[]={29,32,54};
  for(uint16_t type:types){uint32_t head=0;if(!read(image+0x1144cec+(DarkPlayer*0x43+type)*4,head))continue; /* per-player settler list, next word +0x2A */
   uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<4096;guard++){uintptr_t e=entity(id);if(!e)break;uint16_t next=value<uint16_t>(e+0x2a);
    uint32_t bits=value<uint32_t>(e+0x14);BYTE cls=value<BYTE>(e+10),owner=value<BYTE>(e+32),hp=value<BYTE>(e+33);
    if(cls==1&&value<uint16_t>(e+12)==type&&(owner>>4)==DarkPlayer&&hp&&(bits&0x3000000)&&!(bits&0x4000000)&&!(bits&0x8000))out.push_back({value<uint16_t>(e+24),value<uint16_t>(e+26)});
    id=next;}}
 }
 // A soldier the AI may give up: own, field-ready (moves accepted only with
 // flag 0x2000), not hidden, alive, of a fighting type in group 2..5.
 bool soldier(uintptr_t e,uint8_t p,uint16_t&type)const{
  uint32_t uid=0,bits=0;BYTE cls=0,owner=0,hp=0;
  if(!e||!read(e+4,uid)||!foundation(uid)||!read(e+10,cls)||cls!=1||!read(e+32,owner)||(owner>>4)!=p||!read(e+33,hp)||!hp||!read(e+0x14,bits)||(bits&0x2008000)!=0x2000000||!(bits&0x2000)||(bits&0x4000000))return false;
  unsigned group=bits&0xf;return group>=2&&group<=5&&read(e+12,type)&&soldierType(type);
 }
 // Free soldiers of the sector nearest the target first: reservoir members
 // before guard-squad members, so tower guards are taken last.
 void available(uint8_t p,int i,int x,int y,std::vector<Soldier>&out){
  out.clear();sources.clear();uintptr_t s=sector(p,i);if(!s)return;
  struct Found {Soldier s;int rank;};std::vector<Found> found;
  auto take=[&](uintptr_t owner,int rank){uintptr_t rec=0;if(!read(owner+0x10,rec))return;
   for(int guard=0;rec&&guard<4096;guard++){uint16_t id=0;uintptr_t at=0,next=0;if(!read(rec,id)||!read(rec+8,at)||!read(rec+0x10,next))break;
    uintptr_t e=entity(id);uint16_t type=0;if(at==owner&&e&&value<uintptr_t>(e+0x10)==rec&&soldier(e,p,type)){
     Soldier v;v.id=id;v.type=type;v.uid=value<uint32_t>(e+4);v.x=value<uint16_t>(e+24);v.y=value<uint16_t>(e+26);found.push_back({v,rank});sources.push_back({id,rec,owner});}
    rec=next;}};
  for(int g=2;g<=5;g++){uintptr_t pool=0,vt=0;uint32_t kind=0;if(read(s+0x1c+4*g,pool)&&pool&&read(pool,vt)&&vt==image+0xc2a950&&read(pool+4,kind)&&kind==1)take(pool,0);}
  uintptr_t tf=0;if(read(s+0xc8,tf))for(int guard=0;tf&&guard<256;guard++){uintptr_t vt=0,next=0;uint32_t kind=0;int state=0;if(!read(tf,vt)||!read(tf+4,kind)||!read(tf+8,state)||!read(tf+0x24,next))break;
   if(vt==image+0xc2a91c&&kind==2&&state==5)take(tf,1);tf=next;}
  std::stable_sort(found.begin(),found.end(),[&](const Found&a,const Found&b){if(a.rank!=b.rank)return a.rank<b.rank;return distance(a.s.x,a.s.y,x,y)<distance(b.s.x,b.s.y,x,y);});
  for(auto&f:found)out.push_back(f.s);
 }
 // A mod squad: in the sector's list, a squad in state 6 with command 0.
 bool modSquad(uintptr_t s,uintptr_t want)const{
  uintptr_t tf=0;if(!s||!read(s+0xc8,tf))return false;
  for(int guard=0;tf&&guard<256;guard++){uintptr_t vt=0,next=0;uint32_t kind=0,bits=0;int state=0;if(!read(tf,vt)||!read(tf+4,kind)||!read(tf+8,state)||!read(tf+0x38,bits)||!read(tf+0x24,next))return false;
   if(tf==want)return vt==image+0xc2a91c&&kind==2&&state==6&&((bits>>4)&0xff)==0&&value<uintptr_t>(tf+0x28)==s+0x58;tf=next;}
  return false;
 }
 // Native squads in state 6 always carry command 3 or 4 (attack agent), so
 // state 6 with command 0 is a mod squad restored from a save.
 void modSquads(uint8_t p,int i,std::vector<uintptr_t>&out)const{
  out.clear();uintptr_t s=sector(p,i),tf=0;if(!s||!read(s+0xc8,tf))return;
  for(int guard=0;tf&&guard<256;guard++){uintptr_t next=0;if(!read(tf+0x24,next))break;if(modSquad(s,tf))out.push_back(tf);tf=next;}
 }
 // Create a state-6 squad (0x45020, as AI.NewSquad and the Dark squad manager
 // do), park it with command 0 and move the chosen soldiers into it with the
 // squad's own AddMember (detaches them from their reservoir or guard squad
 // with the native counters). Returns the member count; the squad in squad.
 int raise(uint8_t p,int i,const std::vector<Soldier>&chosen,uintptr_t&squad){
  squad=0;reason="";uintptr_t s=sector(p,i);uint32_t guard=0;if(!s||!read(image+0xe9339c,guard)||!guard){reason="task forces not ready";return 0;}
  uintptr_t tf=create((void*)(s+0x58),6);dtmem::invalidate();
  uintptr_t head=0;if(!tf||!modSquadShape(tf,s)||!read(s+0xc8,head)||head!=tf){reason="squad creation failed";return 0;}
  uintptr_t vt=value<uintptr_t>(tf);((Command)value<uintptr_t>(vt+0x20))((void*)tf,0,0,0);dtmem::invalidate();
  int added=0;
  for(auto&c:chosen){auto it=std::find_if(sources.begin(),sources.end(),[&](const Source&v){return v.id==c.id;});if(it==sources.end())continue;
   uintptr_t e=entity(c.id);uint16_t type=0;if(!e||value<uint32_t>(e+4)!=c.uid||!soldier(e,p,type)||value<uintptr_t>(e+0x10)!=it->record||value<uintptr_t>(it->record+8)!=it->owner)continue;
   int r=((AddMember)value<uintptr_t>(vt+0x14))((void*)tf,(void*)it->record,0);dtmem::invalidate();if(r==3&&value<uintptr_t>(it->record+8)==tf)added++;}
  if(!added){((Delete)value<uintptr_t>(vt+8))((void*)tf,1);dtmem::invalidate();reason="no soldier could join";return 0;}
  squad=tf;return added;
 }
 bool modSquadShape(uintptr_t tf,uintptr_t s)const{uintptr_t vt=0,mgr=0;uint32_t kind=0;int state=0;return read(tf,vt)&&vt==image+0xc2a91c&&read(tf+4,kind)&&kind==2&&read(tf+8,state)&&state==6&&read(tf+0x28,mgr)&&mgr==s+0x58;}
 bool members(uint8_t p,int i,uintptr_t tf,std::vector<Soldier>&out)const{
  out.clear();uintptr_t s=sector(p,i);if(!modSquad(s,tf))return false;uintptr_t rec=0;if(!read(tf+0x10,rec))return false;
  for(int guard=0;rec&&guard<4096;guard++){uint16_t id=0;uintptr_t at=0,next=0;if(!read(rec,id)||!read(rec+8,at)||!read(rec+0x10,next))return false;
   uintptr_t e=entity(id);BYTE cls=0,hp=0;uint32_t bits=0;
   if(at==tf&&e&&value<uintptr_t>(e+0x10)==rec&&read(e+10,cls)&&cls==1&&read(e+33,hp)&&hp&&read(e+0x14,bits)&&!(bits&0x4000000)){Soldier v;v.id=id;v.type=value<uint16_t>(e+12);v.uid=value<uint32_t>(e+4);v.x=value<uint16_t>(e+24);v.y=value<uint16_t>(e+26);out.push_back(v);}
   rec=next;}
  return true;
 }
 // Give the soldiers back as CAIAgentSquads does with an unbound squad
 // (0x33C15..0x33C3A): command 0, members to their reservoirs (0x3FA50), then
 // delete the squad - only when it is empty, so no soldier loses its record.
 bool dissolve(uint8_t p,int i,uintptr_t tf){
  uintptr_t s=sector(p,i);if(!modSquad(s,tf))return false;uintptr_t vt=value<uintptr_t>(tf);
  ((Command)value<uintptr_t>(vt+0x20))((void*)tf,0,0,0);dtmem::invalidate();
  dissolveSquad((void*)s,(void*)tf);dtmem::invalidate();
  int left=-1;if(!read(tf+0xc,left)||left)return false;
  ((Delete)value<uintptr_t>(vt+8))((void*)tf,1);dtmem::invalidate();return true;
 }
 // Native credited destroy 0x136A80(building, player): only the fallback for
 // a building whose ring is already green (an object or flag 0x40 on the last
 // ring cell stops the native trigger). Guards: the campaign Fortress branch
 // (GlobalGameState +0x2D8 == 0xB, +0x2D4 in {0xF, 4}) marks the owner lost
 // without removing it; a Temple/Fortress whose role still parks entering
 // settlers ([role+0x1A4] != [role+0x1A8]) waits until the role's own tick
 // has released them (one per 31 ticks), so none is left off the map.
 bool destroy(uint8_t player,const Target&t){
  reason="";uintptr_t e=entity(t.id);if(!e||value<uint32_t>(e+4)!=t.uid||!darkBuilding(e,t.kind)){reason="target gone";return false;}
  BYTE race=value<BYTE>(e+32)&15;if(race!=3){reason="not Dark";return false;}
  if(t.kind==Fortress){uintptr_t g=value<uintptr_t>(image+0xe968a0);uint32_t mode=0,sub=0;if(g&&read(g+0x2d8,mode)&&read(g+0x2d4,sub)&&mode==0xb&&(sub==0xf||sub==4)){reason="campaign fortress";return false;}}
  if(t.kind==Temple||t.kind==Fortress){uintptr_t role=value<uintptr_t>(e+0x50),first=0,last=0;if(!read(role+0x1a4,first)||!read(role+0x1a8,last)){reason="role unreadable";return false;}if(first!=last){reason="settlers parked inside";return false;}}
  if(player<2||player>8){reason="attacker";return false;}
  destroyBuilding((void*)e,player);dtmem::invalidate();
  uint32_t after=0;return !read(e+4,after)||(after&0x20000000)||!(value<uint32_t>(e+0x14)&0x2000000)||(value<uint32_t>(e+0x14)&0x4000000);
 }
 // Gardeners (settler type 50, role CGardenerRole vtable 0xC5B51C): job word
 // entity+0x44 (50 default/idle, 0xAE walk-search, 0xAF greening, 0xB0 force
 // walk), role+4 task (17 wait, 31 search), +0x38/+0x3C work centre,
 // +0x40/+0x44 reserved cell (-1 none), +0x48 failed search slices.
 bool gardener(uintptr_t e,uint8_t p,Gardener&g)const{
  uint32_t uid=0,bits=0;BYTE cls=0,owner=0,hp=0;uint16_t type=0;uintptr_t role=0,vt=0;
  if(!e||!read(e+4,uid)||!foundation(uid)||!read(e+10,cls)||cls!=1||!read(e+12,type)||type!=GardenerType||!read(e+32,owner)||(owner>>4)!=p||!read(e+33,hp)||!hp||!read(e+0x14,bits)||(bits&0x2008000)!=0x2000000||(bits&0x4000000))return false;
  if(!read(e+0x58,role)||!read(role,vt)||vt!=image+0xc5b51c||!readable(role,0x50))return false;
  uint16_t job=value<uint16_t>(e+0x44);BYTE task=value<BYTE>(role+4);int rx=value<int>(role+0x40),ry=value<int>(role+0x44);
  g.id=value<uint16_t>(e+8);g.uid=uid;g.x=value<uint16_t>(e+24);g.y=value<uint16_t>(e+26);g.centreX=value<int>(role+0x38);g.centreY=value<int>(role+0x3c);g.misses=value<int>(role+0x48);
  /* The job decides: a gardener that gave up keeps a stale reserved cell (the
     WORK start failure 0x17974E and the give-up 0x1799B7 do not clear it). */
  g.work=job==0xaf?Work::Working:(job==50||(job==0xb0&&task==17))?Work::Idle:task==31?Work::Searching:Work::Busy;
  g.reservedX=job==0xaf?rx:-1;g.reservedY=job==0xaf?ry:-1;
  /* r30: the search 0x179F80 writes its chosen cell to +0x40/+0x44 (0x17A116/0x17A119) before it pushes
     job 0xAF (greening, adjacent) or 0xAE (walking to it): the aimed cell under both jobs. */
  g.aimX=(job==0xae||job==0xaf)?rx:-1;g.aimY=(job==0xae||job==0xaf)?ry:-1;
  return true;
 }
 // Living gardeners of player p (per-player settler list, next word +0x2A), optionally in one land region.
 void gardeners(uint8_t p,int wanted,std::vector<Gardener>&out)const{
  out.clear();uint32_t head=0;if(p<1||p>8||!read(image+0x1144cec+(p*0x43+GardenerType)*4,head))return;
  uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<4096;guard++){uintptr_t e=entity(id);if(!e)break;uint16_t next=value<uint16_t>(e+0x2a);
   Gardener g;if(gardener(e,p,g)&&(!wanted||region(g.x,g.y)==wanted))out.push_back(g);id=next;}
 }
 bool gardener(uint8_t p,uint16_t id,uint32_t uid,Gardener&g)const{uintptr_t e=entity(id);return e&&value<uint32_t>(e+4)==uid&&gardener(e,p,g);}
 // The ring a gardener must green: walkable footprint cells (buildingPosLines
 // minus blockPosLines) next to a blocked cell. Dark = flag 0x04; occupied = a
 // unit or building id in the ground map (the gardener's cell test 0x179E70
 // skips those).
 static bool inSet(const dt::Footprint*f,int x,int y){if(!f)return false;for(int i=0;i<f->count;i++)if(f->cells[i].x==x&&f->cells[i].y==y)return true;return false;}
 bool footprint(const Target&t,int x,int y)const{return inSet(dt::placementFootprint(t.kind),x-t.x,y-t.y);}
 void ring(const Target&t,std::vector<RingCell>&out)const{
  out.clear();auto*place=dt::placementFootprint(t.kind);auto*solid=dt::solidFootprint(t.kind);if(!place||!solid)return;
  static const int nx[6]={1,-1,0,0,1,-1},ny[6]={0,0,1,-1,1,-1};
  for(int i=0;i<place->count;i++){int cx=place->cells[i].x,cy=place->cells[i].y;if(inSet(solid,cx,cy))continue;bool edge=false;for(int k=0;k<6;k++)if(inSet(solid,cx+nx[k],cy+ny[k]))edge=true;if(!edge)continue;
   unsigned at=0;int x=t.x+cx,y=t.y+cy;if(!cell(x,y,at))continue;RingCell r;r.x=x;r.y=y;r.dark=(flags(x,y)&0x04)!=0;r.occupied=ground(x,y)!=0;out.push_back(r);}
 }
 // A destination beside a ring cell: walkable, unoccupied, outside the
 // footprint or already green (never the ring cell itself: a gardener cannot
 // work the cell it stands on).
 bool beside(const Target&t,const RingCell&c,int&x,int&y)const{
  static const int nx[6]={1,-1,0,0,1,-1},ny[6]={0,0,1,-1,1,-1};int best=-1;
  for(int k=0;k<6;k++){int px=c.x+nx[k],py=c.y+ny[k];unsigned at=0;if(!cell(px,py,at))continue;BYTE f=flags(px,py);if((f&0x01)||ground(px,py))continue;
   bool out=!footprint(t,px,py);if(!out&&(f&0x04))continue;int score=out?0:1;if(best<0||score<best){best=score;x=px;y=py;}}
  return best>=0;
 }
 // Nearest free cell outside the target's footprint for a unit standing on it that accept() takes
 // (r30: never a cell within the citadel fire's FireLine).
 template<class Accept> bool outside(const Target&t,int fx,int fy,Accept accept,int&x,int&y)const{
  for(int r=1;r<=5;r++)for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){int px=fx+dx,py=fy+dy;if(distance(fx,fy,px,py)!=r||footprint(t,px,py)||!free(px,py)||!accept(px,py))continue;x=px;y=py;return true;}
  return false;
 }
 // r30: the Dark player's ready-farm counter, as the citadel fire 0x1458C0 reads it (0x1458F4:
 // [0x10EF7AC + p*0x298], the ready slot of type 49 in the per-player building counts at
 // 0x10EF624 + (p*0x53 + type)*8); -1 when unreadable or out of range.
 int readyFarms()const{int n=-1;if(!read(image+0x10ef7ac+DarkPlayer*0x298,n)||n<0||n>2048)return -1;return n;}
 // r30 census (log only): living soldiers of player p (types 29..44, 61..63) standing within FireLine on a
 // walkable Dark cell (flags & 5 == 4, the fire's own victim test) that no mod squad in own holds.
 int fireCensus(uint8_t p,const Fire&f,const std::vector<uintptr_t>&own,int&fx,int&fy)const{
  int n=0;uint32_t head=0;if(!f.on||f.z.empty()||p<2||p>8)return 0;
  for(uint16_t type=29;type<=63;type++){if(type>44&&type<61)continue;if(!read(image+0x1144cec+(p*0x43+type)*4,head))continue;uint16_t id=(uint16_t)head;
   for(int guard=0;id&&guard<4096;guard++){uintptr_t e=entity(id);if(!e)break;uint16_t next=value<uint16_t>(e+0x2a);uint32_t bits=value<uint32_t>(e+0x14);
    if(value<BYTE>(e+10)==1&&value<uint16_t>(e+12)==type&&(value<BYTE>(e+32)>>4)==p&&value<BYTE>(e+33)&&(bits&0x2008000)==0x2000000&&!(bits&0x4000000)){
     int x=value<uint16_t>(e+24),y=value<uint16_t>(e+26);
     if(inFire(x,y,f)&&(flags(x,y)&0x05)==0x04){uintptr_t rec=value<uintptr_t>(e+0x10),tf=rec?value<uintptr_t>(rec+8):0;if(std::find(own.begin(),own.end(),tf)==own.end()){n++;fx=x;fy=y;}}}
    id=next;}}
  return n;
 }
 // A cell the escort may hold: free and at least EscortGap from every footprint cell.
 bool spare(const Target&t,int x,int y)const{auto*f=dt::placementFootprint(t.kind);if(!f||!free(x,y))return false;for(int i=0;i<f->count;i++)if(distance(x,y,t.x+f->cells[i].x,t.y+f->cells[i].y)<EscortGap)return false;return true;}
 // A Temple/Fortress role still parking entering settlers ([role+0x1A4] != [role+0x1A8]).
 bool parked(const Target&t)const{if(t.kind!=Temple&&t.kind!=Fortress)return false;uintptr_t e=entity(t.id),role=0,first=0,last=0;return e&&value<uint32_t>(e+4)==t.uid&&read(e+0x50,role)&&read(role+0x1a4,first)&&read(role+0x1a8,last)&&first!=last;}
 // Nearest Dark cell a gardener can work (Dark, not blocked, unoccupied) within
 // radius, in land region wanted (0: any), outside every keep circle; with
 // owner set, only cells of that player's territory (owner()).
 bool darkNear(int x,int y,int radius,const std::vector<Keep>&keep,int wanted,int&dx,int&dy,uint8_t owner=0)const{
  int n=dtmap::size(image);uintptr_t fl=0,gr=0;if(!n||!read(image+0x11630d8,fl)||!read(image+0x11630dc,gr)||!readable(fl,(size_t)n*n)||!readable(gr,(size_t)n*n*2))return false;
  bool found=false;int best=0;
  for(int py=std::max(2,y-radius);py<=std::min(n-3,y+radius);py++)for(int px=std::max(2,x-radius);px<=std::min(n-3,x+radius);px++){
   BYTE f=*(BYTE*)(fl+py*n+px);if((f&0x25)!=0x04||*(uint16_t*)(gr+2*(py*n+px)))continue;int d=distance(x,y,px,py);if(d>radius||(found&&d>=best))continue;
   if(kept(keep,px,py)||(wanted&&region(px,py)!=wanted)||(owner&&this->owner(px,py)!=owner))continue;found=true;best=d;dx=px;dy=py;}
  return found;
 }
 // Territory owner of a cell, as the native reader 0x58A40 (and S4ModApi LandscapeGetOwner):
 // tile = word [[0xEF1A18] + 2*cell], owner = dword [0xEF1A20 + 20*tile] & 0xF; 0 = nobody.
 uint8_t owner(int x,int y)const{
  unsigned at=0;uintptr_t map=0;uint16_t tile=0;uint32_t head=0;
  if(!cell(x,y,at)||!read(image+0xef1a18,map)||!read(map+2*at,tile)||tile>=0x8000||!read(image+0xef1a20+tile*20u,head))return 0;
  return (head&15)<=8?(uint8_t)(head&15):0;
 }
 // Dark land the computers own (they cannot build on it: BuildBuilding 0x13E4E0 tests flags & 0x5F
 // and the owner of every footprint cell). A rotating band scan of the flag map counts walkable Dark
 // cells per player and 16x16 block; a full pass takes side/rows ticks. Territory claims never skip
 // Dark cells (0x20CD30) and the human's Dark land spreads into computer territory (0x175199).
 static constexpr int Block=16,Blocks=36;
 uint16_t building[9][Blocks*Blocks]={},owned[9][Blocks*Blocks]={};int scanRow=0,scanSide=0;bool scanned=false;
 void resetTerritory(){memset(building,0,sizeof(building));memset(owned,0,sizeof(owned));scanRow=0;scanSide=0;scanned=false;}
 void scanTerritory(int rows){
  int n=dtmap::size(image);if(!n||n>Blocks*Block)return;if(n!=scanSide){resetTerritory();scanSide=n;}
  uintptr_t fl=0,map=0;if(!read(image+0x11630d8,fl)||!read(image+0xef1a18,map)||!fl||!map||!readable(fl,(size_t)n*n)||!readable(map,(size_t)n*n*2)||!readable(image+0xef1a20,0x8000*20))return;
  for(int r=0;r<rows;r++){int y=scanRow;
   for(int x=0;x<n;x++){BYTE f=*(BYTE*)(fl+y*n+x);if((f&0x05)!=0x04)continue;uint16_t tile=*(uint16_t*)(map+2*(y*n+x));if(tile>=0x8000)continue;
    int p=*(uint32_t*)(image+0xef1a20+tile*20u)&15;if(p<2||p>8)continue;uint16_t&k=building[p][(y/Block)*Blocks+x/Block];if(k<0xffff)k++;}
   if(++scanRow>=n){scanRow=0;memcpy(owned,building,sizeof(owned));memset(building,0,sizeof(building));scanned=true;}}
 }
 int ownedDark(uint8_t p)const{int t=0;if(p>=2&&p<=8)for(auto k:owned[p])t+=k;return t;}
 // Nearest workable Dark cell of player p's territory: the blocks with owned Dark land nearest first
 // (at most 32 searched; blocks wholly inside a keep circle are skipped), the nearest cell in the first block that has one.
 bool ownDark(uint8_t p,int x,int y,int radius,const std::vector<Keep>&keep,int wanted,int&dx,int&dy)const{
  int n=dtmap::size(image);if(!scanned||!n||n!=scanSide||p<2||p>8)return false;
  uintptr_t fl=0,gr=0;if(!read(image+0x11630d8,fl)||!read(image+0x11630dc,gr)||!readable(fl,(size_t)n*n)||!readable(gr,(size_t)n*n*2))return false;
  struct Near {int d,bx,by;};std::vector<Near> list;int nb=(n+Block-1)/Block;
  for(int by=0;by<nb;by++)for(int bx=0;bx<nb;bx++)if(owned[p][by*Blocks+bx]){int d=distance(x,y,bx*Block+Block/2,by*Block+Block/2);if(d<=radius+Block)list.push_back({d,bx,by});}
  std::sort(list.begin(),list.end(),[](const Near&a,const Near&b){return a.d<b.d;});
  int tried=0;for(auto&b:list){bool hidden=false;for(auto&k:keep)if(distance(b.bx*Block+Block/2,b.by*Block+Block/2,k.x,k.y)+Block<=k.r)hidden=true;if(hidden)continue;
   if(++tried>32)break;bool found=false;int best=0;
   for(int py=std::max(2,b.by*Block);py<std::min(n-2,(b.by+1)*Block);py++)for(int px=std::max(2,b.bx*Block);px<std::min(n-2,(b.bx+1)*Block);px++){
    BYTE f=*(BYTE*)(fl+py*n+px);if((f&0x25)!=0x04||*(uint16_t*)(gr+2*(py*n+px)))continue;int d=distance(x,y,px,py);if(d>radius||(found&&d>=best))continue;
    if(owner(px,py)!=p||kept(keep,px,py)||(wanted&&region(px,py)!=wanted))continue;found=true;best=d;dx=px;dy=py;}
   if(found)return true;}
  return false;
 }
 bool reach(int x,int y,int wanted)const{return wanted&&region(x,y)==wanted;}
 // Native gardener recruitment, as the specialist menu does (net message
 // 0x13AF -> 0xD5100 on the player's economy): a carrier walks to a shovel
 // (GOOD_SHOVEL 31) and becomes a gardener. Economy of a cell: EcoSectorPool
 // +6 -> AllEcoSectors (RVA 0x106C8A4), owner byte +0x12 (0xF61E0).
 uintptr_t economy(uint8_t p,int x,int y)const{
  unsigned at=0;uintptr_t map=0,eco=0;uint16_t tile=0,id=0,self=0;BYTE owner=0;
  if(!cell(x,y,at)||!read(image+0xef1a18,map)||!read(map+2*at,tile)||tile>=32768||!read(image+0xef1a26+tile*20,id)||!id||id>=0x4000||!read(image+0x106c8a4+4*id,eco)||!eco||!readable(eco,0x290)||!read(eco+0x12,owner)||owner!=p||!read(eco+0x10,self)||self!=id)return 0;
  return eco;
 }
 struct Supply {int alive=0,pending=0,shovels=0;bool carriers=false;uintptr_t eco=0;};
 Supply supply(uint8_t p,int x,int y)const{
  Supply s;std::vector<Gardener> all;gardeners(p,0,all);s.alive=(int)all.size();s.eco=economy(p,x,y);if(!s.eco)return s;
  s.pending=value<signed char>(s.eco+0x28c);
  uint16_t total=value<uint16_t>(s.eco+0x26),busy=value<uint16_t>(s.eco+0x22),reserve=value<uint16_t>(s.eco+0x20),idle=value<uint16_t>(s.eco+0xac);
  s.carriers=total>busy&&total-busy>reserve&&idle;
  /* Free shovels as 0x170B80 counts them: the pile stock (0xD4280 sums vtbl
     +0x28 over the eco's pile list for the good, word eco+0x130+good*2, next
     word +0x2A) minus each pile's reservations (byte +0x42). */
  s.shovels=((int(__thiscall*)(void*,int))(image+0xd4280))((void*)s.eco,31);
  uint16_t id=value<uint16_t>(s.eco+0x130+31*2);for(int guard=0;id&&guard<1024;guard++){uintptr_t e=entity(id);if(!e)break;s.shovels-=value<BYTE>(e+0x42);id=value<uint16_t>(e+0x2a);}
  return s;
 }
 // Read-only census of what keeps a computer attacking (logged as OFFENCE_SUPPLY): ready barracks
 // (type 24, flag 0x2000000), free stock of the home economy (0xD4280: SWORD 34, BOW 8, the race
 // weapons BATTLEAXE 5 / BLOWGUN 6 / BACKPACKCATAPULT 38, SHOVEL 31), carriers (eco +0x26 total,
 // +0x22 in flight, +0x20 reserve) and living soldiers (settler types 29..44 and 61..63).
 // r29: the computer's toolsmiths and the eco's shovel order (research/AI_OFFENCE_0215.md, r29).
 // Toolsmith = building type 21 (0x15) in the per-player building list ([0x10EEA74 + (p*0x53+type)*4],
 // next word +0x2A, as CEcoManager 0x4F4A0 counts them), ready like the net handler 0xF687D wants it
 // (GetEntityWithOwner 0xF6120: class 8, not deleted, owner; flag 0x2000000; role kind 4 = the
 // CProductionBuildingRole vtable 0xC59094, whose slot 28 0x13ADA0 returns 4). Its economy as 0xF6200
 // computes it: door +0x48 -> tile word [[0x11630C4] + 2*cell] -> EcoSectorPool+6 -> AllEcoSectors.
 uintptr_t buildingEconomy(uintptr_t e)const{
  int n=dtmap::size(image);uint32_t door=0;uintptr_t map=0,eco=0;uint16_t tile=0,id=0;if(!n||!read(e+0x48,door))return 0;int x=door&0xffff,y=(int)door>>16;
  if(x<0||y<0||x>=n||y>=n||!read(image+0x11630c4,map)||!map||!read(map+2*(unsigned)(y*n+x),tile)||tile>=32768||!read(image+0xef1a26+tile*20,id)||!id||id>=0x4000||!read(image+0x106c8a4+4*id,eco))return 0;
  return eco;
 }
 struct Tools {int toolsmiths=0,elsewhere=0,ordered=-1;};
 Tools tools(uint8_t p,uintptr_t eco)const{
  Tools k;if(p<2||p>8||!eco)return k;uint32_t head=0;
  if(read(image+0x10eea74+(p*0x53+21)*4,head)){uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<512;guard++){uintptr_t e=entity(id);if(!e)break;uint16_t next=value<uint16_t>(e+0x2a);
   uint32_t bits=value<uint32_t>(e+0x14);uintptr_t role=value<uintptr_t>(e+0x50);
   if(value<BYTE>(e+10)==8&&value<uint16_t>(e+12)==21&&(value<BYTE>(e+32)>>4)==p&&(bits&0x2000000)&&!(bits&0x4000000)&&role&&value<uintptr_t>(role)==image+0xc59094){if(buildingEconomy(e)==eco)k.toolsmiths++;else k.elsewhere++;}
   id=next;}}
  k.ordered=shovelOrdered(eco);return k;
 }
 // The eco's order count for SHOVEL (31): the tool list is the byte vector eco+0x354..+0x358, the
 // order counts the parallel byte vector eco+0x348..+0x34C (0xD6A90, consumer 0x14D008, save 0xD3F02).
 // -1 when the economy has no shovel slot.
 int shovelOrdered(uintptr_t eco)const{
  uintptr_t list=0,listEnd=0,counts=0,countsEnd=0;if(!eco||!read(eco+0x354,list)||!read(eco+0x358,listEnd)||!read(eco+0x348,counts)||!read(eco+0x34c,countsEnd))return -1;
  if(!list||listEnd<list||listEnd-list>64||!counts||countsEnd<counts||countsEnd-counts>64||!readable(list,listEnd-list)||!readable(counts,countsEnd-counts))return -1;
  for(uintptr_t i=0;i<listEnd-list;i++)if(*(BYTE*)(list+i)==31)return i<countsEnd-counts?*(BYTE*)(counts+i):-1;
  return -1;
 }
 // An economy remembered from an earlier tick is still the player's live one: its own id (+0x10) maps back
 // to it in AllEcoSectors (RVA 0x106C8A4) and its owner byte (+0x12, 0xF61E0) is the player, as economy() checks.
 bool economyLive(uint8_t p,uintptr_t eco)const{
  uint16_t self=0;uintptr_t back=0;BYTE owner=0;
  return eco&&readable(eco,0x360)&&read(eco+0x10,self)&&self&&self<0x4000&&read(image+0x106c8a4+4*self,back)&&back==eco&&read(eco+0x12,owner)&&owner==p;
 }
 // Place the order as the toolsmith menu does (net event 0x13A9 -> 0xD6A90(eco, good, amount),
 // thiscall, ret 8): only with a ready toolsmith in that economy, never on a continuous (100) order.
 // Returns whether the order count rose.
 bool orderShovels(const Tools&k,uintptr_t eco,int amount){
  if(!eco||k.toolsmiths<=0||amount<=0||amount>=100)return false;int before=shovelOrdered(eco);if(before<0||before>=100)return false;
  ((void(__thiscall*)(void*,int,int))(image+0xd6a90))((void*)eco,31,amount);dtmem::invalidate();
  return shovelOrdered(eco)>before;
 }
 struct Census {int barracks=0,swords=0,bows=0,special=0,shovels=0,carriers=0,busy=0,reserve=0,soldiers=0,gardeners=0,toolsmiths=0,shovelOrder=-1,ironbars=0,coal=0;bool eco=false;};
 Census census(uint8_t p,int x,int y)const{
  Census c;if(p<2||p>8)return c;uint32_t head=0;
  if(read(image+0x10eea74+(p*0x53+24)*4,head)){uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<512;guard++){uintptr_t e=entity(id);if(!e)break;if(value<BYTE>(e+10)==8&&(value<uint32_t>(e+0x14)&0x2000000))c.barracks++;id=value<uint16_t>(e+0x2a);}}
  for(uint16_t type=29;type<=63;type++){if(type>44&&type<61)continue;if(!read(image+0x1144cec+(p*0x43+type)*4,head))continue;uint16_t id=(uint16_t)head;for(int guard=0;id&&guard<4096;guard++){uintptr_t e=entity(id);if(!e)break;if(value<uint16_t>(e+12)==type)c.soldiers++;id=value<uint16_t>(e+0x2a);}}
  std::vector<Gardener> g;gardeners(p,0,g);c.gardeners=(int)g.size();
  uintptr_t eco=economy(p,x,y);if(!eco)return c;c.eco=true;auto stock=[&](int good){return ((int(__thiscall*)(void*,int))(image+0xd4280))((void*)eco,good);};
  c.swords=stock(34);c.bows=stock(8);c.special=stock(5)+stock(6)+stock(38);c.shovels=stock(31);c.ironbars=stock(20);c.coal=stock(10);dtmem::invalidate();
  Tools k=tools(p,eco);c.toolsmiths=k.toolsmiths;c.shovelOrder=k.ordered;
  c.carriers=value<uint16_t>(eco+0x26);c.busy=value<uint16_t>(eco+0x22);c.reserve=value<uint16_t>(eco+0x20);
  return c;
 }
 bool recruit(const Supply&s){
  if(!s.eco)return false;signed char before=value<signed char>(s.eco+0x28c);
  ((void(__thiscall*)(void*,int,int))(image+0xd5100))((void*)s.eco,GardenerType,1);dtmem::invalidate();
  return value<signed char>(s.eco+0x28c)>before;
 }
 // Hand every mod squad back (plugin stopped): soldiers return to the AI's reservoirs.
 int releaseAll(){int n=0;for(uint8_t p=2;p<=8;p++)for(int i=0;i<sectorCount(p);i++){std::vector<uintptr_t> found;modSquads(p,i,found);for(auto tf:found)if(dissolve(p,i,tf))n++;}return n;}
 // Units the native group order (0x16A800) accepts: alive, on the map.
 bool movable(uint16_t id,uint32_t uid)const{uintptr_t e=entity(id);uint32_t bits=0;return e&&value<uint32_t>(e+4)==uid&&read(e+0x14,bits)&&(bits&0x2008000)==0x2000000;}
};
}
