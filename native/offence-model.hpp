#pragma once
// Computer offence against the Dark Tribe (rounds r27/r28): pure decision logic.
// The native AI only targets towers/castles (types 46..48, group nibble 0xC),
// so it never attacks Dark buildings (research/AI_OFFENCE_0215.md). Each
// computer sector AI gets a director that uses the AI's own config, soldiers,
// gardeners and strength values. Mushroom Farms fall first; once no ready farm
// stands (the citadel's fire defence is off) the Temple and the Fortress are
// the goal, then the Manacopter Halls. Dark buildings fall the native way: the
// soldiers clear the defenders and the AI's own Gardeners (type 50) turn the
// ring of walkable footprint cells around the blocked core green, which
// collapses the building natively (credited to the attacker).
// r28: six attack styles chosen with the plugin's own generator (raid, assault,
// pincer, sweep, hunt, allied), short attack delays and rests so that a new
// army forms whenever soldiers are free, and steady land reclamation: Dark
// ground inside the computer's own territory first, then the Dark border.
// r30: citadel fire keep-off. The Temple/Fortress fire (0x1458C0) is a
// Euclidean disc of up to 24 around the building position + (1,2) while the
// Dark player has a ready farm (citadel-fire.hpp). While it is on (native
// ready-farm counter, listed farms or a Farm site), every soldier destination
// stays soldierKeep(N) away, every gardener destination and ring cell
// GardenerKeep away, legs are routed around it, farms within FarmLock and
// Halls within HallLock are never targets, and every director step pulls
// computer units that still got within FireLine back out.
// r32: every style grows with the army (raid a third, hunt half, sweep escort a
// quarter, today's sizes as the floor; gardeners base + soldiers/15, at most
// gardenersWanted(free soldiers)), and the army marches as one convoy with its
// gardeners: the army's next destination is at most ConvoyLead ahead of the
// rearmost unit, it holds while a gardener lags, takes a screen position
// towards the nearest Dark fighters at the target before the gardeners work,
// top-ups come only from gardeners already near the army, a hunt's gardeners
// join only when the army has walked to them, and the way home is a convoy too.
// No engine reads here: the engine side is an Ops object (dark-tribe.cpp /
// offence-tests.cpp).
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include "citadel-fire.hpp"
namespace dtoffence {
constexpr uint16_t Farm=49,Temple=50,Fortress=51,Hall=80;
constexpr uint8_t DarkPlayer=1;
// AttackDelay counts in 840-tick units (one game minute, native agent 0x2FAF0).
constexpr uint32_t DelayUnit=840;
// Pressure: the first attack after a third of the AI's AttackDelay, kept
// within 3..8 minutes; a check every 256..383 ticks succeeds with 50%, +25%
// per failed check; a finished campaign rests 600..1,499 ticks, then the next
// army forms from whatever soldiers are free.
constexpr uint32_t FirstMin=3*DelayUnit,FirstMax=8*DelayUnit,CheckEvery=256,ChanceStart=0x8000,ChanceStep=0x4000,ChanceFull=0x10000,CooldownMin=600,CooldownSpread=900;
constexpr uint32_t Period=10; // director step, game ticks
constexpr int MinArmy=5,HomeReserve=2,RaidMax=10,HuntMax=16,SweepEscort=8,AssaultMin=12,PincerMin=14,PincerGap=12,AdoptedRaid=12,RaidChain=3;
constexpr int RaidRadius=20,DefenceRadius=10,SiegeRadius=7,StageDistance=16,AssaultDistance=40;
// r30 fire keep-off (Euclidean, from each fire centre; shared geometry in citadel-fire.hpp):
// FireReach 24 = the native maximum, FireLine 27 = no computer unit may stand within it,
// GardenerKeep 28 = gardener destinations, ring and beside cells, FarmLock 33 = a farm
// position closer than this is locked (its ring and beside cells would reach GardenerKeep),
// HallLock 44 = the same for a Hall (its ring reaches 15), HuntKeep 42 = native soldiers chase
// Dark fighters 13/15 tiles. Soldier destinations keep soldierKeep(N) = FireLine + the group
// order's formation spread (click + 2 * spiral) and straight legs legKeep(N).
using dtfire::FireReach;using dtfire::FireLine;using dtfire::FarmReach;using dtfire::FarmLock;
constexpr int GardenerKeep=FireLine+1,HallReach=15,HallLock=FireLine+HallReach+2,LegSlack=3,HuntChase=15,HuntKeep=FireLine+HuntChase,LureLimit=3,NoFire=1<<30;
constexpr uint32_t LureWindow=600,BenchTicks=600,GuardResend=30,ViaTimeout=1500;
// r30 review: RetryPlan = a campaign without a safe way home holds outside the fire and plans again this often;
// HotHold = a squad on its way home is never handed back with a member within FireLine before this many ticks
// (reservoirs never move soldiers; only units the fire cannot reach, on green land with no way out, last that long).
constexpr uint32_t RetryPlan=60,HotHold=12000;
constexpr uint32_t MarchTimeout=4000,TargetTimeout=9000,CampaignTimeout=40000,HomeTimeout=3000,ReorderEvery=300,AvoidTicks=1000,IgnoreCheck=60,UnknownRetry=100,RetryRaze=30;
constexpr double PincerAngle=1.05; // the two flanks: the line from home turned by +-60 degrees around the door
// Gardeners (native role CGardenerRole, one cell per ~31-tick WORK).
constexpr uint16_t GardenerType=50;
// r32: MaxGardeners (20) is the most one campaign holds and the most a large computer keeps alive
// (gardenersWanted); ParkedDark is the ring size from which a parked citadel's hold is checked.
constexpr int MaxGardeners=20,MinGardeners=2,SweepGardeners=3,SearchMisses=40,EscortGap=3,DriftLimit=4,ParkedDark=14;
constexpr uint32_t GardenerResend=60,GreenWait=20;
// A Temple/Fortress parking entering settlers is spared its last ring cells for at most HoldMax ticks
// (continuous recruitment must not make it unbreakable); an allied escort goes home after EscortStall
// ticks without a greened ring cell.
constexpr uint32_t HoldMax=900,EscortStall=1500,LostAvoid=6000; // LostAvoid: a target that cost a style's gardeners is avoided this long
// Land reclamation by the free gardeners: Dark ground in the player's own
// territory always (OwnRadius around each gardener: the computers cannot
// build on Dark ground), the Dark border after a
// minute (ReclaimRadius around the gardener, within ReclaimHome of the
// sector's home); never into fire, next to defenders, within CitadelKeep of a
// Temple/Fortress (sieges handle those) or within SiteKeep of a growing site.
// The keeps cover the ring of a Temple/Fortress/Hall (walkable footprint cells next to the core lie
// 4..15 tiles from the building position) plus 3, so no gardener is ever sent onto one.
constexpr int OwnRadius=120,ReclaimRadius=50,ReclaimHome=80,ReclaimPerPass=4,CitadelKeep=18,SiteKeep=18,ParkedKeep=20,SpreadKeep=3;
constexpr uint32_t ReclaimEvery=150,ReclaimStart=DelayUnit,RecallEvery=30;
// Sweep: an escort holds a front of Dark land while the gardeners green it,
// the front moves SweepStep tiles towards the Dark base every SweepAdvance
// ticks or when the land around it is green; a target building near the front
// is besieged on the way.
constexpr int SweepRadius=10,SweepStep=8,SweepSearch=120,SweepLand=25,SweepTarget=CitadelKeep+6,FrontKeep=FireLine+SweepRadius;
constexpr uint32_t SweepAdvance=450,SweepTime=9000;
// Hunt: the army goes for the largest group of Dark fighters (outside fire).
constexpr int HuntMin=3,HuntRadius=25,HuntCluster=6;
constexpr uint32_t HuntQuiet=300,HuntFollow=150,HuntTime=5000;
// Allied: an assault on a building calls the other computer enemies for CallTicks.
constexpr uint32_t CallTicks=1500;
// Native recruitment of gardeners (carrier + shovel, eco order 0xD5100): keep
// this many alive, order one at a time, never use the last shovels.
constexpr int GardenersWanted=12,ShovelReserve=2;
// r32 scaling. Soldiers per style: raid a third of the army (at least RaidMax), hunt half (at least
// HuntMax), sweep escort a quarter (at least SweepEscort), each at most the army; assault, pincer and
// allied take the whole army. Gardeners per style: the style's base plus one per GardenerPer soldiers
// it takes, at most gardenersWanted(free soldiers of the sector). A computer keeps GardenersWanted alive,
// one more per WantedPer soldiers beyond WantedFrom (12 at 40 soldiers, 20 from 120), at most MaxGardeners.
constexpr int GardenerPer=15,WantedFrom=40,WantedPer=10;
inline int raidSize(int army){return std::max(0,std::min(army,std::max(RaidMax,army/3)));}
inline int huntSize(int army){return std::max(0,std::min(army,std::max(HuntMax,army/2)));}
inline int escortSize(int army){return std::max(0,std::min(army,std::max(SweepEscort,army/4)));}
inline int gardenersWanted(int soldiers){return std::min(MaxGardeners,GardenersWanted+std::max(0,soldiers-WantedFrom)/WantedPer);}
// Native weight table RVA 0xD63238 (E(p)+0x38/+0x40): swordsman/bowman
// level 1..3 = 2/3/4, medic/axe/blowgun/backpack catapult 3/4/5, squad leader 10.
inline int soldierWeight(uint16_t type){
 if(type>=29&&type<=34)return 2+(type-29)%3;
 if((type>=35&&type<=43)||(type>=61&&type<=63))return 3+(type>=61?type-61:(type-35)%3);
 return type==44?10:0;
}
inline bool soldierType(uint16_t type){return soldierWeight(type)>0;}
// Dark fighters that defend a building and block a siege (Warrior, Thrower, Shaman).
inline bool defenderType(uint16_t type){return type==29||type==32||type==54;}
// S4 map distance (hex grid with neighbours (+-1,0), (0,+-1), (1,1), (-1,-1)).
inline int distance(int ax,int ay,int bx,int by){int dx=bx-ax,dy=by-ay;return (dx>=0)==(dy>=0)?std::max(abs(dx),abs(dy)):abs(dx)+abs(dy);}
// Temple/Fortress fire defence (0x1458C0): a Euclidean disc around the building
// position +(1,2), radius 16 + 2 per ready farm (at most 24); off without ready
// farms. Documentation and tests only: the keep-off always uses FireReach.
inline int fireRadius(int readyFarms){return readyFarms>0?std::min(FireReach,16+2*readyFarms):0;}
inline uint32_t firstAttack(int delay){return std::min(FirstMax,std::max(FirstMin,(uint32_t)std::max(0,delay)*DelayUnit/3));}
struct Config {int mode=0,delay=0,chancePartial=0,thresholdPartial=0,thresholdTotal=0,squadMin=10,squadExtra=2;bool ok=false;};
struct Strength {int fighters=0,strength=0,darkValue=0,region=0,homeX=0,homeY=0;bool ok=false;};
// defence = weight (2 each) of Dark Warriors/Throwers/Shamans within RaidRadius of the door.
// A point target (id 0: sweep front, hunted group) keeps its point in doorX/doorY.
struct Target {uint16_t id=0,kind=0;uint32_t uid=0;int x=0,y=0,doorX=0,doorY=0,defence=0;bool reachable=false;};
struct Soldier {uint16_t id=0,type=0;uint32_t uid=0;int x=0,y=0;};
enum class Phase:uint8_t {Idle,March,Assault,Return};
enum class Style:uint8_t {Raid,Assault,Pincer,Sweep,Hunt,Allied};
inline const char*styleName(Style s){static const char*names[]={"raid","assault","pincer","sweep","hunt","allied"};return names[(int)s];}
// Gardeners each style takes along (Allied without free gardeners joins as an escort): the style's base
// (raid 4, hunt 2, sweep 10, allied 6, assault/pincer 8) plus one per GardenerPer soldiers (r32).
inline int gardenerBase(Style s){switch(s){case Style::Raid:return 4;case Style::Hunt:return 2;case Style::Sweep:return 10;case Style::Allied:return 6;default:return 8;}}
inline int gardenersFor(Style s,int soldiers=0){return std::min(MaxGardeners,gardenerBase(s)+std::max(0,soldiers)/GardenerPer);}
enum class Event:uint8_t {Decline,Launch,Stage,Assault,Siege,Destroyed,Deferred,Retarget,Return,Dissolved,Lost,Adopted,Ignored,Avoid,Collapsed,Greening,Gardeners,Reclaim,Front,Hunt,Join,Call,Fire,Convoy};
// Deterministic generator owned by the plugin; the game's LCG (RVA 0xE66B10)
// drives every native AI decision and must never be consumed.
struct Random {uint32_t state=0x9e3779b9u;uint32_t next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}uint32_t bits16(){return next()>>16;}};
// A Gardener as the engine reads it: work state from its job/task (idle =
// default job 50 or a parked WAIT; working = greening job 0xAF), its work
// centre and failed search slices. reserved: the cell it greens (job 0xAF
// only); aim: the cell its search chose (job 0xAE walking to it or 0xAF
// greening it, role +0x40/+0x44 written by 0x179F80 before either), -1 none.
enum class Work:uint8_t {Idle,Busy,Searching,Working};
struct Gardener {uint16_t id=0;uint32_t uid=0;int x=0,y=0,centreX=0,centreY=0,misses=0,reservedX=-1,reservedY=-1,aimX=-1,aimY=-1;Work work=Work::Idle;};
// A walkable footprint cell next to the blocked core: the ring that must turn green.
struct RingCell {int x=0,y=0;bool dark=false,occupied=false;};
// A circle no gardener is sent into (fire zones, citadels, growing sites, cells already given out):
// hex distance <= r, or for a round (fire) keep Euclidean distance < r.
struct Keep {int x=0,y=0,r=0;bool round=false;};
inline bool kept(const std::vector<Keep>&k,int x,int y){for(auto&e:k)if(e.round?dtfire::fire2(x,y,e.x,e.y)<e.r*e.r:distance(x,y,e.x,e.y)<=e.r)return true;return false;}
// r30: a gardener the fire guard pulled out (held or free): recall episodes within BenchTicks; two
// benches it (dropped from its campaign, sent home, skipped by reclaim and assign until benchUntil).
struct GardenerWatch {uint16_t id=0;uint32_t uid=0,seenAt=0,firstAt=0,sentAt=0,benchUntil=0;int recalls=0;};
// MaxVias: the exit point plus a route's waypoints and the corners of its hex legs. PullSlots: soldiers the guard
// ordered out within GuardResend (one gate per soldier, so a soldier that turns hot is ordered out at once).
constexpr int WatchSlots=16,MaxVias=12,PullSlots=32,HopCost=20; // HopCost: a route pays this many tiles per waypoint or corner (fewer, longer legs)
struct Campaign {
 uint8_t player=0,sector=0;Phase phase=Phase::Idle;Style style=Style::Assault,lastStyle=Style::Assault;
 bool armed=false,raid=false,checked=true,split=false,hunting=false,escort=false;
 uint32_t nextCheck=0,chance=ChanceStart,since=0,started=0,targetSince=0,lastOrder=0,progress=0,deferredSince=0,orderAt=0,nextTry=0,greenSince=0,lastReclaim=0,lastRecall=0,cooldownUntil=0,quietSince=0,frontSince=0,joinedUid=0,lastFollow=0;
 uint16_t gid[MaxGardeners]={};uint32_t guid[MaxGardeners]={},gsent[MaxGardeners]={},lastTopUp=0,lastClear=0;int gcx[MaxGardeners]={},gcy[MaxGardeners]={},gardeners=0,ringDark=-1,ringSize=0,chain=0,topUps=0,lost=0;uint32_t parkedSince=0,progressAt=0;bool razed=false,holding=false;
 uint16_t initial=0;int stageX=0,stageY=0,stage2X=0,stage2Y=0,goalX=0,goalY=0,goal2X=0,goal2Y=0,orderX=0,orderY=0,frontX=0,frontY=0;Target target{};uintptr_t squad=0;Random random{};
 // r30 (transient, like every Campaign field): waypoints around the fire (march and way home), the
 // fire guard's last soldier extraction, lure count per target, recursion depth of retarget().
 int vx[MaxVias]={},vy[MaxVias]={},vias=0,viaAt=0,lures=0;uint32_t fireAt=0,lureSince=0,lureKey=0;uint8_t nesting=0;GardenerWatch watch[WatchSlots]{};
 // r30 review (transient): soldiers the guard ordered out and when; replanAt: a march or way home plans again
 // from where the guard left the army (the stored waypoints led into the fire); homeless: no safe way home
 // yet, the army holds outside the fire and plans again at retryAt.
 uint32_t pullUid[PullSlots]={},pullAt[PullSlots]={},replanAt=0,retryAt=0;bool homeless=false;
 uint32_t avoidUid[4]={},avoidUntil[4]={};
 // r32 (transient): want = gardeners this campaign keeps (its style's scaled number); legX/legY = start of the
 // first leg of the stored route; gap = farthest held gardener from the army centre at the last convoy look;
 // gdx/gdy = each gardener's last convoy cell; convoyAt = last convoy step; waitSince/waited = the army holds
 // for its gardeners since then / for so long; convoyNote = last OFFENCE_CONVOY line; rally = the army walks to
 // its gardeners first; rearX/rearY = where the gardeners wait behind the screen; flankAt = last pincer flank order.
 int want=0,legX=0,legY=0,gap=0,rearX=-1,rearY=-1,gdx[MaxGardeners]={},gdy[MaxGardeners]={};uint32_t convoyAt=0,waitSince=0,waited=0,convoyNote=0,flankAt=0;bool rally=false;
 const char*reason="";int value=0,versus=0,dissolveTries=0;
 int8_t only=-1; // offline tests only: the one style decide() may choose (-1: weighted choice)
};
struct Army {int count=0,x=0,y=0,weight=0;};
inline Army summary(const std::vector<Soldier>&m){Army a;a.count=(int)m.size();if(!a.count)return a;long sx=0,sy=0;for(auto&s:m){sx+=s.x;sy+=s.y;a.weight+=soldierWeight(s.type);}a.x=(int)(sx/a.count);a.y=(int)(sy/a.count);return a;}
inline int within(const std::vector<Soldier>&m,int x,int y,int r){int n=0;for(auto&s:m)if(distance(s.x,s.y,x,y)<=r)n++;return n;}
inline int countKind(const std::vector<Target>&t,uint16_t kind){int n=0;for(auto&b:t)if(b.kind==kind)n++;return n;}
inline bool avoided(const Campaign&c,uint32_t uid,uint32_t t){for(int i=0;i<4;i++)if(c.avoidUid[i]==uid&&t<c.avoidUntil[i])return true;return false;}
inline void avoid(Campaign&c,uint32_t uid,uint32_t t,uint32_t ticks=AvoidTicks){int k=0;for(int i=1;i<4;i++)if(c.avoidUid[i]==uid||c.avoidUntil[i]<c.avoidUntil[k])k=i;c.avoidUid[k]=uid;c.avoidUntil[k]=t+ticks;}
// A pincer army in two halves, by a stable hash of each soldier's identity (a death never swaps flanks).
inline void halves(const std::vector<Soldier>&m,std::vector<Soldier>&a,std::vector<Soldier>&b){a.clear();b.clear();for(auto&s:m)((((s.uid*2654435761u)>>31)&1)?b:a).push_back(s);}
// Soldiers holding the ordered cell (either cell of a split army).
inline int holders(const Campaign&c,const std::vector<Soldier>&m){int n=0;for(auto&s:m)if(distance(s.x,s.y,c.goalX,c.goalY)<=SiegeRadius||(c.split&&distance(s.x,s.y,c.goal2X,c.goal2Y)<=SiegeRadius))n++;return n;}
inline int needed(const std::vector<Soldier>&m){return std::max(2,((int)m.size()+3)/4);}
// r30: the citadel fire as the director sees it. On while the native
// ready-farm counter ([0x10EF7AC + DarkPlayer*0x298], -1 unreadable), the
// listed ready farms or a Farm construction site say so (the largest wins:
// fail safe; a site completes within ~40 ticks). Centres: every Temple and
// Fortress, ready or a site, at building position + (1,2). The radius never
// follows the farm count: every check uses the native maximum (FireReach) plus
// the keeps below.
struct Zone {int x=0,y=0,bx=0,by=0;}; // fire centre and the citadel's building position
struct Site {int x=0,y=0;uint16_t kind=0;}; // a Dark construction site (building position, kind)
struct Fire {bool on=false;int ready=-1,listed=0,farmSites=0;std::vector<Zone> z;};
inline void addCentre(Fire&f,int bx,int by){for(auto&z:f.z)if(z.bx==bx&&z.by==by)return;Zone z;z.x=dtfire::centreX(bx);z.y=dtfire::centreY(by);z.bx=bx;z.by=by;f.z.push_back(z);}
inline Fire fireFrom(const std::vector<Target>&all,int ready,const std::vector<Site>&sites){
 Fire f;f.ready=ready;f.listed=countKind(all,Farm);for(auto&s:sites)if(s.kind==Farm)f.farmSites++;
 f.on=ready>0||f.listed>0||f.farmSites>0;
 for(auto&b:all)if(b.kind==Temple||b.kind==Fortress)addCentre(f,b.x,b.y);
 for(auto&s:sites)if(s.kind==Temple||s.kind==Fortress)addCentre(f,s.x,s.y);
 return f;
}
// Ops: int readyFarms(); void sites(const Campaign&,std::vector<Site>&).
template<class Ops> Fire fire(const Campaign&c,Ops&ops,const std::vector<Target>&all){std::vector<Site> s;ops.sites(c,s);return fireFrom(all,ops.readyFarms(),s);}
// Squared Euclidean distance to the nearest live fire centre (NoFire while the fire is off).
inline int clear2(int x,int y,const Fire&f){if(!f.on)return NoFire;int m=NoFire;for(auto&z:f.z)m=std::min(m,dtfire::fire2(x,y,z.x,z.y));return m;}
// Strictly closer than k to a live fire centre.
inline bool closer(int x,int y,const Fire&f,int k){return clear2(x,y,f)<k*k;}
// Within FireLine: no computer unit may stand here, no gardener may aim here.
inline bool inFire(int x,int y,const Fire&f){return clear2(x,y,f)<=FireLine*FireLine;}
// The group order puts member k at click + 2 * spiral[k] (0x16A800, spiral 0xD69968): N members
// reach 2k (Euclid) with k the smallest ring count holding N (1+3k(k+1) cells), plus 2 for blocked slots.
inline int spread(int n){int k=0;while(1+3*k*(k+1)<n)k++;return 2*k+2;}
inline int soldierKeep(int n){return FireLine+spread(n);}
// A straight leg: members walk their own lines from their own slots around the start to their
// slots around the end, up to spread(n) beside the army's line, and bend a little more (soak).
inline int legKeep(int n){return FireLine+spread(n)+LegSlack;}
// A target the computers never take while the fire is on: the Temple and the Fortress, a farm
// within FarmLock of a fire centre (dtfire::farmTooClose; the Dark blockade keeps new farms out,
// only old saves can have one) and a Hall within HallLock (its ring reaches 15).
inline bool locked(const Target&b,const Fire&f){
 if(!f.on)return false;if(b.kind==Temple||b.kind==Fortress)return true;
 for(auto&z:f.z){if(b.kind==Farm&&dtfire::farmTooClose(b.x,b.y,z.bx,z.by))return true;if(b.kind==Hall&&dtfire::fire2(b.x,b.y,z.x,z.y)<HallLock*HallLock)return true;}
 return false;
}
struct Pick {bool citadel=true;int limit=-1;bool weakest=false;}; // limit: max local defence (-1: any); weakest: fall back to the least defended
// Farms first: while a reachable, unlocked Mushroom Farm stands, only unlocked
// farms are targets. Without one: the Temple/Fortress (only while the fire is
// off), then the Manacopter Halls (unlocked ones while it is on). Locked
// targets are never chosen. Nearest first. Returns an index or -1.
constexpr long HallLater=5000;
inline int chooseTarget(const std::vector<Target>&all,const Fire&f,int fromX,int fromY,const Pick&pick,const Campaign*c=nullptr,uint32_t t=0){
 auto usable=[&](const Target&b){return b.reachable&&!(c&&avoided(*c,b.uid,t));};
 bool farms=false;
 for(auto&b:all)if(b.kind==Farm&&b.reachable&&!locked(b,f))farms=true; /* avoided farms still forbid other targets; locked farms do not */
 int best=-1,low=-1;long bestScore=0,lowScore=0;
 for(int i=0;i<(int)all.size();i++){const Target&b=all[i];if(!usable(b))continue;
  if(locked(b,f))continue;
  if(farms?b.kind!=Farm:!(b.kind==Hall||(pick.citadel&&(b.kind==Temple||b.kind==Fortress))))continue;
  long score=distance(fromX,fromY,b.doorX,b.doorY)+(b.kind==Hall&&pick.citadel&&!f.on?HallLater:0);
  if(pick.limit<0||b.defence<=pick.limit){if(best<0||score<bestScore){best=i;bestScore=score;}}
  long weak=(long)b.defence*100000L+score;if(low<0||weak<lowScore){low=i;lowScore=weak;}
 }
 return best>=0?best:pick.weakest?low:-1;
}
// The two corners of the hex parallelogram of a leg a->b: units walking a shortest hex path from a to b
// only use the two hex axes around the direction a->b, so every such path lies in the parallelogram a, c1,
// b, c2 (the soak's greedy walk bows up to a quarter of the leg's hex length off the straight line). A leg
// along one hex axis has a single shortest path (c1 or c2 equals an end).
inline void corners(int ax,int ay,int bx,int by,int&c1x,int&c1y,int&c2x,int&c2y){
 int dx=bx-ax,dy=by-ay;
 if((dx>=0)==(dy>=0)){int m=abs(dx)<abs(dy)?dx:dy;c1x=ax+m;c1y=ay+m;c2x=ax+dx-m;c2y=ay+dy-m;} /* (m,m) on the diagonal, the rest on an axis */
 else{c1x=ax+dx;c1y=ay;c2x=ax;c2y=ay+dy;}
}
// Squared Euclidean distance (the fire2 metric) from the point p to the segment a-b, in the embedding X = x - y/2, Y = 0.866 y.
inline double seg2(double ax,double ay,double bx,double by,double px,double py){
 const double K=0.8660254037844386;double AX=ax-ay*0.5,AY=ay*K,DX=bx-by*0.5-AX,DY=by*K-AY,PX=px-py*0.5-AX,PY=py*K-AY,l=DX*DX+DY*DY,u=l>0?(PX*DX+PY*DY)/l:0;
 u=u<0?0:u>1?1:u;double qx=u*DX-PX,qy=u*DY-PY;return qx*qx+qy*qy;
}
// A walk a->b keeps clear of the fire: every shortest hex path from a to b (the whole parallelogram, so both
// boundary paths, and no live centre inside) stays at least min(keep, clearance(a), clearance(b)) from every
// live centre, and outside FireLine unless an end already lies within it (then no deeper than that end: a way
// out). Exact (no sampling).
inline bool legClear(int ax,int ay,int bx,int by,const Fire&f,int keep){
 if(!f.on||f.z.empty())return true;
 int ca=clear2(ax,ay,f),cb=clear2(bx,by,f);const double line=(double)FireLine*FireLine;bool deep=ca<=line||cb<=line;
 double need=std::min((double)keep*keep,(double)std::min(ca,cb))-1e-6;
 int c1x=0,c1y=0,c2x=0,c2y=0;corners(ax,ay,bx,by,c1x,c1y,c2x,c2y);
 double ux=c1x-ax,uy=c1y-ay,vx=c2x-ax,vy=c2y-ay,det=ux*vy-uy*vx;
 for(auto&z:f.z){
  if(det!=0){double ox=z.x-ax,oy=z.y-ay,p=(ox*vy-oy*vx)/det,q=(ux*oy-uy*ox)/det;if(p>0&&p<1&&q>0&&q<1)return false;}
  double d2=std::min(std::min(seg2(ax,ay,c1x,c1y,z.x,z.y),seg2(c1x,c1y,bx,by,z.x,z.y)),std::min(seg2(ax,ay,c2x,c2y,z.x,z.y),seg2(c2x,c2y,bx,by,z.x,z.y)));
  if(d2<need||(!deep&&d2<=line))return false;}
 return true;
}
// Cell nearest to the embedded point (X = x - y/2, Y = 0.866 y).
inline void cellAt(double X,double Y,int&x,int&y){y=(int)lround(Y/0.866);x=(int)lround(X+y*0.5);}
// Way out of the fire for n units around (x,y): on the ray from the nearest
// centre through (x,y) (then rays turned by +-30, +-60, ... degrees), the first
// cell at least soldierKeep(n)+2 (and 2 more than (x,y)) from every live centre
// that stand() accepts, searched within 3 tiles of the ray.
template<class Stand> bool exitPoint(int x,int y,const Fire&f,int n,Stand stand,int&ox,int&oy){
 if(!f.on||f.z.empty())return false;const Zone*zc=&f.z[0];for(auto&z:f.z)if(dtfire::fire2(x,y,z.x,z.y)<dtfire::fire2(x,y,zc->x,zc->y))zc=&z;
 double CX=zc->x-zc->y*0.5,CY=zc->y*0.866,X=(x-y*0.5)-CX,Y=y*0.866-CY;if(X*X+Y*Y<0.25){X=1;Y=0;}
 double base=atan2(Y,X);int need=std::max(soldierKeep(n)+2,(int)ceil(sqrt((double)clear2(x,y,f)))+2); /* always further out than (x,y) */
 for(int k=0;k<12;k++){double angle=base+((k+1)/2)*(k%2?1:-1)*0.5235988;
  for(int r=need;r<=need+20;r++){int cx=0,cy=0;cellAt(CX+r*cos(angle),CY+r*sin(angle),cx,cy);bool found=false;int best=0;
   for(int dy=-3;dy<=3;dy++)for(int dx=-3;dx<=3;dx++){int px=cx+dx,py=cy+dy,d=distance(cx,cy,px,py);if(d>3||(found&&d>=best)||closer(px,py,f,need)||!(stand(px,py,0)||stand(px,py,1)))continue;found=true;best=d;ox=px;oy=py;}
   if(found)return true;}}
 return false;
}
// A leg n units can walk (legKeep(n)): every shortest hex path clear, else through one corner of its
// parallelogram (two legs along hex axes, each a single hex path) that keeps dest and stand() accepts.
// (cx,cy) = that corner, -1 for a straight leg.
template<class Stand> bool hexLeg(int ax,int ay,int bx,int by,const Fire&f,int keep,int dest,Stand&stand,int&cx,int&cy){
 cx=cy=-1;if(legClear(ax,ay,bx,by,f,keep))return true;
 int p[2][2]={};corners(ax,ay,bx,by,p[0][0],p[0][1],p[1][0],p[1][1]);
 for(auto&q:p){if((q[0]==ax&&q[1]==ay)||(q[0]==bx&&q[1]==by)||closer(q[0],q[1],f,dest)||!legClear(ax,ay,q[0],q[1],f,keep)||!legClear(q[0],q[1],bx,by,f,keep)||!(stand(q[0],q[1],0)||stand(q[0],q[1],1)))continue;
  cx=q[0];cy=q[1];return true;}
 return false;
}
// The shortest way a -> b (HopCost per stop) for n units over waypoints around the live centres (24 directions, four radii;
// each keeps soldierKeep(n) and stand() accepts it) and hex legs (hexLeg). Up to MaxVias-1 cells (waypoints
// and leg corners) in wx/wy, k = how many. Fails when an end lies within FireLine (leave by exitPoint first)
// or no way fits.
template<class Stand> bool route(int ax,int ay,int bx,int by,const Fire&f,int n,Stand stand,int*wx,int*wy,int&k){
 k=0;int keep=legKeep(n),dest=soldierKeep(n),cx=-1,cy=-1;if(inFire(ax,ay,f)||inFire(bx,by,f))return false;
 if(hexLeg(ax,ay,bx,by,f,keep,dest,stand,cx,cy)){if(cx>=0){wx[0]=cx;wy[0]=cy;k=1;}return true;}
 struct Node {int x,y,from,cx,cy;double d;bool done;};std::vector<Node> v;v.push_back({ax,ay,-1,-1,-1,0,false});v.push_back({bx,by,-1,-1,-1,-1,false});
 for(auto&z:f.z)for(int r:{dest+2,dest+8,dest+16,dest+26})for(int d=0;d<24;d++){double angle=d*0.2617994;int x=0,y=0;cellAt(z.x-z.y*0.5+r*cos(angle),z.y*0.866+r*sin(angle),x,y);
  if(closer(x,y,f,dest)||!(stand(x,y,0)||stand(x,y,1)))continue;v.push_back({x,y,-1,-1,-1,-1,false});}
 auto len=[](int x0,int y0,int x1,int y1){double dx=x1-x0,dy=y1-y0;return sqrt(dx*dx-dx*dy+dy*dy);};
 for(;;){int u=-1;for(int i=0;i<(int)v.size();i++)if(!v[i].done&&v[i].d>=0&&(u<0||v[i].d<v[u].d))u=i;
  if(u<0)return false;if(u==1)break;v[u].done=true;
  for(int i=1;i<(int)v.size();i++){if(v[i].done)continue;double l=len(v[u].x,v[u].y,v[i].x,v[i].y);if(v[i].d>=0&&v[u].d+l+(i!=1?HopCost:0)>=v[i].d)continue; /* a corner never shortens a leg */
   int px=-1,py=-1;if(!hexLeg(v[u].x,v[u].y,v[i].x,v[i].y,f,keep,dest,stand,px,py))continue;
   double w=(px>=0?len(v[u].x,v[u].y,px,py)+len(px,py,v[i].x,v[i].y)+HopCost:l)+(i!=1?HopCost:0);if(v[i].d<0||v[u].d+w<v[i].d){v[i].d=v[u].d+w;v[i].from=u;v[i].cx=px;v[i].cy=py;}}}
 int bx2[2*MaxVias]={},by2[2*MaxVias]={},m=0;
 for(int i=1;i>0;i=v[i].from){if(i!=1){if(m>=2*MaxVias)return false;bx2[m]=v[i].x;by2[m]=v[i].y;m++;}if(v[i].cx>=0){if(m>=2*MaxVias)return false;bx2[m]=v[i].cx;by2[m]=v[i].cy;m++;}}
 if(m>MaxVias-1)return false;
 for(int i=0;i<m;i++){wx[i]=bx2[m-1-i];wy[i]=by2[m-1-i];}k=m;return true;
}
// The campaign's waypoints from (ax,ay) to (bx,by) for n units: the exit point first when it
// starts within GardenerKeep, then route(). False when no route was found (the exit stays, so the
// caller can hold the army outside the fire).
template<class Stand> bool plan(Campaign&c,const Fire&f,int ax,int ay,int bx,int by,int n,Stand stand){
 c.vias=0;c.viaAt=0;c.legX=ax;c.legY=ay;if(!f.on)return true; /* r32: (legX, legY) starts the first leg (convoy) */
 if(closer(ax,ay,f,GardenerKeep)){int ex=0,ey=0;if(!exitPoint(ax,ay,f,n,stand,ex,ey))return false;c.vx[0]=ex;c.vy[0]=ey;c.vias=1;ax=ex;ay=ey;}
 int wx[MaxVias]={},wy[MaxVias]={},k=0;if(!route(ax,ay,bx,by,f,n,stand,wx,wy,k))return false;
 for(int i=0;i<k&&c.vias<MaxVias;i++){c.vx[c.vias]=wx[i];c.vy[c.vias]=wy[i];c.vias++;}
 return true;
}
inline bool walking(const Campaign&c){return c.viaAt<c.vias;}
// (fx,fy) turned by angle (radians) around (cx,cy), in the embedded plane.
inline void turn(int cx,int cy,int fx,int fy,double angle,int&ox,int&oy){
 double X=(fx-fy*0.5)-(cx-cy*0.5),Y=(fy-cy)*0.866,c=cos(angle),s=sin(angle),RX=X*c-Y*s,RY=X*s+Y*c;
 oy=(int)lround(cy+RY/0.866);ox=(int)lround(RX+(cx-cy*0.5)+oy*0.5);
}
// First cell on the line from the door towards (fromX,fromY) at least minDist
// from the door, at least soldierKeep(n) from every live fire centre and with
// a clear leg to the door, passing stand(x,y,pass) (pass 0 prefers ordinary
// land, pass 1 accepts Dark land); while the fire is on and that line has no
// such cell, the line turned by +-30, +-60, +-90, +-120 degrees around the
// door. An army already closer than minDist stays where it is when that cell
// qualifies.
template<class Stand> bool stagePoint(const Target&t,const Fire&f,int fromX,int fromY,int minDist,int n,Stand stand,int&sx,int&sy){
 int keep=soldierKeep(n),leg=legKeep(n);auto safe=[&](int x,int y){return !closer(x,y,f,keep);};
 int total=distance(t.doorX,t.doorY,fromX,fromY);
 if(total<=minDist){if(!safe(fromX,fromY)||(!stand(fromX,fromY,0)&&!stand(fromX,fromY,1))||!legClear(fromX,fromY,t.doorX,t.doorY,f,leg))return false;sx=fromX;sy=fromY;return true;}
 for(int k=0;k<(f.on?9:1);k++){int tx=fromX,ty=fromY;if(k)turn(t.doorX,t.doorY,fromX,fromY,((k+1)/2)*(k%2?1:-1)*0.5235988,tx,ty);
  int steps=std::max(abs(tx-t.doorX),abs(ty-t.doorY));if(steps<=0)continue;
  for(int pass=0;pass<2;pass++)for(int i=1;i<=steps;i++){
   int x=t.doorX+(int)((long)(tx-t.doorX)*i/steps),y=t.doorY+(int)((long)(ty-t.doorY)*i/steps);
   if(distance(t.doorX,t.doorY,x,y)<minDist||!safe(x,y)||!stand(x,y,pass)||!legClear(x,y,t.doorX,t.doorY,f,leg))continue;
   sx=x;sy=y;return true;
  }}
 return false;
}
// Free cell near the door (rings 1..10) for n soldiers, closest to (fromX,fromY); while the fire
// is on only cells at least soldierKeep(n) from every centre. False when none qualifies.
template<class Free> bool assaultPoint(const Target&t,const Fire&f,int fromX,int fromY,int n,Free free,int&ax,int&ay){
 int keep=soldierKeep(n);bool found=false;long best=0;
 for(int ring=1;ring<=10;ring++)for(int dy=-ring;dy<=ring;dy++)for(int dx=-ring;dx<=ring;dx++){
  int x=t.doorX+dx,y=t.doorY+dy;if(distance(t.doorX,t.doorY,x,y)!=ring||closer(x,y,f,keep)||!free(x,y))continue;
  long score=distance(x,y,fromX,fromY);if(!found||score<best){found=true;best=score;ax=x;ay=y;}}
 return found;
}
// The cell with the most Dark fighters within HuntCluster, nearest to (fromX,fromY) on ties;
// fighters within HuntKeep of a live fire centre are ignored (soldiers chase them into the fire).
inline bool cluster(const std::vector<std::pair<int,int>>&fs,const Fire&f,int fromX,int fromY,int&x,int&y,int&n){
 bool found=false;long best=0;n=0;
 for(auto&p:fs){if(closer(p.first,p.second,f,HuntKeep))continue;int k=0;for(auto&q:fs)if(!closer(q.first,q.second,f,HuntKeep)&&distance(p.first,p.second,q.first,q.second)<=HuntCluster)k++;
  long score=(long)k*1000-distance(fromX,fromY,p.first,p.second);if(!found||score>best){found=true;best=score;x=p.first;y=p.second;n=k;}}
 return found;
}
// Ops: bool sector(const Campaign&); Config config(uint8_t player);
// Strength strength(const Campaign&); void targets(const Campaign&,std::vector<Target>&);
// bool alive(const Target&); int defenders(int x,int y,int radius,bool weighted);
// void available(const Campaign&,int x,int y,std::vector<Soldier>&) (reservoirs first, nearest to (x,y));
// int raise(Campaign&,const std::vector<Soldier>&); bool members(const Campaign&,std::vector<Soldier>&);
// void move(const Campaign&,const std::vector<Soldier>&,int x,int y,bool home);
// bool stand(const Campaign&,int x,int y,int pass); bool free(int x,int y); bool reach(const Campaign&,int x,int y) (same land region);
// bool destroy(const Campaign&,const Target&); bool dissolve(Campaign&); void report(Event,const Campaign&).
// Gardeners: void gardeners(const Campaign&,std::vector<Gardener>&) (the player's living gardeners in the
// sector's region that no campaign holds); bool gardener(const Campaign&,uint16_t id,uint32_t uid,Gardener&);
// void sendGardener(const Campaign&,const Gardener&,int x,int y) (one per order: its work centre);
// void moveGardeners(const Campaign&,const std::vector<Gardener>&,int x,int y);
// void ring(const Target&,std::vector<RingCell>&); bool beside(const Target&,const RingCell&,int&x,int&y);
// void clearFootprint(const Campaign&,const Target&,const std::vector<Soldier>&,const Fire&) (never onto a cell within FireLine);
// bool darkNear(const Campaign&,int x,int y,int radius,const std::vector<Keep>&,int&dx,int&dy) (nearest
// workable Dark cell in the sector's region outside every keep circle); bool ownDark(...) (the same, only
// cells of the player's own territory); void sites(const Campaign&,std::vector<Site>&) (the Dark player's
// paid or pending construction sites with their kind); int readyFarms() (the native ready-farm counter
// of the Dark player, -1 unreadable); bool spare(const Target&,int x,int y) (free and at least
// EscortGap from the footprint); bool parked(const Target&) (a Temple/Fortress holds entering settlers);
// void darkFighters(const Campaign&,std::vector<std::pair<int,int>>&) (visible Dark Warriors/Throwers/Shamans);
// bool leads(const Campaign&,const Target&) (no other campaign besieging that target comes first);
// bool call(const Campaign&,uint32_t t,Target&) (another computer's open call); bool announce(const Campaign&,uint32_t t,const Target&)
// (opens a call when another computer enemy of the Dark player exists).
template<class Ops> void decline(Campaign&c,uint32_t t,Ops&ops,const char*why){
 c.reason=why;ops.report(Event::Decline,c);if(c.chance<ChanceFull)c.chance=std::min<uint32_t>(ChanceFull,c.chance+ChanceStep);c.nextCheck=t+CheckEvery+(c.random.next()&127);
}
inline void toIdle(Campaign&c,uint32_t t){c.dissolveTries=0;c.phase=Phase::Idle;c.squad=0;c.progress=0;c.vias=0;c.viaAt=0;c.lures=0;c.replanAt=0;c.homeless=false;c.split=false;c.hunting=false;c.escort=false;c.deferredSince=0;c.nextTry=0;c.greenSince=0;c.ringDark=-1;c.razed=false;c.holding=false;c.gardeners=0;c.chain=0;c.quietSince=0;c.checked=true;c.target=Target{};c.rally=false;c.waitSince=0;c.waited=0;c.gap=0;c.convoyAt=0;c.rearX=c.rearY=-1;c.flankAt=0;
 c.nextCheck=t+CheckEvery+(c.random.next()&127);c.cooldownUntil=t+CooldownMin+c.random.next()%CooldownSpread;c.chance=ChanceStart;c.joinedUid=0;}
