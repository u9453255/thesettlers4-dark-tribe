#include "construction-model.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <utility>
#include <vector>
#include <algorithm>
using Result=dt::ConstructionResult;using Space=dt::SiteSpace;
struct Engine {
 uint32_t uid=100,bank=1000,work=0;bool site=true,dark=true,done=false,beginOk=true,finishOk=true,transactionOk=true,progressOk=true;
 Space room=Space::Clear;
 int charges=0,refunds=0,cancellations=0,begins=0,finishes=0,lastMode=-1,unitWaits=0,evictions=0,duplicates=0,unpaid=0;
 void evict(const dt::Construction&){evictions++;}
 bool sameSite(const dt::Construction&j){return uid==j.identity;}
 bool foundation(const dt::Construction&j){return site&&!done&&dt::foundationIdentity(uid)&&sameSite(j);}
 bool darkLand(const dt::Construction&){return dark;}
 uint32_t mana(){return bank;}
 bool changeMana(int amount){if(!transactionOk||int64_t(bank)+amount<0)return false;bank+=amount;amount<0?charges++:refunds++;return true;}
 bool remove(const dt::Construction&,int mode){lastMode=mode;cancellations++;uid|=0x20000000;return true;}
 bool begin(const dt::Construction&){begins++;return beginOk;}
 bool progress(const dt::Construction&j,uint32_t t){work=dt::growthWork(j,t,200);return progressOk;}
 bool completed(const dt::Construction&j){return done&&sameSite(j);}
 Space space(const dt::Construction&){return room;}
 bool finish(const dt::Construction&){finishes++;if(finishOk)done=true;return finishOk;}
 void report(dt::ConstructionEvent e,const dt::Construction&){if(e==dt::ConstructionEvent::UnitWait)unitWaits++;if(e==dt::ConstructionEvent::Duplicate)duplicates++;if(e==dt::ConstructionEvent::Unpaid)unpaid++;}
};
static dt::Construction job(uint16_t kind){dt::Construction j;j.identity=100;j.site=10;j.kind=kind;j.cost=dt::buildCost(kind);j.x=406;j.y=308;return j;}
// Several native sites at once (two outlines placed quickly): one engine per
// site identity, sharing the mana bank.
struct Sites {
 uint32_t bank=1000;int charges=0,cancellations=0,duplicates=0,unpaid=0,fire=0;bool dark=true;std::vector<uint32_t> gone;
 bool present(uint32_t uid){return std::find(gone.begin(),gone.end(),uid)==gone.end();}
 bool sameSite(const dt::Construction&j){return present(j.identity);}
 bool foundation(const dt::Construction&j){return sameSite(j);}
 bool darkLand(const dt::Construction&){return dark;}
 uint32_t mana(){return bank;}
 bool changeMana(int amount){if(int64_t(bank)+amount<0)return false;bank+=amount;if(amount<0)charges++;return true;}
 bool remove(const dt::Construction&j,int mode){assert(mode==1);cancellations++;gone.push_back(j.identity);return true;}
 bool begin(const dt::Construction&){return true;}
 bool progress(const dt::Construction&,uint32_t){return true;}
 bool completed(const dt::Construction&){return false;}
 dt::SiteSpace space(const dt::Construction&){return dt::SiteSpace::Clear;}
 void evict(const dt::Construction&){}
 bool finish(const dt::Construction&){return true;}
 void report(dt::ConstructionEvent e,const dt::Construction&){if(e==dt::ConstructionEvent::Duplicate)duplicates++;if(e==dt::ConstructionEvent::Unpaid)unpaid++;if(e==dt::ConstructionEvent::FireBlocked)fire++;}
};
// The tick's discovery as in dark-tribe.cpp construction(): recorded jobs and
// completed buildings first, then each new site in scan order.
static dt::BuildingLimit discover(dt::State&s,Sites&e,uint16_t built[2],std::initializer_list<std::pair<uint32_t,uint16_t>> fresh,uint32_t tick){
 dt::BuildingLimit l;dt::countJobs(l,s);for(int i=0;i<2;i++)for(int n=0;n<built[i];n++)l.countBuilt(i?51:50);
 for(auto site:fresh){auto&j=s.construction[s.constructionCount++];j=job(site.second);j.identity=site.first;j.site=(uint16_t)site.first;dt::discoverConstruction(j,tick,e,l);}
 return l;
}
// r30 citadel fire blockade: geometry, both directions, discovery refusal
// before payment, same-tick order and player texts.
static dt::Construction site(uint32_t uid,uint16_t kind,int x,int y){dt::Construction j;j.identity=uid;j.site=(uint16_t)uid;j.kind=kind;j.cost=dt::buildCost(kind);j.x=(uint16_t)x;j.y=(uint16_t)y;return j;}
static void fireBlockade(){
 using dtfire::FarmLock;static_assert(FarmLock==33&&dtfire::FireLine==27&&dtfire::FarmReach==4,"blockade shares the offence farm lock");
 const int bx=300,by=300,cx=bx+1,cy=by+2; /* Temple position; fire centre = position + (1,2) */
 dt::FireBlockade b;b.add(50,bx,by);
 // Euclidean on the axial grid, strict: fire2 < 33*33 is blocked.
 assert(b.blocked(49,cx+32,cy)&&!b.blocked(49,cx+33,cy)&&b.blocked(49,cx-32,cy)&&!b.blocked(49,cx-33,cy));
 assert(b.blocked(49,cx,cy+32)&&!b.blocked(49,cx,cy+33)&&b.blocked(49,cx+32,cy+32)&&!b.blocked(49,cx+33,cy+33));
 // Between the axes (2k,k): Euclid sqrt(3)k; hex distance 2k overstates it. k=19 (hex 38, Euclid 32.9) is blocked, k=20 (34.6) is not.
 assert(dtfire::fire2(cx+38,cy+19,cx,cy)==1083&&b.blocked(49,cx+38,cy+19)&&!b.blocked(49,cx+40,cy+20));
 assert(b.blocked(49,bx,by)&&b.farmBlocker(cx+10,cy)==&b.citadels[0]&&!b.farmBlocker(cx+40,cy));
 // Only farms are blocked by citadels; the Hall (80) has no fire and is never blocked; a citadel alone blocks no citadel.
 assert(!b.blocked(80,cx,cy)&&!b.blocked(51,cx+5,cy)&&!b.blocked(50,cx,cy)&&!b.blocked(0,cx,cy));
 // Fortress fire too, and every cell of a full disc agrees with farmTooClose.
 {dt::FireBlockade f;f.add(51,100,100);assert(f.blocked(49,101+32,102)&&!f.blocked(49,101+33,102));unsigned cells=0;
  for(int y=0;y<200;y++)for(int x=0;x<200;x++){bool in=f.blocked(49,x,y);assert(in==dtfire::farmTooClose(x,y,100,100));if(in)cells++;}
  assert(cells==3943);} /* hex cells with dx*dx-dx*dy+dy*dy < 33*33 (pi*33^2/0.866 is about 3,950) */
 // The other direction: a Temple/Fortress is blocked by any own farm its field would hold.
 {dt::FireBlockade f;f.add(49,cx+20,cy);assert(f.blocked(50,bx,by)&&f.blocked(51,bx,by)&&!f.blocked(80,bx,by)&&!f.blocked(49,bx,by));
  assert(!f.blocked(50,bx-14,by)&&f.blocked(50,bx-12,by)); /* farm at centre+(20,0): Temple 33 or more to the left is free */
  for(int dy=-40;dy<=40;dy++)for(int dx=-40;dx<=40;dx++){dt::FireBlockade t;t.add(50,bx+dx,by+dy);assert(t.blocked(49,cx+20,cy)==f.blocked(50,bx+dx,by+dy));}}
 // Snapshot equality ignores scan order.
 {dt::FireBlockade a,c;a.add(49,1,2);a.add(49,3,1);a.add(51,9,9);c.add(51,9,9);c.add(49,3,1);c.add(49,1,2);assert(a!=c);a.normalize();c.normalize();assert(a==c);c.add(80,5,5);assert(a==c);c.add(49,5,5);assert(a!=c);}
 // Player texts: short, no coordinates or internals.
 for(int k:{49,50,51}){const char*t=dt::fireNotice(k);assert(t[0]&&strlen(t)<100);for(const char*q=t;*q;q++)assert(*q<'0'||*q>'9');}
 assert(!dt::fireNotice(80)[0]&&!dt::fireNotice(0)[0]);
 // Discovery: a Farm site inside a Temple field is cancelled with mode 1 before payment and never charged, counted or rediscovered.
 {dt::State s;Sites e;dt::BuildingLimit l;dt::FireBlockade f;f.add(50,bx,by);
  auto&in=s.construction[s.constructionCount++];in=site(100,49,cx+20,cy);dt::discoverConstruction(in,10,e,l,&f);
  assert(in.refunded&&in.removed&&in.cancelRequested&&!in.pad&&e.fire==1&&e.cancellations==1&&e.bank==1000&&!e.charges&&f.farms.empty());
  assert(dt::updateConstruction(in,11,e)==Result::Retired&&!e.charges&&dt::valid(s));
  auto&out=s.construction[s.constructionCount++];out=site(101,49,cx+40,cy);dt::discoverConstruction(out,10,e,l,&f);
  assert(!out.refunded&&e.fire==1&&f.farms.size()==1&&f.farms[0].x==cx+40);
  assert(dt::updateConstruction(out,10,e)==Result::Waiting&&e.charges==1); /* paid as before */
  // The kept farm site now blocks a Temple/Fortress whose field would hold it; a far Fortress is kept and joins the blockade.
  auto&t=s.construction[s.constructionCount++];t=site(102,51,cx+40,cy+5);dt::discoverConstruction(t,10,e,l,&f);assert(t.refunded&&e.fire==2&&!l.exists(51));
  auto&distant=s.construction[s.constructionCount++];distant=site(103,51,cx+120,cy);dt::discoverConstruction(distant,10,e,l,&f);assert(!distant.refunded&&l.exists(51)&&f.citadels.size()==2);
  // ... and that Fortress's field now refuses farms next to it.
  auto&near2=s.construction[s.constructionCount++];near2=site(104,49,cx+110,cy);dt::discoverConstruction(near2,10,e,l,&f);assert(near2.refunded&&e.fire==3);
  // A second Temple is a Duplicate first (one notice), even inside the field.
  dt::BuildingLimit one;one.countBuilt(50);auto&dup=s.construction[s.constructionCount++];dup=site(105,50,bx,by);dt::discoverConstruction(dup,10,e,one,&f);assert(dup.refunded&&e.duplicates==1&&e.fire==3);
  assert(dt::valid(s));}
 // Without a blockade (old call form) nothing changes.
 {dt::State s;Sites e;dt::BuildingLimit l;auto&j=s.construction[s.constructionCount++];j=site(100,49,cx,cy);dt::discoverConstruction(j,10,e,l);assert(!j.refunded&&!e.fire);}
 // Same tick, citadel first (as construction() orders new sites): the Temple is kept, the farm beside it refused.
 {dt::State s;Sites e;dt::BuildingLimit l;dt::FireBlockade f;
  auto&t=s.construction[s.constructionCount++];t=site(100,50,bx,by);dt::discoverConstruction(t,10,e,l,&f);
  auto&m=s.construction[s.constructionCount++];m=site(101,49,cx+3,cy+20);dt::discoverConstruction(m,10,e,l,&f);
  assert(!t.refunded&&m.refunded&&e.fire==1&&l.exists(50));}
}
static void limits(){
 using dt::LimitFree;using dt::LimitBuilt;using dt::LimitBuilding;
 // Only Temple (50) and Fortress (51) are limited; Farm and Hall never are.
 assert(dt::limitSlot(50)==0&&dt::limitSlot(51)==1&&dt::limitSlot(49)<0&&dt::limitSlot(80)<0&&dt::limitSlot(0)<0);
 assert(!strcmp(dt::limitNotice(50),"You already have a Dark Temple.")&&!strcmp(dt::limitNotice(51),"You already have a Dark Fortress.")&&!dt::limitNotice(49)[0]);
 {dt::BuildingLimit l;for(int k=0;k<5;k++){l.countBuilt(49);l.countBuilding(80);}assert(!l.exists(49)&&!l.exists(80)&&l.state(49)==LimitFree&&l.state(80)==LimitFree);}
 uint16_t none[2]={0,0},templeBuilt[2]={1,0},both[2]={1,1};
 // None: the first Temple and the first Fortress are admitted (and paid by the job update as before).
 {dt::State s;Sites e;auto l=discover(s,e,none,{{100,50},{101,51}},10);
  assert(l.state(50)==LimitBuilding&&l.state(51)==LimitBuilding&&!e.cancellations&&!e.duplicates&&!s.construction[0].refunded&&!s.construction[1].refunded);
  assert(dt::updateConstruction(s.construction[0],10,e)==dt::ConstructionResult::Waiting&&e.charges==1&&s.construction[0].removed&&s.construction[0].pad==dt::NativeGrowth);}
 // Completed: a second Temple is refused before payment (mode 1, never charged); a Fortress is still allowed.
 {dt::State s;Sites e;auto l=discover(s,e,templeBuilt,{{100,50},{101,51}},10);auto&t=s.construction[0];
  assert(t.refunded&&t.removed&&t.cancelRequested&&!t.pad&&e.duplicates==1&&e.cancellations==1&&e.bank==1000&&!e.charges);
  assert(l.state(50)==LimitBuilt&&l.state(51)==LimitBuilding&&!s.construction[1].refunded);
  // The refused record is kept until the site disappears and is never charged, counted or completed.
  assert(dt::updateConstruction(t,11,e)==dt::ConstructionResult::Retired&&!e.charges);
  e.gone.clear();for(uint32_t k=11;k<400;k++)assert(dt::updateConstruction(t,k,e)==dt::ConstructionResult::Waiting);assert(!e.charges&&e.cancellations==1);
  dt::BuildingLimit again;dt::countJobs(again,s);assert(!again.exists(50)&&again.exists(51));
  assert(dt::valid(s)); /* schema-2 record: removed/refunded/cancelRequested, pad 0 */}
 {dt::State s;Sites e;auto l=discover(s,e,both,{{100,50},{101,51}},10);assert(e.duplicates==2&&e.cancellations==2&&e.bank==1000&&l.state(50)==LimitBuilt&&l.state(51)==LimitBuilt);}
 // Site / pending job: an unpaid job and a paid, growing job of the kind both count (they will become one).
 for(int paid=0;paid<2;paid++){dt::State s;s.constructionCount=1;s.construction[0]=job(51);s.construction[0].identity=7;if(paid){s.construction[0].removed=1;s.construction[0].pad=dt::NativeGrowth;}
  Sites e;auto l=discover(s,e,none,{{100,51},{101,50}},20);
  assert(l.state(51)==LimitBuilding&&s.construction[1].refunded&&e.duplicates==1&&e.cancellations==1&&!s.construction[2].refunded&&l.state(50)==LimitBuilding);}
 // Built wins over building in the shown state.
 {dt::BuildingLimit l;l.countBuilding(50);assert(l.state(50)==LimitBuilding);l.countBuilt(50);assert(l.state(50)==LimitBuilt);}
 // Refunded / lost: a refunded job does not count, so the base can be rebuilt.
 {dt::State s;s.constructionCount=1;s.construction[0]=job(50);s.construction[0].removed=1;s.construction[0].refunded=1;s.construction[0].cancelRequested=1;
  Sites e;auto l=discover(s,e,none,{{100,50}},30);assert(!s.construction[1].refunded&&!e.duplicates&&l.state(50)==LimitBuilding);}
 {Engine lost;auto j=job(50);dt::updateConstruction(j,10,lost);dt::State s;s.constructionCount=1;s.construction[0]=j;
  dt::BuildingLimit before;dt::countJobs(before,s);assert(before.exists(50));
  lost.site=false;assert(dt::updateConstruction(s.construction[0],20,lost)==dt::ConstructionResult::Refunded&&s.construction[0].refunded);
  dt::BuildingLimit after;dt::countJobs(after,s);assert(!after.exists(50)&&after.state(50)==LimitFree);}
 // A destroyed completed base is no longer counted (the engine skips UIDs marked for deletion): rebuilding is allowed.
 {dt::State s;Sites e;auto l=discover(s,e,none,{{100,50}},40);assert(!e.duplicates&&l.state(50)==LimitBuilding);}
 // Two quick placements in one tick: the first outline is kept, the second cancelled, nothing charged for it.
 for(uint16_t k:{50,51}){dt::State s;Sites e;auto l=discover(s,e,none,{{100,k},{101,k},{102,k}},50);
  assert(!s.construction[0].refunded&&s.construction[1].refunded&&s.construction[2].refunded&&e.duplicates==2&&e.cancellations==2&&l.state(k)==LimitBuilding);
  assert(dt::updateConstruction(s.construction[0],50,e)==dt::ConstructionResult::Waiting&&e.charges==1&&e.bank==1000-dt::buildCost(k));
  // Next tick: the kept job alone counts; another outline is refused again.
  auto next=discover(s,e,none,{{103,k}},51);assert(s.construction[3].refunded&&e.duplicates==3&&e.charges==1&&next.state(k)==LimitBuilding);}
 // Two quick placements without the mana: both are refused as unfunded, not as duplicates (a refused site is not counted).
 {dt::State s;Sites e;e.bank=99;auto l=discover(s,e,none,{{100,50},{101,50}},60);assert(e.unpaid==2&&!e.duplicates&&l.state(50)==LimitFree&&!e.charges);}
 // Off Dark land: refused, not counted.
 {dt::State s;Sites e;e.dark=false;auto l=discover(s,e,none,{{100,51}},70);assert(e.unpaid==1&&l.state(51)==LimitFree);}
}
int main(){
 assert(dt::FoundationCancelMode==1&&dt::growthTicks(49)==40&&dt::growthTicks(80)==60&&dt::growthTicks(50)==80&&dt::growthTicks(51)==100);
 assert(dt::foundationIdentity(100)&&!dt::foundationIdentity(0)&&!dt::foundationIdentity(0x20000064)&&!dt::foundationIdentity(0x40000064));
 for(uint16_t kind:{49,50,51,80}){
  // Successful construction keeps the same site identity: no cancellation,
  // no deletion and no recreation. Growth is monotonic and below total.
  Engine e;auto j=job(kind);uint32_t initial=e.bank,last=0,ready=10+dt::growthTicks(kind);
  assert(dt::updateConstruction(j,10,e)==Result::Waiting&&j.removed&&j.pad==dt::NativeGrowth&&!j.cancelRequested&&!j.refunded);
  assert(e.charges==1&&e.bank==initial-j.cost&&e.begins==1&&j.readyAt==ready&&!e.cancellations);
  for(uint32_t t=11;t<ready;t++){assert(dt::updateConstruction(j,t,e)==Result::Waiting);assert(e.work>=last&&e.work<200);last=e.work;}
  assert(!e.finishes);
  assert(dt::updateConstruction(j,ready,e)==Result::Complete&&e.finishes==1&&e.begins==1&&e.charges==1&&!e.refunds&&!e.cancellations&&e.uid==100);
  // Units on the reserved solid shape are waited for (as native phase 7),
  // never refunded by timeout or forced through.
  Engine busy;busy.room=Space::MobileBlocked;auto w=job(kind);
  for(uint32_t t=10;t<5000;t++)assert(dt::updateConstruction(w,t,busy)==Result::Waiting);
  assert(!busy.finishes&&!busy.refunds&&!busy.cancellations&&busy.unitWaits>0);
  assert(busy.evictions>0&&busy.evictions<=(5000-10)/20+2); // throttled eviction orders while waiting
  busy.room=Space::Clear;assert(dt::updateConstruction(w,5000,busy)==Result::Complete&&busy.charges==1);
  // Static obstruction: one refund, native mode-1 cancellation, retained identity never recharged.
  Engine stuck;stuck.room=Space::StaticBlocked;auto s=job(kind);
  assert(dt::updateConstruction(s,10,stuck)==Result::Waiting);
  assert(dt::updateConstruction(s,ready,stuck)==Result::Refunded&&s.refunded&&stuck.bank==initial&&stuck.lastMode==1&&stuck.cancellations==1);
  stuck.uid=100; // Cancellation accepted but the original stays present.
  for(uint32_t t=ready+1;t<3000;t++)assert(dt::updateConstruction(s,t,stuck)==Result::Waiting);
  assert(stuck.charges==1&&stuck.refunds==1&&!stuck.finishes);
  stuck.uid=101;assert(dt::updateConstruction(s,3000,stuck)==Result::Retired);
  // Native start refusal and later site loss refund exactly once.
  Engine refused;refused.beginOk=false;auto r=job(kind);
  assert(dt::updateConstruction(r,10,refused)==Result::Refunded&&refused.bank==initial&&refused.cancellations==1&&r.refunded);
  Engine lost;auto l=job(kind);dt::updateConstruction(l,10,lost);lost.site=false;
  assert(dt::updateConstruction(l,20,lost)==Result::Refunded&&lost.refunds==1&&!lost.cancellations);
  // Refuse unpaid foundations even if future native production adds mana.
  Engine poor;poor.bank=j.cost-1;auto u=job(kind);
  assert(dt::updateConstruction(u,10,poor)==Result::Retired&&u.refunded&&poor.cancellations==1);
  poor.bank=1000;poor.uid=100;assert(dt::updateConstruction(u,400,poor)==Result::Waiting&&!poor.charges&&!poor.begins);
  // Old paid cancel/recreate jobs are refunded once and never recreated.
  Engine legacy;legacy.uid=0x20000064;auto old=job(kind);old.removed=1;old.cancelRequested=1;old.readyAt=40;legacy.bank=initial-old.cost;
  assert(dt::updateConstruction(old,500,legacy)==Result::Refunded&&legacy.bank==initial&&!legacy.cancellations&&!legacy.begins&&!legacy.finishes);
  // Transaction or native completion failure stops the plugin before further mutation.
  Engine bad;bad.transactionOk=false;auto b=job(kind);
  assert(dt::updateConstruction(b,10,bad)==Result::Fatal&&!b.removed&&!bad.begins);
  Engine broken;broken.finishOk=false;auto f=job(kind);dt::updateConstruction(f,10,broken);
  assert(dt::updateConstruction(f,ready,broken)==Result::Fatal&&broken.refunds==0);
 }
 dt::State st;st.constructionCount=1;st.construction[0]=job(49);
 st.construction[0].refunded=1;assert(!dt::valid(st));
 st.construction[0].removed=1;assert(dt::valid(st));
 st.construction[0].cancelRequested=2;assert(!dt::valid(st));
 st.construction[0]=job(50);st.construction[0].pad=1;assert(!dt::valid(st));
 st.construction[0].removed=1;assert(dt::valid(st));
 st.construction[0].pad=2;assert(!dt::valid(st));
 st.construction[0].pad=1;st.construction[0].cancelRequested=1;assert(!dt::valid(st));
 st.construction[0].refunded=1;assert(dt::valid(st));
 limits();
 fireBlockade();
 puts("PASS: all four in-place constructions keep their site identity, wait for units without timeout refunds, single refunds/cancellation for static or lost sites, unpaid and legacy jobs never recreated, fatal transaction/completion failures");
 puts("PASS: one Dark Temple and one Dark Fortress: built, site and pending jobs count, refunded/lost do not, a second site (also two in one tick) is cancelled before payment");
 puts("PASS: citadel fire blockade (r30): no Farm within Euclid 33 of a Temple/Fortress fire centre, no citadel whose field would hold a Farm, refused before payment, Hall never blocked");
}
