#include "offence-model.hpp"
#include "economy-telemetry.hpp"
#include <cstring>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <functional>
using namespace dtoffence;
// Fake engine: one computer sector at home (40,40); Dark buildings to the east.
// Moves teleport units to the ordered cell. Each Dark building has a ring of
// cells; every working gardener greens one ring cell per director step, and a
// building collapses when its ring is green (unless the trigger is blocked).
// Dark land cells (darkCells/ownCells) turn green where a gardener is sent and
// within 2 tiles of every working gardener (its native search).
struct FakeGardener {Gardener g;bool alive=true;};
struct Engine {
 Config cfg{};Strength str{};bool sectorOk=true,raiseOk=true,lostSquad=false,failDissolve=false,blockTrigger=false,allies=false;
 std::vector<Target> world;std::vector<Soldier> pool,army;std::vector<std::pair<int,int>> darkUnits,darkCells,ownCells;std::vector<FakeGardener> gards;
 std::map<uint16_t,std::vector<RingCell>> rings;
 int moves=0,homeMoves=0,destroyed=0,collapsed=0,dissolved=0,raises=0,declines=0,launches=0,adopted=0,lost=0,ignored=0,avoids=0,sieges=0,greenings=0,gardenerOrders=0,groupOrders=0,clears=0,reclaims=0,fronts=0,hunts=0,joins=0,calls=0,fires=0;
 // r30: the native ready-farm counter (-1 unreadable) and an order audit against the keeps while the fire is on.
 int ready=-1;bool checkKeep=false;int keepViolations=0;const char*lastFire="";const char*lastAvoid="";
 std::vector<Site> growing;
 Fire fireNow(){return fireFrom(world,ready,growing);}
 void audit(int x,int y,int keep){if(!checkKeep)return;Fire f=fireNow();if(closer(x,y,f,keep)){keepViolations++;fprintf(stderr,"keep violation at %d,%d keep %d clearance %.1f\n",x,y,keep,sqrt((double)clear2(x,y,f)));}}
 std::vector<uint16_t> order;std::vector<std::pair<int,int>> goals;const char*lastDecline="";const char*lastReturn="";const char*lastReclaim="";
 Target callTarget{};uint32_t callUntil=0;uint8_t caller=0;
 Engine(int gardeners=6){cfg.ok=true;cfg.mode=2;cfg.delay=1;cfg.chancePartial=20;cfg.thresholdPartial=175;cfg.thresholdTotal=225;
  str.ok=true;str.fighters=40;str.strength=400;str.darkValue=40;str.region=7;str.homeX=40;str.homeY=40;
  for(int i=0;i<40;i++)pool.push_back(Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),40,40});
  for(int i=0;i<gardeners;i++){FakeGardener f;f.g.id=(uint16_t)(300+i);f.g.uid=3000u+i;f.g.x=42;f.g.y=40;f.g.centreX=42;f.g.centreY=40;f.g.work=Work::Idle;gards.push_back(f);}}
 virtual ~Engine(){}
 Target&add(uint16_t id,uint16_t kind,int x,int y,bool reach=true){Target t;t.id=id;t.kind=kind;t.uid=id*7u;t.x=x;t.y=y;t.doorX=x-1;t.doorY=y+3;t.reachable=reach;world.push_back(t);return world.back();}
 bool sector(const Campaign&){return sectorOk;}
 Config config(uint8_t){return cfg;}
 Strength strength(const Campaign&){return str;}
 int defenders(int x,int y,int r,bool weighted){int n=0;for(auto&d:darkUnits)if(distance(d.first,d.second,x,y)<=r)n+=weighted?2:1;return n;}
 void targets(const Campaign&,std::vector<Target>&out){out=world;for(auto&t:out)t.defence=defenders(t.doorX,t.doorY,RaidRadius,true);}
 bool alive(const Target&t){for(auto&b:world)if(b.id==t.id&&b.uid==t.uid)return true;return false;}
 void available(const Campaign&,int,int,std::vector<Soldier>&out){out=pool;}
 int raise(Campaign&c,const std::vector<Soldier>&chosen){raises++;if(!raiseOk)return 0;army=chosen;for(auto&s:chosen)pool.erase(std::remove_if(pool.begin(),pool.end(),[&](const Soldier&p){return p.id==s.id;}),pool.end());c.squad=0x1234;return (int)army.size();}
 bool members(const Campaign&c,std::vector<Soldier>&out){if(lostSquad||c.squad!=0x1234)return false;out=army;return true;}
 int readyFarms(){return ready;}
 virtual void move(const Campaign&,const std::vector<Soldier>&m,int x,int y,bool home){moves++;if(home)homeMoves++;goals.push_back({x,y});audit(x,y,soldierKeep((int)m.size()));for(auto&s:army)for(auto&o:m)if(o.id==s.id){s.x=x;s.y=y;}}
 bool stand(const Campaign&,int x,int y,int){return x>=0&&y>=0;}
 bool free(int x,int y){for(auto&b:world)if(distance(x,y,b.x,b.y)<=1)return false;return true;}
 bool reach(const Campaign&,int,int){return true;}
 std::map<uint16_t,int> extra;std::set<uint16_t> cut;int pathCalls=0;int pathX=-1,pathY=-1; /* r37: walking paths = hex + extra per target id; cut = no walk from anywhere */
 std::function<int(int,int,int,int)> walkHook;int targetLists=0; /* r38: walked steps between points (hex by default), OFFENCE_TARGETS count */
 int walkSteps(const Campaign&,int fx,int fy,int tx,int ty){return walkHook?walkHook(fx,fy,tx,ty):std::max(1,distance(fx,fy,tx,ty));}
 void listTargets(const Campaign&,const std::vector<Target>&,const Fire&,uint32_t){targetLists++;}
 void paths(const Campaign&,int x,int y,std::vector<Target>&out){pathCalls++;pathX=x;pathY=y;for(auto&t:out){if(!t.reachable)continue;if(cut.count(t.id)){t.path=-1;t.reachable=false;continue;}int e=extra.count(t.id)?extra[t.id]:0;t.path=std::max(1,distance(x,y,t.doorX,t.doorY)+e);}}
 bool destroy(const Campaign&,const Target&t){for(size_t i=0;i<world.size();i++)if(world[i].id==t.id){order.push_back(world[i].kind);world.erase(world.begin()+i);destroyed++;return true;}return false;}
 bool dissolve(Campaign&c){if(failDissolve)return false;dissolved++;for(auto&s:army)pool.push_back(s);army.clear();c.squad=0;return true;}
 bool held(const Campaign&c,uint16_t id){for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)return true;return false;}
 void gardeners(const Campaign&c,std::vector<Gardener>&out){out.clear();for(auto&f:gards)if(f.alive&&!held(c,f.g.id))out.push_back(f.g);}
 bool gardener(const Campaign&,uint16_t id,uint32_t uid,Gardener&out){for(auto&f:gards)if(f.alive&&f.g.id==id&&f.g.uid==uid){out=f.g;return true;}return false;}
 FakeGardener*find(uint16_t id){for(auto&f:gards)if(f.g.id==id)return &f;return nullptr;}
 static void greenNear(std::vector<std::pair<int,int>>&cells,int x,int y,int r){cells.erase(std::remove_if(cells.begin(),cells.end(),[&](const std::pair<int,int>&c){return distance(c.first,c.second,x,y)<=r;}),cells.end());}
 // Native-like gardeners: an order makes the destination the work centre and
 // the gardener walks (Busy); next to a Dark ring cell it reserves it and
 // greens it (one cell per step), then takes the next Dark ring cell beside
 // its centre or keeps greening other land there (Working, no ring cell). A
 // group move (march) leaves them greening around the staging cell.
 // r32: ringOrders = orders to a cell beside a Dark ring cell, landOrders = orders onto Dark land (greening work).
 int ringOrders=0,landOrders=0;
 void sendGardener(const Campaign&,const Gardener&g,int x,int y){gardenerOrders++;audit(x,y,GardenerKeep);auto*f=find(g.id);f->g.x=x;f->g.y=y;f->g.centreX=x;f->g.centreY=y;f->g.reservedX=f->g.reservedY=f->g.aimX=f->g.aimY=-1;f->g.work=Work::Busy;f->g.escorted=false;f->g.destX=f->g.destY=-1;
  for(auto&r:rings)for(auto&c:r.second)if(c.dark&&distance(c.x,c.y,x,y)<=1){ringOrders++;goto counted;}counted:
  bool land=false;for(auto&c:darkCells)if(distance(c.first,c.second,x,y)<=1)land=true;for(auto&c:ownCells)if(distance(c.first,c.second,x,y)<=1)land=true;if(land)landOrders++;
  greenNear(darkCells,x,y,1);greenNear(ownCells,x,y,1);if(land)f->g.work=Work::Working;}
 void moveGardeners(const Campaign&,const std::vector<Gardener>&gs,int x,int y){groupOrders++;audit(x,y,soldierKeep((int)gs.size()));for(auto&g:gs){auto*f=find(g.id);f->g.x=x;f->g.y=y;f->g.centreX=x;f->g.centreY=y;f->g.reservedX=f->g.reservedY=f->g.aimX=f->g.aimY=-1;f->g.work=Work::Working;}}
 // r33: WATCH (job 0xB0): the gardener walks there (teleported) and waits; its work centre stays, it greens nothing.
 int parkOrders=0;std::vector<std::pair<int,int>> parks;
 void park(const Campaign&,const Gardener&g,int x,int y){parkOrders++;parks.push_back({x,y});audit(x,y,GardenerKeep);auto*f=find(g.id);f->g.x=x;f->g.y=y;f->g.reservedX=f->g.reservedY=f->g.aimX=f->g.aimY=-1;f->g.work=Work::Idle;f->g.escorted=true;f->g.destX=x;f->g.destY=y;}
 // r33: the Dark presence the bridge reports (tests set it).
 Intrusion intr;void intrusion(const Campaign&,Intrusion&out){out=intr;}
 std::vector<std::pair<int,int>> occupiedCells;bool parkedInside=false;
 std::vector<RingCell>&ringOf(const Target&t){auto&r=rings[t.id];if(r.empty()&&t.id){int n=t.kind==Farm?8:16;for(int i=0;i<n;i++)r.push_back(RingCell{t.doorX-n/2+i,t.doorY+1,true,false});}return r;}
 void ring(const Target&t,std::vector<RingCell>&out){
  auto&r=ringOf(t);
  for(auto&c:r){c.occupied=false;for(auto&o:occupiedCells)if(o.first==c.x&&o.second==c.y)c.occupied=true;}
  for(auto&f:gards){if(!f.alive)continue;Gardener&g=f.g;
   if(g.reservedX>=0){for(auto&c:r)if(c.x==g.reservedX&&c.y==g.reservedY&&c.dark&&!c.occupied){c.dark=false;greened++;}g.reservedX=g.reservedY=-1;}
   if(g.work==Work::Busy||g.work==Work::Working){for(auto&c:r)if(c.dark&&!c.occupied&&distance(c.x,c.y,g.centreX,g.centreY)<=1){g.reservedX=c.x;g.reservedY=c.y;g.work=Work::Working;break;}
    if(g.reservedX<0&&g.work==Work::Busy)g.work=Work::Working;}
  }
  bool green=true;for(auto&c:r)if(c.dark)green=false;
  if(green&&!blockTrigger)for(size_t i=0;i<world.size();i++)if(world[i].id==t.id){order.push_back(world[i].kind);world.erase(world.begin()+i);collapsed++;break;}
  out=r;
 }
 int greened=0;
 bool beside(const Target&,const RingCell&c,int&x,int&y){x=c.x;y=c.y-1;return true;}
 void clearFootprint(const Campaign&,const Target&,const std::vector<Soldier>&,const Fire&){clears++;}
 bool spare(const Target&t,int x,int y){if(!free(x,y)||distance(x,y,t.x,t.y)<3)return false;for(auto&c:ringOf(t))if(distance(c.x,c.y,x,y)<EscortGap)return false;return true;} /* as the bridge: EscortGap off the footprint */
 bool parked(const Target&){return parkedInside;}
 void sites(const Campaign&,std::vector<Site>&out){for(auto&s:growing)out.push_back(s);}
 bool nearest(std::vector<std::pair<int,int>>&cells,int x,int y,int r,const std::vector<Keep>&keep,int&dx,int&dy){
  for(auto&f:gards)if(f.alive&&f.g.work==Work::Working)greenNear(cells,f.g.x,f.g.y,2);
  bool found=false;int best=0;for(auto&d:cells){if(kept(keep,d.first,d.second))continue;int k=distance(x,y,d.first,d.second);if(k<=r&&(!found||k<best)){found=true;best=k;dx=d.first;dy=d.second;}}return found;}
 bool darkNear(const Campaign&,int x,int y,int r,const std::vector<Keep>&keep,int&dx,int&dy){return nearest(darkCells,x,y,r,keep,dx,dy);}
 bool ownDark(const Campaign&,int x,int y,int r,const std::vector<Keep>&keep,int&dx,int&dy){return nearest(ownCells,x,y,r,keep,dx,dy);}
 void darkFighters(const Campaign&,std::vector<std::pair<int,int>>&out){out=darkUnits;}
 bool leader=true;bool leads(const Campaign&,const Target&){return leader;}
 bool call(const Campaign&c,uint32_t t,Target&out){if(t<callUntil&&caller!=c.player){out=callTarget;return true;}return false;}
 bool announce(const Campaign&c,uint32_t t,const Target&target){if(!allies)return false;callTarget=target;callUntil=t+CallTicks;caller=c.player;calls++;return true;}
 std::vector<Pulled> pulls;int pullExitX=-1,pullExitY=-1; /* r36: the fire guard's diagnostics hook */
 void pulled(const Campaign&,const Fire&,const std::vector<Soldier>&,const std::vector<Pulled>&who,int ex,int ey){for(auto&q:who)pulls.push_back(q);pullExitX=ex;pullExitY=ey;}
 void report(Event e,const Campaign&c){if(e==Event::Decline){declines++;lastDecline=c.reason;}if(e==Event::Launch)launches++;if(e==Event::Adopted)adopted++;if(e==Event::Lost)lost++;
  if(e==Event::Ignored)ignored++;if(e==Event::Avoid){avoids++;lastAvoid=c.reason;}if(e==Event::Fire){fires++;lastFire=c.reason;}if(e==Event::Siege)sieges++;if(e==Event::Greening)greenings++;if(e==Event::Reclaim){reclaims++;lastReclaim=c.reason;}if(e==Event::Return)lastReturn=c.reason;
  if(e==Event::Front)fronts++;if(e==Event::Hunt)hunts++;if(e==Event::Join)joins++;if(e==Event::Call)calls+=0;if(e==Event::Convoy){convoys++;lastConvoy=c.reason;}
  if(e==Event::Defend){defends++;if(!*firstDefend)firstDefend=c.reason;lastDefend=c.reason;}}
 int convoys=0,defends=0;const char*lastConvoy="";const char*firstDefend="";const char*lastDefend="";
};
static Fire F(const std::vector<Target>&all,int ready=-1,const std::vector<Site>&sites={}){return fireFrom(all,ready,sites);}
template<class E> static uint32_t run(Campaign&c,E&e,uint32_t from,uint32_t ticks){uint32_t t=from;for(;t<from+ticks;t+=Period)step(c,t,e);return t;}
static Campaign fresh(int only=-1){Campaign c;c.player=2;c.sector=0;c.only=(int8_t)only;return c;}
constexpr uint32_t T0=3000; // after firstAttack(1)
// Force a decision now (chance certain), optionally of one style.
template<class E> static void launch(Campaign&c,E&e,uint32_t t,int only=-1){c.only=(int8_t)only;step(c,t,e);c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,t+Period,e);}
// r32: steps until the campaign reaches a phase (r35: the teleport engine puts the army at each leg end in one step).
template<class E> static uint32_t until(Campaign&c,E&e,uint32_t t,Phase p,uint32_t limit=4000){uint32_t end=t+limit;while(c.phase!=p&&t<end){step(c,t,e);t+=Period;}return t;}
constexpr int A=(int)Style::Assault,R=(int)Style::Raid,P=(int)Style::Pincer,S=(int)Style::Sweep,H=(int)Style::Hunt;