// r30 gardener tracker: the entry of gardener (id, uid), else the oldest one not benched.
inline GardenerWatch&watchOf(Campaign&c,uint16_t id,uint32_t uid,uint32_t t){
 int k=0;auto age=[&](const GardenerWatch&w){return (t<w.benchUntil?0x80000000u:0u)|(w.seenAt>>1);};
 for(int i=0;i<WatchSlots;i++){if(c.watch[i].id==id&&c.watch[i].uid==uid)return c.watch[i];if(age(c.watch[i])<age(c.watch[k]))k=i;}
 c.watch[k]=GardenerWatch{};c.watch[k].id=id;c.watch[k].uid=uid;return c.watch[k];
}
inline bool benched(const Campaign&c,uint16_t id,uint32_t uid,uint32_t t){for(auto&w:c.watch)if(w.id==id&&w.uid==uid&&t<w.benchUntil)return true;return false;}
// One guard look at a gardener in danger: 0 = an order is on its way (no new one), 1 = recall,
// 2 = bench (its second recall within BenchTicks). An episode is a run of steps in danger.
inline int recall(Campaign&c,const Gardener&g,uint32_t t){
 GardenerWatch&w=watchOf(c,g.id,g.uid,t);bool fresh=!w.seenAt||t-w.seenAt>Period;w.seenAt=t?t:1;
 if(fresh){if(!w.firstAt||t-w.firstAt>BenchTicks){w.firstAt=t?t:1;w.recalls=0;}w.recalls++;}
 else if(t-w.sentAt<GuardResend)return 0;
 w.sentAt=t;if(w.recalls>=2){w.benchUntil=t+BenchTicks;return 2;}return 1;
}
// The campaign's gardeners still alive (dead or lost ones are dropped).
template<class Ops> void living(Campaign&c,Ops&ops,std::vector<Gardener>&out){
 out.clear();int n=0;
 for(int i=0;i<c.gardeners;i++){Gardener g;if(!ops.gardener(c,c.gid[i],c.guid[i],g))continue;c.gid[n]=c.gid[i];c.guid[n]=c.guid[i];c.gsent[n]=c.gsent[i];c.gcx[n]=c.gcx[i];c.gcy[n]=c.gcy[i];c.gdx[n]=c.gdx[i];c.gdy[n]=c.gdy[i];n++;out.push_back(g);}
 c.gardeners=n;
}
inline void dropGardener(Campaign&c,int i){if(i<0||i>=c.gardeners)return;for(int k=i+1;k<c.gardeners;k++){c.gid[k-1]=c.gid[k];c.guid[k-1]=c.guid[k];c.gsent[k-1]=c.gsent[k];c.gcx[k-1]=c.gcx[k];c.gcy[k-1]=c.gcy[k];c.gdx[k-1]=c.gdx[k];c.gdy[k-1]=c.gdy[k];}c.gardeners--;}
// Take up to n free gardeners nearest to (x,y) that accept() takes and the fire guard has not
// benched. Returns how many the campaign holds now.
template<class Ops,class Accept> int assign(Campaign&c,Ops&ops,int x,int y,int n,uint32_t t,Accept accept){
 std::vector<Gardener> free;ops.gardeners(c,free);
 std::sort(free.begin(),free.end(),[&](const Gardener&a,const Gardener&b){return distance(a.x,a.y,x,y)<distance(b.x,b.y,x,y);});
 for(auto&g:free){if(c.gardeners>=std::min(n,MaxGardeners))break;bool held=false;for(int i=0;i<c.gardeners;i++)if(c.gid[i]==g.id)held=true;if(held||benched(c,g.id,g.uid,t)||!accept(g))continue;
  c.gid[c.gardeners]=g.id;c.guid[c.gardeners]=g.uid;c.gsent[c.gardeners]=0;c.gcx[c.gardeners]=-1;c.gcy[c.gardeners]=-1;c.gdx[c.gardeners]=-1;c.gdy[c.gardeners]=-1;c.gardeners++;}
 return c.gardeners;
}
template<class Ops> int assign(Campaign&c,Ops&ops,int x,int y,int n,uint32_t t){return assign(c,ops,x,y,n,t,[](const Gardener&){return true;});}
inline void resetCells(Campaign&c){for(int i=0;i<MaxGardeners;i++){c.gcx[i]=-1;c.gcy[i]=-1;c.gsent[i]=0;}c.topUps=0;c.lost=0;c.parkedSince=0;}
template<class Ops> void send(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,int x,int y,bool home){
 Army a=summary(m);c.orderAt=t;c.orderX=a.x;c.orderY=a.y;c.goalX=x;c.goalY=y;c.checked=false;c.lastOrder=t;ops.move(c,m,x,y,home);
}
// A pincer: the even half to (x,y), the odd half to (x2,y2).
template<class Ops> void sendSplit(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,int x,int y,int x2,int y2){
 std::vector<Soldier> a,b;halves(m,a,b);Army s=summary(a);c.orderAt=t;c.orderX=s.x;c.orderY=s.y;c.goalX=x;c.goalY=y;c.goal2X=x2;c.goal2Y=y2;c.checked=false;c.lastOrder=t;
 if(!a.empty())ops.move(c,a,x,y,false);if(!b.empty())ops.move(c,b,x2,y2,false);
}
// Gardeners follow the soldiers (march, way home) as one group.
template<class Ops> void follow(Campaign&c,uint32_t t,Ops&ops,int x,int y){
 std::vector<Gardener> gs;living(c,ops,gs);if(gs.empty())return;ops.moveGardeners(c,gs,x,y);for(int i=0;i<c.gardeners;i++)c.gsent[i]=t;
}
// Units the group orders spread: soldiers and the gardeners that follow them.
inline int groupSize(const Campaign&c,const std::vector<Soldier>&m){return std::max(1,std::max((int)m.size(),c.gardeners));}
// r32 convoy: the army and its gardeners march as one group (the field case: the soldiers stood at a farm while
// their gardeners trailed far behind, unprotected). Every ConvoyEvery ticks the army is ordered to the point of
// its current leg ConvoyLead tiles closer to the leg's end than the convoy's rear (the army centre or the gardeners'
// centre, whichever is farther from it), the gardeners (one id per order) to cells around the point ConvoyBehind tiles behind it, inside the
// army. While a gardener is farther than convoyLimit() from the army centre the army gets no new forward order
// and the gardeners are ordered to it; a gardener still that far after ConvoyStall ticks of waiting is left
// behind (free again). A leg (via, staging cell, home) ends only with the convoy together. SettlerValues.xml
// gives every settler type the same (empty, atoi 0 at 0x183CE1) speed byte, so soldiers and gardeners walk alike;
// the convoy does not rely on it (soaks run gardeners slower too).
// RallyGap/GatherHome: an army in the field (beyond GatherHome of home) whose gardeners' centre lies beyond
// RallyGap walks to them first (a hunt's hand-over). TopUpReach: replacements only from free gardeners this
// close to the army (others wait at home for the next convoy). EscortRadius: a waiting gardener farther from the
// army is called back to the rear point. RearStep: the rear point lies this far behind the screen towards the
// army's approach. ScreenRing/ThreatRadius: the screen stands within ScreenRing of the door on the side of the
// nearest Dark fighter within ThreatRadius. GuardWait: gardeners start greening when the screen holds, or this
// long after the assault began (never while Dark fighters are near the door).
constexpr int ConvoyLead=10,ConvoyGap=8,ConvoyBehind=3,RallyGap=30,GatherHome=30,TopUpReach=20,EscortRadius=16,RearStep=4,ScreenRing=6,ThreatRadius=30;
constexpr uint32_t ConvoyEvery=30,ConvoyStall=600,ConvoyNote=300,GuardWait=300;
inline int convoyLimit(int gardeners){return ConvoyGap+spread(std::max(1,gardeners));}
inline int gapOf(const std::vector<Gardener>&gs,int x,int y){int g=0;for(auto&q:gs)g=std::max(g,distance(q.x,q.y,x,y));return g;}
inline bool convoying(const Campaign&c){return c.gardeners>0&&!c.hunting&&!c.rally;}
// The cell of the segment (ox,oy)-(dx,dy) at hex distance rem from (dx,dy) (clamped to the segment; the hex
// distance is a norm, so it scales linearly along the segment).
inline void along(int ox,int oy,int dx,int dy,int rem,int&cx,int&cy){int len=distance(ox,oy,dx,dy);if(rem<=0||len<=0){cx=dx;cy=dy;return;}if(rem>=len){cx=ox;cy=oy;return;}
 cx=dx+(int)lround((double)(ox-dx)*rem/len);cy=dy+(int)lround((double)(oy-dy)*rem/len);}
