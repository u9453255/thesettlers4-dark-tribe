#include "offence-model.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <map>
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
 void sendGardener(const Campaign&,const Gardener&g,int x,int y){gardenerOrders++;audit(x,y,GardenerKeep);auto*f=find(g.id);f->g.x=x;f->g.y=y;f->g.centreX=x;f->g.centreY=y;f->g.reservedX=f->g.reservedY=f->g.aimX=f->g.aimY=-1;f->g.work=Work::Busy;
  for(auto&r:rings)for(auto&c:r.second)if(c.dark&&distance(c.x,c.y,x,y)<=1){ringOrders++;goto counted;}counted:
  bool land=false;for(auto&c:darkCells)if(distance(c.first,c.second,x,y)<=1)land=true;for(auto&c:ownCells)if(distance(c.first,c.second,x,y)<=1)land=true;if(land)landOrders++;
  greenNear(darkCells,x,y,1);greenNear(ownCells,x,y,1);if(land)f->g.work=Work::Working;}
 void moveGardeners(const Campaign&,const std::vector<Gardener>&gs,int x,int y){groupOrders++;audit(x,y,soldierKeep((int)gs.size()));for(auto&g:gs){auto*f=find(g.id);f->g.x=x;f->g.y=y;f->g.centreX=x;f->g.centreY=y;f->g.reservedX=f->g.reservedY=f->g.aimX=f->g.aimY=-1;f->g.work=Work::Working;}}
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
 void report(Event e,const Campaign&c){if(e==Event::Decline){declines++;lastDecline=c.reason;}if(e==Event::Launch)launches++;if(e==Event::Adopted)adopted++;if(e==Event::Lost)lost++;
  if(e==Event::Ignored)ignored++;if(e==Event::Avoid){avoids++;lastAvoid=c.reason;}if(e==Event::Fire){fires++;lastFire=c.reason;}if(e==Event::Siege)sieges++;if(e==Event::Greening)greenings++;if(e==Event::Reclaim){reclaims++;lastReclaim=c.reason;}if(e==Event::Return)lastReturn=c.reason;
  if(e==Event::Front)fronts++;if(e==Event::Hunt)hunts++;if(e==Event::Join)joins++;if(e==Event::Call)calls+=0;if(e==Event::Convoy){convoys++;lastConvoy=c.reason;}}
 int convoys=0;const char*lastConvoy="";
};
static Fire F(const std::vector<Target>&all,int ready=-1,const std::vector<Site>&sites={}){return fireFrom(all,ready,sites);}
template<class E> static uint32_t run(Campaign&c,E&e,uint32_t from,uint32_t ticks){uint32_t t=from;for(;t<from+ticks;t+=Period)step(c,t,e);return t;}
static Campaign fresh(int only=-1){Campaign c;c.player=2;c.sector=0;c.only=(int8_t)only;return c;}
constexpr uint32_t T0=3000; // after firstAttack(1)
// Force a decision now (chance certain), optionally of one style.
template<class E> static void launch(Campaign&c,E&e,uint32_t t,int only=-1){c.only=(int8_t)only;step(c,t,e);c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;step(c,t+Period,e);}
// r32: steps until the campaign reaches a phase (the convoy marches ConvoyLead tiles per ConvoyEvery ticks in the teleport engine).
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
 assert(firstAttack(0)==FirstMin&&firstAttack(1)==FirstMin&&firstAttack(15)==5*DelayUnit&&firstAttack(20)==20*DelayUnit/3&&firstAttack(40)==FirstMax);
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
 // No fighter count rule any more: 8 free soldiers are enough for a raid (2 stay home).
 {Engine e;e.str.fighters=3;e.pool.resize(8);e.add(1,Farm,140,40);Campaign c=fresh();launch(c,e,T0);assert(e.launches==1&&e.army.size()==6&&e.pool.size()==2);}
 {Engine e;Campaign c=fresh();launch(c,e,T0);assert(!strcmp(e.lastDecline,"no reachable Dark building"));}
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
  std::vector<Soldier> a,b;halves(e.army,a,b);assert(b[0].x==c.stage2X&&b[0].y==c.stage2Y&&distance(a[0].x,a[0].y,40,40)<=ConvoyLead+1); /* r32: the first half leads the convoy */
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
 // A raid razes at most RaidChain buildings, then goes home.
 Engine g;for(int i=0;i<4;i++)g.add((uint16_t)(1+i),Farm,100+30*i,40);Campaign d=fresh();launch(d,g,T0,R);
 {uint32_t t=T0+Period;while(!g.dissolved&&t<T0+15000){step(d,t,g);t+=Period;}}assert(g.collapsed==RaidChain&&g.world.size()==1&&g.dissolved==1&&!strcmp(g.lastReturn,"raid done"));
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
 // Direct assault (target within 40 of home): idle gardeners at home are sent to the ring and the farm collapses.
 {Engine e;e.add(1,Farm,70,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.phase==Phase::Assault);run(c,e,T0+Period,800);assert(e.collapsed==1&&e.gardenerOrders>=3);}
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
 // A hunt follows the Dark group; when none is left it goes on to a farm with its gardeners.
 Engine e;e.add(1,Farm,60,80);for(int i=0;i<4;i++)e.darkUnits.push_back({100+i,60});Campaign c=fresh();launch(c,e,T0,H);
 uint32_t t=run(c,e,T0+Period,Period*20);Army a=summary(e.army);assert(distance(a.x,a.y,101,60)<=3);
 for(auto&u:e.darkUnits)u.first+=6;t=run(c,e,t,HuntFollow+Period);a=summary(e.army);assert(distance(a.x,a.y,107,60)<=3);
 e.darkUnits.clear();t=run(c,e,t,HuntQuiet+HuntFollow*2);assert(e.hunts==1&&!c.hunting&&c.target.kind==Farm);
 run(c,e,t,3000);assert(e.collapsed==1);
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
  int home=0;for(auto&f:e.gards)if(f.alive&&f.g.centreX==40&&f.g.centreY==40)home++;assert(home>=6); /* extra gardeners wait at home, not next to the ring */
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
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,110,44);e.add(3,Farm,300,300);e.callTarget=e.world[1];e.callUntil=100000;e.caller=3;Campaign c=fresh();step(c,T0,e);c.nextCheck=0;c.cooldownUntil=0;c.chance=0;step(c,T0+Period,e);assert(e.joins==0&&e.launches==0);}
 // Only locked farms left: the computers decline with that reason (no citadel, no locked farm).
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,110,44);Campaign c=fresh();launch(c,e,T0);assert(e.launches==0&&!strcmp(e.lastDecline,"farms only inside the fire defence"));}
}
static void guarding(){
 // A soldier within FireLine is ordered out to an exit at least soldierKeep away.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,40);Campaign c=fresh();launch(c,e,T0,A);assert(c.target.id==2);Fire f=e.fireNow();
  e.army[0].x=127;e.army[0].y=42;assert(inFire(127,42,f));int fires=e.fires;run(c,e,T0+2*Period,Period);
  assert(e.fires>fires&&!strcmp(e.lastFire,"soldiers out of the fire defence")&&!closer(e.army[0].x,e.army[0].y,f,soldierKeep(1)));
  // Lured in again and again at the target (Dark fighters at the line): avoided after LureLimit extractions.
  e.darkUnits.push_back({299,43});uint32_t t=T0+3*Period;for(int k=0;k<600&&c.phase!=Phase::Assault;k++)t=run(c,e,t,Period); /* the march walks its waypoints and hex-leg corners (r32: as a convoy) */
  t=run(c,e,t,Period*6);assert(c.phase==Phase::Assault&&c.target.id==2);
  for(int k=0;k<LureLimit&&c.target.id==2;k++){e.army[1].x=126;e.army[1].y=42;t=run(c,e,t,GuardResend);}
  assert(!strcmp(e.lastAvoid,"lured toward the fire defence")&&avoided(c,e.world[1].uid,t));}
 // A free idle gardener within FireLine is recalled at once.
 {Engine e;e.add(1,Temple,100,40);e.add(2,Farm,300,300,false);e.gards[0].g.x=127;e.gards[0].g.y=42;e.gards[0].g.work=Work::Idle;Campaign c=fresh();run(c,e,10,Period);
  assert(!inFire(e.gards[0].g.x,e.gards[0].g.y,e.fireNow())&&!strcmp(e.lastFire,"gardener out of the fire defence"));}
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
  for(int k=0;k<LureLimit&&c.phase==Phase::Assault;k++){e.army[1].x=188;e.army[1].y=42;assert(inFire(188,42,f));t=run(c,e,t,GuardResend);}
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
 bool dark(int x,int y)const{return inside(x,y)&&distance(x,y,CX,CY)<=Disc&&!green[y*N+x]&&!core(x,y);}
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
  if(t%gardenerEvery==0)for(auto&g:gards){if(!g.alive||g.working)continue;Gardener&q=g.g;if(g.tx>=0){walk(q.x,q.y,g.tx,g.ty);if(q.x==g.tx&&q.y==g.ty){g.tx=-1;q.work=Work::Busy;}}
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
 bool destroy(const Campaign&,const Target&t){for(size_t i=0;i<world.size();i++)if(world[i].id==t.id){world.erase(world.begin()+i);collapsed++;return true;}return false;}
 bool dissolve(Campaign&c){for(auto&u:army)pool.push_back(u);army.clear();c.squad=0;return true;}
 static bool held(const Campaign&c,uint16_t id){for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)return true;return false;}
 void gardeners(const Campaign&c,std::vector<Gardener>&out){out.clear();for(auto&g:gards)if(g.alive&&!held(c,g.g.id))out.push_back(g.g);}
 bool gardener(const Campaign&,uint16_t id,uint32_t uid,Gardener&out){for(auto&g:gards)if(g.alive&&g.g.id==id&&g.g.uid==uid){out=g.g;return true;}return false;}
 G*find(uint16_t id){for(auto&g:gards)if(g.g.id==id)return &g;return nullptr;}
 void order(G&g,int x,int y){g.tx=x;g.ty=y;g.working=false;g.g.centreX=x;g.g.centreY=y;g.g.aimX=g.g.aimY=g.g.reservedX=g.g.reservedY=-1;g.g.work=Work::Busy;}
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
 bool ownDark(const Campaign&,int,int,int,const std::vector<Keep>&,int&,int&){return false;}
 void darkFighters(const Campaign&,std::vector<std::pair<int,int>>&out){out=fighters;}
 bool leads(const Campaign&,const Target&){return true;}
 bool call(const Campaign&c,uint32_t,Target&out){if(!calling||c.player==3)return false;out=callTarget;return true;}
 bool announce(const Campaign&,uint32_t,const Target&){return false;}
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
// r32: the convoy in the walking soak (soldiers one cell per 6 ticks, gardeners per 6 or 9 ticks, armies of 34 and 117).
// From the moment the convoy has formed on each march or way home: no held gardener is farther than ConvoyBound from
// the army centre or AloneBound from the nearest soldier; the army never enters the assault more than ArriveWindow
// ticks after it stood at the staging cell and always with its gardeners together; while it waits at the target
// (no ring cell yet) no gardener strays beyond AloneBound from the nearest soldier; nobody burns.
static void convoySoak(){
 const int ConvoyBound=24,AloneBound=24;const uint32_t ArriveWindow=150;
 for(int slow:{6,9})for(int pool:{40,140})for(int style:{R,A,P,S,H,(int)Style::Allied}){
  Soak e;e.gardenerEvery=slow;e.soldiers(pool);if(style==H)for(int i=0;i<4;i++)e.fighters.push_back({290+i,240});
  if(style==(int)Style::Allied){for(auto&b:e.world)if(b.id==36)e.callTarget=b;e.calling=true;}
  Campaign c=fresh(style);Phase last=Phase::Idle;bool formed=false;uint32_t thereAt=0;int maxGap=0,maxAlone=0,assaults=0,apart=0,maxSize=0,maxHeld=0;uint32_t wait=0;
  for(uint32_t t=0;t<T0+12000;t++){e.tick(t);if(t%Period)continue;if(t==T0){c.chance=ChanceFull;c.nextCheck=0;c.cooldownUntil=0;}
   {std::vector<Soldier> b;if(c.phase==Phase::March&&!c.rally&&!c.split&&!walking(c)&&!thereAt&&e.members(c,b)&&!b.empty()&&within(b,c.stageX,c.stageY,10)*10>=(int)b.size()*7)thereAt=t;} /* the army stands at the staging cell */
   Phase before=c.phase;step(c,t,e);std::vector<Soldier> m;if(!e.members(c,m))m.clear();(void)before;
   std::vector<Gardener> gs;for(int i=0;i<c.gardeners;i++){Gardener g;if(e.gardener(c,c.gid[i],c.guid[i],g))gs.push_back(g);}
   maxSize=std::max(maxSize,(int)m.size());maxHeld=std::max(maxHeld,(int)gs.size());
   if(c.phase!=last){if(last==Phase::March&&c.phase==Phase::Assault){assaults++;if(thereAt)wait=std::max(wait,t-thereAt);std::vector<Soldier> h,k;halves(m,h,k);Army a=summary(c.split&&!h.empty()?h:m);if(!gs.empty()&&gapOf(gs,a.x,a.y)>convoyLimit((int)gs.size())+2)apart++;}
    formed=false;thereAt=0;last=c.phase;}
   if(m.empty()||gs.empty())continue;Army a=summary(m);int gap=gapOf(gs,a.x,a.y);
   auto alone=[&](const Gardener&g){int best=1<<30;for(auto&s:m)best=std::min(best,distance(s.x,s.y,g.x,g.y));return best;};
   if((c.phase==Phase::March||c.phase==Phase::Return)&&!c.rally&&!c.split){if(gap<=convoyLimit((int)gs.size()))formed=true;
    if(formed){maxGap=std::max(maxGap,gap);for(auto&g:gs)maxAlone=std::max(maxAlone,alone(g));}
}
   if(c.phase==Phase::Assault&&c.target.id)for(int i=0;i<(int)gs.size();i++)if(c.gcx[i]<0&&gs[i].work!=Work::Working)maxAlone=std::max(maxAlone,alone(gs[i]));}
  fprintf(stderr,"convoy soak %s gardeners 1/%d ticks, army %d, held %d: deaths=%d collapsed=%d max gap %d, max alone %d, assaults %d, stage wait %u, apart %d\n",styleName((Style)style),slow,maxSize,maxHeld,e.deaths,e.collapsed,maxGap,maxAlone,assaults,wait,apart);
  assert(e.deaths==0&&maxGap<=ConvoyBound&&maxAlone<=AloneBound&&wait<=ArriveWindow&&!apart&&e.collapsed>=1&&(style==S||assaults>=1));}
}
// r32: the convoy in the teleport engine: the army holds for a straggler and leaves it behind after ConvoyStall;
// a hunt's hand-over walks the army to the gardeners first (rally); no gardener walks to an army far away.
struct Stubborn:Engine {using Engine::Engine;uint16_t deaf=300;void sendGardener(const Campaign&c,const Gardener&g,int x,int y){if(g.id==deaf){gardenerOrders++;return;}Engine::sendGardener(c,g,x,y);}};
static void convoyRules(){
 {Stubborn e(6);e.gards[0].g.x=e.gards[0].g.centreX=10;e.add(1,Farm,200,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.gardeners==6&&e.moves==0);
  uint32_t t=run(c,e,T0+Period,ConvoyStall-60);assert(e.moves==0&&!strcmp(e.lastConvoy,"waiting for gardeners")&&c.waited>0); /* the army holds */
  for(auto&f:e.gards)if(f.g.id!=300)assert(distance(f.g.x,f.g.y,40,40)<=6); /* the others gathered at the army */
  t=run(c,e,t,120);assert(!strcmp(e.lastConvoy,"gardeners left behind")&&c.gardeners==5&&e.moves>0);
  run(c,e,t,4000);assert(e.collapsed==1);}
 // Convoy steps: the army is never ordered more than ConvoyLead (+1 rounding) ahead of the gardeners' centre.
 {Engine e(6);e.add(1,Farm,240,40);Campaign c=fresh(A);launch(c,e,T0,A);uint32_t t=T0+Period;int worst=0;
  while(c.phase==Phase::March&&t<T0+3000){size_t k=e.goals.size();step(c,t,e);t+=Period;
   if(e.goals.size()>k&&c.phase==Phase::March&&c.gardeners){long gx=0,gy=0;int n=0;for(int i=0;i<c.gardeners;i++)if(auto*f=e.find(c.gid[i])){gx+=f->g.x;gy+=f->g.y;n++;}
    worst=std::max(worst,distance(e.goals.back().first,e.goals.back().second,(int)(gx/n),(int)(gy/n)));}}
  assert(c.phase==Phase::Assault&&worst<=ConvoyLead+ConvoyBehind+2);}
 // Hunt hand-over: the army walks back to the gardeners at home before any gardener is ordered.
 {Engine e;e.add(1,Farm,60,90);for(int i=0;i<4;i++)e.darkUnits.push_back({140+i,40});Campaign c=fresh();launch(c,e,T0,H);
  uint32_t t=run(c,e,T0+Period,Period*20);e.darkUnits.clear();int before=e.gardenerOrders;bool rally=false;
  for(;t<T0+6000&&e.gardenerOrders==before;t+=Period){step(c,t,e);if(!strcmp(e.lastConvoy,"rally to the gardeners"))rally=true;}
  Army a=summary(e.army);assert(rally&&e.gardenerOrders>before&&distance(a.x,a.y,42,40)<=RallyGap);
  run(c,e,t,4000);assert(e.collapsed==1);}
 // r32 review: Dark fighters on the approach side (between the staging cell and the door): the screen stands on
 // their side, the gardeners wait behind it: the rear point and every gardener order of the assault lie farther
 // from the nearest fighter than the screen cell.
 {struct Log:Engine{std::vector<std::pair<int,int>> sent;void sendGardener(const Campaign&c,const Gardener&g,int x,int y){sent.push_back({x,y});Engine::sendGardener(c,g,x,y);}};
  Log e;e.add(1,Farm,100,40);for(int i=0;i<3;i++)e.darkUnits.push_back({88,42+i});Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.gardeners==6);
  uint32_t t=T0+Period;size_t from=0;while(c.phase==Phase::March&&t<T0+4000){from=e.sent.size();step(c,t,e);t+=Period;}assert(c.phase==Phase::Assault);
  auto nearest=[&](int x,int y){int b=1<<30;for(auto&d:e.darkUnits)b=std::min(b,distance(x,y,d.first,d.second));return b;};
  int screen=nearest(c.goalX,c.goalY);assert(screen<nearest(c.target.doorX,c.target.doorY)&&c.rearX>=0&&nearest(c.rearX,c.rearY)>screen);
  run(c,e,t,600);assert(e.sent.size()>from);
  for(size_t i=from;i<e.sent.size();i++){if(nearest(e.sent[i].first,e.sent[i].second)<=screen)fprintf(stderr,"rear: order %d,%d nearer the threat than the screen %d,%d\n",e.sent[i].first,e.sent[i].second,c.goalX,c.goalY);assert(nearest(e.sent[i].first,e.sent[i].second)>screen);}}
 // r32 review: the convoy step only along a clear leg: the army's next point is refused when the leg there passes
 // inside legKeep (the review's case: fire centre 200,200, leg 259,180 -> 239,209, army of 20 at 240,196).
 {Engine e(6);e.army.clear();for(int i=0;i<20;i++)e.army.push_back(Soldier{(uint16_t)(500+i),29,(uint32_t)(5000+i),240,196});
  Fire f;f.on=true;Zone z;z.x=200;z.y=200;f.z.push_back(z);Campaign c=fresh(A);c.phase=Phase::March;c.squad=0x1234;
  for(int i=0;i<6;i++){e.gards[i].g.x=e.gards[i].g.centreX=240;e.gards[i].g.y=e.gards[i].g.centreY=196;c.gid[i]=e.gards[i].g.id;c.guid[i]=e.gards[i].g.uid;c.gcx[i]=c.gcy[i]=c.gdx[i]=c.gdy[i]=-1;}c.gardeners=6;
  int cx=0,cy=0;along(259,180,239,209,distance(240,196,239,209)-ConvoyLead,cx,cy);
  assert(cx==241&&cy==207&&!legClear(240,196,cx,cy,f,legKeep(20))&&legClear(240,196,239,209,f,legKeep(20))&&!closer(240,196,f,soldierKeep(20)));
  assert(convoy(c,T0,e,e.army,f,259,180,239,209,false)&&!e.goals.empty());
  assert(!(e.goals.back().first==cx&&e.goals.back().second==cy)&&legClear(240,196,e.goals.back().first,e.goals.back().second,f,legKeep(20)));}
 // r32 review: every gardener left behind. A gardener that no longer answers keeps its last order; record it.
 struct Deaf:Engine{using Engine::Engine;bool on=false;std::map<uint16_t,std::pair<int,int>> last;
  void sendGardener(const Campaign&c,const Gardener&g,int x,int y){if(on){gardenerOrders++;last[g.id]={x,y};return;}Engine::sendGardener(c,g,x,y);}
  void moveGardeners(const Campaign&c,const std::vector<Gardener>&gs,int x,int y){if(on){for(auto&g:gs)last[g.id]={x,y};return;}Engine::moveGardeners(c,gs,x,y);}};
 // At home: every gardener far away and deaf: the army never leaves home, the target is not avoided, the
 // campaign ends, and each gardener's last order is home.
 {Deaf e(6);e.on=true;for(auto&f:e.gards){f.g.x=f.g.centreX=40;f.g.y=f.g.centreY=140;}e.add(1,Farm,200,40);Campaign c=fresh(A);launch(c,e,T0,A);assert(c.phase==Phase::March&&c.gardeners==6);
  run(c,e,T0+Period,ConvoyStall+300);assert(!strcmp(e.lastReturn,"gardeners left behind")&&e.avoids==0&&e.sieges==0&&c.phase==Phase::Idle&&e.dissolved==1);
  for(auto&g:e.goals)assert(distance(g.first,g.second,40,40)<=GatherHome);
  for(auto&f:e.gards)assert(e.last.count(f.g.id)&&e.last[f.g.id]==std::make_pair(40,40));}
 // In the field: the gardeners stop answering 30 tiles behind the army: it holds, leaves them all behind, avoids
 // the target and walks home without a step towards it; their last order is home.
 {Deaf e(6);e.add(1,Farm,240,40);Campaign c=fresh(A);launch(c,e,T0,A);uint32_t t=T0+Period;
  while(c.phase==Phase::March&&summary(e.army).x<100&&t<T0+4000){step(c,t,e);t+=Period;}assert(c.phase==Phase::March);
  int x0=summary(e.army).x;e.on=true;for(auto&f:e.gards){f.g.x=f.g.centreX=x0-30;f.g.y=f.g.centreY=40;f.g.work=Work::Idle;}size_t k=e.goals.size();
  run(c,e,t,ConvoyStall+300);assert(!strcmp(e.lastReturn,"gardeners left behind")&&!strcmp(e.lastAvoid,"gardeners left behind")&&e.sieges==0&&e.collapsed==0&&c.phase!=Phase::Assault);
  for(size_t i=k;i<e.goals.size();i++)assert(e.goals[i].first<=x0+ConvoyLead+2);
  for(auto&f:e.gards)assert(e.last.count(f.g.id)&&e.last[f.g.id]==std::make_pair(40,40));}
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
int main(){toolsmithShovels();basics();targetOrder();geometry();gating();styles();allied();campaign();reattack();fallback();defence();gardenerLosses();farmReturns();steering();sweeping();hunting();reclaiming();losses();adoption();timeouts();detours();reviewFixes();fireRules();guarding();guardReview();keepAudit();soak();soakTwo();scaling();convoyRules();convoySoak();puts("PASS: offence-tests");return 0;}