static void basics(){
 assert(distance(0,0,3,3)==3&&distance(0,0,3,-3)==6&&distance(5,5,5,9)==4&&distance(0,0,-2,-5)==5);
 assert(fireRadius(0)==0&&fireRadius(1)==18&&fireRadius(3)==22&&fireRadius(4)==24&&fireRadius(11)==24);
 assert(soldierWeight(29)==2&&soldierWeight(31)==4&&soldierWeight(32)==2&&soldierWeight(34)==4);
 assert(soldierWeight(38)==3&&soldierWeight(40)==5&&soldierWeight(43)==5&&soldierWeight(61)==3&&soldierWeight(63)==5&&soldierWeight(44)==10);
 assert(!soldierType(45)&&!soldierType(GardenerType)&&!soldierType(52)&&!soldierType(53)&&!soldierType(54)&&!soldierType(28));
 assert(defenderType(29)&&defenderType(32)&&defenderType(54)&&!defenderType(53)&&!defenderType(52));
 Campaign c;avoid(c,77,100);assert(avoided(c,77,500)&&!avoided(c,77,100+AvoidTicks)&&!avoided(c,78,500));
 // Pressure: a third of AttackDelay, 3..8 minutes.
 assert(firstAttack(0)==FirstMin&&firstAttack(1)==FirstMin&&firstAttack(6)==2*DelayUnit&&firstAttack(9)==3*DelayUnit&&firstAttack(15)==FirstMax&&firstAttack(40)==FirstMax);
 // Native recruitment: one at a time, with the reason when not.
 const char*why="x";assert(recruitOrder(0,0,0,5,true,&why)==1&&!*why);
 assert(recruitOrder(GardenersWanted-1,0,0,5,true)==1&&recruitOrder(GardenersWanted,0,0,5,true,&why)==0&&!strcmp(why,"enough gardeners"));
 assert(recruitOrder(4,0,1,5,true,&why)==0&&!strcmp(why,"order on its way")&&recruitOrder(0,1,0,5,true)==0);
 assert(recruitOrder(0,0,0,ShovelReserve,true,&why)==0&&!strcmp(why,"no spare shovels")&&recruitOrder(0,0,0,5,false,&why)==0&&!strcmp(why,"no spare carriers"));
 // Flank geometry: a turn keeps the distance (60 degrees maps the hex grid onto itself).
 int ox=0,oy=0;turn(100,100,60,100,3.14159265,ox,oy);assert(ox==140&&oy==100);
 turn(100,100,60,100,PincerAngle,ox,oy);assert(abs(distance(100,100,ox,oy)-40)<=2);turn(100,100,60,100,-PincerAngle,ox,oy);assert(abs(distance(100,100,ox,oy)-40)<=2);
 // The largest group of Dark fighters, outside fire.
 std::vector<std::pair<int,int>> f={{10,10},{11,10},{12,11},{80,80}};Fire none;int x=0,y=0,n=0;
 assert(cluster(f,none,0,0,x,y,n)&&n==3&&distance(x,y,11,10)<=1);
 Fire z;z.on=true;addCentre(z,10,8);assert(z.z[0].x==11&&z.z[0].y==10&&cluster(f,z,0,0,x,y,n)&&x==80&&n==1);
 // Style choice: only positive weights; the style used last counts half.
 Campaign s;Option o[3];o[0].style=Style::Raid;o[0].weight=0;o[1].style=Style::Assault;o[1].weight=4;o[2].style=Style::Sweep;o[2].weight=0;
 for(int i=0;i<50;i++)assert(chooseStyle(s,o,3)==1);o[1].weight=0;assert(chooseStyle(s,o,3)<0);
 o[0].weight=4;o[1].weight=4;s.lastStyle=Style::Raid;int raids=0;for(int i=0;i<3000;i++)if(chooseStyle(s,o,3)==0)raids++;assert(raids>800&&raids<1200);
 s.only=(int)Style::Sweep;assert(chooseStyle(s,o,3)<0);s.only=(int)Style::Assault;assert(chooseStyle(s,o,3)==1);
}
static void targetOrder(){
 Pick total;total.citadel=true;
 Engine e;e.add(1,Temple,100,100);e.add(2,Farm,110,104);e.add(3,Farm,160,40);e.add(4,Hall,40,100);
 assert(e.world[chooseTarget(e.world,F(e.world),40,40,total)].id==3); // farm 2 is locked by the Temple's fire
 // r30: an unlocked far farm wins over a nearer locked one; with only locked farms an unlocked Hall is taken.
 assert(e.world[chooseTarget(e.world,F(e.world),105,104,total)].id==3);
 e.world.erase(e.world.begin()+2);assert(e.world[chooseTarget(e.world,F(e.world),40,40,total)].id==4&&e.world[chooseTarget(e.world,F(e.world),109,104,total)].id==4);
 {std::vector<Target> w={e.world[0],e.world[1]};assert(chooseTarget(w,F(w),40,40,total)<0);} /* only locked farms, no Hall: nothing */
 // No farm left: the citadel before the Hall, wherever the army stands.
 e.world.erase(e.world.begin()+1);assert(e.world[chooseTarget(e.world,F(e.world),40,40,total)].id==1&&e.world[chooseTarget(e.world,F(e.world),99,99,total)].id==1);
 // r30: never a citadel while the fire is on: the native counter alone, or a Farm site alone.
 assert(e.world[chooseTarget(e.world,F(e.world,2),40,40,total)].id==4&&e.world[chooseTarget(e.world,F(e.world,-1,{Site{300,300,Farm}}),40,40,total)].id==4);
 Pick hallOnly;hallOnly.citadel=false;assert(e.world[chooseTarget(e.world,F(e.world),99,99,hallOnly)].id==4);
 e.world.pop_back();assert(chooseTarget(e.world,F(e.world),99,99,hallOnly)<0&&chooseTarget(e.world,F(e.world),99,99,total)==0);
 assert(chooseTarget(e.world,F(e.world,-1,{Site{300,300,Farm}}),99,99,total)<0);
 // Temple and Fortress both fall: the nearer first, then the other.
 Engine b;b.add(1,Temple,100,40);b.add(2,Fortress,200,40);assert(b.world[chooseTarget(b.world,F(b.world),40,40,total)].id==1&&b.world[chooseTarget(b.world,F(b.world),190,40,total)].id==2);
 // An unreachable farm never deadlocks farms first, but its fire defence keeps the citadel off limits; an unlocked Hall is fine.
 Engine u;u.add(1,Temple,100,100);u.add(2,Farm,90,90,false);assert(chooseTarget(u.world,F(u.world),40,40,total)<0);u.add(3,Hall,70,70);assert(chooseTarget(u.world,F(u.world),40,40,total)<0);
 u.add(4,Hall,40,100);assert(u.world[chooseTarget(u.world,F(u.world),40,40,total)].id==4);
 Engine n;n.add(1,Temple,100,100,false);assert(chooseTarget(n.world,F(n.world),40,40,total)<0);
 Engine d;d.add(1,Farm,60,40);d.add(2,Farm,120,40);d.world[0].defence=30;d.world[1].defence=4;
 Pick lim;lim.limit=10;assert(d.world[chooseTarget(d.world,F(d.world),40,40,lim)].id==2);
 lim.limit=2;assert(chooseTarget(d.world,F(d.world),40,40,lim)<0);lim.weakest=true;assert(d.world[chooseTarget(d.world,F(d.world),40,40,lim)].id==2);
 Campaign c;avoid(c,d.world[1].uid,100);lim.limit=-1;assert(d.world[chooseTarget(d.world,F(d.world),40,40,lim,&c,200)].id==1);
 // An avoided farm still forbids the citadel (farms first).
 Engine g;g.add(1,Farm,100,40);g.add(2,Temple,160,40);Campaign k=fresh();avoid(k,g.world[0].uid,0);Pick pk;assert(chooseTarget(g.world,F(g.world),40,40,pk,&k,10)<0);
 // r37: the walking path decides, not the hex distance plus the home detour (field: a farm 318 hex away was taken over one
 // 40 hex away): a near farm behind a long walk loses to a farther one with a straight walk; a farm with no walk is skipped.
 {Engine w;w.add(1,Farm,100,40);w.add(2,Farm,140,40);std::vector<Target> all;w.targets(k,all);Pick any;
  assert(w.world[chooseTarget(all,F(all),40,40,any)].id==1);
  w.extra[1]=200;w.paths(k,40,40,all);assert(all[0].path==259&&all[1].path==99&&all[chooseTarget(all,F(all),40,40,any)].id==2);
  w.cut.insert(2);w.targets(k,all);w.paths(k,40,40,all);assert(all[chooseTarget(all,F(all),40,40,any)].id==1);w.cut.insert(1);w.targets(k,all);w.paths(k,40,40,all);assert(chooseTarget(all,F(all),40,40,any)<0);
  /* the home-based detour stays the fallback while no path is known */
  w.cut.clear();w.extra.clear();w.targets(k,all);all[0].detour=300;assert(all[chooseTarget(all,F(all),40,40,any)].id==2);}
 // r37: a farm with neighbours is preferred (ClusterBonus per usable farm within ClusterRadius, at most ClusterMax): the
 // army chains through a group instead of walking to a lone farm that is a few tiles nearer.
 {Engine w;w.add(1,Farm,100,40);w.add(2,Farm,100,100);w.add(3,Farm,110,130);w.add(4,Farm,125,90);w.add(5,Farm,85,120);std::vector<Target> all;w.targets(k,all);Pick any;w.paths(k,40,40,all);
  assert(distance(40,40,all[0].doorX,all[0].doorY)<distance(40,40,all[1].doorX,all[1].doorY)&&all[chooseTarget(all,F(all),40,40,any)].id==2); /* three neighbours: 24 closer */
  w.world.resize(1);w.add(6,Farm,100,140);w.targets(k,all);w.paths(k,40,40,all);assert(all[chooseTarget(all,F(all),40,40,any)].id==1);} /* no neighbours: the nearest */
}
static void geometry(){
 auto any=[](int,int,int){return true;};
 Engine e;e.add(1,Farm,100,40);Target t=e.world[0];Fire none;int sx=0,sy=0;
 assert(stagePoint(t,none,40,40,StageDistance,10,any,sx,sy)&&distance(t.doorX,t.doorY,sx,sy)>=StageDistance&&sx<t.doorX);
 assert(stagePoint(t,none,40,40,StageDistance,10,[](int x,int,int pass){return pass==1||x<70;},sx,sy)&&sx<70);
 assert(stagePoint(t,none,t.doorX-5,t.doorY,StageDistance,10,any,sx,sy)&&sx==t.doorX-5);
 // r30: a Temple between home and the farm: the stage keeps soldierKeep(N) and its leg to the door is clear (off the line if need be).
 e.add(2,Temple,60,38);Fire f=F(e.world);assert(f.on&&!locked(e.world[0],f));
 for(int n:{5,10,34,100}){assert(stagePoint(t,f,40,40,StageDistance,n,any,sx,sy));assert(!closer(sx,sy,f,soldierKeep(n))&&legClear(sx,sy,t.doorX,t.doorY,f,legKeep(n)));}
 // Assault cells: the free cell nearest the army, at least soldierKeep(N) from the fire.
 int ax=0,ay=0;assert(assaultPoint(t,none,40,40,10,[&](int x,int y){return e.free(x,y);},ax,ay)&&distance(ax,ay,t.doorX,t.doorY)<=10&&e.free(ax,ay));
 Engine k;k.add(1,Farm,100,40);k.add(2,Temple,65,40);Fire fk=F(k.world);Target tk=k.world[0];assert(!locked(tk,fk)&&closer(tk.doorX,tk.doorY,fk,soldierKeep(10)));
 for(int n:{5,10,34,100}){assert(assaultPoint(tk,fk,40,40,n,[&](int x,int y){return k.free(x,y);},ax,ay));assert(!closer(ax,ay,fk,soldierKeep(n))&&distance(ax,ay,tk.doorX,tk.doorY)<=10);}
 // Legs and routes around a fire centre at (100,100).
 std::vector<Target> burn;{Target b;b.id=9;b.kind=Temple;b.x=99;b.y=98;burn.push_back(b);b.id=10;b.kind=Farm;b.x=400;b.y=400;burn.push_back(b);}Fire fb=F(burn);assert(fb.z.size()==1&&fb.z[0].x==100&&fb.z[0].y==100);
 assert(!legClear(40,100,160,100,fb,legKeep(10))&&legClear(40,40,160,40,fb,legKeep(10))&&legClear(40,100,160,100,none,legKeep(10)));
 for(int n:{5,34,100}){int wx[MaxVias]={},wy[MaxVias]={},kk=0;assert(route(40,100,160,100,fb,n,any,wx,wy,kk)&&kk>=1&&kk<MaxVias);
  int px=40,py=100;for(int i=0;i<kk;i++){assert(!closer(wx[i],wy[i],fb,soldierKeep(n))&&legClear(px,py,wx[i],wy[i],fb,legKeep(n)));px=wx[i];py=wy[i];}assert(legClear(px,py,160,100,fb,legKeep(n)));}
 {int wx[MaxVias]={},wy[MaxVias]={},kk=0;assert(!route(40,100,100,110,fb,10,any,wx,wy,kk));} /* an end inside the fire: no route */
 {int wx[MaxVias]={},wy[MaxVias]={},kk=0;assert(route(40,40,160,40,fb,10,any,wx,wy,kk)&&kk==0);} /* clear: no waypoint */
 // Two overlapping centres (Temple and Fortress 30 apart): a route around both, or a clean failure.
 {std::vector<Target> two=burn;Target b;b.id=11;b.kind=Fortress;b.x=129;b.y=98;two.push_back(b);Fire f2=F(two);assert(f2.z.size()==2);int wx[MaxVias]={},wy[MaxVias]={},kk=0;
  if(route(40,100,200,100,f2,20,any,wx,wy,kk)){int px=40,py=100;for(int i=0;i<kk;i++){assert(!closer(wx[i],wy[i],f2,soldierKeep(20))&&legClear(px,py,wx[i],wy[i],f2,legKeep(20)));px=wx[i];py=wy[i];}assert(legClear(px,py,200,100,f2,legKeep(20)));}else assert(kk==0);}
 // The way out: on the ray from the centre, at least soldierKeep(N)+2 out.
 {int ox=0,oy=0;assert(exitPoint(110,104,fb,10,any,ox,oy)&&!closer(ox,oy,fb,soldierKeep(10)+2)&&ox>100);assert(exitPoint(100,100,fb,1,any,ox,oy)&&!closer(ox,oy,fb,soldierKeep(1)+2));}
 // A leg from inside the fire may only lead outward.
 assert(legClear(110,100,140,100,fb,GardenerKeep)&&!legClear(110,100,80,100,fb,GardenerKeep));
}
static void gating(){
 // The first attack comes after a third of AttackDelay (3..8 minutes), then within a few checks.
 {Engine e;e.cfg.delay=30;e.add(1,Farm,140,40);Campaign c=fresh();run(c,e,0,firstAttack(30)-Period);assert(e.launches==0&&e.raises==0&&e.declines==0);run(c,e,firstAttack(30),2500);assert(e.launches==1);}
 {Engine e;e.cfg.mode=0;e.add(1,Farm,140,40);Campaign c=fresh();run(c,e,0,60000);assert(e.launches==0&&e.declines==0);}
 // Failed checks raise the chance up to certainty.
 {Engine e;e.pool.resize(6);e.add(1,Farm,140,40);Campaign c=fresh();run(c,e,0,40000);assert(e.launches==0&&e.declines>20&&c.chance==ChanceFull&&!strcmp(e.lastDecline,"no spare soldiers"));}
 {Engine e;e.str.fighters=0;e.str.strength=0;e.add(1,Farm,140,40);Campaign c=fresh();step(c,0,e);c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,T0,e);assert(e.declines==0&&c.nextCheck==T0+UnknownRetry);}
 // r37: an army fit for an assault never waits for chance; a smaller one still does.
 {Engine e;e.add(1,Farm,140,40);Campaign c=fresh();step(c,T0,e);c.chance=0;c.nextCheck=0;c.cooldownUntil=0;step(c,T0+Period,e);assert(e.launches==1&&e.declines==0);}
 {Engine e;e.pool.resize(10);e.add(1,Farm,140,40);Campaign c=fresh();step(c,T0,e);c.chance=0;c.nextCheck=0;c.cooldownUntil=0;step(c,T0+Period,e);assert(e.launches==0&&!strcmp(e.lastDecline,"chance"));}
 assert(firstAttack(20)==FirstMax&&firstAttack(1)==FirstMin&&FirstMax==4*DelayUnit&&CooldownMin+CooldownSpread<=450&&CheckEvery==128);
 // No fighter count rule any more: 8 free soldiers are enough for a raid (2 stay home).
 {Engine e;e.str.fighters=3;e.pool.resize(8);e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0);assert(e.launches==1&&e.army.size()==6&&e.pool.size()==2);}
 {Engine e;Campaign c=fresh();launch(c,e,T0);assert(!strcmp(e.lastDecline,"no reachable Dark building")&&e.targetLists==1);run(c,e,T0+2*Period,TargetsNote-Period);assert(e.targetLists==1);run(c,e,T0+TargetsNote+Period,600);assert(e.targetLists==2);} /* r38: OFFENCE_TARGETS, every TargetsNote */
 // r37: gardeners stranded beyond LaunchReach of home are called home (the nearest CallHome first), then the sector attacks.
 {Engine e(6);e.add(1,Farm,140,40);for(auto&f:e.gards){f.g.x=f.g.centreX=300;f.g.y=f.g.centreY=300;f.g.work=Work::Working;}e.gards[0].g.x=e.gards[0].g.centreX=200;
  Campaign c=fresh();launch(c,e,T0);assert(e.launches==0&&!strcmp(e.lastDecline,"no gardeners")&&!strcmp(e.lastReclaim,"gardeners called home")&&e.gardenerOrders==CallHome);
  auto waiting=[&](){int n=0;for(auto&f:e.gards)if(f.g.centreX==300)n++;return n;};
  assert(e.gards[0].g.centreX==40&&e.gards[0].g.centreY==40&&waiting()==2); /* the nearest went, two still wait */
  run(c,e,T0+2*Period,CallHomeEvery-Period);assert(waiting()==2); /* not again within CallHomeEvery */
  for(auto&f:e.gards){f.g.x=f.g.centreX=42;f.g.y=f.g.centreY=40;f.g.work=Work::Idle;}c.chance=ChanceFull;c.nextCheck=0;run(c,e,T0+CallHomeEvery+Period,600);assert(e.launches==1);}
 // No gardeners: soldiers alone cannot destroy a Dark building (a hunt still could).
 {Engine e(1);e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0);assert(e.launches==0&&!strcmp(e.lastDecline,"no gardeners"));}
 {Engine e(1);e.add(1,Farm,140,40);for(int i=0;i<4;i++)e.darkUnits.push_back({90+i,70});Campaign c=fresh();launch(c,e,T0);assert(e.launches==1&&c.style==Style::Hunt);}
}
static void styles(){
 // Raid: RaidMax soldiers and 4 gardeners against the weakest building.
 {Engine e;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,R);assert(e.launches==1&&c.style==Style::Raid&&c.raid&&(int)e.army.size()==raidSize(34)&&raidSize(34)==11&&c.gardeners==4);}
 // Assault: everything but the home reserve (40/6), 8 gardeners (6 exist).
 {Engine e;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,A);assert(e.launches==1&&c.style==Style::Assault&&!c.raid&&e.army.size()==34&&e.pool.size()==6&&c.gardeners==6);}
 {Engine e(12);e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.gardeners==10&&c.want==10);} /* r32: 8 + 34/15 */
 // AttackMode 1: raids, sweeps and hunts only.
 {Engine e;e.cfg.mode=1;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,A);assert(e.launches==0&&!strcmp(e.lastDecline,"too weak for every attack"));c.only=R;c.chance=ChanceFull;c.nextCheck=0;step(c,T0+400,e);assert(e.launches==1);}
 // Pincer: two halves to two staging cells on either side of the line from home.
 {Engine e;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,P);
  assert(c.style==Style::Pincer&&c.split&&c.phase==Phase::March&&distance(c.stageX,c.stageY,c.stage2X,c.stage2Y)>=PincerGap);
  std::vector<Soldier> a,b;halves(e.army,a,b);assert(b[0].x==c.stage2X&&b[0].y==c.stage2Y&&distance(a[0].x,a[0].y,c.stageX,c.stageY)<=SnapRing); /* r35: each half straight to its staging cell */
  assert(distance(c.stageX,c.stageY,139,43)>=StageDistance-1&&distance(c.stage2X,c.stage2Y,139,43)>=StageDistance-1);
  uint32_t t=until(c,e,T0+Period,Phase::Assault);assert(c.phase==Phase::Assault&&c.split);halves(e.army,a,b);assert(a[0].x!=b[0].x||a[0].y!=b[0].y);
  run(c,e,t,3000);assert(e.collapsed==1&&!c.split);}
 // Pincer without room for two flanks: one army (logged as an assault).
 {struct Narrow:Engine{bool stand(const Campaign&,int x,int y,int){return y>=38&&y<=46&&x>=0;}} n;n.add(1,Farm,140,40);Campaign d=fresh();launch(d,n,T0,P);assert(n.launches==1&&!d.split&&d.style==Style::Assault);}
 // A pincer close to home never orders an unchecked flank cell: no walkable flank, one army.
 {struct Refuse:Engine{bool stand(const Campaign&,int,int,int){return false;}} n;n.add(1,Farm,50,40);Campaign d=fresh();launch(d,n,T0,P);assert(n.launches==1&&!d.split&&d.style==Style::Assault&&d.phase==Phase::Assault);}
 // Sweep: an escort and many gardeners push into the nearest Dark land.
 {Engine e;e.add(1,Temple,160,40);for(int x=70;x<=150;x+=2)e.darkCells.push_back({x,40});Campaign c=fresh();launch(c,e,T0,S);
  assert(e.launches==1&&c.style==Style::Sweep&&(int)e.army.size()==SweepEscort&&c.gardeners==6&&c.target.id==0&&distance(c.frontX,c.frontY,40,40)<=60);}
 // Hunt: the army goes for the largest group of Dark fighters.
 {Engine e;e.add(1,Farm,60,80);for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,60});e.darkUnits.push_back({20,90});Campaign c=fresh();launch(c,e,T0,H);
  assert(e.launches==1&&c.style==Style::Hunt&&c.hunting&&(int)e.army.size()==huntSize(34)&&huntSize(34)==17&&c.gardeners==0&&distance(c.target.doorX,c.target.doorY,101,60)<=2);}
 // Too few fighters to hunt, or too strong a group: no hunt.
 {Engine e;e.darkUnits={{100,60},{101,60}};Campaign c=fresh();launch(c,e,T0,H);assert(e.launches==0);}
 {Engine e;e.pool.resize(10);for(int i=0;i<8;i++)e.darkUnits.push_back({100,60+i%3});Campaign c=fresh();launch(c,e,T0,H);assert(e.launches==0);}
 // r37: with a building in reach a hunt or a sweep is rare (weight 1 against the raid's 2..5 and the assault's 4+).
 {int seen[6]={};for(int k=0;k<200;k++){Engine e(12);e.add(1,Farm,140,40);e.add(2,Farm,150,70);for(int x=70;x<=90;x+=2)e.darkCells.push_back({x,50});for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,90});
   Campaign c=fresh();c.random.state=0x2468ace0u+k*7919u;step(c,T0,e);c.random.state^=k*2654435761u;c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,T0+Period,e);if(e.launches)seen[(int)c.style]++;}
  assert(seen[H]+seen[S]<(seen[R]+seen[A]+seen[P])/3);}
 // Without a forced style every feasible style shows up over many decisions.
 {int seen[6]={};for(int k=0;k<120;k++){Engine e(12);e.add(1,Farm,140,40);e.add(2,Farm,150,70);for(int x=70;x<=90;x+=2)e.darkCells.push_back({x,50});for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,90});
   Campaign c=fresh();c.random.state=0x1234567u+k*7919u;step(c,T0,e);c.random.state^=k*2654435761u;c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,T0+Period,e);if(e.launches)seen[(int)c.style]++;}
  for(int s:{R,A,P,S,H})assert(seen[s]>0);}
}
static void allied(){
 // An assault calls the other computers; a call is joined without the chance check, once.
 {Engine e;e.allies=true;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,A);assert(e.calls==1&&e.callUntil==T0+Period+CallTicks&&e.caller==2);}
 {Engine e;e.allies=true;e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0,R);assert(e.calls==0);}
 {Engine e;e.add(1,Farm,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);
  assert(e.joins==1&&e.launches==0&&c.style==Style::Allied&&!c.escort&&c.gardeners==6&&e.army.size()==34&&c.joinedUid==e.world[0].uid);
  run(c,e,T0+2*Period,3000);assert(e.collapsed==1);}
 // Without free gardeners the ally joins as an escort and goes home when the target fell.
 {Engine e(0);e.add(1,Farm,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);
  assert(e.joins==1&&c.escort&&c.phase!=Phase::Idle);uint32_t t=run(c,e,T0+2*Period,600);assert(c.phase==Phase::Assault);
  e.world.clear();t=run(c,e,t,Period*2);assert(!strcmp(e.lastReturn,"allied target fell"));run(c,e,t,HomeTimeout+3000);assert(e.joins==1);}
 // A call from the same player is never joined.
 {Engine e;e.add(1,Farm,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=2;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);assert(e.joins==0);}
}
static void campaign(){
 // An assault razes every farm first, then the Temple and the Fortress, then the Hall.
 Engine e(8);e.add(1,Temple,200,40);e.add(2,Farm,150,40);e.add(3,Farm,120,90);e.add(4,Hall,230,60);e.add(5,Fortress,215,80);
 Campaign c=fresh();launch(c,e,T0,A);assert(e.launches==1&&!c.raid);
 run(c,e,T0+Period,30000);
 assert(e.collapsed==5&&e.destroyed==0&&e.order[0]==Farm&&e.order[1]==Farm&&(e.order[2]==Temple||e.order[2]==Fortress)&&(e.order[3]==Temple||e.order[3]==Fortress)&&e.order[4]==Hall);
 assert(e.gardenerOrders>0&&e.groupOrders==0&&e.clears>0&&e.sieges>=5&&e.greenings>0&&e.homeMoves>0&&e.dissolved==1&&c.phase==Phase::Idle&&c.squad==0&&c.gardeners==0&&e.ignored==0);
 Campaign r=fresh();r.chance=ChanceFull;r.phase=Phase::Return;r.gardeners=3;toIdle(r,5000);
 assert(r.chance==ChanceStart&&r.nextCheck>=5000+CheckEvery&&r.nextCheck<5000+CheckEvery+128&&r.cooldownUntil>=5000+CooldownMin&&r.cooldownUntil<5000+CooldownMin+CooldownSpread&&r.squad==0&&r.gardeners==0);
 // A raid razes at most RaidChain buildings, then goes home (r37: 5).
 Engine g;for(int i=0;i<RaidChain+1;i++)g.add((uint16_t)(1+i),Farm,100+30*i,40);Campaign d=fresh();launch(d,g,T0,R);
 {uint32_t t=T0+Period;while(!g.dissolved&&t<T0+25000){step(d,t,g);t+=Period;}}assert(g.collapsed==RaidChain&&g.world.size()==1&&g.dissolved==1&&!strcmp(g.lastReturn,"raid done"));
 // Without farms a raid takes the citadel too (its fire is out).
 Engine h;h.add(1,Temple,120,40);Campaign k=fresh();launch(k,h,T0,R);assert(h.launches==1&&k.target.kind==Temple);run(k,h,T0+Period,5000);assert(h.collapsed==1);
}
static void reattack(){
 // A finished campaign rests, then the next army forms: raids come again and again.
 Engine e;for(int i=0;i<6;i++)e.add((uint16_t)(1+i),Farm,100+25*i,40+10*(i%2));Campaign c=fresh(R);
 run(c,e,0,80000);assert(e.launches>=2&&e.collapsed==6&&e.dissolved>=2);
 // An army that was destroyed is replaced as soon as soldiers are free again.
 Engine g;g.add(1,Farm,200,40);Campaign d=fresh(A);launch(d,g,T0,A);assert(g.launches==1);
 g.army.clear();uint32_t t=run(d,g,T0+Period,Period*3);assert(d.phase==Phase::Idle&&g.dissolved==1&&!strcmp(g.lastReturn,"army lost"));
 t=run(d,g,t,CooldownMin+CooldownSpread+2000);assert(g.launches==1&&!strcmp(g.lastDecline,"no spare soldiers"));
 for(int i=0;i<20;i++)g.pool.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),40,40});
 run(d,g,t,3000);assert(g.launches==2);
}
static void fallback(){
 // An all-green ring whose native trigger is blocked: the credited native destroy finishes the building.
 Engine e;e.add(1,Farm,100,40);e.blockTrigger=true;Campaign c=fresh();launch(c,e,T0,A);
 run(c,e,T0+Period,600);assert(e.destroyed==1&&e.collapsed==0&&e.order[0]==Farm);
}
static void defence(){
 // Dark fighters at the farm: no gardener is sent until they are gone.
 Engine e;e.add(1,Farm,100,40);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);
 assert(e.launches==1);run(c,e,T0+Period,3000);assert(e.ringOrders==0&&e.greened==0&&e.collapsed==0&&c.phase==Phase::Assault); /* r32: they wait behind the screen */
 e.darkUnits.clear();run(c,e,T0+3100,1000);assert(e.gardenerOrders>0&&e.collapsed==1);
 // Too many defenders arrive while a raid stages: avoid that target, take another.
 Engine g;g.add(1,Farm,140,40);g.add(2,Farm,140,120);Campaign d=fresh();launch(d,g,T0,R);
 assert(d.phase==Phase::March&&d.target.id==1);for(int i=0;i<60;i++)g.darkUnits.push_back({139,43});for(uint32_t t=T0+Period;!g.avoids&&t<T0+2000;t+=Period)step(d,t,g);assert(g.avoids==1&&d.target.id==2); /* r32: at the staging cell (the convoy needs a few steps) */
}
static void gardenerLosses(){
 // All escorted gardeners die: free ones are taken; with none left the army goes home.
 Engine e(8);e.add(1,Temple,100,40);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);assert(c.gardeners==8);
 uint32_t t=until(c,e,T0+Period,Phase::Assault);
 for(auto&f:e.gards)if(f.g.id<306)f.alive=false;t=run(c,e,t,Period*3);assert(c.gardeners==2&&c.phase==Phase::Assault);
 for(auto&f:e.gards)f.alive=false;run(c,e,t,Period*3);assert((c.phase==Phase::Return||c.phase==Phase::Idle)&&!strcmp(e.lastReturn,"no gardeners left"));
 // r32: all gardeners die on the march: the army goes home at once.
 {Engine g(8);g.add(1,Temple,200,40);Campaign d=fresh();launch(d,g,T0,A);assert(d.phase==Phase::March);for(auto&f:g.gards)f.alive=false;run(d,g,T0+Period,Period*2);assert(d.phase!=Phase::March&&!strcmp(g.lastReturn,"no gardeners left"));}
}
static void farmReturns(){
 // A farm completed during a citadel siege turns the fire back on: the army leaves the citadel at once, then the farm comes first.
 Engine e;e.add(1,Temple,100,40);Campaign c=fresh();launch(c,e,T0,A);
 uint32_t t=run(c,e,T0+Period,30);assert(c.target.kind==Temple&&e.collapsed==0);
 e.add(9,Farm,90,80);t=run(c,e,t,Period*2);assert(!strcmp(e.lastReturn,"fire defence back on")&&c.phase!=Phase::Assault);
 run(c,e,t,9000);assert(e.order.size()==2&&e.order[0]==Farm&&e.order[1]==Temple);
 // r30 (design 8): a Farm site appears during a Temple siege: the army returns, the exit point comes first, no order lands inside.
 Engine g;g.add(1,Temple,100,40);g.darkUnits.push_back({99,43});Campaign d=fresh();launch(d,g,T0,A); /* a defender at the door: no gardener yet, the Temple stands */t=until(d,g,T0+Period,Phase::Assault);t=run(d,g,t,40);assert(d.target.kind==Temple&&d.phase==Phase::Assault);
 Army inside=summary(g.army);g.growing.push_back(Site{300,300,Farm});Fire f=g.fireNow();assert(f.on&&closer(inside.x,inside.y,f,GardenerKeep));
 size_t before=g.goals.size();g.checkKeep=true;t=run(d,g,t,Period);
 assert(!strcmp(g.lastReturn,"fire defence back on")&&d.phase==Phase::Return&&d.vias>=1&&g.goals.size()>before);
 assert(g.goals[before].first==d.vx[0]&&g.goals[before].second==d.vy[0]&&!closer(d.vx[0],d.vy[0],f,soldierKeep((int)g.army.size())));
 run(d,g,t,HomeTimeout);assert(g.keepViolations==0&&d.phase==Phase::Idle&&g.collapsed==0);
}
static void steering(){
 // Direct assault (target within AssaultDistance of home with the gardeners together, r35; r33: 2*ConvoyLead; farther ones
 // stage first): idle gardeners at home are sent to the ring and the farm collapses.
 {Engine e;e.add(1,Farm,58,38);Campaign c=fresh();launch(c,e,T0,A);assert(c.phase==Phase::Assault);run(c,e,T0+Period,800);assert(e.collapsed==1&&e.gardenerOrders>=3);}
 {Engine e;e.add(1,Farm,70,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.phase==Phase::Assault);run(c,e,T0+Period,1200);assert(e.collapsed==1&&e.gardenerOrders>=3);}
 {Engine e;e.add(1,Farm,100,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.phase==Phase::March);run(c,e,T0+Period,1600);assert(e.collapsed==1&&e.gardenerOrders>=3);}
 // Two farms close together: after the first collapse every gardener gets a cell at the second one.
 {Engine e;e.add(1,Farm,70,40);e.add(2,Farm,90,40);Campaign c=fresh();launch(c,e,T0,A);run(c,e,T0+Period,2000);assert(e.collapsed==2&&e.order[0]==Farm&&e.order[1]==Farm);}
 // Ring cells nearest the army first, one gardener per cell, occupied cells last.
 {Engine e;e.add(1,Hall,100,40);Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);
  e.occupiedCells.push_back({e.world[0].doorX-8,e.world[0].doorY+1});
  run(c,e,s+100,Period*3);std::vector<RingCell> ring;e.ring(e.world[0],ring);
  Army a=summary(e.army);int nearest=1<<30;for(auto&r:ring)if(r.dark&&!r.occupied)nearest=std::min(nearest,distance(r.x,r.y,a.x,a.y));
  bool any=false;for(int i=0;i<c.gardeners;i++)if(c.gcx[i]>=0){any=true;for(int j=0;j<c.gardeners;j++)if(j!=i)assert(!(c.gcx[i]==c.gcx[j]&&c.gcy[i]==c.gcy[j]));assert(!(c.gcx[i]==e.world[0].doorX-8&&c.gcy[i]==e.world[0].doorY+1));}
  assert(any&&nearest<1<<30);}
 // A gardener working its own ring cell is not re-ordered.
 {Engine e;e.add(1,Temple,100,40);Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);run(c,e,s,Period*4);int before=e.gardenerOrders;
  run(c,e,s+Period*4,Period*2);assert(e.gardenerOrders-before<=8);}
 // Partial losses are topped up from free gardeners near the army (r32: TopUpReach); far ones wait at home.
 {Engine e(12);e.add(1,Temple,100,40);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);assert(c.gardeners==10);uint32_t s=until(c,e,T0+Period,Phase::Assault);
  Army a=summary(e.army);for(auto&f:e.gards){bool held=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==f.g.id)held=true;if(!held){f.g.x=f.g.centreX=a.x-6;f.g.y=f.g.centreY=a.y;}}
  int killed=0;for(int i=0;i<c.gardeners&&killed<3;i++)for(auto&f:e.gards)if(f.g.id==c.gid[i]){f.alive=false;killed++;}
  s=run(c,e,s,GardenerResend+Period);assert(c.gardeners==9&&c.phase==Phase::Assault);
  /* once half the style's number died there, no more replacements */
  killed=0;for(int i=0;i<c.gardeners&&killed<3;i++)for(auto&f:e.gards)if(f.g.id==c.gid[i]&&f.alive){f.alive=false;killed++;}
  for(int k=0;k<20;k++){FakeGardener f;f.g.id=(uint16_t)(400+k);f.g.uid=4000u+k;f.g.x=f.g.centreX=a.x-4;f.g.y=f.g.centreY=a.y;e.gards.push_back(f);}
  run(c,e,s,GardenerResend*3);assert(c.gardeners==6&&c.phase==Phase::Assault);}
 {Engine e(12);e.add(1,Temple,100,40);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);
  int killed=0;for(int i=0;i<c.gardeners&&killed<3;i++)for(auto&f:e.gards)if(f.g.id==c.gid[i]){f.alive=false;killed++;}
  int orders=e.gardenerOrders;run(c,e,s,GardenerResend*3);assert(c.gardeners==7&&c.phase==Phase::Assault);
  for(auto&f:e.gards){bool held=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==f.g.id)held=true;if(f.alive&&!held)assert(distance(f.g.x,f.g.y,40,40)<=3);}(void)orders;} /* the two free ones stay at home */
 // A gardener whose own search drifted away from its still-Dark cell frees it: it is re-sent.
 {Engine e;e.add(1,Hall,100,40);Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);run(c,e,s+100,Period*3);
  int k=-1;for(int i=0;i<c.gardeners;i++)if(c.gcx[i]>=0){k=i;break;}assert(k>=0);auto*f=e.find(c.gid[k]);
  f->g.x+=DriftLimit+6;f->g.centreX+=DriftLimit+6;f->g.work=Work::Working;f->g.reservedX=f->g.reservedY=-1;int before=e.gardenerOrders;
  for(uint32_t t=s+130;e.gardenerOrders==before&&t<s+130+GardenerResend+Period*2;t+=Period)step(c,t,e);assert(e.gardenerOrders>before&&distance(f->g.centreX,f->g.centreY,c.gcx[k],c.gcy[k])<=1);}
 // Settlers parked in the Temple: the last two ring cells wait, the building stands until they are released.
 {Engine e;e.add(1,Temple,100,40);e.parkedInside=true;Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);run(c,e,s,600);
  assert(e.collapsed==0&&c.ringDark>=1&&c.ringDark<=2&&c.holding);
  e.darkUnits.push_back({e.world[0].doorX,e.world[0].doorY});run(c,e,s+600,150);assert(e.collapsed==0); /* a defender arriving does not end the hold */
  e.darkUnits.clear();e.parkedInside=false;run(c,e,s+750,1500);assert(e.collapsed==1);}
 // Settlers parked for good (continuous recruitment): the hold ends after HoldMax, the citadel is not unbreakable.
 {Engine e;e.add(1,Temple,100,40);e.parkedInside=true;Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);run(c,e,s,HoldMax+1500);assert(e.collapsed==1);}
}
static void sweeping(){
 // A sweep greens its way along the Dark land towards the Temple, besieges it on the way and goes home when no Dark land is in reach.
 Engine e(8);e.add(1,Temple,160,40);for(int x=70;x<=150;x+=2)e.darkCells.push_back({x,40});Campaign c=fresh();launch(c,e,T0,S);assert(c.style==Style::Sweep&&c.gardeners==8);
 uint32_t t=run(c,e,T0+Period,6000);assert(e.fronts>=3&&e.gardenerOrders>=8);
 run(c,e,t,9000);assert(e.collapsed==1&&e.order[0]==Temple&&e.darkCells.empty()&&e.dissolved==1&&!strcmp(e.lastReturn,"no Dark land in reach"));
 // The escort holds the front: a gardener is sent only while no Dark fighter is near it.
 Engine d;for(int x=70;x<=90;x+=2)d.darkCells.push_back({x,40});d.darkUnits.push_back({72,41});Campaign k=fresh();launch(k,d,T0,S);int before=d.gardenerOrders;
 before=d.landOrders;run(k,d,T0+Period,300);assert(d.landOrders==before);d.darkUnits.clear();run(k,d,T0+310,300);assert(d.landOrders>before);
 // A sweep never goes into the fire defence: with a farm standing, Dark land next to the Temple is left alone.
 Engine f;f.add(1,Temple,90,40);f.add(2,Farm,200,200,false);for(int x=60;x<=80;x+=2)f.darkCells.push_back({x,40});Campaign q=fresh();launch(q,f,T0,S);assert(f.launches==0);
 // A sweep has its own time limit.
 Engine g(8);for(int x=70;x<=400;x+=1)g.darkCells.push_back({x,40+x%5});Campaign s=fresh();launch(s,g,T0,S);run(s,g,T0+Period,SweepTime+Period*2);assert(!strcmp(g.lastReturn,"campaign time")||!strcmp(g.lastReturn,"no gardeners left"));
}
static void hunting(){
 // A hunt follows the Dark group; when none is left it goes on to a farm with its gardeners (r33: within PickReach of the army).
 Engine e;e.add(1,Farm,60,80);for(int i=0;i<4;i++)e.darkUnits.push_back({80+i,60});Campaign c=fresh();launch(c,e,T0,H);
 uint32_t t=run(c,e,T0+Period,Period*20);Army a=summary(e.army);assert(distance(a.x,a.y,81,60)<=3);
 for(auto&u:e.darkUnits)u.first+=6;t=run(c,e,t,HuntFollow+Period);a=summary(e.army);assert(distance(a.x,a.y,87,60)<=3);
 e.darkUnits.clear();t=run(c,e,t,HuntQuiet+HuntFollow*2);assert(e.hunts==1&&!c.hunting&&c.target.kind==Farm);
 run(c,e,t,3000);assert(e.collapsed==1);
 // r33: a hunt that ends beyond PickReach of the free gardeners goes home (the next convoy takes them there).
 {Engine f;f.add(1,Farm,60,80);for(int i=0;i<4;i++)f.darkUnits.push_back({140+i,60});Campaign d=fresh();launch(d,f,T0,H);uint32_t u=run(d,f,T0+Period,Period*20);
  f.darkUnits.clear();int before=f.gardenerOrders+f.parkOrders;run(d,f,u,HuntQuiet+HuntFollow*3);assert(!strcmp(f.lastReturn,"hunt over")&&f.gardenerOrders+f.parkOrders==before);}
 // Without gardeners a hunt ends at home.
 Engine g(1);for(int i=0;i<4;i++)g.darkUnits.push_back({100+i,60});Campaign d=fresh();launch(d,g,T0,H);assert(d.gardeners==0);
 for(auto&f:g.gards)f.alive=false;t=run(d,g,T0+Period,Period*20);g.darkUnits.clear();run(d,g,t,HuntQuiet+HuntFollow*3);assert(!strcmp(g.lastReturn,"hunt over")||d.phase==Phase::Idle);
}
static void reclaiming(){
 // The Dark border: after ReclaimStart, up to four idle gardeners per pass, each to its own cell.
 {Engine e;for(int x=60;x<=80;x+=2)e.darkCells.push_back({x,40});Campaign c=fresh();run(c,e,ReclaimStart+10,ReclaimEvery);assert(e.reclaims>=1&&e.gardenerOrders>=2&&e.gardenerOrders<=ReclaimPerPass&&!strcmp(e.lastReclaim,"border"));}
 {Engine e;e.darkCells={{60,40}};Campaign c=fresh();run(c,e,10,200);assert(e.gardenerOrders==0);}
 // The player's own territory: any time, also without attacks (AttackMode 0).
 {Engine e;e.ownCells={{50,40}};e.cfg.mode=0;Campaign c=fresh();run(c,e,10,200);assert(e.gardenerOrders==1&&!strcmp(e.lastReclaim,"own territory"));}
 {Engine e;e.darkCells={{60,40}};e.cfg.mode=0;Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 // Never next to Dark fighters, near a Temple/Fortress, near a growing site or into fire.
 {Engine e;e.darkCells={{60,40}};e.darkUnits.push_back({61,41});Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 {Engine e;e.darkCells={{60,40}};e.add(1,Temple,64,40,false);Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 {Engine e;e.darkCells={{55,40}};e.add(1,Temple,70,40,false);Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);} /* a Temple ring cell (up to 15 tiles out) */
 {Engine e;e.darkCells={{60,40}};e.add(1,Temple,80,40,false);Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==1);}
 {Engine e;e.darkCells={{60,40}};e.growing.push_back({65,42});Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 {Engine e;e.darkCells={{60,40}};e.add(1,Temple,80,40,false);e.add(2,Farm,200,200,false);Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 // Far land is left alone (beyond ReclaimRadius of the gardener).
 {Engine e;e.darkCells={{40+ReclaimRadius+5,40}};Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 // Called home: a working free gardener in fire, or near a Temple that still parks settlers.
 {Engine e;e.add(1,Temple,90,40,false);e.add(2,Farm,200,200,false);e.gards[0].g.x=e.gards[0].g.centreX=85;e.gards[0].g.y=e.gards[0].g.centreY=40;e.gards[0].g.work=Work::Working;
  Campaign c=fresh();run(c,e,10,Period);assert(e.gards[0].g.centreX==40&&e.gards[0].g.centreY==40&&!strcmp(e.lastFire,"gardener out of the fire defence")); /* r30: at once, every step */
  int orders=e.gardenerOrders;run(c,e,100,RecallEvery*4);assert(e.gardenerOrders==orders);}
 {Engine e;e.add(1,Temple,90,40,false);e.parkedInside=true;e.gards[0].g.x=e.gards[0].g.centreX=80;e.gards[0].g.y=e.gards[0].g.centreY=40;e.gards[0].g.work=Work::Working;
  Campaign c=fresh();run(c,e,10,RecallEvery*2);assert(e.gards[0].g.centreX==40&&!strcmp(e.lastReclaim,"gardener called back"));}
 {Engine e;e.add(1,Temple,90,40,false);e.gards[0].g.x=e.gards[0].g.centreX=80;e.gards[0].g.y=e.gards[0].g.centreY=40;e.gards[0].g.work=Work::Working;
  Campaign c=fresh();run(c,e,10,RecallEvery*2);assert(e.gards[0].g.centreX==80);}
 // Reclaiming goes on while an army is away (free gardeners only) and never changes the campaign's goal.
 {Engine e(12);e.add(1,Farm,200,40);e.darkUnits.push_back({199,43});Campaign c=fresh();launch(c,e,T0,A);assert(c.gardeners==10);
  for(int x=50;x<=70;x+=4)e.darkCells.push_back({x,30});int before=e.reclaims;run(c,e,T0+Period,ReclaimEvery+Period);assert(e.reclaims>before&&!strcmp(e.lastReclaim,"border")&&c.gardeners==10);}
}
static void losses(){
 Engine e;e.add(1,Temple,100,40);Campaign c=fresh();launch(c,e,T0,A);
 e.army.resize(e.army.size()*25/100);run(c,e,T0+Period,Period);assert(c.phase==Phase::Return&&e.homeMoves==1&&!strcmp(e.lastReturn,"army beaten"));
 run(c,e,T0+100,Period*3);assert(e.dissolved==1&&c.phase==Phase::Idle);
 // An assault fights on below 60% (raids turn back at half).
 Engine k;k.add(1,Temple,100,40);Campaign a=fresh();launch(a,k,T0,A);k.army.resize(k.army.size()*35/100);run(a,k,T0+Period,Period);assert(a.phase!=Phase::Return);
 Engine r;r.add(1,Farm,100,40);Campaign b=fresh();launch(b,r,T0,R);r.army.resize(4);run(b,r,T0+Period,Period);assert(b.phase==Phase::Return);
 Engine g;g.add(1,Farm,100,40);Campaign d=fresh();launch(d,g,T0,A);g.lostSquad=true;run(d,g,T0+Period,Period);assert(d.phase==Phase::Idle&&g.lost==1&&g.dissolved==0);
 Engine h;h.add(1,Farm,100,40);Campaign q=fresh();launch(q,h,T0,A);h.sectorOk=false;run(q,h,T0+Period,Period);assert(q.phase==Phase::Idle&&h.dissolved==0);
 Engine f;f.add(1,Farm,100,40);Campaign w=fresh();launch(w,f,T0,A);f.army.resize(2);f.failDissolve=true;run(w,f,T0+Period,Period*2);assert(w.phase==Phase::Return&&w.squad==0x1234);
 run(w,f,T0+Period*3,Period*10);assert(w.phase==Phase::Idle&&f.lost==1);
}
static void adoption(){
 Engine e;e.add(1,Farm,100,40);e.add(2,Temple,150,40);e.army=std::vector<Soldier>(10,Soldier{500,29,1,60,40});Campaign c=fresh();
 adopt(c,5000,0x1234,10,e);assert(e.adopted==1&&c.phase==Phase::Assault&&c.raid&&c.style==Style::Raid&&c.gardeners==0);
 run(c,e,5000+Period,3000);assert(e.collapsed==2&&e.order[0]==Farm&&e.order[1]==Temple&&e.dissolved==1&&c.phase==Phase::Idle);
 Engine g;g.add(2,Temple,150,40);g.army=std::vector<Soldier>(20,Soldier{500,29,1,60,40});Campaign d=fresh();adopt(d,5000,0x1234,20,g);assert(!d.raid&&d.style==Style::Assault);
 run(d,g,5000+Period,3000);assert(g.collapsed==1&&g.order[0]==Temple);
 // r32 review: a scaled raid (more than AdoptedRaid soldiers) stays a raid: the launch army is estimated from the free
 // soldiers now plus the squad (60 + 16: army 64, raid 21); a squad larger than that raid is an assault.
 {Engine h;for(int i=0;i<20;i++)h.pool.push_back(Soldier{(uint16_t)(200+i),29,(uint32_t)(2000+i),40,40});Campaign r=fresh();adopt(r,5000,0x1234,16,h);assert(r.style==Style::Raid&&r.raid&&r.want<=gardenersWanted(76));
  Campaign q=fresh();adopt(q,5000,0x1234,30,h);assert(q.style==Style::Assault&&!q.raid&&q.want<=gardenersWanted(90));}
}
struct Stuck:Engine{void move(const Campaign&,const std::vector<Soldier>&,int,int,bool home)override{moves++;if(home)homeMoves++;}};
static void timeouts(){
 Stuck s;s.add(1,Farm,100,40);Campaign d=fresh();launch(d,s,T0,A);
 run(d,s,T0+Period,IgnoreCheck+Period);assert(s.ignored==1);
 run(d,s,T0+Period+IgnoreCheck+Period,TargetTimeout);assert((d.phase==Phase::Return||d.phase==Phase::Idle)&&s.collapsed==0&&avoided(d,s.world[0].uid,d.since+1));
}
static void detours(){
 Engine e;e.add(1,Temple,120,40);e.add(2,Farm,200,42);e.add(3,Farm,205,90);Campaign c=fresh();e.checkKeep=true;launch(c,e,T0,A);
 assert(c.phase==Phase::March&&c.vias>=1);Fire f=e.fireNow();
 for(int i=0;i<c.vias;i++)assert(!closer(c.vx[i],c.vy[i],f,soldierKeep((int)e.army.size())));
 run(c,e,T0+Period,4000);assert(e.keepViolations==0&&e.collapsed>=1);
}
static void reviewFixes(){
 // A: eight gardeners whose native search reserves neighbouring ring cells on its own: a Temple that parks settlers keeps its last two cells.
 {Engine e(8);e.add(1,Temple,100,40);e.parkedInside=true;Campaign c=fresh();launch(c,e,T0,A);int lowest=99;
  for(uint32_t t=T0+Period;t<T0+800;t+=Period){step(c,t,e);std::vector<RingCell> r;if(!e.world.empty()){e.ring(e.world[0],r);int d=0;for(auto&x:r)if(x.dark)d++;lowest=std::min(lowest,d);}}
  assert(e.collapsed==0&&lowest>=1&&c.holding);
  /* r33 review: extra gardeners wait parked (WATCH) behind the screen near the army, not next to the ring and never sent home alone */
  int home=0,waiting=0;for(auto&f:e.gards)if(f.alive){if(distance(f.g.x,f.g.y,40,40)<=GatherHome)home++;if(f.g.escorted&&distance(f.g.x,f.g.y,c.target.doorX,c.target.doorY)<=EscortRadius)waiting++;}
  if(home||waiting<6)fprintf(stderr,"parked citadel: home %d waiting %d\n",home,waiting);assert(home==0&&waiting>=6);
  e.parkedInside=false;run(c,e,T0+800,3000);assert(e.collapsed==1);}
 // B: replacements are capped per target (a farm in the fire defence must not burn the whole pool).
 {Engine e(30);e.add(1,Farm,100,40);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);int want=c.want;uint32_t t=T0+Period;
  for(int round=0;round<6;round++){for(int i=0;i<c.gardeners;i++)if(auto*f=e.find(c.gid[i]))f->alive=false;t=run(c,e,t,GardenerResend+Period*2);}
  int dead=0;for(auto&f:e.gards)if(!f.alive)dead++;assert(dead<=want+(want+1)/2&&(c.phase==Phase::Return||c.phase==Phase::Idle));}
 // C: a Dark fighter next to the nearest cell no longer stops reclaiming: other cells are taken.
 {Engine e;e.ownCells={{50,40},{30,40},{40,62}};e.darkUnits.push_back({51,41});e.cfg.mode=0;Campaign c=fresh();run(c,e,10,400);assert(e.gardenerOrders>=2);
  for(auto&f:e.gards)if(f.g.work!=Work::Idle)assert(distance(f.g.centreX,f.g.centreY,51,41)>DefenceRadius);}
 // D: pincer halves are stable when a soldier dies.
 {std::vector<Soldier> m;for(int i=0;i<20;i++)m.push_back(Soldier{(uint16_t)(100+i),29,(uint32_t)(1000+i*37),0,0});std::vector<Soldier> a,b,a2,b2;halves(m,a,b);
  m.erase(m.begin()+3);halves(m,a2,b2);for(auto&s:a2){bool was=false;for(auto&o:a)if(o.id==s.id)was=true;assert(was);}for(auto&s:b2){bool was=false;for(auto&o:b)if(o.id==s.id)was=true;assert(was);}assert(!a.empty()&&!b.empty());}
 // E: an allied escort without gardeners goes home when a farm appears instead of standing there alone.
 {Engine e(0);e.add(1,Hall,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);
  assert(c.escort);e.add(2,Farm,90,90);run(c,e,T0+2*Period,Period*3);assert(!strcmp(e.lastReturn,"allied escort released"));}
 // F: a parked Temple keeps reclaim destinations as far away as its recall radius (no send/recall loop).
 {Engine e;e.darkCells={{71,40}};e.add(1,Temple,90,40,false);e.parkedInside=true;Campaign c=fresh();run(c,e,ReclaimStart+10,600);assert(e.gardenerOrders==0);}
 // Fire: a farm within FarmLock of the citadel is no target for any style, for every farm count (one farm burns too).
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,104,44);e.add(3,Farm,200,200);e.add(4,Farm,210,200);Pick any;any.weakest=true;
  int i=chooseTarget(e.world,F(e.world),40,40,any);assert(i>=0&&e.world[i].id!=2);e.world.erase(e.world.begin()+2,e.world.end());
  assert(chooseTarget(e.world,F(e.world),40,40,any)<0);e.add(5,Farm,96,36);e.add(6,Farm,106,36);
  assert(chooseTarget(e.world,F(e.world),40,40,any)<0);}
 // An allied escort goes home when nobody greens the ring any more.
 {Engine e(0);e.add(1,Hall,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);
  assert(c.escort);run(c,e,T0+2*Period,EscortStall+600);assert(!strcmp(e.lastReturn,"allied siege stalled"));}
 // A hunt leaves its gardeners at home while it marches and hunts.
 {Engine e;e.add(1,Farm,60,90);for(int i=0;i<4;i++)e.darkUnits.push_back({140+i,40});Campaign c=fresh();launch(c,e,T0,H);assert(c.hunting&&c.phase==Phase::March&&c.gardeners==0);
  run(c,e,T0+Period,Period*10);for(auto&f:e.gards)assert(distance(f.g.x,f.g.y,40,40)<=3);}
 // Gardeners dying at one target: the target is given up once as many died as the style takes.
 {Engine e(30);e.add(1,Farm,100,40);e.add(2,Farm,160,90);e.darkUnits.push_back({99,43});Campaign c=fresh();launch(c,e,T0,A);assert(c.want==10);uint32_t t=until(c,e,T0+Period,Phase::Assault);
  Army a=summary(e.army);for(auto&f:e.gards){bool held=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==f.g.id)held=true;if(!held){f.g.x=f.g.centreX=a.x-5;f.g.y=f.g.centreY=a.y;}} /* free ones within TopUpReach */
  for(int n:{4,3,3}){int killed=0;for(int i=0;i<c.gardeners&&killed<n;i++)if(auto*f=e.find(c.gid[i]))if(f->alive){f->alive=false;killed++;}t=run(c,e,t,GardenerResend+Period*2);}
  int dead=0;for(auto&f:e.gards)if(!f.alive)dead++;assert(dead==10&&e.avoids>=1&&c.target.id==2&&c.phase!=Phase::Idle);}
 // All held gardeners die at once: the target is avoided long enough that the next army goes elsewhere.
 {Engine e(16);e.add(1,Farm,100,40);e.add(2,Farm,160,90);e.darkUnits.push_back({99,43});Campaign c=fresh(A);launch(c,e,T0,A);assert(c.target.id==1);
  for(int i=0;i<c.gardeners;i++)if(auto*f=e.find(c.gid[i]))f->alive=false;uint32_t t=run(c,e,T0+Period,Period*3);assert(e.avoids==1&&!strcmp(e.lastReturn,"no gardeners left"));
  uint32_t f1=e.world[0].uid;t=run(c,e,t,HomeTimeout+CooldownMin+CooldownSpread+2000);bool farm1=false,farm2=false;for(auto&b:e.world){if(b.id==1)farm1=true;if(b.id==2)farm2=true;}
  assert(e.launches>=2&&farm1&&!farm2&&e.collapsed==1);(void)f1;}
 // The parked hold counts per target: a queue that drains and refills cannot extend it forever.
 {Engine e;e.add(1,Temple,100,40);e.parkedInside=true;Campaign c=fresh();launch(c,e,T0,A);uint32_t t=T0+Period;
  for(int k=0;k<20&&e.collapsed==0;k++){t=run(c,e,t,200);e.parkedInside=!e.parkedInside;t=run(c,e,t,Period);e.parkedInside=true;}assert(e.collapsed==1&&t<T0+Period+HoldMax+2000);}
 // Only the leading campaign works a parked citadel's ring.
 {Engine e;e.add(1,Temple,100,40);e.parkedInside=true;e.leader=false;Campaign c=fresh();launch(c,e,T0,A);run(c,e,T0+Period,600);
  std::vector<RingCell> r;e.ring(e.world[0],r);int d=0;for(auto&x:r)if(x.dark)d++;assert(e.collapsed==0&&d>=(int)r.size()-6);} /* only the first round before the parked check, then nothing */
 // An escort that marched far does not leave on its first siege step.
 {Engine e(0);e.add(1,Hall,240,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);
  assert(c.escort&&c.phase==Phase::March);c.since-=EscortStall+100;run(c,e,T0+2*Period,Period*5);assert(c.phase==Phase::Assault&&strcmp(e.lastReturn,"allied siege stalled"));}
 // Locked farms do not forbid the (unlocked) Halls.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,104,44);e.add(3,Farm,96,36);e.add(4,Farm,106,36);e.add(5,Hall,200,200);Pick any;any.weakest=true;
  int i=chooseTarget(e.world,F(e.world),40,40,any);assert(i>=0&&e.world[i].id==5);}
 // Reclaim rotates through the gardeners: four that find nothing do not block the others for good.
 {Engine e(8);for(int k=0;k<4;k++){e.gards[k].g.x=e.gards[k].g.centreX=400;e.gards[k].g.y=e.gards[k].g.centreY=400;}e.darkCells={{60,40},{64,44},{68,48}};
  Campaign c=fresh();run(c,e,ReclaimStart+10,ReclaimEvery*4);assert(e.gardenerOrders>=1);}
 // A hunt that finds no building afterwards orders no gardener.
 {Engine e;for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,60});Campaign c=fresh();launch(c,e,T0,H);uint32_t t=run(c,e,T0+Period,Period*20);e.darkUnits.clear();
  int before=e.gardenerOrders;run(c,e,t,HuntQuiet+HuntFollow*3);assert(!strcmp(e.lastReturn,"hunt over")&&e.gardenerOrders==before&&c.gardeners==0);}
 // G: a player whose army came home joins a new call on the same building.
 {Engine e;e.add(1,Farm,140,40);e.callTarget=e.world[0];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);assert(e.joins==1);
  toIdle(c,T0+100);assert(c.joinedUid==0);}
}
// r30: the citadel fire keep-off (citadel-fire.hpp, research/AI_OFFENCE_0215.md r30).
static void fireRules(){
 // Metric: the native R=24 spiral holds exactly the 2,083 cells with fire2 <= 576 (up to 27 hex steps);
 // along (2k,k) hex 26 is still inside.
 int cells=0,hex=0;for(int dy=-40;dy<=40;dy++)for(int dx=-40;dx<=40;dx++)if(dtfire::fire2(dx,dy,0,0)<=FireReach*FireReach){cells++;hex=std::max(hex,distance(0,0,dx,dy));}
 assert(cells==2083&&hex==27&&dtfire::fire2(26,13,0,0)==507&&distance(0,0,26,13)==26);
 // Keeps: formation spread 2k+2 (k rings hold N), soldier and leg keeps, the shared constants.
 assert(spread(1)==2&&spread(7)==4&&spread(8)==6&&spread(19)==6&&spread(37)==8&&spread(61)==10&&spread(91)==12&&spread(100)==14);
 assert(soldierKeep(7)==31&&soldierKeep(19)==33&&soldierKeep(37)==35&&soldierKeep(100)==41&&legKeep(19)==36&&GardenerKeep==28&&FarmLock==33&&HallLock==44&&HuntKeep==42&&FrontKeep==37);
 assert(FireReach==24&&FireLine==27&&FarmReach==4&&dtfire::CentreDX==1&&dtfire::CentreDY==2);
 // Fire state: the native counter, the listed farms or a Farm site alone turn it on; a Temple/Fortress site adds a centre.
 std::vector<Target> w;{Target b;b.id=1;b.kind=Temple;b.x=100;b.y=100;w.push_back(b);}
 Fire off=F(w);assert(!off.on&&off.z.size()==1&&clear2(0,0,off)==NoFire&&!inFire(101,102,off)&&!closer(101,102,off,99));
 assert(F(w,1).on&&F(w,-1,{Site{9,9,Farm}}).on&&!F(w,0).on&&!F(w,-1,{Site{9,9,Temple}}).on);
 {std::vector<Target> v=w;Target b;b.id=2;b.kind=Farm;b.x=300;b.y=300;v.push_back(b);Fire f=F(v,-1);assert(f.on&&f.listed==1&&f.ready==-1);}
 {Fire f=F(w,3,{Site{200,100,Fortress},Site{150,150,Farm},Site{100,100,Temple}});assert(f.on&&f.z.size()==2&&f.z[1].x==201&&f.z[1].y==102&&f.farmSites==1&&f.ready==3);}
 // Locks: a farm closer than FarmLock (Euclid 32 locked, 33 not), a Hall closer than HallLock, the citadels while on.
 Fire on=F(w,4);auto at=[&](uint16_t kind,int x,int y){Target b;b.kind=kind;b.x=x;b.y=y;b.reachable=true;return b;};
 assert(locked(at(Farm,101+32,102),on)&&!locked(at(Farm,101+33,102),on)&&locked(at(Hall,101,102+43),on)&&!locked(at(Hall,101,102+44),on));
 assert(locked(at(Farm,101+38,102+19),on)&&distance(0,0,38,19)==38); /* hex 38 along (2k,k) is Euclid 32.9 */
 assert(locked(at(Temple,500,500),on)&&locked(at(Fortress,500,500),on)&&!locked(at(Farm,101+20,102),off)&&!locked(at(Temple,100,100),off));
 assert(dtfire::farmTooClose(132,102,100,100)&&!dtfire::farmTooClose(134,102,100,100)); /* the blockade's own test */
 // Hunts ignore fighters within HuntKeep (41: ignored, 43: taken).
 {int x=0,y=0,n=0;std::vector<std::pair<int,int>> g41,g43;for(int i=0;i<4;i++){g41.push_back({101+41,102+i%2});g43.push_back({101+43+i%2,102});}
  assert(!cluster(g41,on,0,0,x,y,n)&&cluster(g43,on,0,0,x,y,n)&&n==4);}
 // An allied call on a target locked since the call is refused.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,110,44);e.add(3,Farm,300,300);e.callTarget=e.world[1];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);assert(e.joins==0&&(e.launches==0||c.target.id==3));} /* r37: the unlocked farm is attacked instead (no chance gate) */
 // Only locked farms left: the computers decline with that reason (no citadel, no locked farm).
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,110,44);Campaign c=fresh();launch(c,e,T0);assert(e.launches==0&&!strcmp(e.lastDecline,"farms only inside the fire defence"));}
}
static void guarding(){
 // A soldier within FireLine is ordered out to an exit at least soldierKeep away.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.target.id==2);Fire f=e.fireNow();
  e.army[0].x=127;e.army[0].y=42;assert(inFire(127,42,f));int fires=e.fires;uint16_t pulledId=e.army[0].id;run(c,e,T0+2*Period,Period);
  assert(e.fires>fires&&!strcmp(e.lastFire,"soldiers out of the fire defence")&&!closer(e.army[0].x,e.army[0].y,f,soldierKeep(1)));
  assert(e.pulls.size()==1&&e.pulls[0].id==pulledId&&e.pulls[0].x==127&&e.pulls[0].y==42&&!e.pulls[0].gardener&&e.pullExitX==e.army[0].x&&e.pullExitY==e.army[0].y); /* r36: where it stood, where it was sent */
  // Lured in again and again at the target (Dark fighters at the line): avoided after LureLimit extractions.
  e.darkUnits.push_back({299,43});uint32_t t=T0+3*Period;for(int k=0;k<600&&c.phase!=Phase::Assault;k++)t=run(c,e,t,Period); /* the march walks its waypoints and hex-leg corners (r32: as a convoy) */
  t=run(c,e,t,Period*6);assert(c.phase==Phase::Assault&&c.target.id==2);
  for(int k=0;k<LureLimit&&c.target.id==2;k++){e.army[1].x=126;e.army[1].y=42;t=run(c,e,t,GuardResend);}
  assert(c.target.id==2&&strcmp(e.lastAvoid,"lured toward the fire defence")&&e.fires>fires+LureLimit); /* r38: single stragglers are pulled out but never count */
  for(int k=0;k<LureLimit&&c.target.id==2;k++){for(int j=1;j<=LureMin;j++){e.army[j].x=126;e.army[j].y=42;}t=run(c,e,t,GuardResend);}
  assert(!strcmp(e.lastAvoid,"lured toward the fire defence")&&avoided(c,e.world[1].uid,t));}
 // A free idle gardener within FireLine is recalled at once.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,300,false);e.gards[0].g.x=127;e.gards[0].g.y=42;e.gards[0].g.work=Work::Idle;Campaign c=fresh();run(c,e,10,Period);
  assert(!inFire(e.gards[0].g.x,e.gards[0].g.y,e.fireNow())&&!strcmp(e.lastFire,"gardener out of the fire defence"));
  assert(e.pulls.size()==1&&e.pulls[0].gardener&&e.pulls[0].free&&!e.pulls[0].benched&&e.pulls[0].x==127&&e.pulls[0].y==42);} /* r36: a free gardener, first recall */
 // A gardener at 32 whose aimed cell (job 0xAE) lies at 25 is recalled in the same step, further out.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,300,false);auto&g=e.gards[0].g;g.x=133;g.y=42;g.aimX=126;g.aimY=42;g.work=Work::Busy;Campaign c=fresh();int before=e.gardenerOrders;run(c,e,10,Period);
  assert(e.gardenerOrders==before+1&&clear2(g.x,g.y,e.fireNow())>32*32&&g.aimX<0);}
 // Held gardeners: recalled and kept; benched (dropped, not taken again) at the second recall within BenchTicks.
 {Engine e(8);e.add(1,Temple,100,40);e.add(2,Farm,300,40);Campaign c=fresh();launch(c,e,T0,A);int held=c.gardeners;assert(held>=4);
  auto*f=e.find(c.gid[0]);uint16_t id=f->g.id;uint32_t uid=f->g.uid;f->g.x=127;f->g.y=42;uint32_t t=run(c,e,T0+2*Period,Period);
  assert(c.gardeners==held&&!inFire(f->g.x,f->g.y,e.fireNow()));
  t=run(c,e,t,Period*3);f->g.x=127;f->g.y=42;t=run(c,e,t,Period);
  bool still=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)still=true;assert(!still&&c.gardeners==held-1&&benched(c,id,uid,t)&&!inFire(f->g.x,f->g.y,e.fireNow()));
  t=run(c,e,t,GardenerResend*3);still=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)still=true;assert(!still);}
 // Siege: ring cells within GardenerKeep are never given out; a ring with only such cells is avoided.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,400,400,false);e.add(3,Hall,100,100);for(int i=0;i<6;i++)e.rings[3].push_back(RingCell{110+i,50,true,false});
  assert(!locked(e.world[2],e.fireNow()));Campaign c=fresh();launch(c,e,T0,A);uint32_t s=until(c,e,T0+Period,Phase::Assault);run(c,e,s,Period*6);assert(!strcmp(e.lastAvoid,"ring inside the fire defence"));}
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,400,400,false);e.add(3,Hall,100,100);for(int i=0;i<4;i++)e.rings[3].push_back(RingCell{110+i,50,true,false});for(int i=0;i<4;i++)e.rings[3].push_back(RingCell{96+i,103,true,false});
  e.checkKeep=true;Campaign c=fresh();launch(c,e,T0,A);Fire f=e.fireNow();for(uint32_t t=T0+Period;t<T0+3000;t+=Period){step(c,t,e);for(int i=0;i<c.gardeners;i++)if(c.gcx[i]>=0)assert(!closer(c.gcx[i],c.gcy[i],f,GardenerKeep));}
  assert(e.keepViolations==0&&e.gardenerOrders>0);}
 // Sweep: the front keeps FrontKeep from the fire, never heads for the citadel; a drifting gardener is intercepted.
 {Engine e(10);e.add(1,Temple,160,40);e.add(2,Farm,400,300,false);for(int x=70;x<=240;x+=2)for(int y=20;y<=60;y+=4)e.darkCells.push_back({x,y});
  e.checkKeep=true;Campaign c=fresh(S);launch(c,e,T0,S);assert(e.launches==1&&c.style==Style::Sweep);Fire f=e.fireNow();bool drift=false;
  for(uint32_t t=T0+Period;t<T0+6000;t+=Period){step(c,t,e);if(c.phase==Phase::Assault&&!c.target.id)assert(!closer(c.frontX,c.frontY,f,FrontKeep));
   if(!drift&&c.gardeners&&c.phase==Phase::Assault&&t>T0+600){auto*g=e.find(c.gid[0]);g->g.aimX=140;g->g.aimY=42;g->g.work=Work::Busy;int before=e.gardenerOrders;step(c,t+1,e);assert(e.gardenerOrders>before&&!strcmp(e.lastFire,"gardener out of the fire defence"));drift=true;}}
  assert(drift&&e.keepViolations==0&&e.fronts>=1);}
}
// r30 review: the guard gates its order per soldier; a march or a way home plans again; a way home without a
// safe route holds outside the fire and never dissolves with a soldier inside it; a sweep escort lured at its
// front gives the sweep up and holds beyond the chase reach.
struct Walled:Engine {int below=70;bool stand(const Campaign&,int x,int y,int){return x>=0&&y>=0&&x<below;}};
static void guardReview(){
 // A soldier that turns hot 10 ticks after another was ordered out is ordered out in the same step; one just
 // ordered out is not ordered again within GuardResend.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,40);Campaign c=fresh();launch(c,e,T0,A);Fire f=e.fireNow();uint32_t t=T0+2*Period;
  e.army[0].x=127;e.army[0].y=42;t=run(c,e,t,Period);assert(!inFire(e.army[0].x,e.army[0].y,f));
  e.army[1].x=126;e.army[1].y=44;t=run(c,e,t,Period);assert(!inFire(e.army[1].x,e.army[1].y,f));
  e.army[1].x=126;e.army[1].y=44;int moves=e.moves;t=run(c,e,t,Period);assert(e.moves==moves&&inFire(e.army[1].x,e.army[1].y,f));
  t=run(c,e,t,GuardResend);assert(!inFire(e.army[1].x,e.army[1].y,f));}
 // No way home outside the fire (nothing east of x=70 to stand on): the army is never sent home across the fire, it
 // holds, plans again, and dissolves after HomeTimeout outside the fire.
 {Walled e;e.ready=4;e.add(1,Temple,100,40);e.str.homeX=160;e.str.homeY=42;e.pool.clear();for(int i=0;i<10;i++)e.army.push_back(Soldier{(uint16_t)(100+i),29,(uint32_t)(1000+i),50,42});
  Campaign c=fresh(A);c.armed=true;c.phase=Phase::Assault;c.squad=0x1234;c.initial=10;uint32_t t=T0;startReturn(c,t,e,e.army,"test");
  assert(c.phase==Phase::Return&&c.homeless&&!e.homeMoves);for(auto&g:e.goals)assert(!(g.first==160&&g.second==42));
  for(t+=Period;c.phase==Phase::Return&&t<T0+HomeTimeout+20*Period;t+=Period){step(c,t,e);if(c.phase==Phase::Return)assert(c.homeless);}
  assert(c.phase==Phase::Idle&&e.dissolved==1&&!e.homeMoves);for(auto&g:e.goals)assert(!(g.first==160&&g.second==42));}
 // The same with one soldier inside the fire and no cell to leave it for: never handed back before HotHold.
 {Walled e;e.ready=4;e.below=0;e.add(1,Temple,100,40);e.str.homeX=160;e.str.homeY=42;e.pool.clear();for(int i=0;i<10;i++)e.army.push_back(Soldier{(uint16_t)(100+i),29,(uint32_t)(1000+i),30,42});
  e.army[9].x=101;e.army[9].y=60;Campaign c=fresh(A);c.armed=true;c.phase=Phase::Assault;c.squad=0x1234;c.initial=10;uint32_t t=T0;startReturn(c,t,e,e.army,"test");
  assert(c.homeless&&!e.homeMoves);for(t+=Period;t<T0+HomeTimeout+100*Period;t+=Period)step(c,t,e);assert(c.phase==Phase::Return&&!e.dissolved);
  for(;c.phase==Phase::Return&&t<T0+HotHold+20*Period;t+=Period)step(c,t,e);assert(c.phase==Phase::Idle&&e.dissolved==1&&!e.homeMoves);}
 // A sweep escort pulled toward the fire at its front LureLimit times gives the sweep up; it holds beyond HuntKeep.
 {Engine e(10);e.add(1,Temple,160,40);e.add(2,Farm,400,300,false);for(int x=70;x<=240;x+=2)for(int y=20;y<=60;y+=4)e.darkCells.push_back({x,y});
  Campaign c=fresh(S);launch(c,e,T0,S);assert(e.launches==1&&c.style==Style::Sweep);Fire f=e.fireNow();uint32_t t=T0+2*Period;
  for(int k=0;k<400&&!(c.phase==Phase::Assault&&!c.target.id&&c.lastOrder);k++)t=run(c,e,t,Period);
  assert(c.phase==Phase::Assault&&!c.target.id&&!closer(c.goalX,c.goalY,f,HuntKeep));
  for(int k=0;k<LureLimit&&c.phase==Phase::Assault;k++){for(int j=1;j<=LureMin;j++){e.army[j].x=188;e.army[j].y=42;}assert(inFire(188,42,f));t=run(c,e,t,GuardResend);} /* r38: LureMin soldiers at once */
  assert(c.phase!=Phase::Assault&&!strcmp(e.lastReturn,"lured toward the fire defence"));} /* home at once: the fake engine teleports */
}
// No order lands inside its keep, for every style and army size (5..100): stage, via, assault, hold, hunt, exit, return
// cells keep soldierKeep(N) of the group ordered there, every gardener destination keeps GardenerKeep.
static void keepAudit(){
 for(int style:{R,A,P,S,H,(int)Style::Allied})for(int pool:{8,20,40,120}){
  Engine e(12);e.pool.clear();for(int i=0;i<pool;i++)e.pool.push_back(Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),40,40});
  e.add(1,Temple,150,100);e.add(2,Farm,160,104);e.add(3,Farm,200,100);e.add(4,Farm,150,140);e.add(5,Hall,100,170);
  for(int x=60;x<=240;x+=3)for(int y=60;y<=180;y+=6)e.darkCells.push_back({x,y});
  for(int i=0;i<4;i++)e.darkUnits.push_back({60+i,120});
  if(style==(int)Style::Allied){e.callTarget=e.world[3];e.callUntil=100000;e.caller=3;}
  e.checkKeep=true;Campaign c=fresh(style);run(c,e,0,8000);
  if(e.keepViolations||!(e.launches+e.joins))fprintf(stderr,"keep audit: style %s pool %d violations %d launches %d last decline '%s'\n",styleName((Style)style),pool,e.keepViolations,e.launches+e.joins,e.lastDecline);
  assert(e.keepViolations==0&&((e.launches+e.joins)>=1||(style==P&&pool-std::max(HomeReserve,pool/6)<PincerMin)));
  bool locked2=false;for(auto&b:e.world)if(b.id==2)locked2=true;assert(locked2); /* the locked farm stands */
 }
}
// Field-like soak: units walk one cell per 6 ticks along their own straight line, a group order puts member k
// at click + 2*spiral[k], gardeners search natively (the nearest Dark cell within 28 of themselves, the work
// centre as tiebreak; job 0xAE walking to it, 0xAF greening it for 31 ticks while standing on it), and every 31
// ticks while a farm stands the fire kills every computer unit on a Dark cell within 24 of any fire centre
// (the Temple's; fortress() adds a Fortress and its centre, home() moves the computer's home).
struct Soak {
 static constexpr int N=420,CX=201,CY=202,Disc=95,HomeX=340,HomeY=340;
 std::vector<std::pair<int,int>> centres{{CX,CY}};
 double clearance(int x,int y)const{int m=1<<30;for(auto&c:centres)m=std::min(m,dtfire::fire2(x,y,c.first,c.second));return sqrt((double)m);}
 Config cfg{};Strength str{};std::vector<uint8_t> green=std::vector<uint8_t>(N*N,0);
 std::vector<Target> world;std::map<uint16_t,std::vector<std::pair<int,int>>> rings;
 struct Unit {Soldier s;int tx=0,ty=0;};std::vector<Unit> pool,army;
 struct G {Gardener g;int tx=-1,ty=-1,until=0;bool alive=true,working=false;};std::vector<G> gards;
 std::vector<std::pair<int,int>> spiral,fighters;
 int deaths=0,collapsed=0,launches=0,joins=0,returns=0,fireEvents=0;std::vector<uint16_t> sieged;const char*lastReturn="";const char*lastDecline="";
 Target callTarget{};bool calling=false;
 // r32: walking speed, ticks per cell (SettlerValues.xml gives every settler type the same speed; slower gardeners test the convoy's worst case).
 int soldierEvery=6,gardenerEvery=6;
 void soldiers(int n){pool.clear();for(int i=0;i<n;i++){Unit u;u.s=Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),str.homeX,str.homeY};u.tx=str.homeX;u.ty=str.homeY;pool.push_back(u);}}
 Soak(){cfg.ok=true;cfg.mode=2;cfg.delay=1;cfg.thresholdPartial=175;cfg.thresholdTotal=225;str.ok=true;str.fighters=40;str.strength=400;str.darkValue=40;str.region=7;str.homeX=HomeX;str.homeY=HomeY;
  for(int dy=-8;dy<=8;dy++)for(int dx=-8;dx<=8;dx++)if(distance(0,0,dx,dy)<=6)spiral.push_back({dx,dy});
  std::stable_sort(spiral.begin(),spiral.end(),[](const std::pair<int,int>&a,const std::pair<int,int>&b){return dtfire::fire2(a.first,a.second,0,0)<dtfire::fire2(b.first,b.second,0,0);});
  for(int i=0;i<40;i++){Unit u;u.s=Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),HomeX,HomeY};u.tx=HomeX;u.ty=HomeY;pool.push_back(u);}
  for(int i=0;i<12;i++){G g;g.g.id=(uint16_t)(300+i);g.g.uid=3000u+i;g.g.x=HomeX+i%4;g.g.y=HomeY;g.g.centreX=g.g.x;g.g.centreY=g.g.y;gards.push_back(g);}
  Target t;t.kind=Temple;t.id=1;t.uid=7;t.x=200;t.y=200;t.doorX=199;t.doorY=210;t.reachable=true;world.push_back(t);
  farm(20,181,202);farm(30,201,232);farm(36,237,202);farm(44,157,158);}
 void farm(uint16_t id,int x,int y){Target t;t.kind=Farm;t.id=id;t.uid=id*7u;t.x=x;t.y=y;t.doorX=x;t.doorY=y+4;t.reachable=true;world.push_back(t);
  for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++)if(distance(0,0,dx,dy)==3)rings[id].push_back({x+dx,y+dy});}
 int farms()const{int n=0;for(auto&b:world)if(b.kind==Farm)n++;return n;}
 void fortress(int x,int y){Target t;t.kind=Fortress;t.id=2;t.uid=9;t.x=x;t.y=y;t.doorX=x-1;t.doorY=y+10;t.reachable=true;world.push_back(t);centres.push_back({dtfire::centreX(x),dtfire::centreY(y)});}
 void home(int x,int y){str.homeX=x;str.homeY=y;for(auto&u:pool){u.s.x=u.tx=x;u.s.y=u.ty=y;}for(auto&g:gards){g.g.x=g.g.centreX=x+g.g.id%4;g.g.y=g.g.centreY=y;}}
 bool inside(int x,int y)const{return x>=3&&y>=3&&x<N-3&&y<N-3;}
 bool core(int x,int y)const{for(auto&b:world)if(distance(x,y,b.x,b.y)<=(b.kind==Farm?2:4))return true;return false;}
 bool dark(int x,int y)const{return inside(x,y)&&(distance(x,y,CX,CY)<=Disc||ownMask[y*N+x])&&!green[y*N+x]&&!core(x,y);}
 // r33: Dark land in the computer's own territory (ownDark), a disc of radius r.
 std::vector<uint8_t> ownMask=std::vector<uint8_t>(N*N,0);std::vector<std::pair<int,int>> patch;
 void ownPatch(int x,int y,int r){for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++)if(distance(0,0,dx,dy)<=r&&inside(x+dx,y+dy)){ownMask[(y+dy)*N+x+dx]=1;patch.push_back({x+dx,y+dy});}}
 int ownLeft()const{int n=0;for(auto&p:patch)if(dark(p.first,p.second))n++;return n;}
 bool soldierAt(int x,int y)const{for(auto&u:army)if(u.s.x==x&&u.s.y==y)return true;return false;}
 static void walk(int&x,int&y,int tx,int ty){static const int nx[6]={1,-1,0,0,1,-1},ny[6]={0,0,1,-1,1,-1};int best=dtfire::fire2(x,y,tx,ty),bx=x,by=y;
  for(int k=0;k<6;k++){int d=dtfire::fire2(x+nx[k],y+ny[k],tx,ty);if(d<best){best=d;bx=x+nx[k];by=y+ny[k];}}x=bx;y=by;}
 // The native gardener search: nearest Dark cell (Euclid) within 28, the work centre as tiebreak within one tile.
 bool search(G&g){Gardener&q=g.g;int closest=-1;std::vector<std::pair<int,int>> found;
  for(int dy=-28;dy<=28;dy++)for(int dx=-28;dx<=28;dx++){int d=dtfire::fire2(dx,dy,0,0);if(d>28*28)continue;int x=q.x+dx,y=q.y+dy;if(!dark(x,y)||soldierAt(x,y))continue;
   bool taken=false;for(auto&o:gards)if(&o!=&g&&o.alive&&o.g.aimX==x&&o.g.aimY==y)taken=true;if(taken)continue;found.push_back({x,y});if(closest<0||d<closest)closest=d;}
  if(closest<0)return false;double lim=sqrt((double)closest)+1;long best=-1;
  for(auto&c:found){double d=sqrt((double)dtfire::fire2(c.first,c.second,q.x,q.y));if(d>lim)continue;long s=dtfire::fire2(c.first,c.second,q.centreX,q.centreY)+2L*dtfire::fire2(c.first,c.second,q.x,q.y);if(best<0||s<best){best=s;q.aimX=c.first;q.aimY=c.second;}}
  return true;}
 void tick(uint32_t t){
  if(t%soldierEvery==0)for(auto&u:army){walk(u.s.x,u.s.y,u.tx,u.ty);if(getenv("SOAK_LOG")&&clearance(u.s.x,u.s.y)<=FireLine)fprintf(stderr,"  t=%u soldier %u at %d,%d (%.1f) heading %d,%d (%.1f)\n",t,u.s.id,u.s.x,u.s.y,clearance(u.s.x,u.s.y),u.tx,u.ty,clearance(u.tx,u.ty));}
  if(t%gardenerEvery==0)for(auto&g:gards){if(!g.alive||g.working)continue;Gardener&q=g.g;if(g.tx>=0){walk(q.x,q.y,g.tx,g.ty);if(q.x==g.tx&&q.y==g.ty){g.tx=-1;q.work=q.escorted?Work::Idle:Work::Busy;}}
    else if(q.aimX>=0){if(distance(q.x,q.y,q.aimX,q.aimY)<=1){q.x=q.aimX;q.y=q.aimY;g.working=true;g.until=(int)t+31;q.reservedX=q.aimX;q.reservedY=q.aimY;q.work=Work::Working;}else walk(q.x,q.y,q.aimX,q.aimY);}}
  for(auto&g:gards){if(!g.alive)continue;Gardener&q=g.g;
   if(g.working&&(int)t>=g.until){if(dark(q.aimX,q.aimY))green[q.aimY*N+q.aimX]=1;g.working=false;q.aimX=q.aimY=q.reservedX=q.reservedY=-1;q.work=Work::Busy;}
   if(!g.working&&g.tx<0&&q.aimX<0&&q.work!=Work::Idle&&t%6==3){if(!search(g))q.work=Work::Idle;}}
  for(size_t i=0;i<world.size();)if(world[i].kind==Farm){bool all=true;for(auto&c:rings[world[i].id])if(dark(c.first,c.second))all=false;if(all){world.erase(world.begin()+i);collapsed++;continue;}i++;}else i++;
  if(t%31==0&&farms()>0){ /* the fire pass of every Temple/Fortress: 100% from 4 farms, kept for every count (worst case) */
   auto burns=[&](int x,int y){if(!dark(x,y))return false;for(auto&c:centres)if(dtfire::fire2(x,y,c.first,c.second)<=FireReach*FireReach)return true;return false;};
   for(size_t i=0;i<army.size();)if(burns(army[i].s.x,army[i].s.y)){fprintf(stderr,"soak: soldier burnt at %d,%d\n",army[i].s.x,army[i].s.y);deaths++;army.erase(army.begin()+i);}else i++;
   for(auto&g:gards)if(g.alive&&burns(g.g.x,g.g.y)){fprintf(stderr,"soak: gardener %u burnt at %d,%d\n",g.g.id,g.g.x,g.g.y);deaths++;g.alive=false;}}
 }
 // Ops.
 bool sector(const Campaign&){return true;}
 Config config(uint8_t){return cfg;}
 Strength strength(const Campaign&){return str;}
 int readyFarms(){return farms();}
 void sites(const Campaign&,std::vector<Site>&){}
 void targets(const Campaign&,std::vector<Target>&out){out=world;}
 bool alive(const Target&t){for(auto&b:world)if(b.id==t.id&&b.uid==t.uid)return true;return false;}
 int defenders(int x,int y,int r,bool weighted){int n=0;for(auto&d:fighters)if(distance(d.first,d.second,x,y)<=r)n+=weighted?2:1;return n;}
 void available(const Campaign&,int,int,std::vector<Soldier>&out){out.clear();for(auto&u:pool)out.push_back(u.s);}
 int raise(Campaign&c,const std::vector<Soldier>&chosen){for(auto&s:chosen)for(size_t i=0;i<pool.size();i++)if(pool[i].s.id==s.id){army.push_back(pool[i]);pool.erase(pool.begin()+i);break;}c.squad=0x1234;return (int)army.size();}
 bool members(const Campaign&c,std::vector<Soldier>&out){if(c.squad!=0x1234)return false;out.clear();for(auto&u:army)out.push_back(u.s);return true;}
 void move(const Campaign&,const std::vector<Soldier>&m,int x,int y,bool){int k=0;for(auto&o:m)for(auto&u:army)if(u.s.id==o.id){auto&d=spiral[std::min<size_t>(k++,spiral.size()-1)];u.tx=x+2*d.first;u.ty=y+2*d.second;}}
 bool stand(const Campaign&,int x,int y,int pass){return inside(x,y)&&!core(x,y)&&(pass||!dark(x,y));}
 bool free(int x,int y){return inside(x,y)&&!core(x,y);}
 bool reach(const Campaign&,int,int){return true;}
 void paths(const Campaign&,int x,int y,std::vector<Target>&out){for(auto&t:out)if(t.reachable)t.path=std::max(1,distance(x,y,t.doorX,t.doorY));} /* r37 */
 int walkSteps(const Campaign&,int fx,int fy,int tx,int ty){return std::max(1,distance(fx,fy,tx,ty));}void listTargets(const Campaign&,const std::vector<Target>&,const Fire&,uint32_t){} /* r38 */
 bool destroy(const Campaign&,const Target&t){for(size_t i=0;i<world.size();i++)if(world[i].id==t.id){world.erase(world.begin()+i);collapsed++;return true;}return false;}
 bool dissolve(Campaign&c){for(auto&u:army)pool.push_back(u);army.clear();c.squad=0;return true;}
 static bool held(const Campaign&c,uint16_t id){for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)return true;return false;}
 void gardeners(const Campaign&c,std::vector<Gardener>&out){out.clear();for(auto&g:gards)if(g.alive&&!held(c,g.g.id))out.push_back(g.g);}
 bool gardener(const Campaign&,uint16_t id,uint32_t uid,Gardener&out){for(auto&g:gards)if(g.alive&&g.g.id==id&&g.g.uid==uid){out=g.g;return true;}return false;}
 G*find(uint16_t id){for(auto&g:gards)if(g.g.id==id)return &g;return nullptr;}
 void order(G&g,int x,int y){g.tx=x;g.ty=y;g.working=false;g.g.centreX=x;g.g.centreY=y;g.g.aimX=g.g.aimY=g.g.reservedX=g.g.reservedY=-1;g.g.work=Work::Busy;g.g.escorted=false;g.g.destX=g.g.destY=-1;}
 // r33: WATCH: walk there (job 0xB0, no search on the way or on arrival), then wait; the work centre stays.
 int parkOrders=0;
 void park(const Campaign&,const Gardener&g,int x,int y){if(auto*f=find(g.id))if(f->alive){parkOrders++;f->tx=x;f->ty=y;f->working=false;f->g.aimX=f->g.aimY=f->g.reservedX=f->g.reservedY=-1;f->g.work=Work::Busy;f->g.escorted=true;f->g.destX=x;f->g.destY=y;}}
 Intrusion intr;void intrusion(const Campaign&,Intrusion&out){out=intr;}
 void sendGardener(const Campaign&,const Gardener&g,int x,int y){if(auto*f=find(g.id))if(f->alive)order(*f,x,y);}
 void moveGardeners(const Campaign&,const std::vector<Gardener>&gs,int x,int y){int k=0;for(auto&g:gs)if(auto*f=find(g.id))if(f->alive){auto&d=spiral[std::min<size_t>(k++,spiral.size()-1)];order(*f,x+2*d.first,y+2*d.second);}}
 void ring(const Target&t,std::vector<RingCell>&out){out.clear();for(auto&c:rings[t.id])out.push_back(RingCell{c.first,c.second,dark(c.first,c.second),soldierAt(c.first,c.second)});}
 bool beside(const Target&t,const RingCell&c,int&x,int&y){static const int nx[6]={1,-1,0,0,1,-1},ny[6]={0,0,1,-1,1,-1};for(int k=0;k<6;k++){int px=c.x+nx[k],py=c.y+ny[k];if(distance(px,py,t.x,t.y)==4){x=px;y=py;return true;}}return false;}
 // Soldiers on Dark ring cells step outside the footprint, never onto a cell within FireLine (as the bridge's outside()).
 void clearFootprint(const Campaign&,const Target&t,const std::vector<Soldier>&,const Fire&f){
  for(auto&u:army){bool on=false;for(auto&c:rings[t.id])if(c.first==u.s.x&&c.second==u.s.y&&dark(c.first,c.second))on=true;if(!on)continue;
   for(int r=1;r<=5&&on;r++)for(int dy=-r;dy<=r&&on;dy++)for(int dx=-r;dx<=r&&on;dx++){int x=u.s.x+dx,y=u.s.y+dy;if(distance(u.s.x,u.s.y,x,y)!=r||distance(x,y,t.x,t.y)<=3||inFire(x,y,f)||soldierAt(x,y))continue;u.tx=x;u.ty=y;on=false;}}}
 bool spare(const Target&t,int x,int y){return free(x,y)&&distance(x,y,t.x,t.y)>=7;}
 bool parked(const Target&){return false;}
 bool darkNear(const Campaign&,int x,int y,int r,const std::vector<Keep>&keep,int&dx,int&dy){bool found=false;int best=0;
  for(int py=y-r;py<=y+r;py++)for(int px=x-r;px<=x+r;px++){int d=distance(x,y,px,py);if(d>r||(found&&d>=best)||!dark(px,py)||kept(keep,px,py))continue;found=true;best=d;dx=px;dy=py;}return found;}
 bool ownDark(const Campaign&,int x,int y,int r,const std::vector<Keep>&keep,int&dx,int&dy){bool found=false;int best=0;
  for(auto&p:patch){int d=distance(x,y,p.first,p.second);if(d>r||(found&&d>=best)||!dark(p.first,p.second)||soldierAt(p.first,p.second)||kept(keep,p.first,p.second))continue;found=true;best=d;dx=p.first;dy=p.second;}return found;}
 void darkFighters(const Campaign&,std::vector<std::pair<int,int>>&out){out=fighters;}
 bool leads(const Campaign&,const Target&){return true;}
 bool call(const Campaign&c,uint32_t,Target&out){if(!calling||c.player==3)return false;out=callTarget;return true;}
 bool announce(const Campaign&,uint32_t,const Target&){return false;}
 int pullNotes=0;void pulled(const Campaign&,const Fire&,const std::vector<Soldier>&,const std::vector<Pulled>&who,int,int){pullNotes+=(int)who.size();} /* r36 */
 void report(Event e,const Campaign&c){if(e==Event::Launch)launches++;if(e==Event::Join)joins++;if(e==Event::Return){returns++;lastReturn=c.reason;}if(e==Event::Decline)lastDecline=c.reason;if(e==Event::Fire)fireEvents++;
  if(getenv("SOAK_LOG")&&e!=Event::Decline)fprintf(stderr,"  ev %d phase %d style %s target %u door %d,%d gardeners %d ring %u/%d value %d reason '%s'\n",(int)e,(int)c.phase,styleName(c.style),c.target.id,c.target.doorX,c.target.doorY,c.gardeners,c.progress,c.ringSize,c.value,c.reason);
  if((e==Event::Siege||e==Event::Stage||e==Event::Assault)&&c.target.id)sieged.push_back(c.target.id);}
};
static void soak(){
 int edge=0;
 for(int drift=0;drift<2;drift++)for(int style:{R,A,P,S,H,(int)Style::Allied}){
  Soak e;if(style==H)for(int i=0;i<4;i++)e.fighters.push_back({290+i,240});
  /* drift: four free gardeners start greening at Euclid 30 from the centre; their native search walks them inward */
  if(drift){const int at[4][2]={{Soak::CX+30,Soak::CY},{Soak::CX-30,Soak::CY+5},{Soak::CX,Soak::CY-30},{Soak::CX+30,Soak::CY+30}};for(int i=0;i<4;i++)e.order(e.gards[8+i],at[i][0],at[i][1]);}
  if(style==(int)Style::Allied){for(auto&b:e.world)if(b.id==36)e.callTarget=b;e.calling=true;}
  Campaign c=fresh(style);uint32_t t=0;
  for(;t<T0+9000;t++){e.tick(t);if(t%Period==0){if(t==T0){c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;}step(c,t,e);}}
  bool f20=false,f30=false,f36=false,f44=false;for(auto&b:e.world){if(b.id==20)f20=true;if(b.id==30)f30=true;if(b.id==36)f36=true;if(b.id==44)f44=true;}
  bool hit20=false,hit30=false;for(auto id:e.sieged){if(id==20)hit20=true;if(id==30)hit30=true;}
  edge+=drift?e.fireEvents:0;
  fprintf(stderr,"soak%s %s: deaths=%d launches=%d joins=%d collapsed=%d farms 20/30/36/44 standing %d%d%d%d fire events %d last return '%s' last decline '%s'\n",drift?" (drift)":"",styleName((Style)style),e.deaths,e.launches,e.joins,e.collapsed,f20,f30,f36,f44,e.fireEvents,e.lastReturn,e.lastDecline);
  assert(e.deaths==0&&f20&&!hit20&&!hit30&&(e.launches+e.joins)>=1);
  if(style==R||style==A||style==P)assert(!f36&&!f44); /* farms outside FarmLock still fall */
  if(style==(int)Style::Allied)assert(!f36);
 }
 assert(edge>0); /* the drift runs exercised the guard */
}
// r30 review: the Temple and a Fortress side by side (two overlapping fire zones), the home at several angles
// (on and between the hex axes) beyond the Fortress, a farm on the far side of the Temple. Every unit walks
// hex steps (its path bows off the straight line), both fires burn soldiers and gardeners: nobody dies and the
// far farm falls.
static void soakTwo(){
 const int homes[][2]={{400,202},{400,240},{380,140},{360,300},{330,110}};
 for(auto&h:homes)for(int style:{R,A,P}){
  Soak e;e.fortress(245,200);e.home(h[0],h[1]);
  for(size_t i=0;i<e.world.size();)if(e.world[i].id==36||e.world[i].id==44)e.world.erase(e.world.begin()+i);else i++;
  e.farm(50,150,202);
  Campaign c=fresh(style);uint32_t t=0;double low=1e9;
  for(;t<T0+12000;t++){e.tick(t);if(t%Period==0){if(t==T0){c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;}step(c,t,e);}for(auto&u:e.army)low=std::min(low,e.clearance(u.s.x,u.s.y));}
  bool f50=false,f20=false;for(auto&b:e.world){if(b.id==50)f50=true;if(b.id==20)f20=true;}
  fprintf(stderr,"soak two zones home %d,%d %s: deaths=%d launches=%d farm 50 %s, closest soldier %.1f, fire events %d, last return '%s'\n",h[0],h[1],styleName((Style)style),e.deaths,e.launches,f50?"standing":"fell",low,e.fireEvents,e.lastReturn);
  assert(e.deaths==0&&f20&&e.launches>=1&&!f50&&low>FireReach);
 }
}
// r32: every style grows with the army; today's sizes are the floor; gardeners grow with the soldiers a style takes.
static void scaling(){
 struct Row {int army,raid,hunt,escort,raidG,huntG,sweepG,assaultG,alliedG,wanted;};
 const Row rows[]={{10,10,10,8,4,2,10,8,6,12},{30,10,16,8,4,3,10,10,8,12},{60,20,30,15,5,4,11,12,10,14},{120,40,60,30,6,6,12,16,14,20}};
 for(auto&r:rows){
  assert(raidSize(r.army)==r.raid&&huntSize(r.army)==r.hunt&&escortSize(r.army)==r.escort);
  assert(gardenersFor(Style::Raid,r.raid)==r.raidG&&gardenersFor(Style::Hunt,r.hunt)==r.huntG&&gardenersFor(Style::Sweep,r.escort)==r.sweepG);
  assert(gardenersFor(Style::Assault,r.army)==r.assaultG&&gardenersFor(Style::Pincer,r.army)==r.assaultG&&gardenersFor(Style::Allied,r.army)==r.alliedG&&gardenersWanted(r.army)==r.wanted);}
 assert(raidSize(4)==4&&huntSize(3)==3&&escortSize(0)==0&&gardenersFor(Style::Assault,1000)==MaxGardeners&&gardenersWanted(1000)==MaxGardeners&&gardenersWanted(0)==GardenersWanted);
 // Recruitment and the toolsmith orders follow the larger number.
 assert(recruitOrder(12,0,0,5,true,nullptr,16)==1&&recruitOrder(16,0,0,5,true,nullptr,16)==0&&shovelTarget(12,0,0,16)==ShovelBatch&&shovelOrder(15,0,ShovelReserve,0,1,nullptr,16)==1);
 // Launches: armies of 10, 30, 60 and 120 (the pool minus the home reserve) take the scaled sizes, gardeners capped by what exists.
 for(int army:{10,30,60,120})for(int style:{R,A,P,S,H}){
  int pool=army+std::max(HomeReserve,army/5);while(pool-std::max(HomeReserve,pool/6)<army)pool++;while(pool-std::max(HomeReserve,pool/6)>army)pool--;
  if(pool-std::max(HomeReserve,pool/6)!=army)continue;
  Engine e(MaxGardeners);e.pool.clear();for(int i=0;i<pool;i++)e.pool.push_back(Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),40,40});
  e.add(1,Farm,140,40);for(int x=70;x<=150;x+=2)e.darkCells.push_back({x,40});if(style==H)for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,90});
  Campaign c=fresh(style);launch(c,e,T0,style);if(style==P&&army<PincerMin){assert(e.launches==0);continue;}
  int take=style==R?raidSize(army):style==H?huntSize(army):style==S?escortSize(army):army;
  int want=std::min(gardenersWanted(pool),gardenersFor((Style)style,take));
  if(e.launches!=1||(int)e.army.size()!=take||c.want!=want)fprintf(stderr,"scaling: army %d style %s launches %d took %d want %d/%d\n",army,styleName((Style)style),e.launches,(int)e.army.size(),c.want,want);
  assert(e.launches==1&&(int)e.army.size()==take&&c.want==want&&c.gardeners==(style==H?0:want));}
 // Variety stays for large armies: every style shows up over many decisions with 120 soldiers.
 {int seen[6]={};for(int k=0;k<150;k++){Engine e(MaxGardeners);e.pool.clear();for(int i=0;i<144;i++)e.pool.push_back(Soldier{(uint16_t)(100+i),(uint16_t)(29+i%2),(uint32_t)(1000+i),40,40});
   e.add(1,Farm,140,40);e.add(2,Farm,150,70);for(int x=70;x<=90;x+=2)e.darkCells.push_back({x,50});for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,90});
   Campaign c=fresh();c.random.state=0x7654321u+k*7919u;step(c,T0,e);c.random.state^=k*2654435761u;c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,T0+Period,e);if(e.launches)seen[(int)c.style]++;}
  for(int s:{R,A,P,S,H})assert(seen[s]>=5);}
}
// r32/r33/r35: the convoy in the walking soak (soldiers one cell per 6 ticks, gardeners per 6 or 9 ticks, armies of 34 and 117).
// r35 bounds (user: the army must move fast, not babysit its gardeners): on a march the army never stands still for more
// than StallLooks looks in a row away from its order's cell (r33 held it for its gardeners, field: thousands of ticks);
// it enters the assault at most StageWait + ArriveWindow ticks after it stood at the staging cell; once a gardener has
// joined the army at the target (within EscortRadius) it stays within EscortRadius + GapSlack of the nearest soldier
// unless it greens a ring cell; no gardener is ever released away from home; nobody burns.
static void convoySoak(){
 const int GapSlack=4,StallLooks=3;const uint32_t ArriveWindow=150;
 for(int slow:{6,9})for(int pool:{40,140})for(int style:{R,A,P,S,H,(int)Style::Allied}){
  Soak e;e.gardenerEvery=slow;e.soldiers(pool);if(style==H)for(int i=0;i<4;i++)e.fighters.push_back({290+i,240});
  if(style==(int)Style::Allied){for(auto&b:e.world)if(b.id==36)e.callTarget=b;e.calling=true;}
  Campaign c=fresh(style);Phase last=Phase::Idle;uint32_t thereAt=0;int maxGap=0,maxAlone=0,assaults=0,apart=0,maxSize=0,maxHeld=0,stall=0,maxStall=0,bx=-1,by=-1;uint32_t wait=0;std::set<uint16_t> joined;
  for(uint32_t t=0;t<T0+12000;t++){e.tick(t);if(t%Period)continue;if(t==T0){c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;}
   {std::vector<Soldier> b;if(c.phase==Phase::March&&!c.split&&!walking(c)&&!thereAt&&e.members(c,b)&&!b.empty()&&within(b,c.stageX,c.stageY,10)*10>=(int)b.size()*7)thereAt=t;} /* the army stands at the staging cell */
   step(c,t,e);std::vector<Soldier> m;if(!e.members(c,m))m.clear();
   std::vector<Gardener> gs;for(int i=0;i<c.gardeners;i++){Gardener g;if(e.gardener(c,c.gid[i],c.guid[i],g))gs.push_back(g);}
   maxSize=std::max(maxSize,(int)m.size());maxHeld=std::max(maxHeld,(int)gs.size());
   if(c.phase!=last){if(last==Phase::March&&c.phase==Phase::Assault){assaults++;if(thereAt)wait=std::max(wait,t-thereAt);std::vector<Soldier> h,k,core;halves(m,h,k);body(c.split&&!h.empty()?h:m,&core);if(!gs.empty()&&spanOf(gs,core)>convoyLimit((int)gs.size())+2)apart++;}
    thereAt=0;last=c.phase;stall=0;bx=by=-1;joined.clear();}
   if(m.empty())continue;std::vector<Soldier> core;Army a=body(m,&core);
   if(c.phase==Phase::March&&!c.split&&!c.replanAt){ /* standing still away from its order's cell */
    bool still=bx>=0&&distance(a.x,a.y,bx,by)<1&&distance(a.x,a.y,c.goalX,c.goalY)>10;stall=still?stall+1:0;maxStall=std::max(maxStall,stall);
    if(still&&stall>StallLooks&&getenv("SOAK_LOG"))fprintf(stderr,"  t=%u stalled %d at %d,%d goal %d,%d\n",t,stall,a.x,a.y,c.goalX,c.goalY);}
   bx=a.x;by=a.y;
   if(gs.empty())continue;maxGap=std::max(maxGap,spanOf(gs,core));
   if(c.phase==Phase::Assault&&c.target.id){std::vector<RingCell> ring;e.ring(c.target,ring);if(c.split)core=m; /* a pincer: either half */
    for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){int mine=c.gcx[i]>=0?ringIndex(ring,c.gcx[i],c.gcy[i]):-1;if(mine>=0&&ring[mine].dark)continue;int d=reachOf(core,gs[i].x,gs[i].y);
     if(d<=EscortRadius)joined.insert(gs[i].id);if(!joined.count(gs[i].id))continue;maxAlone=std::max(maxAlone,d);
     if(d>EscortRadius+GapSlack&&getenv("SOAK_LOG"))fprintf(stderr,"  t=%u alone %d gardener %u at %d,%d work %d escorted %d cell %d,%d\n",t,d,gs[i].id,gs[i].x,gs[i].y,(int)gs[i].work,(int)gs[i].escorted,c.gcx[i],c.gcy[i]);}}}
  fprintf(stderr,"convoy soak %s gardeners 1/%d ticks, army %d, held %d: deaths=%d collapsed=%d max gap %d, max stall %d looks, max alone %d, assaults %d, stage wait %u, apart %d, parks %d\n",styleName((Style)style),slow,maxSize,maxHeld,e.deaths,e.collapsed,maxGap,maxStall,maxAlone,assaults,wait,apart,e.parkOrders);
  assert(e.deaths==0&&maxStall<=StallLooks&&maxAlone<=EscortRadius+GapSlack&&wait<=StageWait+ArriveWindow&&e.collapsed>=1&&(style==S||assaults>=1)&&e.parkOrders>0);}
}
// r32/r33/r35: the convoy in the teleport engine. r35: the army never holds or walks back for its gardeners: each leg
// is one order to its end, the gardeners follow parked (WATCH) to cells there and are never dropped or sent home alone;
// only the staging cell waits for stragglers, StageWait ticks at most ("assault before the gardeners").
struct Stubborn:Engine {using Engine::Engine;uint16_t deaf=300;
 void sendGardener(const Campaign&c,const Gardener&g,int x,int y){if(g.id==deaf){gardenerOrders++;return;}Engine::sendGardener(c,g,x,y);}
 void park(const Campaign&c,const Gardener&g,int x,int y){if(g.id==deaf){parkOrders++;return;}Engine::park(c,g,x,y);}};
