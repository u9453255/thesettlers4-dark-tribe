#pragma once
#include <windows.h>
#include <atomic>
#include "fast-memory.hpp"
#include "map-size.hpp"
#include "construction-model.hpp"
#include "site-panel-model.hpp"
#include "footprints.hpp"
#include "construction-signatures.hpp" // Private, generated from the pinned file.
namespace dtconstruction {
// Native CBuildingSiteRole (vtable RVA 0xC591BC), pinned executable only:
// +0x178 building info, +0x180 levelling slot, +0x18C phase, +0x19C progress,
// +0x1A0 total, +0x1AC clipping, +0x1B0/2/4 material pile IDs.
// Site init RVA 0x143680 sends Farm (49) and Hall (80) to the builder-free
// grow path (phase 6->7->8, completion at RVA 0x142312); Temple/Fortress use the
// ordinary levelling/builder path whose completion pair is RVA 0x1424C0/0x1429F0.
class Native {
 uintptr_t image=0;
 using BuildingAction=void(__thiscall*)(void*);
 using SiteAction=bool(__thiscall*)(void*);
 using SiteStep=void(__thiscall*)(void*,void*);
 using Detach=void(__thiscall*)(void*,uint32_t);
 using Delete=void(__stdcall*)(uint32_t,uint32_t);
 BuildingAction reserve=nullptr,complete=nullptr;
 SiteAction prepare=nullptr;
 SiteStep release=nullptr,finishSite=nullptr;
 Delete removeObject=nullptr;
 static bool readable(uintptr_t p,size_t bytes){return dtmem::readable(p,bytes);}
 template<class T> static bool read(uintptr_t p,T&v){if(!readable(p,sizeof(T)))return false;memcpy(&v,(void*)p,sizeof(T));return true;}
 template<class T> static bool write(uintptr_t p,T v){if(!dtmem::writable(p,sizeof(T)))return false;memcpy((void*)p,&v,sizeof(T));return true;}
 static bool grows(uint16_t kind){return kind==49||kind==80;}
 uintptr_t entity(uint16_t id)const{uintptr_t p=0;uint16_t slot=0;if(!id||!read(image+0xe9bc38+id*4,p)||!read(p+8,slot)||slot!=id||!readable(p,0x54))return 0;return p;}
 uintptr_t site(const dt::Construction&j)const{
  uintptr_t p=entity(j.site),role=0,table=0,info=0;uint32_t uid=0,total=0,scale=0;uint16_t type=0,ownerSlot=0;BYTE owner=0,cls=0;
  if(!p||!read(p+4,uid)||uid!=j.identity||!dt::foundationIdentity(uid)||!read(p+10,cls)||cls!=8||!read(p+32,owner)||owner!=0x13||!read(p+12,type)||type!=j.kind||!read(p+80,role)||!readable(role,0x1c8)||!read(role,table)||table!=image+0xc591bc)return 0;
  if(!read(role+6,ownerSlot)||ownerSlot!=j.site||!read(role+0x178,info)||(info!=image+0x109c600+(3*83+j.kind)*0x32c&&info!=safeInfo(j.kind))||!read(role+0x1a0,total)||!read(role+0x1a4,scale)||!total||total>1000000||!scale||scale>0xffff)return 0;
  uint16_t x=0,y=0;if(!read(p+24,x)||!read(p+26,y)||x!=j.x||y!=j.y)return 0;
  return role;
 }
 // Native map stride (RVA 0x1054138) of a supported map size; refuse anything else.
 bool cell(int x,int y,unsigned&at)const{int n=dtmap::size(image);if(!n||x<1||y<1||x>=n-1||y>=n-1)return false;at=y*n+x;return true;}
 // Same sector->economy lookup as RVA 0x1429F0 and the site tick head.
 uint16_t economy(uintptr_t p)const{
  uint32_t door=0;uintptr_t sectors=0;uint16_t sector=0,id=0;unsigned at=0;
  if(!read(p+0x48,door)||!cell(door&0xffff,(int32_t)door>>16,at)||!read(image+0xef1a18,sectors)||!read(sectors+2*at,sector)||!read(image+0xef1a26+sector*20,id))return 0;
  return id;
 }
 bool graphics(uint16_t type)const{
  // Native draw stage bits 2/3 use these original building components. Check
  // both vectors before funding an animation; never call a renderer on ticks.
  uintptr_t manager=0,bank=0,record=0,first=0,second=0;uint32_t count=0,id=0;
  return read(image+0xe94814,manager)&&read(manager+13*8+0x5f0,count)&&type<count&&read(manager+13*8+0x5ec,bank)&&read(bank+0x14+type*4,record)&&read(record,first)&&read(record+4,second)&&read(first,id)&&read(second,id);
 }
 // Hidden settlers (class 1 with flag 0x8000: inside a building or aircraft) do
 // not stand on the map. Not aircraft: their constructor always sets 0x8000.
 bool hidden(uint16_t id)const{uintptr_t p=entity(id);uint32_t bits=0;BYTE cls=0;return p&&read(p+10,cls)&&cls==1&&read(p+0x14,bits)&&(bits&0x8000);}
 bool mobile(uint16_t id)const{uintptr_t p=entity(id);uint32_t uid=0;BYTE cls=0;return p&&read(p+4,uid)&&dt::foundationIdentity(uid)&&read(p+10,cls)&&(cls==1||cls==0x40);}
 // Mirror RVA 0x1429F0's pile removal for a site without an economy: detach
 // through CBuilding slot 16 (RVA 0x137100), lower the pile count, delete mode 0.
 // The native site panel (role slot 22, RVA 0x143D10) dereferences the
 // ordinary economy at the site entrance for the inhabitant (+0x1DE) and for
 // each material pile (+0x1B0/2/4). Dark land has no such economy, so clicking
 // a growing Farm/Hall/Temple/Fortress crashed (RVAs 0x143E07, 0xD428A).
 // Growing sites therefore use a per-kind copy of the building info with the
 // inhabitant/tool fields cleared, and their piles are removed at payment.
 // The copy shares the native mask vectors (shallow) and is never freed.
 alignas(8) BYTE safeCopies[4][0x32c]{};bool safeReady[4]{};
 static int kindSlot(uint16_t kind){return kind==49?0:kind==50?1:kind==51?2:kind==80?3:-1;}
 uintptr_t safeInfo(uint16_t kind)const{int k=kindSlot(kind);return k<0||!safeReady[k]?1:(uintptr_t)safeCopies[k];}
 bool useSafeInfo(const dt::Construction&j,uintptr_t role){
  int k=kindSlot(j.kind);if(k<0)return false;
  if(!safeReady[k]){uintptr_t global=image+0x109c600+(3*83+j.kind)*0x32c;if(!readable(global,0x32c))return false;memcpy(safeCopies[k],(void*)global,0x32c);safeCopies[k][0x1de]=0;safeCopies[k][0x1df]=0;safeReady[k]=true;}
  uintptr_t info=0;if(!read(role+0x178,info))return false;
  return info==(uintptr_t)safeCopies[k]||write<uintptr_t>(role+0x178,(uintptr_t)safeCopies[k]);
 }
 // Site slot 8 (RVA 0x142BF0) "pause construction" runs its economy branch
 // (RVA 0x142CCE -> 0xD5B70, null economy on Dark land) only while entity flag
 // 0x1000 is set. Frozen Dark sites keep it clear (the native tick is frozen
 // anyway); it is restored before completion so the building is not paused.
 bool activeFlag(uintptr_t p,bool on){uint32_t bits=0;if(!read(p+0x14,bits))return false;uint32_t next=on?(bits|0x1000u):(bits&~0x1000u);return next==bits||write<uint32_t>(p+0x14,next);}
 bool removePiles(uintptr_t role,uintptr_t p){
  uint16_t piles[3]{};uintptr_t table=0,slot=0;uint32_t count=0,needed=0;
  if(!read(p,table)||!read(table+0x40,slot)||slot!=image+0x137100||!read(image+0xdcdb4c,count))return false;
  // Validate every pile before the first native mutation.
  for(int i=0;i<3;i++){if(!read(role+0x1b0+2*i,piles[i]))return false;if(piles[i]){if(!entity(piles[i])||piles[i]==(uint16_t)(p?*(uint16_t*)(p+8):0))return false;needed++;}}
  if(needed>count)return false;
  for(int i=0;i<3;i++){
   if(!piles[i])continue;
   ((Detach)slot)((void*)p,piles[i]);
   if(!read(image+0xdcdb4c,count)||!count||!write<uint32_t>(image+0xdcdb4c,count-1))return false;
   removeObject(piles[i],0);
   if(!write<uint16_t>(role+0x1b0+2*i,0))return false;
  }
  return true;
 }
 // Mirror native phase 4 (RVA 0x141F1B..0x1421CE) for ordinary sites frozen
 // before it. When building info+6 is set, the +0x314 mask cells leave the
 // object layer and are claimed (bit 8). Always: the site anchor enters the
 // building layer, which the renderer draws from. Without that anchor the
 // Temple/Fortress rise was invisible until completion.
 bool claimOrdinary(const dt::Construction&j,uintptr_t role){
  uintptr_t info=0,ground=0,objects=0,flags=0,first=0,last=0;BYTE alternate=0;signed char hx=0,hy=0;unsigned at=0;
  if(!read(role+0x178,info)||!read(info+6,alternate)||!read(info,hx)||!read(info+1,hy)||!cell(j.x,j.y,at)||!read(image+0x11630dc,ground)||!read(image+0x11630e0,objects)||!read(image+0x11630d8,flags))return false;
  if(alternate){
   if(!read(info+0x314,first)||!read(info+0x318,last)||last<first||(last-first)%4||(last-first)/4>64||!readable(first,last-first+1))return false;
   for(uintptr_t row=first;row<last;row+=4){
    uint32_t bits=0;if(!read(row,bits))return false;int r=(int)((row-first)/4);
    for(int b=0;b<32;b++)if(bits&(1u<<b)){
     unsigned c=0;BYTE f=0;if(!cell(j.x-hx+(31-b),j.y-hy+r,c)||!write<uint16_t>(objects+2*c,0)||!read(flags+c,f)||!write<BYTE>(flags+c,f|8))return false;
    }
   }
  }
  return write<uint16_t>(ground+2*at,j.site)&&write<uint16_t>(objects+2*at,0);
 }
public:
 // Mobile entities standing on the solid (blockPosLines) cells or the anchor.
 void blockers(const dt::Construction&j,std::vector<uint16_t>&out)const{
  out.clear();auto*solid=dt::solidFootprint(j.kind);uintptr_t ground=0;int n=dtmap::size(image);if(!n||!solid||!read(image+0x11630dc,ground)||!readable(ground,n*n*2))return;
  for(int i=-1;i<solid->count;i++){int x=j.x+(i<0?0:solid->cells[i].x),y=j.y+(i<0?0:solid->cells[i].y);if(x<1||y<1||x>=n-1||y>=n-1)continue;
   uint16_t id=0;if(!read(ground+2*(y*n+x),id)||!id||id==j.site||!mobile(id)||hidden(id))continue;if(std::find(out.begin(),out.end(),id)==out.end())out.push_back(id);}
 }
 struct Block {const char*reason="none";int x=0,y=0;uint16_t id=0;};
 bool initialize(uintptr_t module){
  image=module;reserve=(BuildingAction)(module+0x136470);complete=(BuildingAction)(module+0x1362f0);prepare=(SiteAction)(module+0x142d50);
  release=(SiteStep)(module+0x1424c0);finishSite=(SiteStep)(module+0x1429f0);removeObject=(Delete)(module+0x16d030);
  return intact(module);
 }
 bool completed(const dt::Construction&j)const{
  uintptr_t p=entity(j.site),r=0,v=0;uint32_t uid=0;uint16_t type=0,x=0,y=0;BYTE owner=0;
  if(!p||!read(p+4,uid)||uid!=j.identity||!dt::foundationIdentity(uid)||!read(p+32,owner)||owner!=0x13||!read(p+12,type)||type!=j.kind||!read(p+24,x)||!read(p+26,y)||x!=j.x||y!=j.y||!read(p+80,r)||!read(r,v))return false;
  return v==image+(j.kind==49?0xc58ae8:j.kind==80?0xc593d0:0xc59284);
 }
 dt::SiteSpace space(const dt::Construction&j,bool claimed,Block*why=nullptr)const{
  auto fail=[&](dt::SiteSpace result,const char*reason,int x,int y,uint16_t id=0){if(why)*why={reason,x,y,id};return result;};
  uintptr_t r=site(j);if(!r)return fail(dt::SiteSpace::Invalid,"site identity or native role",j.x,j.y);
  BYTE phase=0;int32_t level=0;if(!read(r+0x18c,phase)||!read(r+0x180,level))return dt::SiteSpace::Invalid;
  auto*body=dt::placementFootprint(j.kind);auto*solid=dt::solidFootprint(j.kind);if(!body||!solid)return dt::SiteSpace::Invalid;
  int n=dtmap::size(image);if(!n)return dt::SiteSpace::Invalid;
  uintptr_t flags=0,land=0,ground=0;if(!read(image+0x11630d8,flags)||!read(image+0x11630d4,land)||!read(image+0x11630dc,ground)||!readable(flags,n*n)||!readable(land,n*n*4)||!readable(ground,n*n*2))return dt::SiteSpace::Invalid;
  // Before the reservation, repeat the native placement acceptance. Once this
  // site owns its cells, native phase 7 checks only entity occupancy: objects
  // created nearby (e.g. planted mushrooms, RVA 0x163510) legitimately add the
  // keep-clear bit 0x40 to reserved cells and must not cancel the building.
  if(!claimed)for(int i=0;i<body->count;i++){
   int x=j.x+body->cells[i].x,y=j.y+body->cells[i].y;if(x<1||y<1||x>=n-1||y>=n-1)return dt::SiteSpace::Invalid;
   unsigned at=y*n+x;BYTE f=0,t=0,d=0;if(!read(flags+at,f)||!read(land+4*at+1,t)||!read(land+4*at+2,d))return dt::SiteSpace::Invalid;
   // Reservation bit 8 and the native solid bit 2 are expected on a claimed site.
   unsigned mask=0x57;if(phase==2&&level>=0)mask&=~0x10;
   if(!(d&64)||!dt::darkBuildTerrain(t)||(f&mask)!=4)return fail(dt::SiteSpace::StaticBlocked,"terrain or static flags",x,y);
  }
  uintptr_t table=image+0xe9bc38;if(!readable(table,65536*4))return dt::SiteSpace::Invalid;
  for(uint32_t id=1;id<65536;id++){
   if(id==j.site)continue;uintptr_t p=0;memcpy(&p,(void*)(table+id*4),4);BYTE cls=0;uint16_t type=0,x=0,y=0;if(!p||!read(p+10,cls)||cls!=8||!read(p+12,type)||!read(p+24,x)||!read(p+26,y))continue;
   if(abs((int)x-j.x)>64||abs((int)y-j.y)>64)continue;
   // Other building kinds were excluded by the native placement validator;
   // their entrances/anchors are still caught by the entity-map check below.
   auto*other=dt::placementFootprint(type);if(!other)continue;
   for(int a=0;a<body->count;a++)for(int b=0;b<other->count;b++)if(j.x+body->cells[a].x==x+other->cells[b].x&&j.y+body->cells[a].y==y+other->cells[b].y)return fail(dt::SiteSpace::StaticBlocked,"other building mask",x,y,(uint16_t)id);
  }
  // Native phase 7 waits on the solid (blockPosLines) cells' entity map.
  dt::SiteSpace result=dt::SiteSpace::Clear;
  for(int i=-1;i<solid->count;i++){
   int x=j.x+(i<0?0:solid->cells[i].x),y=j.y+(i<0?0:solid->cells[i].y);if(x<1||y<1||x>=n-1||y>=n-1)return dt::SiteSpace::Invalid;
   uint16_t id=0;if(!read(ground+2*(y*n+x),id))return dt::SiteSpace::Invalid;if(!id||id==j.site)continue;
   // Only another building is permanent. Settlers, aircraft, animals and
   // non-entity markers (native 0x20D870 treats 1..6 as transient) are waited
   // for, exactly as native phase 7 waits on this layer.
   if(hidden(id))continue; // stale entry of a unit inside a building/aircraft
   uintptr_t q=entity(id);BYTE cls=0;
   if(q&&read(q+10,cls)&&cls==8)return fail(dt::SiteSpace::StaticBlocked,"building on reserved cell",x,y,id);
   // Wild animals (class 0x80, owner 0; the Dark-land critters of types 2/3)
   // idle on reserved cells for minutes (field log: every Fortress waited on
   // one). Native construction checks occupancy only before claiming cells and
   // never at completion, so they do not block here. Exception: the anchor
   // tile, which completion writes into the entity map.
   BYTE owner=0;bool wild=q&&cls==0x80&&read(q+32,owner)&&owner==0;
   if(wild&&i>=0)continue;
   result=fail(dt::SiteSpace::MobileBlocked,wild?"wild animal on building anchor":q&&mobile(id)?"unit clearing reserved site":"entity on reserved cell",x,y,id);
  }
  return result;
 }
 // Paid start: freeze the native state machine (phase 1 is outside the
 // dispatched 2..8 range), detach ordinary levelling/builder requests and
 // apply the native reservation once. The building is never deleted.
 Block startWhy;
 Block lastStart()const{return startWhy;}
 bool begin(const dt::Construction&j){
  auto refuse=[&](const char*reason){startWhy={reason,j.x,j.y,0};return false;};
  startWhy={};uintptr_t r=site(j),p=entity(j.site);if(!r||!p)return refuse("site identity or native role");if(!graphics(j.kind))return refuse("building graphics unavailable");
  BYTE phase=0;int32_t level=0;if(!read(r+0x18c,phase)||!read(r+0x180,level)||phase<2||phase>8)return refuse("unexpected native site phase");
  bool grow=grows(j.kind);
  // Grow sites receive native solid flags in phase 6 and the reservation at the
  // end of phase 7; ordinary sites receive both at the end of phase 4.
  bool flagged=grow?phase>=7:phase>=5,reserved=grow?phase>=8:phase>=5;
  // The native validator accepted this placement on the click tick. Native
  // site init then edits these cells itself (Temple/Fortress terrain, stakes
  // via RVA 0x163940/0x163510, claim bit 8), so the flag check is never repeated.
  (void)flagged;auto available=space(j,true,&startWhy);if(available!=dt::SiteSpace::Clear&&available!=dt::SiteSpace::MobileBlocked)return false;
  if(!grow){
   if(phase==2&&(level< -1||level>=30))return refuse("unexpected levelling slot");
   // Dark placement can leave state 2 with no ordinary levelling slot (-1).
   // Do not let the native detach method index another player's slot then.
   if(phase==2&&level==-1&&!write<BYTE>(r+0x18c,1))return false;
   // Slot 11 detaches levelling/economy requests; it never deletes the building.
   if(!prepare((void*)r)||!site(j))return refuse("native request detach refused");
  }
  if(!write<BYTE>(r+0x18c,1)||!write<uint32_t>(r+0x19c,0)||!write<uint32_t>(r+0x1ac,0))return refuse("site fields not writable");
  if(!removePiles(r,p))return refuse("material pile removal refused");
  if(!useSafeInfo(j,r))return refuse("panel-safe building info unavailable");
  if(!activeFlag(p,false))return refuse("site flags not writable");
  if(!grow&&!reserved&&!claimOrdinary(j,r))return refuse("native phase-4 claim failed");
  if(!reserved)reserve((void*)p); // Native collision/path reservation, no unit relocation.
  return site(j)!=0;
 }
 bool progress(const dt::Construction&j,uint32_t tick){
  uintptr_t r=site(j);uint32_t total=0;if(!r||!read(r+0x1a0,total))return false;
  if(!useSafeInfo(j,r)||!activeFlag(entity(j.site),false))return false;
  uint32_t work=dt::growthWork(j,tick,total);
  return write<BYTE>(r+0x18c,1)&&write<uint32_t>(r+0x19c,work)&&write<uint32_t>(r+0x1ac,work);
 }
 // Native completion. Ordinary sites with an economy use the phase-5 pair
 // (piles, economy bookkeeping, flags, role change). Grow sites, and Dark land
 // without an ordinary economy, use the phase-8 sequence plus the same pile removal.
 bool finish(const dt::Construction&j){
  uintptr_t r=site(j),p=entity(j.site);uint32_t total=0,bits=0;uintptr_t ground=0,work=0;unsigned at=0;
  if(!r||!p||space(j,true)!=dt::SiteSpace::Clear||!read(r+0x1a0,total)||!read(p+0x14,bits)||!cell(j.x,j.y,at)||!read(image+0x11630dc,ground)||!read(image+0x11630e0,work))return false;
  if(!write<uint32_t>(r+0x19c,total)||!write<uint32_t>(r+0x1ac,total)||!activeFlag(p,true)||!read(p+0x14,bits))return false;
  // Phase 4/8: the building owns its anchor in the entity map; work slot cleared.
  if(!write<uint16_t>(ground+2*at,j.site)||!write<uint16_t>(work+2*at,0))return false;
  if(!grows(j.kind)&&economy(p)){
   release((void*)r,(void*)p);finishSite((void*)r,(void*)p);
   return completed(j);
  }
  if(!removePiles(r,p)||!write<uint32_t>(p+0x14,(bits&~0x1000000u)|0x2000000u))return false;
  complete((void*)p);
  return completed(j);
 }
};
// r34 site selection crash fix. Field crash (pid 8768, r33): a Dark Farm site
// was clicked in the same input batch that created it. Logic::Update (RVA
// 0xF2720) -> sub_E9AC0 runs the plugin tick hook (0xE9B18) once, then the
// event ring: the build net event (CNetInputProcessor, BuildBuilding at
// 0xF6714) and later click events (0x231100 -> SelectSingleEntity 0xEB3B0 ->
// 0xEE730 -> role slot 22). The new site still had the native info (inhabitant
// 53) and no economy: 0x143E07 read 0x8E. No native flag keeps a class-8
// entity of the player out of SelectOneAtPoint (0xED630), and the post-load
// role slot 7 (0x1419C0) writes the native info back too, so no tick-side fix
// can close the window. The CBuildingSiteRole slot-22 entry (RVA 0xC59214 in
// vtable 0xC591BC, pinned) is replaced once at startup by a wrapper that
// repeats the panel's own economy lookup (RVA 0x143D43..0x143D7B) and, only
// when there is no economy and a field the panel reads would need one, calls
// the original with a transient view (info copy without inhabitant/tool, piles
// 0) and restores the site's fields right after. Every caller (selection, box
// selection, the per-tick refresh 0xEDC10, 0xEF030) and every site (any race,
// kind, owner, paid or not, refused, after a load, plugin disabled) is covered.
// Completion is untouched: each completed role writes the native info itself.
// Runs on the game thread inside the native input loop: heap-free, lock-free,
// no Lua, no log(); the tick logs the counters (SITE_PANEL_GUARD).
struct PanelGuard {
 using Panel=void(__thiscall*)(void*,void*,uint32_t);
 static inline uintptr_t image=0;static inline Panel original=nullptr;static inline bool busy=false;
 static inline std::atomic<uint32_t> views{0},skips{0},lastSite{0},lastKind{0};
 alignas(8) static inline uint8_t view[dt::BuildingInfoSize]{};
 template<class T> static bool get(uintptr_t p,T&v){if(!dtmem::readable(p,sizeof(T)))return false;memcpy(&v,(void*)p,sizeof(T));return true;}
 // The same reads as RVA 0x143D43..0x143D7B (door [Entity[role+6]+0x48],
 // stride [0x1054138], tile map [0x11630C4], record 0xEF1A26+20*tile,
 // AllEcoSectors 0x106C8A4) and the info/pile fields the panel reads.
 static dt::PanelSite inspect(uintptr_t role,uintptr_t building,uintptr_t&info,uint16_t&type){
  dt::PanelSite s;uint16_t id=0,tile=0,eco=0;uintptr_t p=0,tiles=0,sector=0;uint32_t door=0,flags=0,stride=0;BYTE owner=0;
  if(!get(role+6,id)||!id||!get(image+0xe9bc38+4u*id,p)||!p||!get(p+0x48,door)||!get(p+0x14,flags)||!get(building+0xc,type)||!get(building+0x20,owner))return s;
  if(!get(image+0x1054138,stride)||!get(image+0x11630c4,tiles))return s;
  int32_t at=((int32_t)door>>16)*(int32_t)stride+(int32_t)(door&0xffff);
  if(!get(tiles+2*at,tile)||!get(image+0xef1a26+20u*tile,eco)||!get(image+0x106c8a4+4u*eco,sector))return s;
  if(!get(role+0x178,info)||!get(info+dt::InfoInhabitant,s.inhabitant)||!get(info+dt::InfoTool,s.tool)||!dtmem::readable(info+0x1ec,4)||!get(role+0x1b0,s.piles))return s;
  (void)flags;(void)owner;s.economy=sector!=0;s.readable=true;s.busy=busy;
  s.writable=dtmem::readable(info,dt::BuildingInfoSize)&&dtmem::writable(role+0x178,4)&&dtmem::writable(role+0x1b0,6);
  return s;
 }
 static void __fastcall panel(void*role,void*,void*building,uint32_t flag){
  uintptr_t info=0;uint16_t type=0;dt::PanelSite s=inspect((uintptr_t)role,(uintptr_t)building,info,type);
  switch(dt::panelCall(s)){
   case dt::PanelCall::Native:original(role,building,flag);return;
   case dt::PanelCall::Skip:skips++;lastKind=type;return;
   case dt::PanelCall::SafeView:break;
  }
  views++;lastKind=type;{uint16_t id=0;memcpy(&id,(BYTE*)role+6,2);lastSite=id;}
  busy=true;dt::panelSafeInfo(view,(const uint8_t*)info);
  dt::withPanelView((uintptr_t*)((BYTE*)role+0x178),(uint16_t*)((BYTE*)role+0x1b0),(uintptr_t)view,[&]{original(role,building,flag);});
  busy=false;
 }
 // Once at startup, after the pinned signatures passed: the entry must still
 // be the native panel (RVA 0x143D10, `ret 8`).
 static bool install(uintptr_t module){
  uintptr_t*slot=(uintptr_t*)(module+0xc59214);
  if(*slot==(uintptr_t)&panel)return true;
  if(*slot!=module+0x143d10)return false;
  image=module;original=(Panel)(module+0x143d10);
  DWORD old=0;if(!VirtualProtect(slot,4,PAGE_READWRITE,&old))return false;
  *slot=(uintptr_t)&panel;VirtualProtect(slot,4,old,&old);
  return *slot==(uintptr_t)&panel;
 }
};
}