// The i-th cell of the hex spiral around (0,0): ring k starts at k*(1,0) and walks k steps along each side.
inline void slot(int i,int&dx,int&dy){dx=dy=0;if(i<=0)return;static const int nx[6]={1,1,0,-1,-1,0},ny[6]={0,1,1,0,-1,-1};int k=1,n=i-1;while(n>=6*k){n-=6*k;k++;}int j=n/k,s=n%k;dx=k*nx[j]+s*nx[(j+2)%6];dy=k*ny[j]+s*ny[(j+2)%6];}
// The start of the current leg of the stored route.
inline void legFrom(const Campaign&c,int&x,int&y){if(c.viaAt>0&&c.viaAt<=c.vias){x=c.vx[c.viaAt-1];y=c.vy[c.viaAt-1];}else{x=c.legX;y=c.legY;}}
// Gardeners (one id per order, FORWARD) to their own cells around (x,y) at twice the hex spiral offsets (the point
// itself when that cell is not standable, keeps GardenerKeep or lies next to the target's footprint), only along
// a clear leg; take(i, g) picks who goes; one already sent close to its cell within 2*ReorderEvery is left alone.
// r32 review: spot(x, y) refuses slot cells (a rear point's cells nearer the threat than the screen); the point (x,y)
// itself is taken as it is.
template<class Ops,class Take,class Spot> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y,Take take,Spot spot){
 int sent=0;auto bad=[&](int qx,int qy){return closer(qx,qy,f,GardenerKeep)||!ops.stand(c,qx,qy,1)||(c.target.id&&!ops.spare(c.target,qx,qy));};
 for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){const Gardener&g=gs[i];if(!take(i,g))continue;int ox=0,oy=0;slot(i,ox,oy);int px=x+2*ox,py=y+2*oy;
  if(bad(px,py)||!spot(px,py)){px=x;py=y;if(bad(px,py))continue;}
  if(c.gdx[i]>=0&&distance(c.gdx[i],c.gdy[i],px,py)<2&&t-c.gsent[i]<2*ReorderEvery)continue;
  if(!legClear(g.x,g.y,px,py,f,GardenerKeep))continue; /* never across the fire: the guard brings it out first */
  ops.sendGardener(c,g,px,py);c.gdx[i]=px;c.gdy[i]=py;c.gsent[i]=t;sent++;}
 return sent;
}
template<class Ops,class Take> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y,Take take){return gather(c,t,ops,gs,f,x,y,take,[](int,int){return true;});}
template<class Ops> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y){return gather(c,t,ops,gs,f,x,y,[](int,const Gardener&){return true;});}
// The convoy is together: no gardener (held, living) farther than convoyLimit() from the army centre.
template<class Ops> bool together(Campaign&c,Ops&ops,const std::vector<Soldier>&m){
 if(!convoying(c)||m.empty())return true;std::vector<Gardener> gs;living(c,ops,gs);if(gs.empty())return true;Army a=summary(m);c.gap=gapOf(gs,a.x,a.y);return c.gap<=convoyLimit((int)gs.size());
}
// One convoy step of the army m towards (dx,dy) along the leg from (ox,oy). False when neither the leg from the
// army to its next point nor to (dx,dy) is clear of the fire (the caller plans again).
template<class Ops> bool convoy(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,int ox,int oy,int dx,int dy,bool home){
 std::vector<Gardener> gs;living(c,ops,gs);c.convoyAt=t?t:1;
 if(m.empty()){c.gap=0;if(!gs.empty())gather(c,t,ops,gs,f,dx,dy);return true;} /* the army is gone: the gardeners walk on */
 if(gs.empty()){c.gap=0;c.waitSince=0;c.waited=0;if(c.goalX!=dx||c.goalY!=dy||!c.orderAt||t-c.lastOrder>=2*ReorderEvery)send(c,t,ops,m,dx,dy,home);return true;}
 Army a=summary(m);int n=groupSize(c,m),limit=convoyLimit((int)gs.size());c.gap=gapOf(gs,a.x,a.y);
 /* An army near the fire (leaving it by its exit leg) never holds or steps short: everyone straight to (dx,dy). */
 if(f.on&&closer(a.x,a.y,f,soldierKeep(n))){c.waitSince=0;c.waited=0;if(c.goalX!=dx||c.goalY!=dy||!c.orderAt||t-c.lastOrder>=GuardResend)send(c,t,ops,m,dx,dy,home);gather(c,t,ops,gs,f,dx,dy);return true;}
 if(c.gap>limit){
  if(!c.waitSince)c.waitSince=t?t:1;c.waited=t-c.waitSince;c.lastOrder=t; /* holding is the army's order */
  if(c.waited<ConvoyStall){gather(c,t,ops,gs,f,a.x,a.y);
   if(!c.convoyNote||t-c.convoyNote>=ConvoyNote){c.convoyNote=t?t:1;Campaign note=c;note.value=0;note.versus=limit;note.goalX=a.x;note.goalY=a.y;note.reason="waiting for gardeners";ops.report(Event::Convoy,note);}
   return true;}
  /* r32 review: a gardener left behind walks home (never on to the army's waiting cell, its last work centre). */
  Strength hs=ops.strength(c);int k=0;for(int i=std::min((int)gs.size(),c.gardeners)-1;i>=0;i--)if(distance(gs[i].x,gs[i].y,a.x,a.y)>limit){dropGardener(c,i);k++;if(hs.ok)sendOut(c,ops,gs[i],f,hs.homeX,hs.homeY);}
  living(c,ops,gs);Campaign note=c;note.value=k;note.versus=limit;note.goalX=a.x;note.goalY=a.y;note.reason="gardeners left behind";c.convoyNote=t?t:1;ops.report(Event::Convoy,note);
  c.waitSince=0;c.waited=0;c.gap=gapOf(gs,a.x,a.y);
  /* r32 review: every gardener left behind on a march: never on without them (as when all died); the target is
     avoided only once the army has left home (stragglers at home say nothing about the target). */
  if(gs.empty()){if(c.phase==Phase::March&&!c.hunting&&!c.escort){if(c.target.id&&(!hs.ok||distance(a.x,a.y,hs.homeX,hs.homeY)>GatherHome)){avoid(c,c.target.uid,t,LostAvoid);c.reason="gardeners left behind";ops.report(Event::Avoid,c);}startReturn(c,t,ops,m,"gardeners left behind");}return true;}}
 c.waitSince=0;c.waited=0;
 long gx=0,gy=0;for(auto&g:gs){gx+=g.x;gy+=g.y;}int rear=std::max(distance(a.x,a.y,dx,dy),distance((int)(gx/(long)gs.size()),(int)(gy/(long)gs.size()),dx,dy)); /* the rear group: the army or its gardeners' centre */
 int cx=0,cy=0,bx=0,by=0;along(ox,oy,dx,dy,rear-ConvoyLead,cx,cy);along(ox,oy,dx,dy,rear-ConvoyLead+ConvoyBehind,bx,by);
 /* r32 review: the army goes to its next point only along a clear leg, else straight to (dx,dy) along a clear leg. */
 if(f.on&&!legClear(a.x,a.y,cx,cy,f,legKeep(n))){if(!legClear(a.x,a.y,dx,dy,f,legKeep(n)))return false;cx=dx;cy=dy;}
 if(!closer(cx,cy,f,soldierKeep(n))&&(distance(cx,cy,c.goalX,c.goalY)>=2||!c.orderAt||t-c.lastOrder>=2*ReorderEvery))send(c,t,ops,m,cx,cy,home);
 gather(c,t,ops,gs,f,bx,by);
 return true;
}
// The army and its gardeners on to (x,y): a convoy step, or a plain order without gardeners.
template<class Ops> void advance(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,int x,int y,bool home){
 if(!convoying(c)){if(!m.empty())send(c,t,ops,m,x,y,home);if(!c.rally)follow(c,t,ops,x,y);return;}
 int ox=0,oy=0;legFrom(c,ox,oy);if(!convoy(c,t,ops,m,f,ox,oy,x,y,home))c.replanAt=t+Period;
}
// The way home from (fx,fy) for the army and its gardeners: routed around the fire (plan()). Without
// a safe way the army is never sent straight home: it walks out to the exit point (when it stands
// near the fire) or holds where it is, and plans again after RetryPlan ticks (homeless).
template<class Ops> void homeward(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,const Strength&s,int fx,int fy){
 auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};
 if(plan(c,f,fx,fy,s.homeX,s.homeY,groupSize(c,m),stand)){c.homeless=false;int x=walking(c)?c.vx[0]:s.homeX,y=walking(c)?c.vy[0]:s.homeY;advance(c,t,ops,m,f,x,y,true);return;} /* r32: home as a convoy */
 bool first=!c.homeless;c.homeless=true;c.retryAt=t+RetryPlan;c.lastOrder=t;int x=-1,y=-1;
 if(walking(c)){x=c.vx[0];y=c.vy[0];if(!m.empty())send(c,t,ops,m,x,y,true);follow(c,t,ops,x,y);}
 c.vias=0;c.viaAt=0;
 if(first){Campaign note=c;note.value=(int)m.size();note.versus=0;note.goalX=x;note.goalY=y;note.reason="no way home outside the fire defence, holding";ops.report(Event::Fire,note);}
}
template<class Ops> void homeward(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,const Strength&s){
 if(!m.empty()){Army a=summary(m);homeward(c,t,ops,m,f,s,a.x,a.y);return;}
 std::vector<Gardener> gs;living(c,ops,gs);if(gs.empty())return;long sx=0,sy=0;for(auto&g:gs){sx+=g.x;sy+=g.y;}
 homeward(c,t,ops,m,f,s,(int)(sx/(long)gs.size()),(int)(sy/(long)gs.size()));
}
// Home, around the fire (homeward()).
template<class Ops> void startReturn(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const char*why){
 c.reason=why;c.phase=Phase::Return;c.since=t;c.vias=0;c.viaAt=0;c.split=false;c.hunting=false;c.rally=false;c.waitSince=0;c.waited=0;c.replanAt=0;c.homeless=false;Strength s=ops.strength(c);ops.report(Event::Return,c);
 if(!s.ok)return;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);homeward(c,t,ops,m,f,s);
}
template<class Ops> bool retarget(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,bool take=false);
// Free cell near (x0,y0) for an escort (rings 1..10): ordinary land first, then Dark land; at least
// keep from every live fire centre; closest to (fromX,fromY).
template<class Ops> bool holdPoint(const Campaign&c,Ops&ops,const Fire&f,int x0,int y0,int fromX,int fromY,int keep,int&hx,int&hy){
 for(int pass=0;pass<2;pass++){bool found=false;long best=0;
  for(int ring=1;ring<=10;ring++)for(int dy=-ring;dy<=ring;dy++)for(int dx=-ring;dx<=ring;dx++){int x=x0+dx,y=y0+dy;if(distance(x0,y0,x,y)!=ring||closer(x,y,f,keep)||!ops.stand(c,x,y,pass))continue;
   long score=ring*100L+distance(x,y,fromX,fromY);if(!found||score<best){found=true;best=score;hx=x;hy=y;}}
  if(found)return true;}
 return false;
}
// r32 screen: a cell within ScreenRing of the door (rings 2..ScreenRing) nearest to the threat (tx,ty), at least
// soldierKeep(n) from every live fire centre, that free() accepts.
template<class Free> bool screenPoint(const Target&t,const Fire&f,int tx,int ty,int n,Free free,int&ax,int&ay){
 int keep=soldierKeep(n);bool found=false;long best=0;
 for(int ring=2;ring<=ScreenRing;ring++)for(int dy=-ring;dy<=ring;dy++)for(int dx=-ring;dx<=ring;dx++){
  int x=t.doorX+dx,y=t.doorY+dy;if(distance(t.doorX,t.doorY,x,y)!=ring||closer(x,y,f,keep)||!free(x,y))continue;
  long score=distance(x,y,tx,ty)*16L+ring;if(!found||score<best){found=true;best=score;ax=x;ay=y;}}
 return found;
}
// The threat a screen faces: the nearest Dark fighter within ThreatRadius of the door (fighters within HuntKeep
// of a live fire centre are left alone: the screen never leans towards the fire).
template<class Ops> bool threat(const Campaign&c,Ops&ops,const Fire&f,int&tx,int&ty){
 std::vector<std::pair<int,int>> fs;ops.darkFighters(c,fs);bool found=false;int best=0;
 for(auto&p:fs){int d=distance(p.first,p.second,c.target.doorX,c.target.doorY);if(d>ThreatRadius||closer(p.first,p.second,f,HuntKeep)||(found&&d>=best))continue;found=true;best=d;tx=p.first;ty=p.second;}
 return found;
}
// r32: the gardeners wait behind the screen: RearStep tiles from the screen cell (x,y) towards (fx,fy) (where the
// army came from), a spare cell keeping GardenerKeep, else the screen cell. Gardeners without a ring cell that are
// not greening go there (one id per order).
// r32 review: facing a threat (tx,ty) the rear point lies on the far side of the screen: RearStep tiles from the
// screen straight away from the threat, else the spare cell within RearStep+2 of the screen nearest that point
// that is farther from the threat than the screen by at least RearStep/2, else the screen cell itself (never
// between the screen and the threat).
template<class Ops> void rearPoint(const Campaign&c,Ops&ops,const Fire&f,int x,int y,int fx,int fy,bool th,int tx,int ty,int&rx,int&ry){
 auto bad=[&](int px,int py){return closer(px,py,f,GardenerKeep)||!ops.spare(c.target,px,py);};
 rx=x;ry=y;int len=th?distance(x,y,tx,ty):0;
 if(!th||len<=0){along(fx,fy,x,y,RearStep,rx,ry);if(bad(rx,ry)){rx=x;ry=y;}return;}
 int ix=x+(int)lround((double)(x-tx)*RearStep/len),iy=y+(int)lround((double)(y-ty)*RearStep/len);
 if(!bad(ix,iy)&&distance(ix,iy,tx,ty)>=len+RearStep/2){rx=ix;ry=iy;return;}
 bool found=false;long best=0;
 for(int ring=1;ring<=RearStep+2;ring++)for(int dy=-ring;dy<=ring;dy++)for(int dx=-ring;dx<=ring;dx++){int px=x+dx,py=y+dy;
  if(distance(x,y,px,py)!=ring||distance(px,py,tx,ty)<len+RearStep/2||bad(px,py))continue;
  long score=distance(px,py,ix,iy);if(!found||score<best){found=true;best=score;rx=px;ry=py;}}
 if(!found){rx=x;ry=y;}
}
template<class Ops> void rearGuard(Campaign&c,uint32_t t,Ops&ops,const Fire&f,int x,int y,int fx,int fy,bool th=false,int tx=0,int ty=0){
 int rx=x,ry=y;rearPoint(c,ops,f,x,y,fx,fy,th,tx,ty,rx,ry);
 c.rearX=rx;c.rearY=ry;if(!c.gardeners)return;std::vector<Gardener> gs;living(c,ops,gs);
 int reach0=th?distance(x,y,tx,ty):0;
 gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&g){return c.gcx[i]<0&&g.work!=Work::Working;},[&](int px,int py){return !th||distance(px,py,tx,ty)>reach0;});
}
// March orders go to the next waypoint, then the staging cell: as a convoy with the gardeners (r32), the army
// alone while it walks to its gardeners (rally) or hunts. A pincer's gardeners march with its first half, the
// second half goes straight to its own staging cell. The assault cell keeps soldierKeep(N) while the fire is on;
// without one the target is avoided (LostAvoid) and the army re-targets (never the door inside the fire). An
// army facing Dark fighters near the door takes a screen cell on their side (r32); the gardeners wait behind it.
template<class Ops> void order(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.phase==Phase::March){
  if(c.split){
   if(!convoying(c)){sendSplit(c,t,ops,m,c.stageX,c.stageY,c.stage2X,c.stage2Y);follow(c,t,ops,c.stageX,c.stageY);return;}
   std::vector<Soldier> a,b;halves(m,a,b);
   if(!b.empty()&&(c.goal2X!=c.stage2X||c.goal2Y!=c.stage2Y||!c.flankAt||t-c.flankAt>=2*ReorderEvery)){c.goal2X=c.stage2X;c.goal2Y=c.stage2Y;c.flankAt=t?t:1;ops.move(c,b,c.stage2X,c.stage2Y,false);}
   std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
   if(!convoy(c,t,ops,a.empty()?b:a,f,c.legX,c.legY,c.stageX,c.stageY,false))c.replanAt=t+Period;return;}
  int x=walking(c)?c.vx[c.viaAt]:c.stageX,y=walking(c)?c.vy[c.viaAt]:c.stageY;
  if(!convoying(c)){send(c,t,ops,m,x,y,false);return;} /* hunting, a rally, or no gardeners */
  std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);int ox=0,oy=0;legFrom(c,ox,oy);
  if(!convoy(c,t,ops,m,f,ox,oy,x,y,false))c.replanAt=t+Period;return;}
 if(!c.target.id){send(c,t,ops,m,c.target.doorX,c.target.doorY,false);return;} /* hunted group */
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 auto spot=[&](int fx,int fy,int n,int&x,int&y){x=c.target.doorX;y=c.target.doorY;
  return assaultPoint(c.target,f,fx,fy,n,[&](int px,int py){return ops.spare(c.target,px,py);},x,y)||assaultPoint(c.target,f,fx,fy,n,[&](int px,int py){return ops.free(px,py);},x,y)||!f.on;};
 int x=0,y=0,x2=0,y2=0,tx=0,ty=0;bool ok=false;Army a=summary(m);bool th=threat(c,ops,f,tx,ty);
 if(c.split){std::vector<Soldier> h,k;halves(m,h,k);ok=spot(c.stageX,c.stageY,std::max(1,(int)h.size()),x,y)&&spot(c.stage2X,c.stage2Y,std::max(1,(int)k.size()),x2,y2);}
 else{int n=std::max(1,(int)m.size());ok=(th&&screenPoint(c.target,f,tx,ty,n,[&](int px,int py){return ops.spare(c.target,px,py);},x,y))||spot(a.x,a.y,n,x,y);}
 if(!ok){avoid(c,c.target.uid,t,LostAvoid);c.reason="no safe assault cell";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}
 if(c.split)sendSplit(c,t,ops,m,x,y,x2,y2);else send(c,t,ops,m,x,y,false);
 rearGuard(c,t,ops,f,x,y,a.x,a.y,th,tx,ty); /* r32 review: behind the screen, away from the threat */
}
// March to a staging cell before the target (routed around the fire), or
// straight to the assault when the army is close and the leg is clear. A pincer
// takes two staging cells, the line from home turned by +-60 degrees around
// the door; it marches as one army when they do not fit. While the fire is on
// and no safe approach exists the target is avoided and the army goes home.
template<class Ops> bool approach(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const std::vector<Target>&all,int fromX,int fromY){
 auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};int sx=0,sy=0;Army a=summary(m);Fire f=fire(c,ops,all);int n=groupSize(c,m);
 bool pincer=c.split;c.split=false;c.vias=0;c.viaAt=0;c.replanAt=0;c.since=t;c.progressAt=t;c.greenSince=0;c.ringDark=-1;c.razed=false;c.holding=false;c.rally=false;c.waitSince=0;c.waited=0;c.rearX=c.rearY=-1;c.flankAt=0;c.legX=a.x;c.legY=a.y;
 resetCells(c); /* a new target: every gardener gets a cell again */
 /* r32 rally: an army in the field whose gardeners stand far away (a hunt's hand-over) walks to them first; then approach() again. */
 if(convoying(c)&&!c.escort&&c.nesting<3){std::vector<Gardener> gs;living(c,ops,gs);
  if(!gs.empty()){long gx=0,gy=0;for(auto&g:gs){gx+=g.x;gy+=g.y;}int cx=(int)(gx/(long)gs.size()),cy=(int)(gy/(long)gs.size());Strength s=ops.strength(c);
   if(distance(a.x,a.y,cx,cy)>RallyGap&&(!s.ok||distance(a.x,a.y,s.homeX,s.homeY)>GatherHome)){int rx=cx,ry=cy;
    if(holdPoint(c,ops,f,cx,cy,a.x,a.y,soldierKeep(n),rx,ry)&&plan(c,f,a.x,a.y,rx,ry,n,stand)){c.rally=true;c.split=false;c.phase=Phase::March;c.stageX=rx;c.stageY=ry;
     Campaign note=c;note.gap=distance(a.x,a.y,cx,cy);note.value=0;note.versus=RallyGap;note.goalX=rx;note.goalY=ry;note.reason="rally to the gardeners";c.convoyNote=t?t:1;ops.report(Event::Convoy,note);order(c,t,ops,m);return true;}
    c.vias=0;c.viaAt=0;}}}
 if(pincer&&c.target.id){int half=std::max((a.count+1)/2,std::max(1,c.gardeners)),leg=legKeep(half),lx=0,ly=0,rx=0,ry=0,ax=0,ay=0,bx=0,by=0;
  turn(c.target.doorX,c.target.doorY,fromX,fromY,PincerAngle,lx,ly);turn(c.target.doorX,c.target.doorY,fromX,fromY,-PincerAngle,rx,ry);
  if(stagePoint(c.target,f,lx,ly,StageDistance,half,stand,ax,ay)&&stagePoint(c.target,f,rx,ry,StageDistance,half,stand,bx,by)&&distance(ax,ay,bx,by)>=PincerGap&&legClear(a.x,a.y,ax,ay,f,leg)&&legClear(a.x,a.y,bx,by,f,leg)){
   c.split=true;c.phase=Phase::March;c.stageX=ax;c.stageY=ay;c.stage2X=bx;c.stage2Y=by;ops.report(Event::Stage,c);order(c,t,ops,m);return true;}
  if(c.style==Style::Pincer)c.style=Style::Assault; /* no room for two flanks: one army */
 }
 bool legOk=legClear(a.x,a.y,c.target.doorX,c.target.doorY,f,legKeep(std::max(1,a.count)));
 bool direct=distance(a.x,a.y,c.target.doorX,c.target.doorY)<=AssaultDistance&&legOk&&together(c,ops,m),staged=false; /* r32: scattered gardeners: march (gather) first */
 if(!direct&&stagePoint(c.target,f,fromX,fromY,StageDistance,n,stand,sx,sy)){
  if(plan(c,f,a.x,a.y,sx,sy,n,stand)){staged=true;c.phase=Phase::March;c.stageX=sx;c.stageY=sy;ops.report(Event::Stage,c);}
  else c.vias=0;
 }
 if(!staged){
  if(!direct&&f.on&&!legOk){avoid(c,c.target.uid,t);c.reason="no way outside the fire defence";ops.report(Event::Avoid,c);startReturn(c,t,ops,m,"no way outside the fire defence");return false;}
  c.phase=Phase::Assault;ops.report(Event::Assault,c);
 }
 if(c.phase==Phase::Assault&&c.style==Style::Sweep&&!c.target.id){c.lastOrder=0;return true;} /* sweep() places the escort */
 order(c,t,ops,m);return c.phase!=Phase::Return&&c.phase!=Phase::Idle;
}
// A sweep goes back to its front (after a building there fell or timed out).
inline void resume(Campaign&c,uint32_t t){c.target=Target{};c.target.doorX=c.frontX;c.target.doorY=c.frontY;c.phase=Phase::Assault;c.split=false;c.vias=0;c.viaAt=0;c.replanAt=0;c.lastOrder=0;c.frontSince=t;c.ringDark=-1;resetCells(c);}
inline int defenceLimit(const Config&cfg,int weight){return cfg.thresholdPartial>0?(int)((int64_t)weight*100/cfg.thresholdPartial):weight;}
inline bool light(const Campaign&c){return c.style==Style::Raid||c.style==Style::Hunt;}
// Pick the next target from the army's position: raids and hunts keep to what
// they can beat (a raid goes home after RaidChain buildings); the others take
// the least defended when nothing is weak enough. A sweep without a target
// goes back to its front. Locked targets (fire) are never chosen.
template<class Ops> bool retarget(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,bool take){
 c.split=false;
 if(c.style==Style::Raid&&c.chain>=RaidChain){startReturn(c,t,ops,m,"raid done");return false;}
 if(c.nesting>=3){startReturn(c,t,ops,m,"no safe target");return false;} /* order() -> retarget() -> approach() -> order() */
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Army a=summary(m);Config cfg=ops.config(c.player);
 Pick pick;pick.citadel=true;pick.limit=defenceLimit(cfg,a.weight);pick.weakest=!light(c);
 int i=chooseTarget(all,f,a.x,a.y,pick,&c,t);
 if(c.style==Style::Sweep&&(i<0||distance(all[i].x,all[i].y,c.frontX,c.frontY)>SweepTarget)){resume(c,t);return true;} /* a sweep stays on its front */
 if(i<0){startReturn(c,t,ops,m,c.style==Style::Hunt?"hunt over":light(c)?"raid found no weak target":"no target left");return false;}
 c.target=all[i];c.targetSince=t;c.deferredSince=0;c.nextTry=0;
 if(take&&!c.gardeners){if(!c.want)c.want=gardenersFor(c.style,(int)m.size());assign(c,ops,a.x,a.y,c.want,t);if(!c.gardeners){startReturn(c,t,ops,m,c.style==Style::Hunt?"hunt over":"no gardeners left");return false;}} /* r32: nearest the army; approach() walks the army to them */
 c.nesting++;bool ok=approach(c,t,ops,m,all,a.x,a.y);c.nesting--;return ok;
}
// A situation the style choice weighs (one entry per style).
struct Option {Style style=Style::Raid;int weight=0,target=-1,take=0,want=0;};
// Weighted choice with the plugin's generator; the style used last counts half.
inline int chooseStyle(Campaign&c,const Option*o,int n){
 if(c.only>=0){for(int i=0;i<n;i++)if((int)o[i].style==c.only&&o[i].weight>0)return i;return -1;}
 int total=0,w[8]={};for(int i=0;i<n&&i<8;i++){w[i]=o[i].weight>0?(o[i].style==c.lastStyle?(o[i].weight+1)/2:o[i].weight):0;total+=w[i];}
 if(total<=0)return -1;int r=(int)(c.random.next()%(uint32_t)total);for(int i=0;i<n&&i<8;i++){if(r<w[i])return i;r-=w[i];}return -1;
}
// Circles no gardener is sent into: the live fire (round, GardenerKeep from each centre),
// Temples/Fortresses, growing sites.
template<class Ops> void keeps(const Campaign&c,Ops&ops,const std::vector<Target>&all,const Fire&f,std::vector<Keep>&out){
 out.clear();if(f.on)for(auto&z:f.z)out.push_back({z.x,z.y,GardenerKeep,true});
 for(auto&b:all)if(b.kind==Temple||b.kind==Fortress)out.push_back({b.x,b.y,ops.parked(b)?ParkedKeep:CitadelKeep});
 std::vector<Site> sites;ops.sites(c,sites);for(auto&p:sites)out.push_back({p.x,p.y,SiteKeep});
}
// Sweep fronts keep FrontKeep from each live centre (the gardeners green SweepRadius around them).
inline void frontKeeps(const Fire&f,std::vector<Keep>&keep){if(f.on)for(auto&z:f.z)keep.push_back({z.x,z.y,FrontKeep,true});}
template<class Ops> void decide(Campaign&c,uint32_t t,Ops&ops){
 Config cfg=ops.config(c.player);
 if(!cfg.ok||cfg.mode<=0||t<=c.nextCheck||t<firstAttack(cfg.delay)||t<c.cooldownUntil)return;
 // F and Str are zeroed by the attack agent's reset (load, end of a native
 // attack) until CAIAgentSquads writes them again: not known yet, no failure.
 Strength s=ops.strength(c);if(s.ok&&!s.fighters&&!s.strength){c.nextCheck=t+UnknownRetry;return;}
 if(!s.ok){decline(c,t,ops,"no sector");return;}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 // Another computer called for an assault: join it without waiting for chance (never a target the fire now locks).
 Target call;bool allied=ops.call(c,t,call)&&call.uid!=c.joinedUid;
 if(allied){allied=false;for(auto&b:all)if(b.id==call.id&&b.uid==call.uid&&b.reachable&&!locked(b,f)){call=b;allied=true;}}
 if(!allied&&c.random.bits16()>=c.chance){decline(c,t,ops,"chance");return;}
 std::vector<Soldier> free;ops.available(c,s.homeX,s.homeY,free);int avail=(int)free.size(),army=avail-std::max(HomeReserve,avail/6);
 if(army<MinArmy){c.value=avail;decline(c,t,ops,"no spare soldiers");return;}
 std::vector<Gardener> spare;ops.gardeners(c,spare);int gfree=0;for(auto&g:spare)if(!benched(c,g.id,g.uid,t))gfree++;
 int cap=gardenersWanted(avail);auto wantOf=[&](Style st,int take){return std::min(cap,gardenersFor(st,take));}; /* r32: gardeners grow with the soldiers a style takes */
 auto weightOf=[&](int n){int w=0;for(int k=0;k<n&&k<avail;k++)w+=soldierWeight(free[k].type);return w;};
 c.value=cfg.thresholdTotal>0?(int)((int64_t)s.strength*100/cfg.thresholdTotal):s.strength;c.versus=s.darkValue;
 Option o[6];int n=0,any=chooseTarget(all,f,s.homeX,s.homeY,Pick{},&c,t),fx=0,fy=0,hx=0,hy=0,hn=0;
 bool farms=false;for(auto&b:all)if(b.kind==Farm&&!locked(b,f))farms=true;
 bool lockedOnly=false;if(any<0)for(auto&b:all)if(b.kind==Farm&&b.reachable&&locked(b,f))lockedOnly=true; /* only farms the fire locks are left */
 if(allied){Option&a=o[n++];a.style=Style::Allied;a.weight=1;a.take=army;a.want=gfree>=MinGardeners?wantOf(Style::Allied,army):0;a.target=-2;}
 else{
  // Raid: a few soldiers against the weakest building they can beat.
  {Option&r=o[n++];r.style=Style::Raid;r.take=raidSize(army);r.want=wantOf(Style::Raid,r.take);Pick p;p.limit=defenceLimit(cfg,weightOf(r.take));r.target=chooseTarget(all,f,s.homeX,s.homeY,p,&c,t);
   if(r.target>=0&&gfree>=MinGardeners)r.weight=army<AssaultMin?5:2;}
  // Assault / pincer: everything but the home reserve, farms then the citadel.
  int main=-1;if(cfg.mode>=2&&gfree>=MinGardeners){Pick p;p.limit=defenceLimit(cfg,weightOf(army));p.weakest=true;main=chooseTarget(all,f,s.homeX,s.homeY,p,&c,t);}
  {Option&a=o[n++];a.style=Style::Assault;a.take=army;a.want=wantOf(Style::Assault,army);a.target=main;if(main>=0)a.weight=(army>=AssaultMin?4:1)+(!farms&&(all[main].kind==Temple||all[main].kind==Fortress)?3:0);}
  {Option&p=o[n++];p.style=Style::Pincer;p.take=army;p.want=wantOf(Style::Pincer,army);p.target=main;if(main>=0&&army>=PincerMin)p.weight=3;}
  // Sweep: an escort and many gardeners push into the nearest Dark land (fronts outside FrontKeep).
  {Option&w=o[n++];w.style=Style::Sweep;w.take=escortSize(army);w.want=wantOf(Style::Sweep,w.take);std::vector<Keep> keep;keeps(c,ops,all,f,keep);frontKeeps(f,keep);
   if(gfree>=SweepGardeners&&ops.darkNear(c,s.homeX,s.homeY,SweepSearch,keep,fx,fy))w.weight=3+(any<0?2:0);}
  // Hunt: the army goes for the largest group of Dark fighters well outside the fire.
  {Option&h=o[n++];h.style=Style::Hunt;h.take=huntSize(army);h.want=wantOf(Style::Hunt,h.take);std::vector<std::pair<int,int>> fs;ops.darkFighters(c,fs);
   if(cluster(fs,f,s.homeX,s.homeY,hx,hy,hn)&&hn>=HuntMin&&ops.reach(c,hx,hy)&&weightOf(h.take)>=3*hn&&legClear(s.homeX,s.homeY,hx,hy,f,legKeep(h.take))){h.weight=2+(distance(hx,hy,s.homeX,s.homeY)<=50?2:0);h.target=-3;}}
 }
 int k=chooseStyle(c,o,n);
 if(k<0){decline(c,t,ops,gfree<MinGardeners?"no gardeners":lockedOnly?"farms only inside the fire defence":any<0?"no reachable Dark building":"too weak for every attack");return;}
 Option pick=o[k];free.resize(std::max(0,std::min(pick.take,avail)));
 c.style=pick.style;c.raid=light(c);c.split=pick.style==Style::Pincer;c.hunting=pick.style==Style::Hunt;c.escort=pick.style==Style::Allied&&!pick.want;c.chain=0;c.quietSince=0;c.lastFollow=0;c.lures=0;c.vias=0;c.viaAt=0;
 if(pick.style==Style::Allied){c.target=call;c.joinedUid=call.uid;}
 else if(pick.style==Style::Sweep){c.target=Target{};c.target.doorX=c.frontX=fx;c.target.doorY=c.frontY=fy;c.frontSince=t;}
 else if(pick.style==Style::Hunt){c.target=Target{};c.target.doorX=hx;c.target.doorY=hy;c.value=hn;}
 else{c.target=all[pick.target];if(pick.style==Style::Raid){c.versus=c.target.defence;}}
 int got=ops.raise(c,free);if(got<=0){decline(c,t,ops,"squad not created");return;}
 c.gardeners=0;c.want=pick.want;if(pick.style!=Style::Hunt)assign(c,ops,s.homeX,s.homeY,pick.want,t); /* a hunt takes its gardeners at the hand-over */
 c.initial=(uint16_t)got;c.started=t;c.targetSince=t;c.deferredSince=0;c.nextTry=0;c.lastStyle=pick.style;c.reason=styleName(pick.style);ops.report(pick.style==Style::Allied?Event::Join:Event::Launch,c);
 std::vector<Soldier> m;if(!ops.members(c,m)){c.reason="squad vanished";ops.report(Event::Lost,c);toIdle(c,t);return;}
 approach(c,t,ops,m,all,s.homeX,s.homeY);
 if((c.style==Style::Assault||c.style==Style::Pincer)&&c.target.id&&c.phase!=Phase::Return&&ops.announce(c,t,c.target))ops.report(Event::Call,c);
}
// A gardener leaving the fire: to (x,y) when that cell keeps GardenerKeep and the leg there is
// clear, else to the exit point; false when neither exists (it is tried again next step).
template<class Ops> bool sendOut(const Campaign&c,Ops&ops,const Gardener&g,const Fire&f,int x,int y){
 auto stand=[&](int px,int py,int pass){return ops.stand(c,px,py,pass);};
 if(!(x>=0&&y>=0&&!closer(x,y,f,GardenerKeep)&&legClear(g.x,g.y,x,y,f,GardenerKeep))&&!exitPoint(g.x,g.y,f,1,stand,x,y))return false;
 ops.sendGardener(c,g,x,y);return true;
}
// Free gardeners of the player take back land, in every phase (they never
// touch the campaign's own state): Dark ground inside the player's territory
// first, the Dark border after ReclaimStart; never into fire, next to Dark
// fighters, near a Temple/Fortress or a growing site. Every step (r30), every
// free gardener whose own cell or aimed cell lies within FireLine walks out
// (idle ones too; benched ones home); one near a Temple/Fortress still parking
// entering settlers (a native collapse would leave those settlers off the map)
// walks home. Reclaim passes are staggered by player and sector; at most
// ReclaimPerPass searches may fail.
template<class Ops> void reclaim(Campaign&c,uint32_t t,Ops&ops){
 Config cfg=ops.config(c.player);if(!cfg.ok)return;Strength s=ops.strength(c);if(!s.ok)return;
 std::vector<Gardener> gs;ops.gardeners(c,gs);if(gs.empty())return;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Campaign note=c;note.value=0;note.versus=0;
 std::vector<Keep> keep;bool keepReady=false;
 auto keepNow=[&](){if(keepReady)return;keepReady=true;keeps(c,ops,all,f,keep);std::vector<std::pair<int,int>> fighters;ops.darkFighters(c,fighters);for(auto&q:fighters)keep.push_back({q.first,q.second,DefenceRadius});};
 std::vector<std::pair<int,int>> parked;for(auto&b:all)if((b.kind==Temple||b.kind==Fortress)&&ops.parked(b))parked.push_back({b.x,b.y});
 int out=0,bench=0,back=0;
 for(auto&g:gs){
  if(inFire(g.x,g.y,f)||(g.aimX>=0&&inFire(g.aimX,g.aimY,f))){int r=recall(c,g,t);if(!r)continue;int x=s.homeX,y=s.homeY;
   if(r==1){keepNow();int dx=0,dy=0;if(ops.darkNear(c,g.x,g.y,ReclaimRadius,keep,dx,dy)){x=dx;y=dy;}}else bench++;
   if(sendOut(c,ops,g,f,x,y))out++;continue;}
  if(g.work==Work::Idle||(g.work==Work::Busy&&distance(g.centreX,g.centreY,s.homeX,s.homeY)<=2))continue;
  bool parkedBy=false;for(auto&p:parked)if(distance(g.x,g.y,p.first,p.second)<=ParkedKeep)parkedBy=true;
  if(parkedBy&&sendOut(c,ops,g,f,s.homeX,s.homeY))back++;}
 if(out){note.value=out;note.versus=bench;note.goalX=note.goalY=-1;note.reason="gardener out of the fire defence";ops.report(Event::Fire,note);}
 if(back){note.value=back;note.versus=0;note.reason="gardener called back";ops.report(Event::Reclaim,note);}
 /* Staggered by player and sector, so the computers do not all search on the same tick. */
 uint32_t phase=(uint32_t)(c.player*7+c.sector*3);if((t/Period+phase)%(ReclaimEvery/Period))return;c.lastReclaim=t;
 bool border=cfg.mode>0&&t>=ReclaimStart;keepNow();int sent=0,own=0;
 int failed=0;
 size_t start=(size_t)((t/ReclaimEvery)%gs.size());
 for(size_t k=0;k<gs.size();k++){const Gardener&g=gs[(start+k)%gs.size()];if(sent>=ReclaimPerPass||failed>=ReclaimPerPass)break;bool stuck=g.work==Work::Idle||(g.work==Work::Searching&&g.misses>SearchMisses);if(!stuck||benched(c,g.id,g.uid,t)||inFire(g.x,g.y,f))continue;
  int dx=0,dy=0;bool mine=ops.ownDark(c,g.x,g.y,OwnRadius,keep,dx,dy);
  if(!mine&&(!border||!ops.darkNear(c,g.x,g.y,ReclaimRadius,keep,dx,dy)||distance(dx,dy,s.homeX,s.homeY)>ReclaimHome)){failed++;continue;}
  if(ops.defenders(dx,dy,DefenceRadius,false))continue;
  if(!legClear(g.x,g.y,dx,dy,f,GardenerKeep)){failed++;continue;} /* never across the fire */
  ops.sendGardener(c,g,dx,dy);sent++;if(mine)own++;keep.push_back({dx,dy,SpreadKeep});note.goalX=dx;note.goalY=dy;}
 if(sent){note.value=sent;note.versus=own;note.reason=own?"own territory":"border";ops.report(Event::Reclaim,note);}
}
// The fire guard, every director step of a campaign while the fire is on: a
// citadel target (or a target the fire now locks) is left; every soldier
// standing within FireLine that was not ordered out within GuardResend gets an
// order out to the exit point (one gate per soldier: a soldier that turns hot
// is ordered out in the same step). A march or a way home then plans again
// from where the guard leaves the army (replanAt). Extraction episodes (at most
// one per GuardResend) are counted per target, sweep front or phase: at
// LureLimit within LureWindow a target is avoided, a march or a sweep front
// given up, a way home held outside the fire (Dark fighters keep pulling the
// army in, or its legs keep bending into the fire). Held gardeners whose own
// cell or aimed cell lies within FireLine are recalled beside their ring cell,
// to the army's cell or out, and benched (dropped, sent home) at their second
// recall within BenchTicks. Returns true when the campaign changed course (the
// step ends).
inline bool pulled(const Campaign&c,uint32_t uid,uint32_t t){for(int i=0;i<PullSlots;i++)if(c.pullAt[i]&&c.pullUid[i]==uid&&t-c.pullAt[i]<GuardResend)return true;return false;}
inline void pull(Campaign&c,uint32_t uid,uint32_t t){int k=0;for(int i=0;i<PullSlots;i++){if(c.pullUid[i]==uid&&c.pullAt[i]){k=i;break;}if(c.pullAt[i]<c.pullAt[k])k=i;}c.pullUid[k]=uid;c.pullAt[k]=t?t:1;}
template<class Ops> bool guard(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f){
 if(!f.on||f.z.empty())return false;
 auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};
 if(c.phase!=Phase::Return&&c.target.id&&locked(c.target,f)){
  if(c.target.kind==Temple||c.target.kind==Fortress){startReturn(c,t,ops,m,"fire defence back on");return true;}
  avoid(c,c.target.uid,t);c.reason="target inside the fire defence";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return true;}
 Campaign note=c;
 std::vector<Soldier> fresh;for(auto&s:m)if(inFire(s.x,s.y,f)&&!pulled(c,s.uid,t))fresh.push_back(s);
 if(!fresh.empty()){Army h=summary(fresh);int ex=0,ey=0;bool way=exitPoint(h.x,h.y,f,h.count,stand,ex,ey);if(way)ops.move(c,fresh,ex,ey,false);for(auto&s:fresh)pull(c,s.uid,t); /* no way out: tried again after GuardResend */
  note.value=h.count;note.versus=0;note.goalX=ex;note.goalY=ey;note.reason=way?"soldiers out of the fire defence":"soldiers in the fire defence, no way out";ops.report(Event::Fire,note);
  if(c.phase==Phase::March||c.phase==Phase::Return)c.replanAt=t+GuardResend; /* the stored waypoints led here: a new way once they are out */
  if(!c.fireAt||t-c.fireAt>=GuardResend){c.fireAt=t?t:1; /* a new extraction episode */
   bool front=c.phase==Phase::Assault&&!c.target.id&&!c.hunting; /* a sweep's escort at its front */
   uint32_t key=((uint32_t)c.phase<<28)^(c.target.id?c.target.uid:front?(0x4000000u|((uint32_t)c.frontX<<13)|(uint32_t)c.frontY):(c.hunting?1u:2u));
   if(c.lureKey!=key||!c.lureSince||t-c.lureSince>LureWindow){c.lureKey=key;c.lureSince=t?t:1;c.lures=0;}
   if(++c.lures>=LureLimit){c.lures=0;
    if(c.phase==Phase::Return){c.replanAt=0;c.homeless=true;c.retryAt=t+ReorderEvery;c.vias=0;c.viaAt=0;return true;} /* hold outside, plan again later */
    if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="lured toward the fire defence";ops.report(Event::Avoid,c);retarget(c,t,ops,m);}
    else startReturn(c,t,ops,m,"lured toward the fire defence");
    return true;}}}
 std::vector<Gardener> gs;living(c,ops,gs);int out=0,bench=0;Strength s{};bool home=false;
 for(int i=std::min((int)gs.size(),c.gardeners)-1;i>=0;i--){const Gardener&g=gs[i];
  if(!inFire(g.x,g.y,f)&&!(g.aimX>=0&&inFire(g.aimX,g.aimY,f)))continue;
  int r=recall(c,g,t);if(!r)continue;
  if(r==2){if(!home){s=ops.strength(c);home=true;}dropGardener(c,i);if(sendOut(c,ops,g,f,s.ok?s.homeX:-1,s.ok?s.homeY:-1))out++;bench++;continue;}
  int x=-1,y=-1;bool cell=false;
  if(c.gcx[i]>=0&&c.target.id){RingCell rc;rc.x=c.gcx[i];rc.y=c.gcy[i];rc.dark=true;int bx=0,by=0;if(ops.beside(c.target,rc,bx,by)&&!closer(bx,by,f,GardenerKeep)&&legClear(g.x,g.y,bx,by,f,GardenerKeep)){x=bx;y=by;cell=true;}}
  if(x<0&&c.orderAt&&!closer(c.goalX,c.goalY,f,GardenerKeep)&&legClear(g.x,g.y,c.goalX,c.goalY,f,GardenerKeep)){x=c.goalX;y=c.goalY;}
  if(sendOut(c,ops,g,f,x,y))out++;c.gsent[i]=t;if(!cell){c.gcx[i]=-1;c.gcy[i]=-1;}}
 if(out||bench){note.value=out;note.versus=bench;note.goalX=note.goalY=-1;note.reason="gardener out of the fire defence";ops.report(Event::Fire,note);}
 return false;
}
// The siege: the soldiers hold a cell 3+ tiles off the footprint and, once
// the army has arrived, are moved off Dark ring cells (units on ring cells
// keep them Dark). Once no Dark fighter is near the door, each gardener gets
// its own Dark, unoccupied ring cell, nearest to the army first, with no Dark
// fighter near it, and is sent beside it (never onto it). A gardener is left
// alone while its reserved cell is a Dark ring cell (mid-work) or while it
// walks to or works on its still-Dark cell; it gets a new cell when its cell
// is green, when it is idle, failed its search or drifted away. Lost
// gardeners are topped up from free ones. The building collapses natively when
// the ring is green; if it still stands with an all-green ring, the native
// credited destroy finishes it. A Temple/Fortress still holding entering
// settlers keeps its last two ring cells until they are released. While the
// fire is on, ring and beside cells within GardenerKeep are never given out
// (a ring with only such cells: the target is avoided), and a gardener is
// sent only along a clear leg.
inline int ringIndex(const std::vector<RingCell>&ring,int x,int y){for(int i=0;i<(int)ring.size();i++)if(ring[i].x==x&&ring[i].y==y)return i;return -1;}
template<class Ops> void siege(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 Army a=summary(m);std::vector<Gardener> gs;int had=c.gardeners;living(c,ops,gs);c.lost+=had-c.gardeners;int want=c.escort?0:c.want;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 /* Gardeners dying at one target: replacements only while fewer than half the style's number died; once as
    many as the style takes died there, the target is given up (a hopeless target must not eat the pool). */
 int budget=(want+1)/2;
 if((int)gs.size()<want&&c.lost<budget&&c.topUps<budget&&t-c.lastTopUp>=GardenerResend){c.lastTopUp=t;int before=c.gardeners;
  assign(c,ops,a.x,a.y,std::min(want,c.gardeners+budget-c.topUps),t,[&](const Gardener&g){return distance(g.x,g.y,a.x,a.y)<=TopUpReach&&legClear(g.x,g.y,a.x,a.y,f,legKeep(std::max(1,c.gardeners)));}); /* r32: only gardeners the screen covers */
  if(c.gardeners>before){c.topUps+=c.gardeners-before;living(c,ops,gs);c.value=c.gardeners;ops.report(Event::Gardeners,c);}}
 if(want&&c.lost>0&&(c.lost>=want||gs.empty())){avoid(c,c.target.uid,t,LostAvoid);c.reason="gardeners lost";ops.report(Event::Avoid,c);
  if(gs.empty()){startReturn(c,t,ops,m,"no gardeners left");return;}retarget(c,t,ops,m);return;}
 if(gs.empty()&&!c.escort){startReturn(c,t,ops,m,"no gardeners left");return;}
 std::vector<RingCell> ring;ops.ring(c.target,ring);int dark=0,usable=0;for(auto&r:ring)if(r.dark){dark++;if(!closer(r.x,r.y,f,GardenerKeep))usable++;}c.ringSize=(int)ring.size();c.progress=(uint32_t)(ring.size()-dark);
 if(dark&&!usable){avoid(c,c.target.uid,t,LostAvoid);c.reason="ring inside the fire defence";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}
 if(c.ringDark<0||dark<c.ringDark)c.progressAt=t;
 /* An allied escort (no gardeners) goes home when nobody greens this ring any more. */
 if(c.escort&&t-c.progressAt>EscortStall){startReturn(c,t,ops,m,"allied siege stalled");return;}
 if(dark!=c.ringDark){if(c.ringDark<0)ops.report(Event::Siege,c);else if(dark<c.ringDark&&(dark==0||dark%8==0))ops.report(Event::Greening,c);c.ringDark=dark;}
 if(!dark){
  if(!c.greenSince)c.greenSince=t;
  else if(t-c.greenSince>=GreenWait&&t>=c.nextTry){c.nextTry=t+RetryRaze;if(!c.deferredSince){c.deferredSince=t;ops.report(Event::Deferred,c);}
   if(ops.destroy(c,c.target)){c.razed=true;ops.report(Event::Destroyed,c);}}
  return;
 }
 c.greenSince=0;
 /* r32: the screen holds (or GuardWait passed since the assault began with the army within EscortRadius of the
    door) and no Dark fighter is near the door. */
 bool secure=(holders(c,m)>=needed(m)||(t-c.since>=GuardWait&&distance(a.x,a.y,c.target.doorX,c.target.doorY)<=EscortRadius))&&!ops.defenders(c.target.doorX,c.target.doorY,DefenceRadius,false);
 /* Soldiers standing on Dark ring cells keep them Dark: move them off, once the army has arrived (not while it walks across). */
 if(secure&&t-c.lastOrder>=IgnoreCheck&&t-c.lastClear>=GardenerResend){c.lastClear=t;ops.clearFootprint(c,c.target,m,f);}
 /* A Temple/Fortress that still parks entering settlers should not collapse (they would be left off the
    map, as in the unmodded game): at most dark-2 gardeners of the campaign that called (none of an allied
    one) may work its ring, each greening one cell per ~31 ticks while the director looks every 10; the
    others walk home, because native gardeners keep greening the Dark land around wherever they stand. At
    most HoldMax ticks per target, so continuous recruitment cannot make the citadel unbreakable. */
 bool parked=(c.target.kind==Temple||c.target.kind==Fortress)&&dark<=ParkedDark&&ops.parked(c.target);
 if(parked&&!c.parkedSince)c.parkedSince=t; /* the budget counts per target (reset with a new target only) */
 bool parkedNow=parked&&t-c.parkedSince<HoldMax,hold=parkedNow&&dark<=2;
 if(hold!=c.holding){c.holding=hold;c.reason=hold?"settlers inside":"";if(hold)ops.report(Event::Deferred,c);}
 int homeX=a.x,homeY=a.y;if(parkedNow){Strength s=ops.strength(c);if(s.ok){homeX=s.homeX;homeY=s.homeY;}}
 /* r32: until then the gardeners wait behind the screen: one without a ring cell that strayed beyond EscortRadius
    from the army (and is not greening) is called back to the rear point. */
 /* r32 review: facing a threat the rear point follows it (away from the threat behind the army's cell), and a
    waiting gardener nearer the threat than that cell is called back too. */
 if(!secure){int rx=c.rearX>=0?c.rearX:a.x,ry=c.rearX>=0?c.rearY:a.y,tx=0,ty=0;bool th=c.orderAt&&!c.split&&threat(c,ops,f,tx,ty);int screenGap=th?distance(c.goalX,c.goalY,tx,ty):0;
  if(th){rearPoint(c,ops,f,c.goalX,c.goalY,a.x,a.y,true,tx,ty,rx,ry);c.rearX=rx;c.rearY=ry;}
  gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&g){return c.gcx[i]<0&&g.work!=Work::Working&&(distance(g.x,g.y,a.x,a.y)>EscortRadius||(th&&distance(g.x,g.y,tx,ty)<=screenGap))&&t-c.gsent[i]>=GardenerResend;},[&](int px,int py){return !th||distance(px,py,tx,ty)>screenGap;});}
 if(!secure&&!parkedNow)return;
 /* Ring cells: unoccupied first, then farthest from the fire, then nearest to the army. */
 std::vector<std::pair<int,int>> foes;ops.darkFighters(c,foes);auto defended=[&](int x,int y){for(auto&q:foes)if(distance(x,y,q.first,q.second)<=DefenceRadius)return true;return false;};
 std::vector<int> cells;for(int i=0;i<(int)ring.size();i++)if(ring[i].dark&&!closer(ring[i].x,ring[i].y,f,GardenerKeep))cells.push_back(i);
 std::sort(cells.begin(),cells.end(),[&](int p,int q){if(ring[p].occupied!=ring[q].occupied)return !ring[p].occupied;int ep=clear2(ring[p].x,ring[p].y,f),eq=clear2(ring[q].x,ring[q].y,f);if(ep!=eq)return ep>eq;return distance(ring[p].x,ring[p].y,a.x,a.y)<distance(ring[q].x,ring[q].y,a.x,a.y);});
 int allow=parkedNow?((c.style==Style::Allied||!ops.leads(c,c.target))?0:std::max(0,dark-2)):MaxGardeners,engaged=0; /* one campaign per parked citadel */
 for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){const Gardener&g=gs[i];
  int mine=c.gcx[i]>=0?ringIndex(ring,c.gcx[i],c.gcy[i]):-1;bool assigned=mine>=0&&ring[mine].dark;
  int held=g.reservedX>=0?ringIndex(ring,g.reservedX,g.reservedY):-1;bool working=held>=0&&ring[held].dark;
  bool nearby=assigned||working;if(!nearby&&g.work==Work::Working)for(auto&r:ring)if(r.dark&&distance(r.x,r.y,g.x,g.y)<=2){nearby=true;break;}
  if(parkedNow){ /* within the allowance a gardener works on; every other one walks home (a working one at once) */
   if(nearby&&engaged<allow)engaged++;
   else if(nearby||engaged>=allow){if(working||distance(g.centreX,g.centreY,homeX,homeY)>2){sendOut(c,ops,g,f,homeX,homeY);c.gsent[i]=t;}c.gcx[i]=c.gcy[i]=-1;continue;}}
  if(working)continue;
  bool stuck=g.work==Work::Idle||(g.work==Work::Searching&&g.misses>SearchMisses);
  /* Its own search can drift outward from a still-Dark cell; then the cell is free again. */
  bool drifted=assigned&&g.work==Work::Working&&distance(g.x,g.y,ring[mine].x,ring[mine].y)>DriftLimit;
  if(assigned&&!stuck&&!drifted)continue;
  if(!secure||t-c.gsent[i]<GardenerResend)continue;
  if(parkedNow&&!nearby&&engaged>=allow)continue; /* no room for another worker */
  for(int k:cells){bool taken=false;for(int j=0;j<c.gardeners;j++)if(j!=i&&c.gcx[j]==ring[k].x&&c.gcy[j]==ring[k].y)taken=true;
   if(taken||defended(ring[k].x,ring[k].y))continue;
   int x=ring[k].x,y=ring[k].y;ops.beside(c.target,ring[k],x,y);if(closer(x,y,f,GardenerKeep))continue;
   if(!legClear(g.x,g.y,x,y,f,GardenerKeep)){ /* not straight there: to the army's cell first, the cell is given out later */
    if(c.orderAt&&!closer(c.goalX,c.goalY,f,GardenerKeep)&&legClear(g.x,g.y,c.goalX,c.goalY,f,GardenerKeep)){ops.sendGardener(c,g,c.goalX,c.goalY);c.gsent[i]=t;}break;}
   ops.sendGardener(c,g,x,y);c.gcx[i]=ring[k].x;c.gcy[i]=ring[k].y;c.gsent[i]=t;if(parkedNow&&!nearby)engaged++;break;}
 }
}
// A march plans again from where the army stands (after a guard extraction, a late or bent leg): as one
// army (a pincer's flank legs no longer hold); without a safe way the target is avoided and the army goes home.
template<class Ops> void remarch(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};Army a=summary(m);
 c.split=false;c.replanAt=0;
 if(plan(c,f,a.x,a.y,c.stageX,c.stageY,groupSize(c,m),stand)){c.since=t;order(c,t,ops,m);return;}
 if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="no way outside the fire defence";ops.report(Event::Avoid,c);}
 startReturn(c,t,ops,m,"no way outside the fire defence");
}
// March: through the waypoints, then wait until 70% of the army stands at the
// staging cell (each half at its own cell for a pincer), or MarchTimeout, then
// assault. A late or re-sent leg is checked from where the army stands; when it
// is not clear (or the guard pulled soldiers out) the march plans again.
// r32: a leg ends only with the convoy together (every held gardener within convoyLimit() of the army centre, of
// the first half for a pincer), the convoy steps every ConvoyEvery ticks; a campaign whose gardeners were all
// left behind goes home; a rally ends at the gardeners with a new approach from there.
template<class Ops> void march(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.replanAt){if(t<c.replanAt)return;remarch(c,t,ops,m);return;}
 /* r32: all died on the way: the target is avoided, the army goes home. All left behind: convoy() sends it home. */
 if(c.gardeners){std::vector<Gardener> gs;living(c,ops,gs);if(!c.gardeners&&!c.hunting&&!c.escort&&!c.rally){
  if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="gardeners lost";ops.report(Event::Avoid,c);}startReturn(c,t,ops,m,"no gardeners left");return;}}
 bool late=t-c.since>MarchTimeout,step=convoying(c)?t-c.convoyAt>=ConvoyEvery:t-c.lastOrder>=2*ReorderEvery;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 if(c.split){std::vector<Soldier> a,b;halves(m,a,b);bool arrived=within(a,c.stageX,c.stageY,10)*10>=(int)a.size()*7&&within(b,c.stage2X,c.stage2Y,10)*10>=(int)b.size()*7&&together(c,ops,a.empty()?b:a);
  Army sa=summary(a),sb=summary(b);bool clear=(a.empty()||legClear(sa.x,sa.y,c.stageX,c.stageY,f,legKeep(std::max(1,sa.count))))&&(b.empty()||legClear(sb.x,sb.y,c.stage2X,c.stage2Y,f,legKeep(std::max(1,sb.count))));
  if(arrived){c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);order(c,t,ops,m);}
  else if((late||t-c.lastOrder>=2*ReorderEvery)&&!clear)remarch(c,t,ops,m);
  else if(late){c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);order(c,t,ops,m);}
  else if(step)order(c,t,ops,m);
  return;}
 bool via=walking(c);int gx=via?c.vx[c.viaAt]:c.stageX,gy=via?c.vy[c.viaAt]:c.stageY,there=within(m,gx,gy,10);
 bool arrived=there*10>=(int)m.size()*7&&together(c,ops,m);Army a=summary(m);int keep=legKeep(std::max(1,groupSize(c,m)));
 if(via&&arrived){c.viaAt++;c.since=t;order(c,t,ops,m);return;}
 if(via&&late){remarch(c,t,ops,m);return;}
 if(arrived||late){
  if(c.rally){c.rally=false;if(c.style==Style::Pincer)c.split=true;c.nesting++;approach(c,t,ops,m,all,a.x,a.y);c.nesting--;return;} /* r32: at the gardeners: on with them */
  if(late&&!arrived&&c.target.id&&!legClear(a.x,a.y,c.target.doorX,c.target.doorY,f,keep)){remarch(c,t,ops,m);return;} /* never straight to the door across the fire */
  // Re-check the defence at the staging cell: too strong now -> another target.
  if(c.target.id&&c.raid){int now=ops.defenders(c.target.doorX,c.target.doorY,RaidRadius,true),limit=defenceLimit(ops.config(c.player),summary(m).weight);
   if(now*2>limit*3){c.value=limit;c.versus=now;avoid(c,c.target.uid,t);ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}}
  c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);
  if(c.style==Style::Sweep&&!c.target.id){c.lastOrder=0;return;}
  order(c,t,ops,m);
 }
 else if(step){if(convoying(c)||legClear(a.x,a.y,gx,gy,f,keep))order(c,t,ops,m);else remarch(c,t,ops,m);} /* the convoy checks its own legs */
}
// Sweep: the escort holds ordinary land next to the front, the gardeners each
// get their own Dark cell near it (they keep greening around it natively);
// the front moves on towards the nearest Temple/Fortress (else the nearest
// Dark building) while the fire is off, towards the nearest unlocked farm while
// it is on; fronts keep FrontKeep from the fire. A target building near the
// front is besieged first.
template<class Ops> void sweep(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.phase==Phase::March){march(c,t,ops,m);return;}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 std::vector<Gardener> gs;living(c,ops,gs);int want=c.want;Army a=summary(m);
 if((int)gs.size()<want&&c.topUps<(want+1)/2&&t-c.lastTopUp>=GardenerResend*5){c.lastTopUp=t;int before=c.gardeners;
  assign(c,ops,a.x,a.y,std::min(want,c.gardeners+(want+1)/2-c.topUps),t,[&](const Gardener&g){return distance(g.x,g.y,a.x,a.y)<=TopUpReach&&legClear(g.x,g.y,c.frontX,c.frontY,f,GardenerKeep);}); /* r32: only gardeners the escort covers */
  if(c.gardeners>before){c.topUps+=c.gardeners-before;living(c,ops,gs);c.value=c.gardeners;ops.report(Event::Gardeners,c);}}
 if(gs.empty()){startReturn(c,t,ops,m,"no gardeners left");return;}
 std::vector<Keep> keep;keeps(c,ops,all,f,keep);frontKeeps(f,keep);
 Pick pick;pick.weakest=true;int i=chooseTarget(all,f,c.frontX,c.frontY,pick,&c,t);
 if(i>=0&&distance(all[i].x,all[i].y,c.frontX,c.frontY)<=SweepTarget){c.target=all[i];c.targetSince=t;c.deferredSince=0;c.nextTry=0;c.reason="on the front";ops.report(Event::Retarget,c);approach(c,t,ops,m,all,a.x,a.y);return;}
 int dx=0,dy=0;bool land=ops.darkNear(c,c.frontX,c.frontY,SweepRadius,keep,dx,dy),held=c.lastOrder&&holders(c,m)>=needed(m);
 if((!land||t-c.frontSince>=SweepAdvance)&&held){ /* r32: the front moves only while the escort holds it */
  int gx=c.frontX,gy=c.frontY;long best=-1;
  for(auto&b:all){if(f.on&&(b.kind!=Farm||locked(b,f)))continue; /* fire on: towards unlocked farms only, never a citadel */
   long d=distance(c.frontX,c.frontY,b.x,b.y)+((b.kind==Temple||b.kind==Fortress)?0:1000);if(best<0||d<best){best=d;gx=b.x;gy=b.y;}}
  int span=distance(c.frontX,c.frontY,gx,gy),px=c.frontX,py=c.frontY;if(span>0){int k=std::min(span,SweepStep);px=c.frontX+(int)((long)(gx-c.frontX)*k/span);py=c.frontY+(int)((long)(gy-c.frontY)*k/span);}
  if(ops.darkNear(c,px,py,SweepRadius+2,keep,dx,dy)||ops.darkNear(c,c.frontX,c.frontY,SweepLand,keep,dx,dy)){
   if(dx!=c.frontX||dy!=c.frontY){c.frontX=dx;c.frontY=dy;c.target.doorX=dx;c.target.doorY=dy;c.lastOrder=0;c.reason="front";ops.report(Event::Front,c);}
   c.frontSince=t;}
  else if(!land){
   /* The land left lies around a target (kept for sieges): besiege it, else the sweep is over. */
   if(i>=0&&distance(all[i].x,all[i].y,c.frontX,c.frontY)<=SweepLand+CitadelKeep){c.target=all[i];c.targetSince=t;c.deferredSince=0;c.nextTry=0;c.reason="last land";ops.report(Event::Retarget,c);approach(c,t,ops,m,all,a.x,a.y);return;}
   startReturn(c,t,ops,m,"no Dark land in reach");return;}
 }
 /* The escort holds beyond the chase reach of defenders inside the fire (HuntKeep), not only its formation keep. */
 if(!c.lastOrder||(t-c.lastOrder>=ReorderEvery&&holders(c,m)<needed(m))){int hx=c.frontX,hy=c.frontY,hk=std::max(soldierKeep(std::max(1,a.count)),HuntKeep);
  if(holdPoint(c,ops,f,c.frontX,c.frontY,a.x,a.y,hk,hx,hy)||!closer(hx,hy,f,hk)){send(c,t,ops,m,hx,hy,false);c.rearX=hx;c.rearY=hy;}}
 /* r32: gardeners without a cell near the escort walk with it to its hold cell (one id per order), while Dark
    fighters are near the front to 2*RearStep behind it (away from the front). */
 bool foes=ops.defenders(c.frontX,c.frontY,DefenceRadius,false);
 if(c.rearX>=0){int rx=c.rearX,ry=c.rearY,len=distance(c.frontX,c.frontY,rx,ry);
  if(foes&&len>0){rx+=(int)lround((double)(c.rearX-c.frontX)*2*RearStep/len);ry+=(int)lround((double)(c.rearY-c.frontY)*2*RearStep/len);}
  gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&g){return c.gcx[i]<0&&g.work!=Work::Working&&distance(g.x,g.y,rx,ry)>(foes?2:EscortRadius);});}
 if(holders(c,m)<needed(m)||foes)return;
 /* r32: gardener cells within EscortRadius of the escort; a gardener beyond EscortRadius
    of the escort gets a new cell (the Dark cell nearest the escort). */
 std::vector<Keep> taken=keep;for(int j=0;j<c.gardeners;j++)if(c.gcx[j]>=0)taken.push_back({c.gcx[j],c.gcy[j],SpreadKeep});
 for(int j=0;j<(int)gs.size()&&j<c.gardeners;j++){const Gardener&g=gs[j];
  bool stuck=g.work==Work::Idle||(g.work==Work::Searching&&g.misses>SearchMisses),away=g.work!=Work::Busy&&distance(g.x,g.y,a.x,a.y)>EscortRadius;
  if(!(stuck||away||c.gcx[j]<0)||t-c.gsent[j]<GardenerResend)continue; /* r32: one that walked with the escort has no cell yet */
  int cx=0,cy=0;if(!ops.darkNear(c,a.x,a.y,EscortRadius,taken,cx,cy))break; /* the Dark land nearest the escort */
  if(!legClear(g.x,g.y,cx,cy,f,GardenerKeep))continue;
  ops.sendGardener(c,g,cx,cy);c.gcx[j]=cx;c.gcy[j]=cy;c.gsent[j]=t;taken.push_back({cx,cy,SpreadKeep});}
}
// Hunt: march, then follow the hunted group (re-found within HuntRadius every
// HuntFollow ticks, never within HuntKeep of the fire, only along a clear leg);
// when no Dark fighter group is left for HuntQuiet ticks, the army goes on to
// a building it can beat (with its gardeners) or home.
template<class Ops> void hunt(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.phase==Phase::March){march(c,t,ops,m);return;}
 if(t-c.lastFollow<HuntFollow)return;c.lastFollow=t;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Army a=summary(m);
 std::vector<std::pair<int,int>> fs,close;ops.darkFighters(c,fs);for(auto&p:fs)if(distance(p.first,p.second,c.target.doorX,c.target.doorY)<=HuntRadius)close.push_back(p);
 int x=0,y=0,n=0;
 if(t-c.started<HuntTime&&cluster(close,f,c.target.doorX,c.target.doorY,x,y,n)&&ops.reach(c,x,y)&&legClear(a.x,a.y,x,y,f,legKeep(std::max(1,a.count)))){c.quietSince=0;c.target.doorX=x;c.target.doorY=y;c.value=n;send(c,t,ops,m,x,y,false);return;}
 if(!c.quietSince)c.quietSince=t;
 if(t-c.quietSince<HuntQuiet&&t-c.started<HuntTime)return;
 c.hunting=false;c.quietSince=0;c.reason="hunt over";ops.report(Event::Hunt,c);
 std::vector<Gardener> gs;living(c,ops,gs);retarget(c,t,ops,m,true);
}
// One director step for one sector, every Period ticks.
template<class Ops> void step(Campaign&c,uint32_t t,Ops&ops){
 if(!c.armed){c.armed=true;c.random.state^=(uint32_t)c.player*0x01000193u+(uint32_t)c.sector*0x9e3779b9u+t;toIdle(c,t);c.cooldownUntil=0;}
 if(!ops.sector(c)){if(c.phase!=Phase::Idle){c.reason="sector gone";ops.report(Event::Lost,c);toIdle(c,t);}return;}
 reclaim(c,t,ops);
 if(c.phase==Phase::Idle){decide(c,t,ops);return;}
 std::vector<Soldier> m;
 if(!ops.members(c,m)){c.reason="squad vanished";ops.report(Event::Lost,c);toIdle(c,t);return;}
 if(!c.checked&&t>=c.orderAt+IgnoreCheck){c.checked=true;std::vector<Soldier> a,b;halves(m,a,b);Army s=summary(c.split?a:m);if(s.count&&distance(s.x,s.y,c.orderX,c.orderY)<2&&distance(s.x,s.y,c.goalX,c.goalY)>ConvoyLead-3){c.reason="order not executed";ops.report(Event::Ignored,c);}}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 if(guard(c,t,ops,m,f))return;
 if(c.phase==Phase::Return){
  Strength s=ops.strength(c);bool hot=false;for(auto&u:m)if(inFire(u.x,u.y,f))hot=true;
  int home=s.ok?within(m,s.homeX,s.homeY,12):(int)m.size();
  bool done=m.empty()||!s.ok||(!walking(c)&&home*10>=(int)m.size()*8&&together(c,ops,m))||t-c.since>HomeTimeout; /* r32: home with its gardeners */
  /* Never hand soldiers back inside the fire (reservoirs never move them): the guard walks them out first.
     Only a squad that still has a member within FireLine after HotHold ticks (on green land, no way out) is released. */
  if(done&&(!hot||t-c.since>HotHold)){
   /* An incomplete give-back keeps the squad and retries; after 5 tries the next load adopts it. */
   if(ops.dissolve(c)){ops.report(Event::Dissolved,c);toIdle(c,t);}else if(++c.dissolveTries>=5){c.reason="give-back incomplete";ops.report(Event::Lost,c);toIdle(c,t);}
   return;}
  if(m.empty()||!s.ok)return;
  if(c.replanAt){if(t>=c.replanAt){c.replanAt=0;homeward(c,t,ops,m,f,s);}return;} /* the guard's way out first, then a new way home */
  if(c.homeless){if(t>=c.retryAt)homeward(c,t,ops,m,f,s);return;} /* holding outside the fire */
  bool step=convoying(c)&&t-c.convoyAt>=ConvoyEvery; /* r32: the way home is a convoy too */
  if(walking(c)){int gx=c.vx[c.viaAt],gy=c.vy[c.viaAt];bool there=within(m,gx,gy,10)*10>=(int)m.size()*7&&together(c,ops,m);
   if(there){c.viaAt++;int x=walking(c)?c.vx[c.viaAt]:s.homeX,y=walking(c)?c.vy[c.viaAt]:s.homeY;advance(c,t,ops,m,f,x,y,true);}
   else if(t-c.orderAt>ViaTimeout||t-c.lastOrder>=2*ReorderEvery)homeward(c,t,ops,m,f,s); /* late or re-sent: a way from where the army stands */
   else if(step)advance(c,t,ops,m,f,gx,gy,true);
   return;}
  if(t-c.lastOrder>=2*ReorderEvery)homeward(c,t,ops,m,f,s); /* never a straight leg home re-sent through the fire */
  else if(step)advance(c,t,ops,m,f,s.homeX,s.homeY,true);
  return;
 }
 if(m.empty()){startReturn(c,t,ops,m,"army lost");return;}
 if((int)m.size()*100<(int)c.initial*(light(c)?50:30)){startReturn(c,t,ops,m,"army beaten");return;}
 if(t-c.started>(c.style==Style::Sweep?SweepTime:CampaignTimeout)){startReturn(c,t,ops,m,"campaign time");return;}
 if(c.style==Style::Hunt&&c.hunting){hunt(c,t,ops,m);return;}
 if(c.style==Style::Sweep&&!c.target.id){sweep(c,t,ops,m);return;}
 if(!c.target.id||!ops.alive(c.target)){
  if(c.target.id){c.chain++;if(!c.razed){c.reason="ring green";ops.report(Event::Collapsed,c);}}
  if(c.escort&&c.target.id){startReturn(c,t,ops,m,"allied target fell");return;}
  if(c.style==Style::Sweep){resume(c,t);return;}
  ops.report(Event::Retarget,c);retarget(c,t,ops,m,c.want>0&&!c.gardeners&&!c.escort);return;} /* r32: an adopted army takes gardeners near it and marches with them */
 if(t-c.targetSince>TargetTimeout){avoid(c,c.target.uid,t);if(c.style==Style::Sweep){resume(c,t);return;}startReturn(c,t,ops,m,"target not reached");return;}
 if(c.target.kind!=Farm){for(auto&b:all)if(b.kind==Farm&&b.reachable&&!locked(b,f)){if(c.escort){startReturn(c,t,ops,m,"allied escort released");return;}c.reason="farm first";ops.report(Event::Retarget,c);retarget(c,t,ops,m);return;}}
 if(c.phase==Phase::March){march(c,t,ops,m);return;}
 siege(c,t,ops,m);if(c.phase!=Phase::Assault)return;
 /* Re-order only an army that does not hold its cell (a re-order spreads it over the footprint again). */
 if(t-c.lastOrder>=ReorderEvery&&holders(c,m)<needed(m))order(c,t,ops,m);
}
// After a load the native AI restores mod squads (state 6, command 0): the
// engine hands them back here; they re-target from where they stand and take
// gardeners again at the next siege. A raid-sized army stays a raid. The fire
// guard walks members out of the fire on the first step.
template<class Ops> void adopt(Campaign&c,uint32_t t,uintptr_t squad,int count,Ops&ops){
 c.armed=true;c.random.state^=(uint32_t)c.player*0x01000193u+(uint32_t)c.sector*0x9e3779b9u+t;c.squad=squad;c.initial=(uint16_t)std::max(1,count);c.phase=Phase::Assault;c.target=Target{};c.started=t;c.targetSince=t;c.since=t;c.lastOrder=0;c.progress=0;c.nextTry=0;c.deferredSince=0;c.greenSince=0;c.ringDark=-1;c.razed=false;c.holding=false;c.gardeners=0;
 /* r32 review: raids grow with the army (raidSize); the army at launch is estimated as the free soldiers of the sector now
    plus this squad, so a scaled raid stays a raid; the gardeners are capped as decide() caps them. */
 int pool=count,raidMax=AdoptedRaid;Strength s=ops.strength(c);if(s.ok){std::vector<Soldier> free;ops.available(c,s.homeX,s.homeY,free);pool+=(int)free.size();raidMax=std::max(raidMax,raidSize(pool-std::max(HomeReserve,pool/6)));}
 c.style=count<=raidMax?Style::Raid:Style::Assault;c.lastStyle=c.style;c.want=std::min(gardenersWanted(pool),gardenersFor(c.style,count));c.raid=light(c);c.vias=0;c.viaAt=0;c.replanAt=0;c.homeless=false;c.split=false;c.hunting=false;c.escort=false;c.chain=0;c.checked=true;
 ops.report(Event::Adopted,c);
}
// Native gardener recruitment for one computer player: order one more when
// fewer than wanted (GardenersWanted, r32: gardenersWanted(soldiers)) are alive or pending, no order is pending or on
// its way, free shovels remain beyond the reserve and the economy has carriers
// to spare. The order (0xD5100) is additive, so never more than one. Returns
// 1 to order, else 0 with the reason in why.
inline int recruitOrder(int alive,int pending,int coming,int shovels,bool carriers,const char**why=nullptr,int wanted=GardenersWanted){ /* r32: wanted = gardenersWanted(soldiers) */
 const char*r=pending>0||coming>0?"order on its way":alive+pending>=wanted?"enough gardeners":shovels<=ShovelReserve?"no spare shovels":!carriers?"no spare carriers":nullptr;
 if(why)*why=r?r:"";return r?0:1;
}
// r29: shovels for the gardeners, through the computer's own toolsmith. The
// native AI never orders shovels for gardeners: its building manager
// (sub_249400) orders them only for its diggers (0x249835: 7-d or 10-2d while
// d < 5 diggers and no shovel is in stock, always positive), and a toolsmith
// makes nothing unless a tool is ordered (0x14D008), so the gardener shovels
// used up by recruitment were never replaced. While gardeners are missing and
// the free shovels are down to the reserve (the "no spare shovels" case of
// recruitOrder), the eco's shovel order (the native order byte, the toolsmith
// menu's own count) is topped up to the shovels still needed, at most
// ShovelBatch; an order of 100 (continuous, set by someone else) is left
// alone; no order without a ready toolsmith in the home economy. ordered < 0:
// the economy has no shovel slot. Returns the amount to add (0: none, the
// reason in why).
constexpr int ShovelBatch=4,ShovelContinuous=100;
// The order byte the plugin wants while shovels are short: the shovels still needed, at most a batch.
inline int shovelTarget(int alive,int pending,int shovels,int wanted=GardenersWanted){
 int missing=wanted-alive-std::max(0,pending),free=std::max(0,shovels);
 return std::min(ShovelBatch,missing+ShovelReserve-free);
}
inline int shovelOrder(int alive,int pending,int shovels,int ordered,int toolsmiths,const char**why=nullptr,int wanted=GardenersWanted){
 int missing=wanted-alive-std::max(0,pending),free=std::max(0,shovels);
 int target=shovelTarget(alive,pending,shovels,wanted);
 const char*r=missing<=0?"enough gardeners":free>ShovelReserve?"spare shovels":toolsmiths<=0?"no toolsmith":ordered<0?"no shovel slot":ordered>=ShovelContinuous?"continuous order":ordered>=target?"already ordered":nullptr;
 if(why)*why=r?r:"";return r?0:target-ordered;
}
// r29 review: the same AI pass can also CLEAR or LOWER the shovel order. While
// its task list holds a built toolsmith waiting for its smith (task +0x54 0x15,
// +0x30 3) it sends SendProduceOrder(toolsmith, SHOVEL, 10-2*diggers)
// (0x249744..0x249770, d = word eco+0x28 via 0x249422); at its steady state of
// 5 diggers (it recruits while d < 10-d, 0x2497A8) that is 0, which 0xD6A90
// stores as 0 (0xD6AEA), and 6 or more diggers give a negative amount that is
// added and clamped at 0 (0xD6B08). The toolsmith takes an order only when a
// production cycle starts and then takes exactly one (0x14D065). So between two
// looks (one per game tick) a drop of one is production, a drop of two or more
// is a clear: the plugin puts back what it still owes (its own part of the
// order, never the AI's digger shovels). A drop from 1 to 0 counts as
// production; the 250-tick evaluation repairs that case. The watch is
// transient (never in dt::State): owed is what the plugin's order left,
// lowered as the toolsmith makes shovels; seen is the byte at the last look.
struct ShovelWatch {int owed=0,seen=-1;};
// One look at the order byte (now < 0: no shovel slot). Returns the amount to
// add back (0: none); the caller re-reads the byte into w.seen after adding.
inline int shovelRestore(ShovelWatch&w,int now){
 if(now<0||now>=ShovelContinuous){w=ShovelWatch{};return 0;}
 int add=0;
 if(w.owed>0&&w.seen>=0){if(now+1<w.seen)add=std::max(0,std::min(w.owed,w.seen)-now);else if(now<w.seen)w.owed=std::min(w.owed,now);}
 w.seen=now;return add;
}
// After the 250-tick evaluation: remember the plugin's part of the order
// (ordered: the byte after it, target: shovelTarget) while shovels are still
// wanted, otherwise forget it so nothing is put back.
inline void shovelNoted(ShovelWatch&w,bool wanted,int ordered,int target){
 if(!wanted||ordered<=0||ordered>=ShovelContinuous||target<=0){w=ShovelWatch{};return;}
 w.owed=std::min(ordered,target);w.seen=ordered;
}
}