static void convoyRules(){
 // One deaf straggler far behind: the army marches on, waits at the staging cell at most StageWait, then assaults with
 // the others; the straggler stays held (no drop, no lone order home) and the farm falls.
 {Stubborn e(6);e.gards[0].g.x=e.gards[0].g.centreX=10;e.add(1,Farm,200,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.gardeners==6);
  uint32_t t=until(c,e,T0+Period,Phase::Assault,StageWait+600);
  assert(c.phase==Phase::Assault&&!strcmp(e.lastConvoy,"assault before the gardeners")&&c.gardeners==6&&e.avoids==0);
  run(c,e,t,3000);assert(e.collapsed==1);}
 // One order per leg: the army's first order is its leg end (far beyond ConvoyLead), re-sent only when the leg changes.
 {Engine e(6);e.add(1,Farm,240,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&!e.goals.empty());
  assert(distance(e.goals.back().first,e.goals.back().second,40,40)>3*ConvoyLead&&distance(e.goals.back().first,e.goals.back().second,c.stageX,c.stageY)<=SnapRing);
  size_t k=e.goals.size();until(c,e,T0+Period,Phase::Assault);assert(c.phase==Phase::Assault&&e.goals.size()-k<=2&&e.parkOrders>0);}
 // Hunt hand-over: no rally back to the gardeners; the army marches on, the gardeners are ordered after it, the farm falls.
 {Engine e;e.add(1,Farm,60,90);for(int i=0;i<4;i++)e.darkUnits.push_back({86+i,40});Campaign c=fresh();launch(c,e,T0,H);
  uint32_t t=run(c,e,T0+Period,Period*20);e.darkUnits.clear();int before=e.gardenerOrders+e.parkOrders;
  for(;t<T0+6000&&e.gardenerOrders+e.parkOrders==before;t+=Period)step(c,t,e);
  assert(e.gardenerOrders+e.parkOrders>before&&strcmp(e.lastConvoy,"rally to the gardeners"));
  run(c,e,t,4000);assert(e.collapsed==1);}
 // r32 review: Dark fighters on the approach side (between the staging cell and the door): the screen stands on
 // their side, the gardeners wait behind it: the rear point and every gardener order of the assault lie farther
 // from the nearest fighter than the screen cell.
 {struct Log:Engine{std::vector<std::pair<int,int>> sent;void sendGardener(const Campaign&c,const Gardener&g,int x,int y){sent.push_back({x,y});Engine::sendGardener(c,g,x,y);}
   void park(const Campaign&c,const Gardener&g,int x,int y){sent.push_back({x,y});Engine::park(c,g,x,y);}};
  Log e;e.add(1,Farm,100,40);for(int i=0;i<3;i++)e.darkUnits.push_back({88,42+i});Campaign c=fresh(A);launch(c,e,T0,A);
  uint32_t t=T0+Period;size_t from=0;while(c.phase==Phase::March&&t<T0+4000){from=e.sent.size();step(c,t,e);t+=Period;}assert(c.phase==Phase::Assault);
  auto nearest=[&](int x,int y){int b=1<<30;for(auto&d:e.darkUnits)b=std::min(b,distance(x,y,d.first,d.second));return b;};
  int screen=nearest(c.goalX,c.goalY);assert(screen<nearest(c.target.doorX,c.target.doorY)&&c.rearX>=0&&nearest(c.rearX,c.rearY)>screen);
  run(c,e,t,600);assert(e.sent.size()>from);
  for(size_t i=from;i<e.sent.size();i++){if(nearest(e.sent[i].first,e.sent[i].second)<=screen)fprintf(stderr,"rear: order %d,%d nearer the threat than the screen %d,%d\n",e.sent[i].first,e.sent[i].second,c.goalX,c.goalY);assert(nearest(e.sent[i].first,e.sent[i].second)>screen);}}
 // r32 review case (fire centre 200,200, leg 259,180 -> 239,209, army of 20 at 240,196): the army goes to the leg end
 // along a clear leg; a leg end behind the fire is refused (false: the caller plans again).
 {Engine e(6);e.army.clear();for(int i=0;i<20;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),240,196});
  Fire f;f.on=true;Zone z;z.x=200;z.y=200;f.z.push_back(z);Campaign c=fresh(A);c.phase=Phase::March;c.squad=0x1234;
  for(int i=0;i<6;i++){e.gards[i].g.x=e.gards[i].g.centreX=240;e.gards[i].g.y=e.gards[i].g.centreY=196;c.gid[i]=e.gards[i].g.id;c.guid[i]=e.gards[i].g.uid;c.gcx[i]=c.gcy[i]=c.gdx[i]=c.gdy[i]=-1;}c.gardeners=6;
  assert(legClear(240,196,239,209,f,legKeep(20))&&!closer(240,196,f,soldierKeep(20)));
  assert(convoy(c,T0,e,e.army,f,259,180,239,209,false)&&!e.goals.empty()&&e.goals.back()==std::make_pair(239,209)&&e.parkOrders==6&&!c.waitSince);
  assert(!legClear(240,196,160,200,f,legKeep(20)));size_t k=e.goals.size();assert(!convoy(c,T0+ConvoyEvery,e,e.army,f,240,196,160,200,false)&&e.goals.size()==k);}
 // r35: a gardener whose straight way to the leg end passes the fire walks to the latest route point it reaches clear.
 {Engine e(2);e.army.clear();for(int i=0;i<10;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),160,250});
  Fire f;f.on=true;Zone z;z.x=200;z.y=200;f.z.push_back(z);Campaign c=fresh(A);c.phase=Phase::March;c.squad=0x1234;c.legX=150;c.legY=150;c.vx[0]=150;c.vy[0]=250;c.vias=1;c.viaAt=1;
  for(int i=0;i<2;i++){c.gid[i]=e.gards[i].g.id;c.guid[i]=e.gards[i].g.uid;c.gcx[i]=c.gcy[i]=c.gdx[i]=c.gdy[i]=-1;}c.gardeners=2;
  e.gards[0].g.x=e.gards[0].g.centreX=155;e.gards[0].g.y=e.gards[0].g.centreY=245; /* near the waypoint: the leg end is clear */
  e.gards[1].g.x=e.gards[1].g.centreX=150;e.gards[1].g.y=e.gards[1].g.centreY=150; /* still at the route start: straight on would cross the fire */
  assert(legClear(155,245,250,250,f,GardenerKeep)&&!legClear(150,150,250,250,f,GardenerKeep)&&legClear(150,150,150,250,f,GardenerKeep));
  assert(convoy(c,T0,e,e.army,f,150,250,250,250,false));
  assert(distance(c.gdx[0],c.gdy[0],250,250)<=4&&distance(c.gdx[1],c.gdy[1],150,250)<=4);}
 // r33: gardeners that stop answering are never sent home alone (r32 did): their last order is a park.
 struct Deaf:Engine{using Engine::Engine;bool on=false;std::map<uint16_t,std::pair<int,int>> last,forward;
  void sendGardener(const Campaign&c,const Gardener&g,int x,int y){if(on){gardenerOrders++;forward[g.id]={x,y};return;}Engine::sendGardener(c,g,x,y);}
  void park(const Campaign&c,const Gardener&g,int x,int y){if(on){parkOrders++;last[g.id]={x,y};return;}Engine::park(c,g,x,y);}};
 // At home: every gardener 100 tiles away and deaf: the army does not wait at home; it marches, waits StageWait at the
 // staging cell and assaults; no gardener is dropped or ordered home alone, every one got a park order.
 {Deaf e(6);e.on=true;for(auto&f:e.gards){f.g.x=f.g.centreX=40;f.g.y=f.g.centreY=140;}e.add(1,Farm,200,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.gardeners==6);
  until(c,e,T0+Period,Phase::Assault,StageWait+600);
  assert(c.phase==Phase::Assault&&c.gardeners==6&&!strcmp(e.lastConvoy,"assault before the gardeners")&&e.forward.empty());
  for(auto&f:e.gards)assert(e.last.count(f.g.id));}
 // In the field: the gardeners stop answering 30 tiles behind the army: it goes on to the target without them (r33 held
 // and walked back); nobody is dropped or sent anywhere but home with FORWARD.
 {Deaf e(6);e.add(1,Farm,240,40);Campaign c=fresh(A);launch(c,e,T0,A);uint32_t t=T0+Period;
  while(c.phase==Phase::March&&summary(e.army).x<100&&t<T0+4000){step(c,t,e);t+=Period;}assert(c.phase==Phase::March);
  int x0=summary(e.army).x;e.on=true;for(auto&f:e.gards){f.g.x=f.g.centreX=x0-30;f.g.y=f.g.centreY=40;f.g.work=Work::Idle;}
  until(c,e,t,Phase::Assault,StageWait+600);assert(c.phase==Phase::Assault&&c.gardeners==6&&distance(summary(e.army).x,summary(e.army).y,240,40)<=2*StageDistance);
  for(auto&f:e.gards)if(e.forward.count(f.g.id))assert(e.forward[f.g.id]==std::make_pair(40,40));}
}
// r33: a walking engine for the field numbers: every director step each soldier and gardener walks one cell towards its
// order (soldiers to click + 2*spiral[k] as the group order puts them); a cell holding a soldier is not standable.
struct Walker:Engine {using Engine::Engine;std::map<uint16_t,std::pair<int,int>> to;std::vector<std::pair<int,int>> spiral;int dropsHome=0,lone=0;
 Walker(int g):Engine(g){for(int dy=-6;dy<=6;dy++)for(int dx=-6;dx<=6;dx++)if(distance(0,0,dx,dy)<=4)spiral.push_back({dx,dy});std::stable_sort(spiral.begin(),spiral.end(),[](const std::pair<int,int>&a,const std::pair<int,int>&b){return dtfire::fire2(a.first,a.second,0,0)<dtfire::fire2(b.first,b.second,0,0);});}
 static void stepTo(int&x,int&y,int tx,int ty){if(x==tx&&y==ty)return;static const int nx[6]={1,-1,0,0,1,-1},ny[6]={0,0,1,-1,1,-1};int best=distance(x,y,tx,ty),bx=x,by=y;for(int k=0;k<6;k++){int d=distance(x+nx[k],y+ny[k],tx,ty);if(d<best){best=d;bx=x+nx[k];by=y+ny[k];}}x=bx;y=by;}
 void move(const Campaign&,const std::vector<Soldier>&m,int x,int y,bool home)override{moves++;if(home)homeMoves++;goals.push_back({x,y});size_t k=0;for(auto&o:m)for(auto&s:army)if(o.id==s.id){auto&d=spiral[std::min(k++,spiral.size()-1)];to[s.id]={x+2*d.first,y+2*d.second};}}
 bool stand(const Campaign&,int x,int y,int){for(auto&s:army)if(s.x==x&&s.y==y)return false;return x>=0&&y>=0;}
 void order(const Gardener&g,int x,int y,bool parked){auto*f=find(g.id);to[g.id]={x,y};f->g.escorted=parked;f->g.destX=parked?x:-1;f->g.destY=parked?y:-1;f->g.work=Work::Busy;if(!parked){f->g.centreX=x;f->g.centreY=y;}
  /* an order far from every soldier and not at home: a lone walk */
  int b=1<<30;for(auto&s:army)b=std::min(b,distance(s.x,s.y,x,y));if(b>convoyLimit(10)+ConvoyLead&&distance(x,y,str.homeX,str.homeY)>GatherHome)lone++;}
 void sendGardener(const Campaign&,const Gardener&g,int x,int y){gardenerOrders++;order(g,x,y,false);if(distance(x,y,str.homeX,str.homeY)<=1)dropsHome++;}
 void park(const Campaign&,const Gardener&g,int x,int y){parkOrders++;order(g,x,y,true);}
 void walk(){for(auto&s:army)if(to.count(s.id))stepTo(s.x,s.y,to[s.id].first,to[s.id].second);
  for(auto&f:gards)if(f.alive&&to.count(f.g.id)){stepTo(f.g.x,f.g.y,to[f.g.id].first,to[f.g.id].second);if(f.g.x==to[f.g.id].first&&f.g.y==to[f.g.id].second&&f.g.escorted)f.g.work=Work::Idle;}}
};
// r33 field numbers (pid 13612): ten gardeners (limit 14) 15..18 behind an army of 20 that stands on its formation
// cells. r35: the army never waits for them (r33 held it): its first convoy look orders it on to the leg end, every
// gardener gets its own cell there within ConvoyEvery + ConvoyResend ticks, nobody is dropped.
static void convoyField(){
 {Walker e(10);e.army.clear();for(int i=0;i<20;i++){Soldier s{(uint16_t)(500+i),29,(uint32_t)(5000+i),100,40};e.army.push_back(s);}
  Campaign c=fresh(A);c.armed=true;c.phase=Phase::March;c.squad=0x1234;c.initial=20;c.style=Style::Assault;c.target=Target{};e.add(1,Farm,240,40);c.target=e.world[0];
  e.move(c,e.army,100,40,false);for(int k=0;k<12;k++)e.walk(); /* the army on its formation cells around 100,40 */
  const int gaps[10]={15,16,17,18,15,16,17,18,16,17};for(int i=0;i<10;i++){auto&g=e.gards[i].g;g.x=g.centreX=100-gaps[i]-2;g.y=g.centreY=40+(i%3);g.work=Work::Idle;c.gid[i]=g.id;c.guid[i]=g.uid;c.gcx[i]=c.gcy[i]=c.gdx[i]=c.gdy[i]=-1;}c.gardeners=10;
  c.legX=40;c.legY=40;c.stageX=200;c.stageY=40;c.since=T0;c.started=T0;c.targetSince=T0;c.goalX=100;c.goalY=40;c.orderAt=T0;c.lastOrder=T0;c.convoyAt=0;
  std::vector<Gardener> gs;living(c,e,gs);std::vector<Soldier> core;body(e.army,&core);int gap0=spanOf(gs,core),limit=convoyLimit(10);assert(limit==14&&gap0>=15&&gap0<=20);
  std::map<uint16_t,uint32_t> ordered;uint32_t t=T0;bool moved=false;int maxHeld=0;
  for(;t<T0+3000&&!moved;t+=Period){Fire none;int before=(int)e.goals.size();if(t-c.convoyAt>=ConvoyEvery)convoy(c,t,e,e.army,none,40,40,200,40,false);e.walk();
   living(c,e,gs);body(e.army,&core);int gap=spanOf(gs,core);maxHeld=std::max(maxHeld,c.gardeners);
   for(int i=0;i<c.gardeners;i++)if(c.gsent[i]&&!ordered.count(c.gid[i]))ordered[c.gid[i]]=c.gsent[i];
   assert(!c.waitSince);(void)gap;
   if((int)e.goals.size()>before&&e.goals.back().first>105)moved=true;} /* on to the leg end */
  assert(ordered.size()==10&&c.gardeners==10&&moved&&t<=T0+ConvoyEvery+Period&&e.goals.back()==std::make_pair(200,40));for(auto&o:ordered)assert(o.second<=T0+ConvoyEvery+ConvoyResend);}
 // Way home with the gardeners 57..78 behind (field: three campaigns ended so): no gardener is dropped or sent home alone;
 // r35: the army walks home at full speed and waits for them there, all reach home.
 {Walker e(10);e.army.clear();for(int i=0;i<20;i++){Soldier s{(uint16_t)(500+i),29,(uint32_t)(5000+i),100,40};e.army.push_back(s);}
  Campaign c=fresh(A);c.armed=true;c.phase=Phase::Assault;c.squad=0x1234;c.initial=20;c.style=Style::Assault;e.add(1,Farm,240,40);c.target=e.world[0];
  e.move(c,e.army,100,40,false);for(int k=0;k<12;k++)e.walk();
  const int gaps[10]={57,60,63,66,69,72,75,78,58,77};for(int i=0;i<10;i++){auto&g=e.gards[i].g;g.x=g.centreX=100+gaps[i];g.y=g.centreY=40+(i%3);g.work=Work::Working;c.gid[i]=g.id;c.guid[i]=g.uid;c.gcx[i]=c.gcy[i]=c.gdx[i]=c.gdy[i]=-1;}c.gardeners=10;
  uint32_t t=T0;startReturn(c,t,e,e.army,"test");assert(c.phase==Phase::Return);int held=10;
  for(t+=Period;c.phase==Phase::Return&&t<T0+3*HomeTimeout;t+=Period){step(c,t,e);e.walk();if(c.phase==Phase::Return)held=std::min(held,c.gardeners);}
  int home=0;for(auto&f:e.gards)if(distance(f.g.x,f.g.y,40,40)<=GatherHome)home++;
  if(held!=10||e.lone||home<10)fprintf(stderr,"field return: held %d lone %d home %d phase %d last convoy '%s'\n",held,e.lone,home,(int)c.phase,e.lastConvoy);
  assert(c.phase==Phase::Idle&&held==10&&e.lone==0&&home==10&&e.dissolved==1);}
}
// r33: the computer defends its land. Engine: ownCells are the computer's own Dark cells (ownDark), intr what the bridge
// reports (units in or near the territory, owned cells and their rise).
static bool sawGoalNear(const Engine&e,size_t from,int x,int y,int r){for(size_t i=from;i<e.goals.size();i++)if(distance(e.goals[i].first,e.goals[i].second,x,y)<=r)return true;return false;}
static void defending(){
 // An intrusion (own Dark cells and a Dark Gardener in the territory) starts a defence: soldiers to the Dark unit, the
 // gardeners reclaim the own land behind them area by area; land clean: home, the gardeners released there.
 {Engine e(8);for(int x=80;x<=100;x+=2)for(int y=56;y<=64;y+=2)e.ownCells.push_back({x,y});e.intr.own=(int)e.ownCells.size();e.intr.gardeners={{90,60}};
  Campaign c=fresh();uint32_t t=run(c,e,10,ReclaimStart+DefendLook*2);
  assert(e.defends>=1&&!strcmp(e.firstDefend,"start: intrusion")&&c.style==Style::Defend&&c.phase!=Phase::Idle&&c.gardeners>0&&c.threatUnits==1);
  std::vector<uint16_t> held(c.gid,c.gid+c.gardeners);t=run(c,e,t,600);assert(sawGoalNear(e,0,90,60,3)); /* the soldiers stand next to the Dark Gardener */
  e.intr.gardeners.clear();t=run(c,e,t,DefendTime);
  assert(e.ownCells.empty()&&!strcmp(e.lastReturn,"land clean")&&c.phase==Phase::Idle&&e.dissolved==1&&c.defendEnd>0&&e.launches==0);
  for(auto&f:e.gards)if(std::find(held.begin(),held.end(),f.g.id)!=held.end())assert(distance(f.g.x,f.g.y,40,40)<=GatherHome+2);} /* the defence's gardeners came home with the army */
 // Minor intrusion during an attack: the attack goes on (free gardeners reclaim); r37: land alone (200 own cells) never
 // recalls it either; Dark fighters near home do.
 /* r35: the teleport engine ends a march in one step; a deaf straggler keeps the army at the staging cell (StageWait) */
 {Stubborn e(10);e.gards[0].g.x=e.gards[0].g.centreX=10;e.add(1,Farm,240,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.style==Style::Assault);
  for(int x=60;x<=70;x+=2)for(int y=60;y<=64;y+=2)e.ownCells.push_back({x,y});e.intr.own=30;uint32_t t=run(c,e,T0+Period,DefendLook*3);
  assert(c.style==Style::Assault&&e.defends==0&&c.phase==Phase::March);
  e.intr.own=SeriousCells+50;t=run(c,e,t,DefendLook*2);assert(c.style==Style::Assault&&e.defends==0&&c.phase==Phase::March);
  e.intr.fighters={{50,44}};t=run(c,e,t,DefendLook*2);assert(c.style==Style::Defend&&!strcmp(e.firstDefend,"recall: serious intrusion")&&c.gardeners>0);
  /* not twice within RecallGap: a second attack launched now is not recalled at once */}
 // A siege two thirds green is finished first; a recall needs the army to outweigh the Dark fighters.
 {Engine e(8);e.add(1,Farm,100,40);Campaign c=fresh(A);launch(c,e,T0,A);std::vector<Soldier> m=e.army;e.intr.own=SeriousCells+10;e.intr.fighters={{50,44}};for(int x=50;x<=56;x+=2)e.ownCells.push_back({x,50});
  c.phase=Phase::Assault;c.ringSize=9;c.ringDark=3;c.lookAt=0;assert(!answer(c,T0+DefendLook*4,e,m)&&c.style==Style::Assault);
  c.ringDark=4;c.lookAt=0;assert(answer(c,T0+DefendLook*5,e,m)&&c.style==Style::Defend);}
 // Dark fighters heavier than the whole army: no defence ("declined: threat too strong").
 {Engine e(8);for(int i=0;i<50;i++)e.intr.fighters.push_back({60+i%5,40+i/5});Campaign c=fresh();run(c,e,10,ReclaimStart+DefendLook*2);
  assert(e.defends>=1&&!strcmp(e.lastDefend,"declined: threat too strong")&&c.phase==Phase::Idle);}
 // A minor intrusion is defended once, then attacks come between (no second minor defence within 4*DefendRest).
 {Engine e(8);for(int x=80;x<=100;x+=2)for(int y=56;y<=64;y+=2)e.ownCells.push_back({x,y});e.intr.own=(int)e.ownCells.size();e.cfg.mode=0;
  Campaign c=fresh();uint32_t t=run(c,e,10,ReclaimStart+DefendLook*2);assert(c.style==Style::Defend);t=run(c,e,t,DefendTime+HomeTimeout);assert(c.phase==Phase::Idle);
  for(int x=80;x<=100;x+=2)e.ownCells.push_back({x,70});int starts=0;e.defends=0;run(c,e,t,DefendRest*2);assert(c.phase==Phase::Idle&&strncmp(e.lastDefend,"start",5));(void)starts;}
 // Never into the fire field: Dark units within HuntKeep of a live fire centre and own Dark land within FrontKeep are left
 // alone; a Dark Gardener outside is defended against with every order outside its keep.
 {Engine e(8);e.add(1,Temple,100,40);e.add(2,Farm,300,300,false);e.intr.gardeners={{110,44}};for(int x=96;x<=110;x+=2)e.ownCells.push_back({x,48});e.intr.own=SeriousCells+1;
  Fire f=e.fireNow();assert(f.on&&closer(110,44,f,HuntKeep));e.checkKeep=true;Campaign c=fresh();uint32_t t=run(c,e,10,ReclaimStart+DefendLook*3);assert(c.phase==Phase::Idle&&e.defends==0);
  e.intr.gardeners.push_back({160,44});t=run(c,e,t,DefendLook*2);assert(c.style==Style::Defend&&c.phase!=Phase::Idle);
  assert(!strcmp(e.firstDefend,"start: intrusion")); /* r33 review: 151 owned cells, none reclaimable (all within FrontKeep): not serious */
  run(c,e,t,3000);assert(e.keepViolations==0);for(auto&g:e.goals)assert(!closer(g.first,g.second,f,HuntKeep-spread(40)));}
}
// r37: land alone never keeps the army home (field, The Alliance: thousands of own Dark cells from the start, 3 attacks in
// 112,000 ticks): with a farm in reach the attacks come first and a land-only defence comes LandDefendGap after the first
// attack time, with an escort only (escortSize), for LandDefendTime at most; a sector that cannot attack defends at once.
static void landFirst(){
 {Engine e(12);e.add(1,Farm,140,40);for(int x=80;x<=120;x+=2)for(int y=60;y<=80;y+=2)e.ownCells.push_back({x,y});e.intr.own=SeriousCells*10;
  Campaign c=fresh();uint32_t t=10;int landDefences=0,maxDefArmy=0;uint32_t firstDefence=0,firstLaunch=0,lastLaunch=0;
  for(;t<firstAttack(1)+LandDefendGap+LandDefendTime+4000;t+=Period){bool idle=c.phase==Phase::Idle;step(c,t,e);
   if(idle&&c.phase!=Phase::Idle){if(c.style==Style::Defend){landDefences++;maxDefArmy=std::max(maxDefArmy,(int)e.army.size());if(!firstDefence)firstDefence=t;}else{lastLaunch=t;if(!firstLaunch)firstLaunch=t;}}
   if(c.phase==Phase::Idle&&e.world.empty())e.add((uint16_t)(100+e.collapsed),Farm,140,40);} /* a new farm for every campaign (a new id: the engine keeps the greened ring per id) */
  fprintf(stderr,"land first: launch %u last %u defence %u (allowed from %u) defences %d army %d launches %d declines %d last decline %s last defend %s last return %s phase %d avoids %d last avoid %s collapsed %d\n",firstLaunch,lastLaunch,firstDefence,firstAttack(1)+LandDefendGap,landDefences,maxDefArmy,e.launches,e.declines,e.lastDecline,e.lastDefend,e.lastReturn,(int)c.phase,e.avoids,e.lastAvoid,e.collapsed);
  assert(firstLaunch&&firstDefence>firstLaunch&&firstDefence>=firstAttack(1)+LandDefendGap&&landDefences>=1&&maxDefArmy<=escortSize(34)&&e.launches>=2);}
 // Without an attack option (no target) the land is defended at once, with the escort.
 {Engine e(12);for(int x=80;x<=120;x+=2)for(int y=60;y<=80;y+=2)e.ownCells.push_back({x,y});e.intr.own=SeriousCells*10;Campaign c=fresh();
  uint32_t t=run(c,e,10,ReclaimStart+DefendLook*3);assert(c.style==Style::Defend&&c.phase!=Phase::Idle&&(int)e.army.size()<=escortSize(34)&&!strcmp(e.firstDefend,"start: serious intrusion"));
  run(c,e,t,LandDefendTime+HomeTimeout);assert(c.phase==Phase::Idle&&(!strcmp(e.lastReturn,"defence time")||!strcmp(e.lastReturn,"land clean")));}
}
// r37: a march is late by its budget or by no progress, never by a fixed 4,000 ticks; a failed target is replaced from where
// the army stands instead of walked home from.
static void marchBudget(){
 assert(legBudget(0)==MarchTimeout&&legBudget(300)==MarchTimeout+300*TicksPerHex&&targetBudget(100)==TargetTimeout+100*TicksPerHex);
 {Engine e;e.army.clear();for(int i=0;i<10;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),40,40});Campaign c=fresh(A);c.since=T0;
  assert(!stalled(c,T0+MarchTimeout+100,e,e.army,40,40,340,40)); /* a 300-hex leg has 9,500 ticks */
  for(int k=1;k<=3;k++){for(auto&s:e.army)s.x=40+10*k;assert(!stalled(c,T0+MarchTimeout+100+k*(NoProgress-100),e,e.army,40,40,340,40));} /* progressing, within the 9,500-tick budget */
  uint32_t t=T0+MarchTimeout+100+3*(NoProgress-100);assert(!stalled(c,t+NoProgress-10,e,e.army,40,40,340,40)&&stalled(c,t+NoProgress+10,e,e.army,40,40,340,40)); /* stuck */
  for(auto&s:e.army)s.x=200;assert(!stalled(c,t+NoProgress+20,e,e.army,40,40,340,40)); /* moved on: the record restarts */
  c.since=T0;assert(stalled(c,T0+legBudget(300)+1,e,e.army,40,40,340,40));} /* the budget */
 // r38 (field pid 3832): the walk loops around a lake, so the hex distance to the goal grows while the walked steps shrink:
 // progress is the walked distance, and the budget follows the walked length (600 steps for a 300-hex leg: 17,000 ticks).
 {Engine e;e.army.clear();for(int i=0;i<10;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),40,40});Campaign c=fresh(A);c.since=T0;
  e.walkHook=[](int fx,int fy,int tx,int ty){(void)fy;(void)tx;(void)ty;if(fx==40)return 600;return std::max(1,600-(int)((long)(fx-40)*2));}; /* every 10 tiles of x walked (the detour) count 20 steps */
  assert(legBudget(600)==MarchTimeout+600*TicksPerHex);
  uint32_t t=T0;for(int k=1;k<=5;k++){for(auto&s:e.army){s.x=40+10*k;s.y=40+30*k;}t=T0+k*(NoProgress-100);assert(!stalled(c,t,e,e.army,40,40,340,40)&&distance(40+10*k,40+30*k,340,40)>distance(40,40,340,40));} /* the hex distance grows, the walk shrinks: never late */
  for(auto&s:e.army){s.x=100;s.y=200;}assert(!stalled(c,T0+legBudget(300)+100,e,e.army,40,40,340,40)&&stalled(c,T0+legBudget(600)+10,e,e.army,40,40,340,40));} /* still progressing past the hex budget; the walked length sets the budget */
 // A march that does not reach its staging cell takes the next farm from where it stands (Stuck: no order is executed, so
 // IgnoreLimit fails the first target); the second farm is attacked without a walk home.
 {Stuck s;s.add(1,Farm,100,40);s.add(2,Farm,100,120);Campaign d=fresh();launch(d,s,T0,A);assert(d.target.id==1);
  uint32_t t=run(d,s,T0+Period,MarchTimeout);assert(d.target.id==2&&d.phase!=Phase::Return&&d.phase!=Phase::Idle&&s.homeMoves==0&&!strcmp(s.lastAvoid,"orders not executed"));(void)t;}
 // The target budget: a siege that never greens (Dark fighters hold the door, the gardeners wait) fails the target after
 // targetBudget(hex) and the army takes the next farm from where it stands, no walk home.
 {Engine s;s.add(1,Farm,100,40);s.add(2,Farm,100,120);s.darkUnits.push_back({99,43});Campaign d=fresh();launch(d,s,T0,A);assert(d.target.id==1);
  uint32_t t=run(d,s,T0+Period,targetBudget(d.targetHex)-Period);assert(d.phase==Phase::Assault&&d.target.id==1&&s.avoids==0&&s.collapsed==0);
  t=run(d,s,t,Period*4);assert(d.target.id==2&&!strcmp(s.lastAvoid,"target not reached")&&s.homeMoves==0&&d.phase!=Phase::Return);run(d,s,t,3000);assert(s.collapsed==1);}
}
// r33: the regression of pid 13612 (no OFFENCE_RECLAIM in a whole game): own Dark land 194 hex from the idle gardeners at
// home is reclaimed; a pass that sends nobody while the player owns Dark land says why.
static void reclaimRegression(){
 /* r33 review: that land is never sent a lone free gardener (r33 sent every idle one there alone, where no campaign could
    muster it): a defence takes it, soldiers and gardeners as one convoy; free gardeners stay within LaunchReach of home. */
 {Engine e;e.cfg.mode=0;for(int x=230;x<=240;x+=2)for(int y=40;y<=48;y+=2)e.ownCells.push_back({x,y});Campaign c=fresh();int lone=0;uint32_t t=10;
  for(;t<ReclaimStart+DefendTime+2*HomeTimeout;t+=Period){e.intr.own=(int)e.ownCells.size();step(c,t,e);for(auto&f:e.gards)if(f.alive&&!e.held(c,f.g.id)&&distance(f.g.x,f.g.y,40,40)>LaunchReach)lone++;}
  if(lone||!e.ownCells.empty())fprintf(stderr,"far own land: lone %d left %d defends %d first '%s' last return '%s'\n",lone,(int)e.ownCells.size(),e.defends,e.firstDefend,e.lastReturn);
  assert(lone==0&&e.defends>=1&&!strcmp(e.firstDefend,"start: intrusion")&&e.ownCells.empty());}
 {Engine e;e.intr.own=40;for(auto&f:e.gards)f.g.work=Work::Busy;Campaign c=fresh();run(c,e,ReclaimStart+10,ReclaimEvery*2);assert(!strcmp(e.lastReclaim,"none sent: no idle free gardener"));}
 {Engine e;e.intr.own=40;Campaign c=fresh();run(c,e,ReclaimStart+10,ReclaimEvery*2);assert(!strcmp(e.lastReclaim,"none sent: no Dark cell in reach"));}
 // A free gardener left parked in the field walks home when no Dark fighter is near, not while one is.
 {Engine e;e.gards[0].g.x=e.gards[0].g.centreX=120;e.gards[0].g.escorted=true;e.gards[0].g.work=Work::Idle;e.darkUnits.push_back({125,40});Campaign c=fresh();run(c,e,ReclaimStart+10,ReclaimEvery*2);
  assert(e.gards[0].g.x==120);e.darkUnits.clear();run(c,e,ReclaimStart+10+ReclaimEvery*2,ReclaimEvery*2);assert(e.gards[0].g.x==40&&!strcmp(e.lastReclaim,"parked gardener sent home"));}
}
// r33: targets campaigns fail to reach are avoided, longer each time; a long walking detour makes a target unreachable.
static void unreachable(){
 assert(detourLimit(227)==1008&&!tooLong(695,227)&&tooLong(1100,227)&&tooLong(-1,10)&&!tooLong(35,10)&&tooLong(141,10)); /* r38: 4 x hex + 100 */
 {Campaign c;failed(c,77,100);assert(failures(c,77)==1&&avoided(c,77,100+LostAvoid-1)&&!avoided(c,77,100+LostAvoid));
  failed(c,77,200);assert(failures(c,77)==2&&avoided(c,77,200+LongAvoid-1)&&!avoided(c,77,200+LongAvoid));
  avoid(c,77,300,AvoidTicks);assert(avoided(c,77,200+LongAvoid-1));} /* a short avoidance never shortens a long one */
 // Scoring by the walking detour: a near farm behind a lake loses to a farther open one.
 {Engine e;e.add(1,Farm,100,40);e.add(2,Farm,160,40);e.world[0].detour=400;Pick any;assert(e.world[chooseTarget(e.world,F(e.world),40,40,any)].id==2);}
 // Orders that are never executed (water, a blocked leg): after IgnoreLimit the target fails and another is taken.
 {Stuck s;s.add(1,Farm,100,40);s.add(2,Farm,100,120);Campaign d=fresh();launch(d,s,T0,A);assert(d.target.id==1);uint32_t uid=s.world[0].uid;
  uint32_t t=T0+Period;while(!failures(d,uid)&&t<T0+MarchTimeout){step(d,t,s);t+=Period;}
  assert(s.ignored>=IgnoreLimit&&failures(d,uid)==1&&avoided(d,uid,t+1000)&&!strcmp(s.lastAvoid,"orders not executed")&&d.target.id==2);}
 // Target timeouts count: the second one avoids the farm for LongAvoid, so the next campaigns take the other farm.
 {Stuck s;s.add(1,Farm,100,40);Campaign d=fresh();uint32_t uid=s.world[0].uid;failed(d,uid,0);d.avoidUntil[0]=0;d.avoidUid[0]=0;failed(d,uid,T0);assert(avoided(d,uid,T0+LongAvoid-1));
  Engine e;e.add(1,Farm,100,40);e.add(2,Farm,100,120);e.world[0].uid=uid;Campaign k=d;k.only=A;launch(k,e,T0+100,A);assert(k.target.id==2);}
 // The shovel order is not put back while the home economy has no iron bar.
 {ShovelWatch w;shovelNoted(w,true,4,4);assert(shovelRestoreWhile(w,0,0)==0&&w.seen==0);shovelNoted(w,true,4,4);assert(shovelRestoreWhile(w,0,5)==4&&shovelRestoreWhile(w,0,-1)==0);
  shovelNoted(w,true,4,4);assert(shovelRestoreWhile(w,1,-1)==3);}
}
// r33: a defence in the walking soak: an own Dark patch (radius 12, 469 cells) 60 tiles from home with a Dark Gardener on it for the
// first 600 ticks; attacks off (AttackMode 0: the defence still runs). r35: on the way the army does not wait for its
// gardeners (the gap is only reported); in place, once a gardener has joined the army (within EscortRadius), a waiting
// one stays within EscortRadius + GapSlack, a working one within EscortRadius + DriftLimit + GapSlack; nobody burns; the
// patch is green at the end and the defence ends "land clean".
// r33 review fixes (defence, convoy hold, parked citadel).
struct Blocked:Engine{int bx=50,by=50;bool stand(const Campaign&,int x,int y,int){return x>=0&&y>=0&&!(x==bx&&y==by);}};
static void reviewR33(){
 // A threat of land only and no gardener at home or near the area: no defence is raised (r33 raised one, it marched out
 // without gardeners, went home "no gardeners left" and launched again every DefendPause).
 {Engine e(0);e.cfg.mode=0;for(int x=80;x<=100;x+=2)for(int y=56;y<=64;y+=2)e.ownCells.push_back({x,y});e.intr.own=(int)e.ownCells.size();Campaign c=fresh();run(c,e,10,ReclaimStart+DefendLook*3);
  assert(e.raises==0&&c.phase==Phase::Idle&&!strcmp(e.lastDefend,"declined: no gardeners"));}
 // Gardeners only near the area (busy there, none at home): the defence goes and adopts them on arrival, the land is cleaned.
 {Engine e(4);e.cfg.mode=0;for(int x=200;x<=210;x+=2)for(int y=40;y<=46;y+=2)e.ownCells.push_back({x,y});for(auto&f:e.gards){f.g.x=f.g.centreX=206;f.g.y=f.g.centreY=50;f.g.work=Work::Busy;}
  Campaign c=fresh();int most=0;uint32_t t=10;for(;t<ReclaimStart+DefendTime;t+=Period){e.intr.own=(int)e.ownCells.size();step(c,t,e);if(c.style==Style::Defend&&c.phase==Phase::Assault)most=std::max(most,c.gardeners);}
  if(most==0||!e.ownCells.empty())fprintf(stderr,"adopting defence: most %d left %d last return '%s' last defend '%s'\n",most,(int)e.ownCells.size(),e.lastReturn,e.lastDefend);
  assert(e.defends>=1&&most>0&&e.ownCells.empty()&&strcmp(e.lastReturn,"no gardeners left"));}
 // Temple guards inside HuntKeep of the fire, within ThreatRadius of the hold cell, heavier than the defence: they neither size
 // the defence nor send it home ("threat too strong" on arrival, relaunched DefendPause later in r33).
 {Engine e(8);e.add(1,Temple,100,40);e.add(2,Farm,300,300,false);e.intr.gardeners={{160,44}};for(int i=0;i<13;i++)e.intr.fighters.push_back({133+i%3,40+i/3});
  Fire f=e.fireNow();for(auto&p:e.intr.fighters)assert(closer(p.first,p.second,f,HuntKeep));Campaign c=fresh();uint32_t t=run(c,e,10,ReclaimStart+DefendLook*2);assert(c.style==Style::Defend&&c.phase!=Phase::Idle);
  run(c,e,t,1500);if(!strcmp(e.lastReturn,"threat too strong"))fprintf(stderr,"fire guards: defence went home '%s'\n",e.lastReturn);assert(strcmp(e.lastReturn,"threat too strong")&&e.raises==1);}
 // r35: no convoy hold any more (r33 stopped the army on its body cell, which can be a tower core): a gardener 90 tiles
 // behind does not stop the army, the order goes to the leg end, never to the unstandable body cell.
 {Blocked e;e.army.clear();const int at[5][2]={{40,50},{40,50},{50,40},{50,40},{60,60}};for(int i=0;i<5;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),at[i][0],at[i][1]});
  Campaign c=fresh(A);c.armed=true;c.phase=Phase::March;c.squad=0x1234;c.style=Style::Assault;c.goalX=120;c.goalY=50;c.orderAt=1;c.lastOrder=T0;
  auto&g=e.gards[0].g;g.x=g.centreX=140;g.y=g.centreY=50;c.gid[0]=g.id;c.guid[0]=g.uid;c.gcx[0]=c.gcy[0]=c.gdx[0]=c.gdy[0]=-1;c.gardeners=1;
  assert(body(e.army).x==50&&body(e.army).y==50);Fire f=e.fireNow();size_t from=e.goals.size();convoy(c,T0,e,e.army,f,40,40,200,50,false);
  assert(!c.waitSince&&e.goals.size()==from+1&&e.goals.back()==std::make_pair(200,50)&&e.parkOrders==1);}
}
static void defenceSoak(){
 const int GapSlack=4;
 for(int slow:{6,9}){
  Soak e;e.gardenerEvery=slow;e.cfg.mode=0;e.ownPatch(360,300,12);int total=e.ownLeft();e.intr.own=total;e.intr.gardeners={{360,300}};
  Campaign c=fresh();int maxOver=-99,maxWait=0,maxWork=0,starts=0;Phase last=Phase::Idle;std::set<uint16_t> joined;
  for(uint32_t t=0;t<T0+9000;t++){e.tick(t);if(t%Period)continue;if(t==T0+600)e.intr.gardeners.clear();e.intr.own=e.ownLeft();
   bool idle=c.phase==Phase::Idle;step(c,t,e);if(idle&&c.phase!=Phase::Idle&&c.style==Style::Defend)starts++;
   if(c.phase!=last){joined.clear();last=c.phase;}
   if(c.style!=Style::Defend||c.phase==Phase::Idle)continue;
   std::vector<Soldier> m;if(!e.members(c,m)||m.empty())continue;std::vector<Gardener> gs;for(int i=0;i<c.gardeners;i++){Gardener g;if(e.gardener(c,c.gid[i],c.guid[i],g))gs.push_back(g);}if(gs.empty())continue;
   std::vector<Soldier> core;Army a=body(m,&core);int gap=spanOf(gs,core),limit=convoyLimit((int)gs.size());
   bool travel=c.phase==Phase::March||c.phase==Phase::Return||(c.phase==Phase::Assault&&distance(a.x,a.y,c.stageX,c.stageY)>StageDistance);
   if(travel){maxOver=std::max(maxOver,gap-limit);continue;}
   for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){int d=reachOf(core,gs[i].x,gs[i].y);if(d<=EscortRadius)joined.insert(gs[i].id);if(!joined.count(gs[i].id))continue;if(c.gcx[i]>=0&&!gs[i].escorted)maxWork=std::max(maxWork,d);else maxWait=std::max(maxWait,d);}}
  fprintf(stderr,"defence soak gardeners 1/%d ticks: deaths=%d defences %d, own Dark %d -> %d, over the convoy limit %d, waiting %d, working %d, last return '%s'\n",slow,e.deaths,starts,total,e.ownLeft(),maxOver,maxWait,maxWork,e.lastReturn);
  assert(starts>=1&&e.deaths==0&&maxWork>0&&maxWait<=EscortRadius+GapSlack&&maxWork<=EscortRadius+DriftLimit+GapSlack&&e.ownLeft()*5<=total&&!strcmp(e.lastReturn,"land clean"));}
}
// r29: a fake toolsmith economy. The order byte follows 0xD6A90 (amount 100
// sets continuous, otherwise add and clamp to 0..100); the toolsmith takes an
// order (decrement unless 100) only when one is set and makes one shovel per
// cycle; recruitment (recruitOrder) turns a carrier plus a shovel into a gardener.
// One step is one game tick in the plugin's order: the watch (shovelRestore)
// at the tick start, the evaluation every `every` ticks, a production cycle
// start every `cycle` ticks (entity update), then the AI building manager
// pass every `clearEvery` ticks sending SendProduceOrder(SHOVEL, aiAmount)
// (10-2*diggers: 0 at 5 diggers, negative from 6), stored through 0xD6A90.
struct FakeSmith {int order=0,shovels=0,alive=0,toolsmiths=1,made=0,orders=0,restores=0,n=0,every=1,cycle=1,clearEvery=0,aiAmount=0;bool guard=true;ShovelWatch w;
 void set(int amount){if(amount==0)order=0;else if(amount==ShovelContinuous)order=ShovelContinuous;else order=std::max(0,std::min(ShovelContinuous,order+amount));}
 void place(int amount){set(amount);orders++;}
 void produce(){if(toolsmiths<=0||order<=0)return;if(order!=ShovelContinuous)order--;shovels++;made++;}
 void step(){n++;
  if(guard&&toolsmiths>0){int r=shovelRestore(w,order);if(r>0){set(r);restores++;}w.seen=order;}
  if(n%every==0){const char*why="";int a=shovelOrder(alive,0,shovels,order,toolsmiths,&why);int target=shovelTarget(alive,0,shovels);if(a>0)place(a);shovelNoted(w,a>0||!strcmp(why,"already ordered"),order,target);}
  if(n%cycle==0)produce();
  if(recruitOrder(alive,0,0,shovels,true)){shovels--;alive++;}
  if(clearEvery&&n%clearEvery==0)set(aiAmount);}
};
static void toolsmithShovels(){
 const char*why="x";
 // Gardeners missing and the shovels down to the reserve: top the order up to the shovels still needed, at most a batch.
 assert(shovelOrder(0,0,ShovelReserve,0,1,&why)==ShovelBatch&&!*why);
 assert(shovelOrder(GardenersWanted-1,0,ShovelReserve,0,1,&why)==1&&shovelOrder(GardenersWanted-1,0,0,0,1)==3&&shovelOrder(GardenersWanted-2,0,-3,0,1)==ShovelBatch);
 // Already ordered: only the difference; a full order adds nothing.
 assert(shovelOrder(0,0,1,1,1,&why)==ShovelBatch-1&&shovelOrder(0,0,1,ShovelBatch,1,&why)==0&&!strcmp(why,"already ordered")&&shovelOrder(0,0,1,ShovelBatch+3,1,&why)==0);
 // A continuous order (100, set by someone else) is never touched.
 assert(shovelOrder(0,0,0,ShovelContinuous,1,&why)==0&&!strcmp(why,"continuous order"));
 // No ready toolsmith in the home economy, or no shovel slot: no order.
 assert(shovelOrder(0,0,0,0,0,&why)==0&&!strcmp(why,"no toolsmith")&&shovelOrder(0,0,0,-1,2,&why)==0&&!strcmp(why,"no shovel slot"));
 // Enough gardeners (alive or pending), or shovels still beyond the reserve: no order.
 assert(shovelOrder(GardenersWanted,0,0,0,1,&why)==0&&!strcmp(why,"enough gardeners")&&shovelOrder(GardenersWanted-1,1,0,0,1,&why)==0&&!strcmp(why,"enough gardeners"));
 assert(shovelOrder(0,0,ShovelReserve+1,0,1,&why)==0&&!strcmp(why,"spare shovels"));
 // The field case (r28, Iron Kingdom): 0 gardeners and 2 shovels. The toolsmith refills the pool and recruitment follows.
 {FakeSmith f;f.shovels=ShovelReserve;for(int i=0;i<200;i++)f.step();assert(f.alive==GardenersWanted&&f.shovels<=ShovelReserve+ShovelBatch&&f.order<=ShovelBatch&&f.made>=GardenersWanted);
  int made=f.made,orders=f.orders;for(int i=0;i<200;i++)f.step();assert(f.orders==orders&&f.made<=made+ShovelBatch&&f.order==0);} /* no new order once every gardener stands */
 // Without a toolsmith nothing is ordered and nobody is recruited.
 {FakeSmith f;f.shovels=ShovelReserve;f.toolsmiths=0;for(int i=0;i<50;i++)f.step();assert(f.orders==0&&f.alive==0&&f.order==0);}
 // A continuous order stays continuous (the plugin never lowers or adds to it).
 {FakeSmith f;f.shovels=0;f.order=ShovelContinuous;for(int i=0;i<30;i++)f.step();assert(f.orders==0&&f.order==ShovelContinuous&&f.alive==GardenersWanted);}
 // Spare shovels: recruitment goes first, the order comes only once the reserve is reached.
 {FakeSmith f;f.shovels=ShovelReserve+5;f.alive=0;f.step();assert(f.orders==0);for(int i=0;i<10;i++)f.step();assert(f.orders>=1&&f.alive>=5);}
 // r29 review: the watch. Production takes one at a time and is never put back.
 {ShovelWatch w;shovelNoted(w,true,4,4);assert(w.owed==4&&w.seen==4);assert(shovelRestore(w,3)==0&&w.owed==3&&w.seen==3);
  // A clear (3 -> 0) puts back what the plugin still owes.
  assert(shovelRestore(w,0)==3&&w.seen==0);w.seen=3;
  // A lowering by two or more (an AI amount of 10-2*7 = -4 on an order of 3+4) puts back only the plugin's part.
  assert(shovelRestore(w,7)==0&&w.seen==7&&w.owed==3&&shovelRestore(w,3)==0&&shovelRestore(w,1)==2);w.seen=3;
  // 1 -> 0 counts as production (the 250-tick evaluation repairs a clear there).
  {ShovelWatch v;shovelNoted(v,true,1,4);assert(v.owed==1&&shovelRestore(v,0)==0&&v.owed==0&&shovelRestore(v,0)==0);}
  // A continuous order or a lost shovel slot forgets the watch; nothing owed, nothing put back.
  assert(shovelRestore(w,ShovelContinuous)==0&&w.owed==0&&shovelRestore(w,0)==0);
  shovelNoted(w,true,3,4);assert(shovelRestore(w,-1)==0&&w.owed==0);
  // Only the plugin's part is owed (an order above the target holds AI digger shovels); unwanted or empty: forget.
  shovelNoted(w,true,6,4);assert(w.owed==4&&w.seen==6&&shovelRestore(w,0)==4);
  shovelNoted(w,false,4,4);assert(w.owed==0);shovelNoted(w,true,0,4);assert(w.owed==0);shovelNoted(w,true,ShovelContinuous,4);assert(w.owed==0);}
 // No AI pass: production drops never trigger a restore (the field case above, every tick).
 {FakeSmith f;f.shovels=ShovelReserve;f.cycle=7;f.every=25;for(int i=0;i<3000;i++)f.step();assert(f.alive==GardenersWanted&&f.restores==0);}
 // The AI pass clears the order (5 diggers, amount 0) or lowers it (7 diggers, amount -4) after each entity update; the
 // evaluation runs every 25 ticks and a cycle starts every 7. Without the watch the order is gone before most cycles
 // start (only a cycle that starts on an evaluation tick gets one: 1,050 ticks to refill); with it every cycle gets
 // its order (126 ticks), and no more shovels are made than gardeners are missing.
 for(int amount:{0,-4}){FakeSmith a,b;a.shovels=b.shovels=ShovelReserve;a.cycle=b.cycle=7;a.every=b.every=25;a.clearEvery=b.clearEvery=3;a.aiAmount=b.aiAmount=amount;b.guard=false;
  int fullA=0,fullB=0;for(int i=0;i<3000;i++){a.step();b.step();if(!fullA&&a.alive==GardenersWanted)fullA=a.n;if(!fullB&&b.alive==GardenersWanted)fullB=b.n;}
  assert(fullA&&a.restores>0&&a.made==GardenersWanted&&(!fullB||fullA*4<fullB));
  /* once every gardener stands nothing is owed: the AI's clears are left alone */
  int r=a.restores,o=a.orders;for(int i=0;i<1000;i++)a.step();assert(a.restores==r&&a.orders==o&&a.w.owed==0);}
}
// r43: the carrier reserve floor grows with the kingdom and only ever raises the AI's own value.
static void carrierReserveFloor(){
 assert(carrierKeep(0)==15&&carrierKeep(19)==24&&carrierKeep(50)==40&&carrierKeep(100)==65&&carrierKeep(400)==120&&carrierKeep(-5)==15);
 assert(carrierReserve(5,50,200,0)==40&&carrierReserve(52,50,200,0)==52&&carrierReserve(5,0,200,0)==15&&carrierReserve(80,400,500,0)==120&&carrierReserve(0,0,200,0)==ReserveBase);
 /* r47: room for missing specialists, the reserve then at most carriers - room (the setter floors at 5) */
 assert(specialistRoom(10,10,0)==0&&specialistRoom(0,5,0)==6&&specialistRoom(6,5,3)==3&&specialistRoom(0,0,20)==14&&specialistRoom(6,2,0)==3&&specialistRoom(-1,99,0)==7&&specialistRoom(6,5,-2)==0);
 assert(carrierReserve(36,100,46,0)==65&&carrierReserve(70,100,46,0)==70&&carrierReserve(36,100,46,12)==34&&carrierReserve(36,100,200,12)==65&&carrierReserve(36,100,5,12)==0&&carrierReserve(70,100,46,3)==43);
 for(int b=0;b<500;b++)assert(carrierKeep(b+1)>=carrierKeep(b)&&carrierKeep(b)>=ReserveBase&&carrierKeep(b)<=ReserveMax&&carrierReserve(b,b,1000,0)>=b&&carrierReserve(b,b,b,RoomMax)<=std::max(0,b-RoomMax));
 /* r45: decorations and military buildings do not count; Iron Kingdom's end list (pid 3572) gives 38 economy buildings and keep 34, not 86 and 58 */
 assert(economyBuilding(1)&&economyBuilding(24)&&economyBuilding(40)&&economyBuilding(42)&&economyBuilding(82)&&!economyBuilding(0)&&!economyBuilding(45)&&!economyBuilding(46)&&!economyBuilding(48)&&!economyBuilding(64)&&!economyBuilding(67)&&!economyBuilding(75)&&!economyBuilding(83));
 {const int list[][2]={{1,1},{3,1},{5,2},{6,3},{8,1},{9,1},{10,2},{11,4},{12,2},{15,3},{16,2},{17,7},{19,1},{20,2},{21,1},{22,2},{24,1},{27,1},{42,1},{46,14},{47,6},{67,25},{68,1},{73,1},{75,1}};int all=0,eco=0;for(auto&e:list){all+=e[1];if(economyBuilding(e[0]))eco+=e[1];}assert(all==86&&eco==38&&carrierKeep(eco)==34&&carrierKeep(all)==58);}
}
// r44: the economy ledger row matches the header column by column and survives a short buffer.
static void ledgerRows(){
 dtledger::Ledger l;l.player=3;l.eco=true;l.population=250;l.carriers=60;l.busy=12;l.reserve=45;l.keep=45;l.housing=300;l.settlers[1]=60;l.settlers[2]=5;l.settlers[3]=8;l.settlers[29]=20;l.settlers[44]=2;l.settlers[50]=4;l.settlers[61]=1;l.ready[40]=3;l.ready[41]=2;l.ready[24]=1;l.ready[21]=1;l.sites[41]=1;l.sites[22]=1;for(int g=0;g<dtledger::GoodCount;g++)l.goods[g]=g;l.tsout=7;l.tsin=3;l.tsworker=1;
 auto commas=[](const char*s){int n=0;for(;*s;s++)n+=*s==',';return n;};
 char row[2600];int w=dtledger::csvRow(l,123,45,500,row,sizeof(row));assert(w>0&&(size_t)w<sizeof(row)&&commas(dtledger::csvHeader())==55&&commas(row)==55);
 assert(!strcmp(row,"123,45,500,3,1,250,60,12,48,45,45,300,3,2,0,7,2,1,1,0,0,0,0,23,2,8,5,4,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,7,3,1,1:60 2:5 3:8 29:20 44:2 50:4 61:1,21:1 24:1 40:3 41:2,22:1 41:1"));
 dtledger::Ledger z;w=dtledger::csvRow(z,1,2,3,row,sizeof(row));assert(w>0&&commas(row)==55&&!strcmp(row+w-3,",,,")&&dtledger::soldiers(z)==0);
 char tiny[16];w=dtledger::csvRow(l,123,45,500,tiny,sizeof(tiny));assert(w>=(int)sizeof(tiny)&&tiny[15]==0&&commas(tiny)<55);
 for(int t=0;t<dtledger::SettlerTypes;t++)l.settlers[t]=99999;char big[64];dtledger::list(big,sizeof(big),l.settlers,dtledger::SettlerTypes);assert(big[63]==0&&strlen(big)<64);
}
// r46: surplus tool orders are cleared (never shovels, never continuous), and the output capacity marks a clog.
static void toolWatch(){
 /* r48: cleared only with a nearly full output and at least two of the tool lying there; a batch for waiting workers runs on */
 assert(surplusOrder(25,2,4,6)&&surplusOrder(18,6,1,7)&&surplusOrder(4,2,99,8)&&!surplusOrder(31,6,4,8)&&!surplusOrder(25,0,4,7)&&!surplusOrder(25,2,0,7)&&!surplusOrder(25,2,100,7)&&!surplusOrder(25,2,-1,7));
 assert(!surplusOrder(25,1,12,2)&&!surplusOrder(25,1,12,7)&&!surplusOrder(25,5,12,5)&&surplusOrder(25,5,12,6)&&OutputNearFull<OutputCapacity&&ToolSurplus>=2);
 assert(!clogged(0)&&!clogged(7)&&clogged(8)&&clogged(9)&&ToolWatchEvery<=50);
 /* r49: drop from the fullest non-shovel pile only at DropAt with no free shovel lying there and never a reserved piece */
 struct L{int good,count,reserved;};
 {L a[]={{4,0,0},{18,1,0},{25,6,0},{31,0,0}};assert(dropCandidate(a,4,7)==2&&dropCandidate(a,4,8)==2&&dropCandidate(a,4,6)==-1);}
 {L b[]={{18,2,0},{25,5,0},{31,1,0}};assert(dropCandidate(b,3,8)==-1);}                  /* a free shovel is there: wait for the recruit */
 {L c[]={{18,2,0},{25,5,0},{31,1,1}};assert(dropCandidate(c,3,8)==1);}                   /* the shovel is reserved: make room for another */
 {L d[]={{18,3,3},{25,4,4},{31,0,0}};assert(dropCandidate(d,3,7)==-1);}                  /* everything reserved: nothing to drop */
 {L e[]={{18,3,1},{25,3,0},{31,0,0}};assert(dropCandidate(e,3,7)==1&&DropAt<OutputCapacity&&DropEvery>=ToolWatchEvery);}
}
int main(){toolsmithShovels();basics();targetOrder();geometry();gating();styles();allied();campaign();reattack();fallback();defence();gardenerLosses();farmReturns();steering();sweeping();hunting();reclaiming();landFirst();marchBudget();losses();adoption();timeouts();detours();reviewFixes();fireRules();guarding();guardReview();keepAudit();soak();soakTwo();scaling();convoyRules();convoyField();convoySoak();defending();reclaimRegression();unreachable();reviewR33();defenceSoak();carrierReserveFloor();ledgerRows();toolWatch();puts("PASS: offence-tests");return 0;}
