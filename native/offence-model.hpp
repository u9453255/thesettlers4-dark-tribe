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
// r33: escorted gardeners move with WATCH (job 0xB0 walks, then waits; its work
// centre stays, so it never searches Dark land on the way), re-ordered from
// their real state; the gap is measured to the nearest soldier of the army body;
// a held army stops, waits and walks back to stragglers instead of dropping them.
// Defence: Dark land or Dark units inside a computer's territory start a 'defend'
// campaign (soldiers to the intrusion, gardeners reclaim own land behind them);
// a serious intrusion recalls a running attack. Reclaim searches the whole own
// territory; targets behind a long detour are unreachable; failed targets are
// avoided longer each time.
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
// Pressure (r37, user: the computers attacked "rarely, really rarely"): the first attack after a third of the AI's
// AttackDelay, kept within 2..4 minutes; a check every 128..255 ticks; an army of AssaultMin or more never waits for
// chance (a smaller one succeeds with 50%, +25% per failed check); a finished campaign rests 150..449 ticks, then the
// next army forms from whatever soldiers are free. r33 had 3..8 minutes, 256..383, chance for every army and a rest of
// 600..1,499 (field pid 17196: 8..14 campaigns in 150,000 ticks, most of the time spent walking home and waiting).
constexpr uint32_t FirstMin=2*DelayUnit,FirstMax=4*DelayUnit,CheckEvery=128,ChanceStart=0x8000,ChanceStep=0x4000,ChanceFull=0x10000,CooldownMin=150,CooldownSpread=300;
constexpr uint32_t Period=10; // director step, game ticks
constexpr int MinArmy=5,HomeReserve=2,RaidMax=10,HuntMax=16,SweepEscort=8,AssaultMin=12,PincerMin=14,PincerGap=12,AdoptedRaid=12,RaidChain=5; // r37: a raid chains up to 5 buildings (3)
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
// r38: an extraction counts as a lure episode only when at least LureMin soldiers or a tenth of the army were pulled at
// once (field pid 3832: a 148-soldier siege was given up after three single stragglers stepped over the line).
constexpr int LureMin=3;
// r30 review: RetryPlan = a campaign without a safe way home holds outside the fire and plans again this often;
// HotHold = a squad on its way home is never handed back with a member within FireLine before this many ticks
// (reservoirs never move soldiers; only units the fire cannot reach, on green land with no way out, last that long).
constexpr uint32_t RetryPlan=60,HotHold=12000;
constexpr uint32_t MarchTimeout=2000,TargetTimeout=6000,CampaignTimeout=40000,HomeTimeout=3000,ReorderEvery=300,AvoidTicks=1000,IgnoreCheck=60,UnknownRetry=100,RetryRaze=30;
// r37: a leg or a target gets its time budget from its length (TicksPerHex: field marches of 280 hex took about 6,000
// ticks), and a march fails only when the army body came no closer to the leg end for NoProgress ticks or its budget is
// spent (r33 gave every leg MarchTimeout 4,000: on First Dawn 8 of 14 campaigns ended "target not reached" on farms
// 290+ hex from home, which were then avoided for 6,000..24,000 ticks). A failed target is replaced from where the army
// stands (retarget), not walked home from.
constexpr uint32_t TicksPerHex=25,NoProgress=1500,TargetsNote=3000; // TargetsNote: OFFENCE_TARGETS at most this often per sector
inline uint32_t legBudget(int hex){return MarchTimeout+TicksPerHex*(uint32_t)std::max(0,hex);}
inline uint32_t targetBudget(int hex){return TargetTimeout+TicksPerHex*(uint32_t)std::max(0,hex);}
constexpr double PincerAngle=1.05; // the two flanks: the line from home turned by +-60 degrees around the door
// Gardeners (native role CGardenerRole, one cell per ~31-tick WORK).
constexpr uint16_t GardenerType=50;
// r32: MaxGardeners (20) is the most one campaign holds and the most a large computer keeps alive
// (gardenersWanted); ParkedDark is the ring size from which a parked citadel's hold is checked.
constexpr int MaxGardeners=20,MinGardeners=2,SweepGardeners=3,SearchMisses=40,EscortGap=3,DriftLimit=4,ParkedDark=14,CallHome=4;constexpr uint32_t CallHomeEvery=600; // r37: far gardeners called home per decision
constexpr uint32_t GardenerResend=60,GreenWait=20;
// A Temple/Fortress parking entering settlers is spared its last ring cells for at most HoldMax ticks
// (continuous recruitment must not make it unbreakable); an allied escort goes home after EscortStall
// ticks without a greened ring cell.
constexpr uint32_t HoldMax=900,EscortStall=1500,LostAvoid=6000; // LostAvoid: a target that cost a style's gardeners is avoided this long
// r33: a target a campaign of the sector failed to reach (march or target timeout, orders not executed, gardeners that do
// not follow) is avoided LostAvoid, from its FailLimit-th failure LongAvoid; IgnoreLimit ignored orders on one target fail it.
constexpr int FailLimit=2,IgnoreLimit=3;constexpr uint32_t LongAvoid=2*LostAvoid; // r37: 12,000 (was 24,000)
// r33: a target whose walking path from the sector home is longer than detourLimit(hex distance) is unreachable for an
// attack (the field case: 695 steps for a farm 227 hex away behind a lake and a river; straight convoy legs cross water).
constexpr int DetourSlack=20;
// r38: 4 x hex + 100 (r33: 2.5 x hex + 20). Armies walk one order per leg over the native pathfinder since r35, so a
// long walk is a far target, not an unreachable one (the r33 case, 695 steps for 227 hex, is attackable).
inline int detourLimit(int hex){return 4*std::max(0,hex)+5*DetourSlack;}
inline bool tooLong(int path,int hex){return path<0||path>detourLimit(hex);}
// Land reclamation by the free gardeners: Dark ground in the player's own
// territory always (OwnRadius around each gardener: the computers cannot
// build on Dark ground), the Dark border after a
// minute (ReclaimRadius around the gardener, within ReclaimHome of the
// sector's home); never into fire, next to defenders, within CitadelKeep of a
// Temple/Fortress (sieges handle those) or within SiteKeep of a growing site.
// The keeps cover the ring of a Temple/Fortress/Hall (walkable footprint cells next to the core lie
// 4..15 tiles from the building position) plus 3, so no gardener is ever sent onto one.
// r33: OwnRadius covers the whole map (the field case: own Dark land 194 hex from the gardeners at home, never found
// within the old 120); ReclaimNote = how often a pass that sent nobody says why.
// r33 review: a free gardener reclaims own Dark land alone only near itself (ReclaimRadius) or, standing within
// LaunchReach of home, within LaunchReach of home; own land farther out is the defence's (soldiers and gardeners as one
// convoy): r33 first sent idle gardeners alone across the map, where no attack or defence could muster them.
constexpr int OwnRadius=4096,ReclaimRadius=50,ReclaimHome=80,ReclaimPerPass=4,CitadelKeep=18,SiteKeep=18,ParkedKeep=20,SpreadKeep=3;
constexpr uint32_t ReclaimEvery=150,ReclaimStart=DelayUnit,RecallEvery=30,ReclaimNote=3000;
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
// this many alive, order one at a time, never use the last shovel.
// r50: ShovelReserve 1 (was 2): since r47 the AI's diggers get their carriers through the reserve room, and the second
// spare shovel cost a full toolsmith cycle per replacement gardener (field pid 18584: 1 to 6 gardeners against 12 wanted
// for 30,000 ticks, 16 waits for shovels; pid 31460: one shovel lay unused for 110,000 ticks with 10 diggers working).
constexpr int GardenersWanted=12,ShovelReserve=1;
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
// r33: detour = walking steps from the sector home beyond the hex distance (0 unknown); a target behind a too long detour is
// not reachable (the bridge, tooLong()). r37: path = walking steps from the point a choice is made from (ops.paths(): one
// breadth-first search over the land region for every target; 0 unknown, -1 none within the limit), so the next target is
// the nearest to walk to from where the army stands (field: a farm 318 hex away was taken over one 40 hex away because
// the home-based detour was added to the army-based distance).
struct Target {uint16_t id=0,kind=0;uint32_t uid=0;int x=0,y=0,doorX=0,doorY=0,defence=0,detour=0,path=0;bool reachable=false;};
struct Soldier {uint16_t id=0,type=0;uint32_t uid=0;int x=0,y=0;};
enum class Phase:uint8_t {Idle,March,Assault,Return};
enum class Style:uint8_t {Raid,Assault,Pincer,Sweep,Hunt,Allied,Defend};
inline const char*styleName(Style s){static const char*names[]={"raid","assault","pincer","sweep","hunt","allied","defend"};return names[(int)s];}
// Gardeners each style takes along (Allied without free gardeners joins as an escort): the style's base
// (raid 4, hunt 2, sweep 10, allied 6, assault/pincer 8) plus one per GardenerPer soldiers (r32).
inline int gardenerBase(Style s){switch(s){case Style::Raid:return 4;case Style::Hunt:return 2;case Style::Sweep:return 10;case Style::Allied:case Style::Defend:return 6;default:return 8;}}
inline int gardenersFor(Style s,int soldiers=0){return std::min(MaxGardeners,gardenerBase(s)+std::max(0,soldiers)/GardenerPer);}
// r43: carrier reserve floor of a computer's home economy, from its kingdom size (completed buildings of every type).
// The AI's own CPeopleManagerTask sets reserve = clamp(min(7*pendingTasks + 10, carriers - 10), 5, 80), which follows
// every draft down to 5 idle carriers; the plugin's hook raises each such write to at least carrierKeep(buildings), so
// the barracks (draft only while carriers > busy + reserve) leaves a pool that grows with the kingdom and new soldiers
// come only from new residences. The native value is only ever raised.
// r45: only economy buildings count (field pid 3572: 86 "buildings" were 28 eye-catcher decorations, 20 towers and a
// castle, so the keep took the whole carrier pool and the computer froze). Military buildings 45..48 (lookout,
// towers, castle) and eye-catchers 64..75 need no carriers; residences and every production building do.
inline bool economyBuilding(int type){return type>=1&&type<=82&&!(type>=45&&type<=48)&&!(type>=64&&type<=75);}
constexpr int ReserveBase=15,ReservePer=2,ReserveMax=120; /* r43: 15 idle carriers plus one per ReservePer completed economy buildings, at most ReserveMax */
inline int carrierKeep(int buildings){return std::min(ReserveMax,ReserveBase+std::max(0,buildings)/ReservePer);}
// r47: specialists (builders 3, diggers 2, the plugin's gardeners 50) are made from idle carriers by the per-tick
// processor sub_D57D0 (Logic::Update) only while reserve < carriers - busy, the same bar as the barracks (field pid 18132
// game 2: every builder died at tick 44,000, carriers fell to the keep and nothing could be recruited for 66,000 ticks;
// the AI's own formula would have kept 10 free). While essential workers are missing the reserve leaves `room` for them
// (missing builders below BuildersMin, diggers below DiggersMin, up to GardenerRoom missing gardeners, at most RoomMax),
// and the hook withholds the same room from the barracks through the economy's +0x1E byte (added to the barracks' bar
// at 0x134D68 but not to the specialists'), so the specialists get those carriers first and nothing drains.
constexpr int BuildersMin=6,DiggersMin=5,GardenerRoom=4,RoomMax=14;
inline int specialistRoom(int builders,int diggers,int gardenersMissing){return std::min(RoomMax,std::max(0,BuildersMin-builders)+std::max(0,DiggersMin-diggers)+std::clamp(gardenersMissing,0,GardenerRoom));}
// The reserve the hook stores: the AI's value raised to the keep; with room, at most carriers - room so that `room`
// carriers stay recruitable (the barracks is held off by the +0x1E byte meanwhile).
inline int carrierReserve(int native,int buildings,int carriers,int room){int keep=carrierKeep(buildings);if(room>0)return std::min(keep,std::max(0,carriers-room));return std::max(native,keep);}
enum class Event:uint8_t {Decline,Launch,Stage,Assault,Siege,Destroyed,Deferred,Retarget,Return,Dissolved,Lost,Adopted,Ignored,Avoid,Collapsed,Greening,Gardeners,Reclaim,Front,Hunt,Join,Call,Fire,Convoy,Defend,DefendFire};
// Deterministic generator owned by the plugin; the game's LCG (RVA 0xE66B10)
// drives every native AI decision and must never be consumed.
struct Random {uint32_t state=0x9e3779b9u;uint32_t next(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}uint32_t bits16(){return next()>>16;}};
// A Gardener as the engine reads it: work state from its job/task (idle =
// default job 50 or a parked WAIT; working = greening job 0xAF), its work
// centre and failed search slices. reserved: the cell it greens (job 0xAF
// only); aim: the cell its search chose (job 0xAE walking to it or 0xAF
// greening it, role +0x40/+0x44 written by 0x179F80 before either), -1 none.
// r33: escorted = job 0xB0 (a WATCH order: walk to destX/destY, role +0x18, then wait; the work centre stays).
// r50: job = the raw job word (entity +0x44), for the far-gardener diagnostics only.
enum class Work:uint8_t {Idle,Busy,Searching,Working};
struct Gardener {uint16_t id=0;uint32_t uid=0;int x=0,y=0,centreX=0,centreY=0,misses=0,reservedX=-1,reservedY=-1,aimX=-1,aimY=-1,destX=-1,destY=-1;Work work=Work::Idle;bool escorted=false;uint16_t job=0;};
// r36 diagnostics (log only, never stored): a unit the fire guard turned back (guard() for soldiers and held gardeners,
// reclaim() for free ones). Gardeners also carry the native aim (job target), work state, WATCH (escorted), the
// campaign cell they had (cellX/cellY) and whether this recall benched them; free = not held by a campaign.
struct Pulled {uint16_t id=0;int x=0,y=0,aimX=-1,aimY=-1,cellX=-1,cellY=-1,centreX=-1,centreY=-1;Work work=Work::Idle;bool gardener=false,escorted=false,benched=false,free=false;};
inline Pulled pulledOf(const Gardener&g,bool benched,bool free,int cellX=-1,int cellY=-1){Pulled q;q.id=g.id;q.x=g.x;q.y=g.y;q.aimX=g.aimX;q.aimY=g.aimY;q.cellX=cellX;q.cellY=cellY;q.centreX=g.centreX;q.centreY=g.centreY;q.work=g.work;q.gardener=true;q.escorted=g.escorted;q.benched=benched;q.free=free;return q;}
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
// r50 diagnostics (log only): the Dark units a defence look left out because they stand within HuntKeep of a live fire
// centre (assess()): how many (fighters, gardeners), the smallest Euclidean distance of one from its nearest fire centre
// (nearest, that centre cx/cy) and the largest (outer: the one closest to the HuntKeep line, at ax/ay).
struct FireDrop {int count=0,fighters=0,gardeners=0,cx=-1,cy=-1,ax=-1,ay=-1;double nearest=-1,outer=-1;};
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
 // at its staging cell for its gardeners since then / for so long (r35, StageWait); convoyNote = last OFFENCE_CONVOY line;
 // rearX/rearY = where the gardeners wait behind the screen; flankAt = last pincer flank order.
 int want=0,legX=0,legY=0,gap=0,rearX=-1,rearY=-1,gdx[MaxGardeners]={},gdy[MaxGardeners]={};uint32_t convoyAt=0,waitSince=0,waited=0,convoyNote=0,flankAt=0;
 // r33 (transient): failUid/fails = targets campaigns of this sector failed to reach and how often; ignoredOrders =
 // ORDER_IGNORED on the current target; lookAt = last intrusion look; defendEnd = when the last defence ended; recallAt =
 // last recall of an attack; reclaimNote/defendNote/defendWhy = rate limits of the idle reclaim and defence notes;
 // threat* = the intrusion as last assessed (units in or near the territory, Dark fighters among them, own Dark cells, rise).
 // r37 (transient): callHomeAt = last call of far gardeners home; landDefendAt = last land-only defence; nearX/nearY/nearBest/
 // nearAt = the leg end the march progress is measured to, the best body distance to it and when it last improved;
 // targetHex = hex distance to the target when it was taken (its time budget).
 uint32_t callHomeAt=0,landDefendAt=0,nearAt=0,targetsNote=0;int nearX=-1,nearY=-1,nearBest=0,targetHex=0;bool landOnly=false; /* landOnly: this defence started against land alone (its budget is LandDefendTime) */
 int ignoredOrders=0,threatUnits=0,threatFighters=0,threatOwn=0,threatRise=0;uint32_t failUid[4]={},lookAt=0,defendEnd=0,recallAt=0,reclaimNote=0,defendNote=0;uint8_t fails[4]={};const char*defendWhy="";
 // r50 (transient): landStep = the time budget of a land-only defence from its start (LandDefendTime, extended by
 // LandDefendStep while it wins, at most DefendTime), landOwnAt = own Dark cells when the current step began; threatLandX/Y
 // = the land point assess() weighed against the units (OFFENCE_DEFEND landAt=, -1 none); beyondFire = the straight way to
 // the area the defence gave up on runs within FireLine of a fire centre; fireDrop/fireDropNote = the last OFFENCE_DEFEND_FIRE.
 uint32_t landStep=0,fireDropNote=0;int landOwnAt=-1,threatLandX=-1,threatLandY=-1;bool beyondFire=false;FireDrop fireDrop{};
 const char*reason="";int value=0,versus=0,dissolveTries=0;
 int8_t only=-1; // offline tests only: the one style decide() may choose (-1: weighted choice)
};
struct Army {int count=0,x=0,y=0,weight=0;};
inline Army summary(const std::vector<Soldier>&m){Army a;a.count=(int)m.size();if(!a.count)return a;long sx=0,sy=0;for(auto&s:m){sx+=s.x;sy+=s.y;a.weight+=soldierWeight(s.type);}a.x=(int)(sx/a.count);a.y=(int)(sy/a.count);return a;}
inline int within(const std::vector<Soldier>&m,int x,int y,int r){int n=0;for(auto&s:m)if(distance(s.x,s.y,x,y)<=r)n++;return n;}
inline int countKind(const std::vector<Target>&t,uint16_t kind){int n=0;for(auto&b:t)if(b.kind==kind)n++;return n;}
inline bool avoided(const Campaign&c,uint32_t uid,uint32_t t){for(int i=0;i<4;i++)if(c.avoidUid[i]==uid&&t<c.avoidUntil[i])return true;return false;}
inline void avoid(Campaign&c,uint32_t uid,uint32_t t,uint32_t ticks=AvoidTicks){int k=-1;uint32_t until=t+ticks;for(int i=0;i<4;i++)if(c.avoidUid[i]==uid)k=i; /* r33: a running longer avoidance is kept */
 if(k>=0)until=std::max(until,c.avoidUntil[k]);else{k=0;for(int i=1;i<4;i++)if(c.avoidUntil[i]<c.avoidUntil[k])k=i;}c.avoidUid[k]=uid;c.avoidUntil[k]=until;}
// r33: a failed target (not reached, orders not executed, gardeners that do not follow): avoided LostAvoid, from its
// FailLimit-th failure in this sector LongAvoid (the field case relaunched on the same unreachable farm five times).
inline int failures(const Campaign&c,uint32_t uid){for(int i=0;i<4;i++)if(uid&&c.failUid[i]==uid)return c.fails[i];return 0;}
inline void failed(Campaign&c,uint32_t uid,uint32_t t){if(!uid)return;int k=-1;for(int i=0;i<4;i++)if(c.failUid[i]==uid)k=i;
 if(k<0){k=0;for(int i=1;i<4;i++)if(c.fails[i]<c.fails[k])k=i;c.failUid[k]=uid;c.fails[k]=0;}if(c.fails[k]<255)c.fails[k]++;avoid(c,uid,t,c.fails[k]>=FailLimit?LongAvoid:LostAvoid);}
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
// r50 diagnostics: a Dark unit at p that a defence look leaves out (within HuntKeep of a live fire centre), counted into d.
inline void dropped(FireDrop&d,const Fire&f,const std::pair<int,int>&p,bool fighter){
 d.count++;if(fighter)d.fighters++;else d.gardeners++;const Zone*z=nullptr;int best=0;
 for(auto&q:f.z){int e=dtfire::fire2(p.first,p.second,q.x,q.y);if(!z||e<best){z=&q;best=e;}}if(!z)return;
 double r=sqrt((double)best);if(d.nearest<0||r<d.nearest){d.nearest=r;d.cx=z->x;d.cy=z->y;}if(r>d.outer){d.outer=r;d.ax=p.first;d.ay=p.second;}
}
// The group order puts member k at click + 2 * spiral[k] (0x16A800, spiral 0xD69968): N members
// reach 2k (Euclid) with k the smallest ring count holding N (1+3k(k+1) cells), plus 2 for blocked slots.
inline int spread(int n){int k=0;while(1+3*k*(k+1)<n)k++;return 2*k+2;}
inline int soldierKeep(int n){return FireLine+spread(n);}
// A straight leg: members walk their own lines from their own slots around the start to their
// slots around the end, up to spread(n) beside the army's line, and bend a little more (soak).
inline int legKeep(int n){return FireLine+spread(n)+LegSlack;}
// r33: the army body: the soldiers within max(BodyReach, 2*spread(N)) of the median cell (x and y medians), so soldiers
// still walking in from scattered reservoirs or formation slots do not drag the reference (the r32 mean did). x,y = the
// median cell; count/weight of the body; core = its soldiers. reachOf = distance to the nearest of them; spanOf = the
// farthest gardener's reachOf (the convoy gap).
constexpr int BodyReach=8;
inline Army body(const std::vector<Soldier>&m,std::vector<Soldier>*core=nullptr){
 Army a;if(core)core->clear();if(m.empty())return a;std::vector<int> xs,ys;xs.reserve(m.size());ys.reserve(m.size());for(auto&s:m){xs.push_back(s.x);ys.push_back(s.y);}
 size_t h=xs.size()/2;std::nth_element(xs.begin(),xs.begin()+h,xs.end());std::nth_element(ys.begin(),ys.begin()+h,ys.end());a.x=xs[h];a.y=ys[h];
 int r=std::max(BodyReach,2*spread((int)m.size()));for(auto&s:m)if(distance(s.x,s.y,a.x,a.y)<=r){a.count++;a.weight+=soldierWeight(s.type);if(core)core->push_back(s);}
 if(!a.count){Army b=summary(m);a.count=b.count;a.weight=b.weight;if(core)*core=m;}
 return a;
}
inline int reachOf(const std::vector<Soldier>&core,int x,int y){int b=1<<30;for(auto&s:core)b=std::min(b,distance(s.x,s.y,x,y));return b;}
inline int spanOf(const std::vector<Gardener>&gs,const std::vector<Soldier>&core){int g=0;for(auto&q:gs)g=std::max(g,reachOf(core,q.x,q.y));return g;}
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
// targets are never chosen. Nearest first: by the walking path from the
// chooser's position when known (r37), else the hex distance plus the home
// detour; a farm with other usable farms within ClusterRadius counts
// ClusterBonus closer per neighbour (at most ClusterMax), so the army chains
// through a group of farms. Returns an index or -1.
constexpr long HallLater=5000;constexpr int ClusterRadius=40,ClusterBonus=8,ClusterMax=24;
inline int chooseTarget(const std::vector<Target>&all,const Fire&f,int fromX,int fromY,const Pick&pick,const Campaign*c=nullptr,uint32_t t=0){
 auto usable=[&](const Target&b){return b.reachable&&b.path>=0&&!(c&&avoided(*c,b.uid,t));}; /* r37: path -1 = no walk from here */
 bool farms=false;
 for(auto&b:all)if(b.kind==Farm&&usable(b)&&!locked(b,f))farms=true; /* r33: neither avoided (failed) nor locked farms forbid other targets */
 int best=-1,low=-1;long bestScore=0,lowScore=0;
 for(int i=0;i<(int)all.size();i++){const Target&b=all[i];if(!usable(b))continue;
  if(locked(b,f))continue;
  if(farms?b.kind!=Farm:!(b.kind==Hall||(pick.citadel&&(b.kind==Temple||b.kind==Fortress))))continue;
  long score=(b.path>0?(long)b.path:distance(fromX,fromY,b.doorX,b.doorY)+std::max(0,b.detour))+(b.kind==Hall&&pick.citadel&&!f.on?HallLater:0); /* r33: plus the walking detour; r37: the walked path when known */
  if(farms){int kin=0;for(auto&o:all)if(&o!=&b&&o.kind==Farm&&usable(o)&&!locked(o,f)&&distance(o.doorX,o.doorY,b.doorX,b.doorY)<=ClusterRadius)kin++;score-=std::min(ClusterMax,ClusterBonus*kin);} /* kin: near/far are Windows macros */
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
// r50 diagnostics: the straight way a->b passes within FireLine of a live fire centre (the area lies beyond the fire, not
// only past the keep of a leg); logged as OFFENCE_DEFEND beyondFire=.
inline bool across(int ax,int ay,int bx,int by,const Fire&f){if(!f.on)return false;for(auto&z:f.z)if(seg2(ax,ay,bx,by,z.x,z.y)<=(double)FireLine*FireLine)return true;return false;}
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
// void sendGardener(const Campaign&,const Gardener&,int x,int y) (one per order, FORWARD: its work centre);
// r33: void park(const Campaign&,const Gardener&,int x,int y) (one per order, WATCH: job 0xB0 walks there and waits,
// the work centre stays); void intrusion(const Campaign&,Intrusion&) (see Intrusion);
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
// r33: the Dark presence in a computer's territory, as the bridge reads it: own = walkable Dark cells the player owns
// (territory scan), rise = their growth over the last full scan pass; gardeners/fighters = Dark Gardeners (type 52) and
// Dark Warriors/Throwers/Shamans standing in or within 4 of the player's territory (the native intruder test 0x1B17E0),
// in the sector's land region.
// r50: ownX/ownY = the centre of the owned Dark cells (their mean position from the scan's 16x16 block counts, -1 none).
struct Intrusion {int own=0,rise=0;int ownX=-1,ownY=-1;std::vector<std::pair<int,int>> gardeners,fighters;};
template<class Ops> void decline(Campaign&c,uint32_t t,Ops&ops,const char*why){
 c.reason=why;ops.report(Event::Decline,c);if(c.chance<ChanceFull)c.chance=std::min<uint32_t>(ChanceFull,c.chance+ChanceStep);c.nextCheck=t+CheckEvery+(c.random.next()&127);
}
inline void toIdle(Campaign&c,uint32_t t){c.dissolveTries=0;c.phase=Phase::Idle;c.squad=0;c.progress=0;c.vias=0;c.viaAt=0;c.lures=0;c.replanAt=0;c.homeless=false;c.split=false;c.hunting=false;c.escort=false;c.deferredSince=0;c.nextTry=0;c.greenSince=0;c.ringDark=-1;c.razed=false;c.holding=false;c.gardeners=0;c.chain=0;c.quietSince=0;c.checked=true;c.target=Target{};c.waitSince=0;c.waited=0;c.gap=0;c.convoyAt=0;c.rearX=c.rearY=-1;c.flankAt=0;c.ignoredOrders=0;
 if(c.style==Style::Defend)c.defendEnd=t?t:1; /* r33: defences rest before the next one */
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
// The campaign's gardeners still alive (dead or lost ones are dropped; r33: counted as lost at a besieged target here,
// whoever looks first: a convoy step on the assault leg drops them before the siege could count them).
template<class Ops> void living(Campaign&c,Ops&ops,std::vector<Gardener>&out){
 out.clear();int n=0;
 for(int i=0;i<c.gardeners;i++){Gardener g;if(!ops.gardener(c,c.gid[i],c.guid[i],g))continue;c.gid[n]=c.gid[i];c.guid[n]=c.guid[i];c.gsent[n]=c.gsent[i];c.gcx[n]=c.gcx[i];c.gcy[n]=c.gcy[i];c.gdx[n]=c.gdx[i];c.gdy[n]=c.gdy[i];n++;out.push_back(g);}
 if(c.phase==Phase::Assault&&c.target.id)c.lost+=c.gardeners-n;c.gardeners=n;
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
 Army a=body(m);c.orderAt=t;c.orderX=a.x;c.orderY=a.y;c.goalX=x;c.goalY=y;c.checked=false;c.lastOrder=t;ops.move(c,m,x,y,home); /* r33: the body, as the ORDER_IGNORED check reads it */
}
// A pincer: the even half to (x,y), the odd half to (x2,y2).
template<class Ops> void sendSplit(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,int x,int y,int x2,int y2){
 std::vector<Soldier> a,b;halves(m,a,b);Army s=body(a);c.orderAt=t;c.orderX=s.x;c.orderY=s.y;c.goalX=x;c.goalY=y;c.goal2X=x2;c.goal2Y=y2;c.checked=false;c.lastOrder=t;
 if(!a.empty())ops.move(c,a,x,y,false);if(!b.empty())ops.move(c,b,x2,y2,false);
}
// Units the group orders spread: soldiers and the gardeners that follow them.
inline int groupSize(const Campaign&c,const std::vector<Soldier>&m){return std::max(1,std::max((int)m.size(),c.gardeners));}
// r32 convoy: the army and its gardeners march as one group (the field case: the soldiers stood at a farm while
// their gardeners trailed far behind, unprotected). SettlerValues.xml gives every settler type the same (empty,
// atoi 0 at 0x183CE1) speed byte, so soldiers and gardeners walk alike; the soaks run gardeners slower too.
// r33: escorted gardeners move with WATCH (park: job 0xB0 walks there and waits, the work centre stays, so no gardener
// greens its way off while following; FORWARD only onto work: a ring cell, a sweep or defence cell, reclaim, the release
// at home). The gap is the farthest held gardener's distance to the nearest soldier of the army body (body()); the
// convoy is together while it is within convoyLimit(). Gardeners are dropped only when dead, benched by the fire guard
// or standing in another land region.
// r35 (user: "it should not babysit gardeners that hard, movement of army should be fast"; field pid 22316, r34: armies
// held 2,000..5,000 ticks "waiting for gardeners" with gaps up to 234, and a 221-soldier army stepped ConvoyLead tiles
// every ConvoyEvery ticks re-formed on each step and hardly moved: "order not executed" on every look): the army walks
// each leg (waypoint, staging cell, assault cell, home) in one order straight to its end and never holds or walks back
// for its gardeners on the way. The gardeners follow to their own cells around the same leg end (follow()), behind the
// army because they start behind it and walk alike. Only the staging cell waits for them, StageWait ticks at most
// (gathered()); the assault then starts and late gardeners join at the rear point behind the screen. ConvoyLead now
// only sizes the short last leg of a direct assault and the ORDER_IGNORED check.
// GatherHome: the home area (the release of a gardener). RallyGap: half of PickReach. TopUpReach: replacements only
// from free gardeners this close to the army (others wait at home for the next convoy). EscortRadius: a waiting
// gardener farther from the army is called back to the rear point. RearStep: the rear point lies this far behind the
// screen towards the army's approach. ScreenRing/ThreatRadius: the screen stands within ScreenRing of the door on the
// side of the nearest Dark fighter within ThreatRadius. GuardWait: gardeners start greening when the screen holds, or
// this long after the assault began (never while Dark fighters are near the door).
// r33: LaunchReach = a new campaign takes free gardeners only this close to home (reclaim sends free gardeners up to
// ReclaimHome+ReclaimRadius out), PickReach = a hand-over or adopted army only this close to the army; SnapRing = a leg
// end on unwalkable land (water) moves to the nearest standable cell within this many rings; ConvoyResend = the earliest
// re-order of one gardener.
constexpr int ConvoyLead=10,ConvoyGap=8,RallyGap=30,GatherHome=30,TopUpReach=20,EscortRadius=16,RearStep=4,ScreenRing=6,ThreatRadius=30;
constexpr int LaunchReach=ReclaimHome+ReclaimRadius,PickReach=2*RallyGap,SnapRing=6;
constexpr uint32_t ConvoyEvery=30,ConvoyNote=300,GuardWait=300,ConvoyResend=GuardResend,StageWait=300;
inline int convoyLimit(int gardeners){return ConvoyGap+spread(std::max(1,gardeners));}
inline int gapOf(const std::vector<Gardener>&gs,int x,int y){int g=0;for(auto&q:gs)g=std::max(g,distance(q.x,q.y,x,y));return g;}
inline bool convoying(const Campaign&c){return c.gardeners>0&&!c.hunting;}
// The cell of the segment (ox,oy)-(dx,dy) at hex distance rem from (dx,dy) (clamped to the segment; the hex
// distance is a norm, so it scales linearly along the segment).
inline void along(int ox,int oy,int dx,int dy,int rem,int&cx,int&cy){int len=distance(ox,oy,dx,dy);if(rem<=0||len<=0){cx=dx;cy=dy;return;}if(rem>=len){cx=ox;cy=oy;return;}
 cx=dx+(int)lround((double)(ox-dx)*rem/len);cy=dy+(int)lround((double)(oy-dy)*rem/len);}
// The i-th cell of the hex spiral around (0,0): ring k starts at k*(1,0) and walks k steps along each side.
inline void slot(int i,int&dx,int&dy){dx=dy=0;if(i<=0)return;static const int nx[6]={1,1,0,-1,-1,0},ny[6]={0,1,1,0,-1,-1};int k=1,n=i-1;while(n>=6*k){n-=6*k;k++;}int j=n/k,s=n%k;dx=k*nx[j]+s*nx[(j+2)%6];dy=k*ny[j]+s*ny[(j+2)%6];}
// The start of the current leg of the stored route.
inline void legFrom(const Campaign&c,int&x,int&y){if(c.viaAt>0&&c.viaAt<=c.vias){x=c.vx[c.viaAt-1];y=c.vy[c.viaAt-1];}else{x=c.legX;y=c.legY;}}
// r33: a gardener parked for (x,y) needs no new order: WATCH (job 0xB0) for that cell and still walking or standing within 3 of it.
inline bool settled(const Gardener&g,int x,int y){return g.escorted&&(g.destX<0||distance(g.destX,g.destY,x,y)<=1)&&(g.work==Work::Busy||distance(g.x,g.y,x,y)<=3);}
// Gardeners (one id per order) parked (r33: WATCH) on their own cells around (x,y): the odd lattice x+2*dx+1, y+2*dy of
// the hex spiral (never a formation slot of a group order to (x,y), where the soldiers stand), else the nearest acceptable
// cell within 3 of it, else of (x,y) itself (r32 skipped the gardener when its slot and the point were occupied, so a
// held army sent nobody); a cell is acceptable when standable, GardenerKeep from the fire, spare next to the target and
// spot() takes it (r32 review: a rear point's cells nearer the threat than the screen are refused); only along a clear
// leg; take(i, g) picks who goes; r33: re-ordered from its real state (settled()), at most every ConvoyResend ticks.
template<class Ops,class Take,class Spot> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y,Take take,Spot spot){
 int sent=0;std::vector<std::pair<int,int>> used;
 auto bad=[&](int qx,int qy){if(closer(qx,qy,f,GardenerKeep)||!ops.stand(c,qx,qy,1)||(c.target.id&&!ops.spare(c.target,qx,qy))||!spot(qx,qy))return true;for(auto&u:used)if(u.first==qx&&u.second==qy)return true;return false;};
 for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){const Gardener&g=gs[i];if(!take(i,g))continue;int ox=0,oy=0;slot(i,ox,oy);int px=x+2*ox+1,py=y+2*oy;
  if(bad(px,py)){bool found=false;int best=0,bx=0,by=0;
   for(int k=0;k<2&&!found;k++){int sx=k?x:px,sy=k?y:py;for(int r=k;r<=3;r++)for(int qy=-r;qy<=r;qy++)for(int qx=-r;qx<=r;qx++){int ax=sx+qx,ay=sy+qy;if(distance(sx,sy,ax,ay)!=r||bad(ax,ay))continue;int d=distance(ax,ay,px,py)*4+r;if(!found||d<best){found=true;best=d;bx=ax;by=ay;}}}
   if(!found)continue;px=bx;py=by;}
  used.push_back({px,py});
  if(c.gdx[i]>=0&&distance(c.gdx[i],c.gdy[i],px,py)<2&&settled(g,c.gdx[i],c.gdy[i]))continue;
  if(c.gsent[i]&&t-c.gsent[i]<ConvoyResend)continue;
  if(!legClear(g.x,g.y,px,py,f,GardenerKeep))continue; /* never across the fire: the guard brings it out first */
  ops.park(c,g,px,py);c.gdx[i]=px;c.gdy[i]=py;c.gsent[i]=t?t:1;sent++;}
 return sent;
}
template<class Ops,class Take> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y,Take take){return gather(c,t,ops,gs,f,x,y,take,[](int,int){return true;});}
template<class Ops> int gather(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int x,int y){return gather(c,t,ops,gs,f,x,y,[](int,const Gardener&){return true;});}
// r33: a held gardener given work (FORWARD: a ring, sweep or defence cell): no longer parked there.
template<class Ops> void work(Campaign&c,uint32_t t,Ops&ops,int i,const Gardener&g,int x,int y){ops.sendGardener(c,g,x,y);if(i>=0&&i<c.gardeners){c.gdx[i]=c.gdy[i]=-1;c.gsent[i]=t?t:1;}}
// The convoy is together: no gardener (held, living) farther than convoyLimit() from the nearest soldier of the army body (r33).
template<class Ops> bool together(Campaign&c,Ops&ops,const std::vector<Soldier>&m){
 if(!convoying(c)||m.empty())return true;std::vector<Gardener> gs;living(c,ops,gs);if(gs.empty())return true;std::vector<Soldier> core;body(m,&core);c.gap=spanOf(gs,core);return c.gap<=convoyLimit((int)gs.size());
}
template<class Ops> bool holdPoint(const Campaign&c,Ops&ops,const Fire&f,int x0,int y0,int fromX,int fromY,int keep,int&hx,int&hy);
template<class Ops> void startReturn(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const char*why);
// r33: held gardeners standing in another land region than the sector's (no walk to the army exists) are dropped.
template<class Ops> void strays(Campaign&c,Ops&ops,std::vector<Gardener>&gs){bool any=false;for(int i=std::min((int)gs.size(),c.gardeners)-1;i>=0;i--)if(!ops.reach(c,gs[i].x,gs[i].y)){dropGardener(c,i);any=true;}if(any)living(c,ops,gs);}
// r35: the held gardeners to their cells around the leg end (dx,dy) (gather(): WATCH, one id per order, re-ordered only
// when the cell changes). A gardener whose straight way there passes the fire (GardenerKeep) takes the latest point of
// the stored route it reaches clear instead: the leg start (ox,oy), then the earlier waypoints back to the route start.
template<class Ops> void follow(Campaign&c,uint32_t t,Ops&ops,const std::vector<Gardener>&gs,const Fire&f,int ox,int oy,int dx,int dy){
 int px[MaxVias+3],py[MaxVias+3],k=0;px[k]=dx;py[k++]=dy;px[k]=ox;py[k++]=oy;
 for(int v=std::min(c.viaAt,c.vias)-1;v>=0;v--){px[k]=c.vx[v];py[k++]=c.vy[v];}px[k]=c.legX;py[k++]=c.legY;
 int pick[MaxGardeners];int held=std::min((int)gs.size(),c.gardeners);
 for(int i=0;i<held;i++){pick[i]=-1;for(int q=0;q<k&&pick[i]<0;q++)if(!f.on||legClear(gs[i].x,gs[i].y,px[q],py[q],f,GardenerKeep))pick[i]=q;}
 for(int q=0;q<k;q++){bool any=false;for(int i=0;i<held;i++)if(pick[i]==q)any=true;
  if(any)gather(c,t,ops,gs,f,px[q],py[q],[&](int i,const Gardener&){return i<held&&pick[i]==q;});}
}
// One convoy step of the army m towards (dx,dy) along the leg from (ox,oy) (r35: one order to the leg end, the
// gardeners following). False when the leg from the army to (dx,dy) is not clear of the fire (the caller plans again).
template<class Ops> bool convoy(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,int ox,int oy,int dx,int dy,bool home){
 std::vector<Gardener> gs;living(c,ops,gs);strays(c,ops,gs);c.convoyAt=t?t:1;c.gap=0;
 if(!m.empty()){std::vector<Soldier> core;Army a=body(m,&core);int n=groupSize(c,m);if(!gs.empty())c.gap=spanOf(gs,core);
  /* An army near the fire (leaving it by its exit leg) goes straight to (dx,dy), re-sent every GuardResend. */
  bool hot=f.on&&closer(a.x,a.y,f,soldierKeep(n));int cx=dx,cy=dy;
  /* r33: a leg end on unwalkable land (water: the group order's walk-sector test refuses it, ORDER_IGNORED) moves to the nearest standable cell. */
  if(!ops.stand(c,cx,cy,1)){for(int r=1;r<=SnapRing;r++){bool ok=false;for(int qy=-r;qy<=r&&!ok;qy++)for(int qx=-r;qx<=r&&!ok;qx++){int px=dx+qx,py=dy+qy;if(distance(dx,dy,px,py)==r&&ops.stand(c,px,py,1)&&!closer(px,py,f,soldierKeep(n))){cx=px;cy=py;ok=true;}}if(ok)break;}}
  if(f.on&&!hot&&!legClear(a.x,a.y,cx,cy,f,legKeep(n)))return false;
  if((hot||!closer(cx,cy,f,soldierKeep(n)))&&(distance(cx,cy,c.goalX,c.goalY)>=2||!c.orderAt||t-c.lastOrder>=(hot?GuardResend:2*ReorderEvery)))send(c,t,ops,m,cx,cy,home);}
 if(!gs.empty())follow(c,t,ops,gs,f,ox,oy,dx,dy); /* the army gone: the gardeners walk on */
 return true;
}
// r35: the army at its staging cell waits for its gardeners (together()) at most StageWait ticks, then goes on without
// the stragglers (they follow to the rear point). waitSince/waited count from the first look there.
template<class Ops> bool gathered(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(together(c,ops,m)){c.waitSince=0;c.waited=0;return true;}
 if(!c.waitSince)c.waitSince=t?t:1;c.waited=std::max<uint32_t>(1,t-c.waitSince);
 bool go=c.waited>=StageWait;
 if(go||!c.convoyNote||t-c.convoyNote>=ConvoyNote){c.convoyNote=t?t:1;Army a=body(m);Campaign note=c;note.value=0;note.versus=convoyLimit(c.gardeners);note.goalX=a.x;note.goalY=a.y;note.reason=go?"assault before the gardeners":"waiting for gardeners";ops.report(Event::Convoy,note);}
 if(go){c.waitSince=0;c.waited=0;}
 return go;
}
// The army and its gardeners on to (x,y): a convoy step, or a plain order without gardeners.
template<class Ops> void advance(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,int x,int y,bool home){
 if(!convoying(c)){if(!m.empty())send(c,t,ops,m,x,y,home);return;} /* r33: no gardeners walk without the convoy (a hunt holds none) */
 int ox=0,oy=0;legFrom(c,ox,oy);if(!convoy(c,t,ops,m,f,ox,oy,x,y,home))c.replanAt=t+Period;
}
// The way home from (fx,fy) for the army and its gardeners: routed around the fire (plan()). Without
// a safe way the army is never sent straight home: it walks out to the exit point (when it stands
// near the fire) or holds where it is, and plans again after RetryPlan ticks (homeless).
template<class Ops> void homeward(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,const Fire&f,const Strength&s,int fx,int fy){
 auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};
 if(plan(c,f,fx,fy,s.homeX,s.homeY,groupSize(c,m),stand)){c.homeless=false;int x=walking(c)?c.vx[0]:s.homeX,y=walking(c)?c.vy[0]:s.homeY;advance(c,t,ops,m,f,x,y,true);return;} /* r32: home as a convoy */
 bool first=!c.homeless;c.homeless=true;c.retryAt=t+RetryPlan;c.lastOrder=t;int x=-1,y=-1;
 if(walking(c)){x=c.vx[0];y=c.vy[0];if(!m.empty())send(c,t,ops,m,x,y,true);std::vector<Gardener> gs;living(c,ops,gs);if(!gs.empty())gather(c,t,ops,gs,f,x,y);} /* r33: parked with the army */
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
 c.reason=why;c.phase=Phase::Return;c.since=t;c.vias=0;c.viaAt=0;c.split=false;c.hunting=false;c.waitSince=0;c.waited=0;c.replanAt=0;c.homeless=false;Strength s=ops.strength(c);
 if(c.style==Style::Defend)ops.report(Event::Defend,c); /* r33: OFFENCE_DEFEND end (phase 3, the reason) */
 ops.report(Event::Return,c);
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
 gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&){return c.gcx[i]<0;},[&](int px,int py){return !th||distance(px,py,tx,ty)>reach0;}); /* r33: working ones too (stray Dark land) */
}
// March orders go to the next waypoint, then the staging cell: as a convoy with the gardeners (r32; r35: one order per
// leg, the gardeners following), the army alone while it hunts. A pincer's gardeners march with its first half, the
// second half goes straight to its own staging cell. The assault cell keeps soldierKeep(N) while the fire is on;
// without one the target is avoided (LostAvoid) and the army re-targets (never the door inside the fire). An
// army facing Dark fighters near the door takes a screen cell on their side (r32); the gardeners wait behind it.
template<class Ops> void order(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.phase==Phase::March){
  if(c.split){
   if(!convoying(c)){sendSplit(c,t,ops,m,c.stageX,c.stageY,c.stage2X,c.stage2Y);return;}
   std::vector<Soldier> a,b;halves(m,a,b);
   if(!b.empty()&&(c.goal2X!=c.stage2X||c.goal2Y!=c.stage2Y||!c.flankAt||t-c.flankAt>=2*ReorderEvery)){c.goal2X=c.stage2X;c.goal2Y=c.stage2Y;c.flankAt=t?t:1;ops.move(c,b,c.stage2X,c.stage2Y,false);}
   std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
   if(!convoy(c,t,ops,a.empty()?b:a,f,c.legX,c.legY,c.stageX,c.stageY,false))c.replanAt=t+Period;return;}
  int x=walking(c)?c.vx[c.viaAt]:c.stageX,y=walking(c)?c.vy[c.viaAt]:c.stageY;
  if(!convoying(c)){send(c,t,ops,m,x,y,false);return;} /* hunting, or no gardeners */
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
 /* r35: the assault cell in one order (r33 walked a long assault leg as a convoy in ConvoyLead steps); the gardeners wait at the rear point */
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
 bool pincer=c.split;c.split=false;c.vias=0;c.viaAt=0;c.replanAt=0;c.since=t;c.progressAt=t;c.targetHex=c.target.id?std::max(distance(fromX,fromY,c.target.doorX,c.target.doorY),c.target.path):0;c.nearX=c.nearY=-1; /* r38: the walked path when known */c.greenSince=0;c.ringDark=-1;c.razed=false;c.holding=false;c.waitSince=0;c.waited=0;c.rearX=c.rearY=-1;c.flankAt=0;c.legX=a.x;c.legY=a.y;c.ignoredOrders=0;
 resetCells(c); /* a new target: every gardener gets a cell again */
 /* r35: no rally: an army in the field whose gardeners stand far away (a hunt's hand-over) marches on, they follow its legs. */
 if(pincer&&c.target.id){int half=std::max((a.count+1)/2,std::max(1,c.gardeners)),leg=legKeep(half),lx=0,ly=0,rx=0,ry=0,ax=0,ay=0,bx=0,by=0;
  turn(c.target.doorX,c.target.doorY,fromX,fromY,PincerAngle,lx,ly);turn(c.target.doorX,c.target.doorY,fromX,fromY,-PincerAngle,rx,ry);
  if(stagePoint(c.target,f,lx,ly,StageDistance,half,stand,ax,ay)&&stagePoint(c.target,f,rx,ry,StageDistance,half,stand,bx,by)&&distance(ax,ay,bx,by)>=PincerGap&&legClear(a.x,a.y,ax,ay,f,leg)&&legClear(a.x,a.y,bx,by,f,leg)){
   c.split=true;c.phase=Phase::March;c.stageX=ax;c.stageY=ay;c.stage2X=bx;c.stage2Y=by;ops.report(Event::Stage,c);order(c,t,ops,m);return true;}
  if(c.style==Style::Pincer)c.style=Style::Assault; /* no room for two flanks: one army */
 }
 bool legOk=legClear(a.x,a.y,c.target.doorX,c.target.doorY,f,legKeep(std::max(1,a.count)));
 bool direct=distance(a.x,a.y,c.target.doorX,c.target.doorY)<=AssaultDistance&&legOk&&together(c,ops,m),staged=false; /* r32: scattered gardeners: march first (r35: the staging cell waits for them at most StageWait) */
 if(!direct&&stagePoint(c.target,f,fromX,fromY,StageDistance,n,stand,sx,sy)){
  if(plan(c,f,a.x,a.y,sx,sy,n,stand)){staged=true;c.phase=Phase::March;c.stageX=sx;c.stageY=sy;ops.report(Event::Stage,c);}
  else c.vias=0;
 }
 if(!staged){
  if(!direct&&f.on&&!legOk){avoid(c,c.target.uid,t);c.reason="no way outside the fire defence";ops.report(Event::Avoid,c);c.beyondFire=across(a.x,a.y,c.target.doorX,c.target.doorY,f);startReturn(c,t,ops,m,"no way outside the fire defence");return false;} /* r50: beyondFire for OFFENCE_DEFEND */
  c.phase=Phase::Assault;ops.report(Event::Assault,c);
 }
 if(c.phase==Phase::Assault&&(c.style==Style::Sweep||c.style==Style::Defend)&&!c.target.id){c.lastOrder=0;return true;} /* sweep()/hold() place the escort */
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
 ops.paths(c,a.x,a.y,all); /* r37: the next target by the walk from where the army stands */
 Pick pick;pick.citadel=true;pick.limit=defenceLimit(cfg,a.weight);pick.weakest=!light(c);
 int i=chooseTarget(all,f,a.x,a.y,pick,&c,t);
 if(c.style==Style::Sweep&&(i<0||distance(all[i].x,all[i].y,c.frontX,c.frontY)>SweepTarget)){resume(c,t);return true;} /* a sweep stays on its front */
 if(i<0){startReturn(c,t,ops,m,c.style==Style::Hunt?"hunt over":light(c)?"raid found no weak target":"no target left");return false;}
 c.target=all[i];c.targetSince=t;c.deferredSince=0;c.nextTry=0;
 if(take&&!c.gardeners){if(!c.want)c.want=gardenersFor(c.style,(int)m.size());assign(c,ops,a.x,a.y,c.want,t,[&](const Gardener&g){return distance(g.x,g.y,a.x,a.y)<=PickReach;});if(!c.gardeners){startReturn(c,t,ops,m,c.style==Style::Hunt?"hunt over":"no gardeners left");return false;}} /* r32: nearest the army; approach() walks the army to them; r33: within PickReach (farther ones: home, the next convoy) */
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
// r33 defence (user, 2026-10-19: when the Dark land spreads into their territory the computers should defend it and
// reclaim it first; field pid 13612: 553 -> 5,805 owned Dark cells while their soldiers stood idle). The native agents
// never treat Dark units as intruders (CAIAgentIntruderCombat's type table 0xBF0438 = 46, 48, 49, 47, 50, 45) and an
// idle soldier's own search reaches 13 tiles only, so the director leads the defence: a 'defend' campaign takes
// soldiers to the intrusion (next to the nearest group of Dark units: computer soldiers attack Dark Gardeners, Warriors,
// Throwers and Shamans on their own once within about 13 tiles; Mushroom Farmers, slaves and servants never), its
// gardeners follow parked behind (convoy) and green the Dark land of the player's own territory near the army, area by
// area, while the soldiers screen them.
// Threat (assess): units = Dark Gardeners and Dark fighters in or within 4 of the territory (none within HuntKeep of a
// live fire: never chased into the fire), own = Dark cells the player owns, rise = their growth in one scan pass. Any: a
// unit, or own >= DefendCells. Serious: own >= SeriousCells, rise >= SeriousRise, a Dark fighter within SeriousHome of
// home, or SeriousGardeners Dark Gardeners. A serious one recalls a running attack (answer(): not a siege whose ring is
// two thirds green, at most every RecallGap ticks); a minor one is defended by an idle sector when the last campaign was
// not a defence (attacks come between) or 4*DefendRest after it; otherwise free gardeners reclaim (reclaim()).
// Size: the larger of DefendMin and a quarter (serious: half) of the army, more until its weight is twice the Dark
// fighters' (2 each); the home reserve stays; no defence when even the whole army is lighter than the Dark fighters.
// Gardeners: base 6 + soldiers/GardenerPer + own/DefendPerGardener, at most gardenersWanted(free soldiers), only from
// within LaunchReach of home. In place (hold()), every DefendLook ticks: Dark fighters within ThreatRadius heavier than
// the army: home ("threat too strong"); the soldiers stand next to the nearest group of Dark units within DefendReach of
// the area, else beside own Dark land near the army; no own Dark land within EscortRadius and no units: the next area
// (the own Dark cell nearest the area), none left: home ("land clean"); DefendTime ends it. Gardeners get own Dark cells
// within EscortRadius of the army body (never within DefendDanger of a Dark fighter, GardenerKeep from the fire), the
// others wait parked behind the army, away from the units; longer moves are convoy steps.
constexpr int DefendCells=24,SeriousCells=150,SeriousRise=40,SeriousHome=40,SeriousGardeners=3,DefendMin=6,DefendReach=30,DefendDanger=4,DefendPerPass=8,DefendPerGardener=100;
constexpr uint32_t DefendLook=40,DefendTime=6000,DefendRest=1500,DefendPause=300,RecallGap=3000,DefendNote=3000;
// r37 (field, The Alliance: the computers own thousands of Dark cells from the map start, so every decision began a
// 6,000-tick "serious intrusion" land defence and 3 attacks came in 112,000 ticks): a threat of land alone never comes
// before an attack. While an attack is possible it is deferred until LandDefendGap after the last one, and a land-only
// defence takes an escort (escortSize) for at most LandDefendTime; only Dark fighters in the territory recall a running
// attack (answer()).
constexpr uint32_t LandDefendGap=12000,LandDefendTime=3000;
// r50 (field pid 18584: the 16-gardener land defence at 105,570 was cutting own Dark 3,567 -> 3,299 when LandDefendTime ended
// it; own Dark then rose to 4,636 by 121,950): a land-only defence that wins goes on. At the end of its budget it gets
// LandDefendStep more, up to DefendTime, while its last step cut own Dark by LandDefendGain or more, Dark units stand near the
// area, or it is still on its way there (extend()); it still ends when the land is clean or the escort is beaten.
constexpr uint32_t LandDefendStep=1500;constexpr int LandDefendGain=40;
// r50 (pid 31460, tick 131,430: 111 soldiers went 118 tiles to two Dark Gardeners while own Dark rose 2,207 -> 3,961
// elsewhere): while own Dark is rising (rise > 0) or above SeriousCells, no Dark fighter stands near home and the unit cluster
// holds fewer than DefendFew units, a cluster more than DefendFar from the land (the reclaimable own Dark cell nearest the
// centre of the own Dark land) is left alone: the defence goes to the land, and its soldiers still answer units that come
// within DefendReach of it (hold()). Splitting a defence into a soldier group and a gardener group is not done.
constexpr int DefendFar=60,DefendFew=3;
// r50 diagnostics: OFFENCE_DEFEND_FIRE (Dark units left out within HuntKeep of the fire) at most this often per sector.
constexpr uint32_t FireDropNote=1000;
// r33 review: land = a reclaimable own Dark cell exists (outside every keep and FrontKeep, workable, in the sector's
// region). The owned-cell count includes cells in the keeps around the Dark base that no gardener may green; it makes an
// intrusion serious (and adds defence gardeners) only while such a cell exists, else such land kept every intrusion
// serious and attacks never came again.
// r50: landAim = the area is the land, not the unit cluster (landX/landY: the land point weighed, -1 when not weighed);
// drop = the units left out near the fire (diagnostics).
struct Threat {bool any=false,serious=false,land=false,landAim=false;int x=-1,y=-1,units=0,fighters=0,gardeners=0,own=0,rise=0,landX=-1,landY=-1;FireDrop drop;};
template<class Ops> Threat assess(const Campaign&c,Ops&ops,const Fire&f,const Strength&s,const std::vector<Target>&all,bool aim=true){ /* r50: aim = may go to the land (not for a recall) */
 Threat th;Intrusion in;ops.intrusion(c,in);th.own=in.own;th.rise=in.rise;std::vector<std::pair<int,int>> units;int close=0;
 for(auto&p:in.fighters)if(!closer(p.first,p.second,f,HuntKeep)){units.push_back(p);th.fighters++;if(distance(p.first,p.second,s.homeX,s.homeY)<=SeriousHome)close++;}else dropped(th.drop,f,p,true);
 for(auto&p:in.gardeners)if(!closer(p.first,p.second,f,HuntKeep)){units.push_back(p);th.gardeners++;}else dropped(th.drop,f,p,false);
 th.units=(int)units.size();int n=0,lx=-1,ly=-1;std::vector<Keep> keep;
 if(in.own>0){keeps(c,ops,all,f,keep);frontKeeps(f,keep);int dx=0,dy=0;if(ops.ownDark(c,s.homeX,s.homeY,OwnRadius,keep,dx,dy)&&ops.reach(c,dx,dy)){th.land=true;lx=dx;ly=dy;}}
 if(!units.empty()){cluster(units,f,s.homeX,s.homeY,th.x,th.y,n);
  /* r50: few units far from rising own Dark land, none near home: the defence goes to the land being lost */
  if(aim&&th.land&&(in.rise>0||in.own>=SeriousCells)&&!close&&n<DefendFew){int gx=lx,gy=ly,qx=0,qy=0;
   if(in.ownX>=0&&ops.ownDark(c,in.ownX,in.ownY,OwnRadius,keep,qx,qy)&&ops.reach(c,qx,qy)){gx=qx;gy=qy;}
   th.landX=gx;th.landY=gy;if(distance(th.x,th.y,gx,gy)>DefendFar){th.x=gx;th.y=gy;th.landAim=true;}}}
 else if(th.land&&in.own>=DefendCells){th.x=lx;th.y=ly;}
 th.any=th.x>=0&&ops.reach(c,th.x,th.y);
 th.serious=th.any&&((th.land&&(in.own>=SeriousCells||in.rise>=SeriousRise))||close>0||th.gardeners>=SeriousGardeners);
 return th;
}
// Defence gardeners for the reclaimable own land (r33 review: none for land no gardener may green).
inline int landGardeners(const Threat&th){return th.land?th.own/DefendPerGardener:0;}
inline void noteThreat(Campaign&c,const Threat&th){c.threatUnits=th.units;c.threatFighters=th.fighters;c.threatOwn=th.own;c.threatRise=th.rise;c.threatLandX=th.landX;c.threatLandY=th.landY;c.fireDrop=th.drop;}
// r50 diagnostics (field question: does HuntKeep hide a threat near the Temple so that it looks land-only?): when a defence
// look left Dark units out because they stand within HuntKeep of a live fire centre, OFFENCE_DEFEND_FIRE with their count and
// distances and what the look made of the rest (none, land, units, serious), at most every FireDropNote ticks per sector.
inline const char*threatName(const Threat&th){return !th.any?"none":th.serious?"serious":th.units?"units":"land";}
template<class Ops> void fireNote(Campaign&c,uint32_t t,Ops&ops,const Threat&th){
 if(!th.drop.count||(c.fireDropNote&&t-c.fireDropNote<FireDropNote))return;c.fireDropNote=t?t:1;
 Campaign note=c;noteThreat(note,th);note.frontX=th.x;note.frontY=th.y;note.reason=threatName(th);ops.report(Event::DefendFire,note);
}
// OFFENCE_DEFEND for a declined defence: on a new reason, then every DefendNote ticks.
template<class Ops> void defendNote(Campaign&c,uint32_t t,Ops&ops,const Threat&th,const char*why){
 if(why==c.defendWhy&&c.defendNote&&t-c.defendNote<DefendNote)return;c.defendWhy=why;c.defendNote=t?t:1;
 Campaign note=c;noteThreat(note,th);note.frontX=th.x;note.frontY=th.y;note.reason=why;ops.report(Event::Defend,note);
}
// Soldiers for a defence of a threat: nearest to it first (free must be sorted so); returns how many, 0 = too strong.
inline int defendSize(const std::vector<Soldier>&free,int army,const Threat&th){
 int take=std::min(army,std::max(DefendMin,th.serious?army/2:army/4)),w=0,dark=2*th.fighters;for(int k=0;k<take;k++)w+=soldierWeight(free[k].type);
 while(take<army&&w<2*dark){w+=soldierWeight(free[take].type);take++;}
 return w<dark?0:take;
}
// r37: whether the sector could launch an attack (a reachable building target and a spare army; before firstAttack too,
// so the land waits for the first attack instead of delaying it).
template<class Ops> bool attackable(Campaign&c,uint32_t t,Ops&ops,const Strength&s,const std::vector<Target>&all,const Fire&f){
 Config cfg=ops.config(c.player);if(!cfg.ok||cfg.mode<=0)return false;
 std::vector<Soldier> free;ops.available(c,s.homeX,s.homeY,free);int avail=(int)free.size();if(avail-std::max(HomeReserve,avail/6)<MinArmy)return false;
 return chooseTarget(all,f,s.homeX,s.homeY,Pick{},&c,t)>=0;
}
// An idle sector defends its land (before any attack decision). True when a defence was launched.
template<class Ops> bool defend(Campaign&c,uint32_t t,Ops&ops){
 if(t<ReclaimStart||(c.lookAt&&t-c.lookAt<DefendLook))return false;c.lookAt=t?t:1;
 Strength s=ops.strength(c);if(!s.ok)return false;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Threat th=assess(c,ops,f,s,all);fireNote(c,t,ops,th); /* r50 */
 if(!th.any)return false;
 if(c.defendEnd&&t-c.defendEnd<(th.serious?DefendPause:DefendRest))return false; /* a breather after a defence */
 if(!th.serious&&(t<c.cooldownUntil||(c.lastStyle==Style::Defend&&c.defendEnd&&t-c.defendEnd<4*DefendRest)))return false; /* minor: attacks come between */
 bool landOnly=!th.units,rush=false;
 /* r37; r50: land being taken now (a rise of SeriousRise or more in one scan pass) skips the gap, old land without a rise
    still waits (The Alliance starts with thousands of own Dark cells and must keep attacking) */
 if(landOnly&&attackable(c,t,ops,s,all,f)){Config cfg=ops.config(c.player);uint32_t since=c.landDefendAt?c.landDefendAt:firstAttack(cfg.delay);
  if(t<since+LandDefendGap){if(!(th.serious&&th.rise>=SeriousRise)){defendNote(c,t,ops,th,"deferred: attacks first");return false;}rush=true;}}
 /* r33 review: a threat of land only needs gardeners, at home (LaunchReach) or near the area (hold() adopts those on
    arrival); without any the army marched out, found no gardener and went home, again every DefendPause. */
 if(!th.units){int g=0;std::vector<Gardener> gf;ops.gardeners(c,gf);for(auto&q:gf)if(!benched(c,q.id,q.uid,t)&&(distance(q.x,q.y,s.homeX,s.homeY)<=LaunchReach||distance(q.x,q.y,th.x,th.y)<=PickReach))g++;
  if(!g){defendNote(c,t,ops,th,"declined: no gardeners");return false;}}
 std::vector<Soldier> free;ops.available(c,th.x,th.y,free);int avail=(int)free.size(),army=avail-std::max(HomeReserve,avail/6);
 if(army<DefendMin){defendNote(c,t,ops,th,"declined: no spare soldiers");return false;}
 int take=landOnly?std::max(1,std::min(army,escortSize(army))):defendSize(free,army,th);if(!take){defendNote(c,t,ops,th,"declined: threat too strong");return false;} /* r37: land alone needs an escort only */
 free.resize(take);Style last=c.style;c.landOnly=landOnly;if(landOnly)c.landDefendAt=t?t:1;
 c.landStep=LandDefendTime;c.landOwnAt=th.own;c.beyondFire=false; /* r50: the land-only budget starts at LandDefendTime (extend()) */
 c.style=Style::Defend;c.raid=false;c.split=false;c.hunting=false;c.escort=false;c.chain=0;c.quietSince=0;c.lastFollow=0;c.lures=0;c.vias=0;c.viaAt=0;
 c.target=Target{};c.target.doorX=c.frontX=th.x;c.target.doorY=c.frontY=th.y;c.frontSince=t;
 int got=ops.raise(c,free);if(got<=0){c.style=last;defendNote(c,t,ops,th,"declined: squad not created");return false;}
 c.gardeners=0;c.want=std::min(gardenersWanted(avail),gardenerBase(Style::Defend)+take/GardenerPer+landGardeners(th));
 assign(c,ops,s.homeX,s.homeY,c.want,t,[&](const Gardener&g){return distance(g.x,g.y,s.homeX,s.homeY)<=LaunchReach;});
 c.initial=(uint16_t)got;c.started=t;c.targetSince=t;c.deferredSince=0;c.nextTry=0;c.lastStyle=Style::Defend;noteThreat(c,th);
 c.reason=rush?"start: serious rise":th.serious?"start: serious intrusion":"start: intrusion";c.defendWhy=c.reason;c.defendNote=t?t:1;ops.report(Event::Defend,c); /* r50: "serious rise" = the gap skipped */
 std::vector<Soldier> m;if(!ops.members(c,m)){c.reason="squad vanished";ops.report(Event::Lost,c);toIdle(c,t);return true;}
 approach(c,t,ops,m,all,s.homeX,s.homeY);return true;
}
// A running attack (march or siege) recalled by a serious intrusion: the same army and gardeners turn to the defence.
template<class Ops> bool answer(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.style==Style::Defend||(c.phase!=Phase::March&&c.phase!=Phase::Assault)||m.empty()||t<ReclaimStart||(c.lookAt&&t-c.lookAt<DefendLook)||(c.recallAt&&t-c.recallAt<RecallGap))return false;c.lookAt=t?t:1;
 Strength s=ops.strength(c);if(!s.ok)return false;std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Threat th=assess(c,ops,f,s,all,false);fireNote(c,t,ops,th); /* r50; a recall answers the Dark fighters as in r37 (no land aim) */
 if(!th.serious)return false;
 if(!th.fighters)return false; /* r37: only Dark fighters in the territory recall an attack (land and Dark Gardeners wait for the next defence) */
 if(!th.units&&!c.gardeners)return false; /* r33 review: land only and no gardeners (an allied escort): nothing to defend with */
 if(c.phase==Phase::Assault&&c.target.id&&c.ringDark>=0&&c.ringSize>0&&c.ringDark*3<=c.ringSize)return false; /* two thirds green: the siege first */
 Army a=body(m);if(summary(m).weight<2*th.fighters){defendNote(c,t,ops,th,"recall declined: threat too strong");return false;}
 c.recallAt=t?t:1;c.style=Style::Defend;c.landOnly=false;c.landStep=0;c.landOwnAt=-1;c.beyondFire=false;c.raid=false;c.split=false;c.hunting=false;c.escort=false;c.chain=0;c.lastFollow=0;c.quietSince=0;c.lures=0;
 c.target=Target{};c.target.doorX=c.frontX=th.x;c.target.doorY=c.frontY=th.y;c.frontSince=t;c.started=t;c.targetSince=t;c.lastStyle=Style::Defend;
 c.want=std::max(c.want,std::min(MaxGardeners,gardenerBase(Style::Defend)+(int)m.size()/GardenerPer+landGardeners(th)));
 noteThreat(c,th);c.reason="recall: serious intrusion";c.defendWhy=c.reason;c.defendNote=t?t:1;ops.report(Event::Defend,c);
 c.nesting++;approach(c,t,ops,m,all,a.x,a.y);c.nesting--;return true;
}
// r50: the time budget of a defence from its start: DefendTime, a land-only one LandDefendTime plus its extensions.
inline uint32_t defendBudget(const Campaign&c){return c.landOnly?std::min(DefendTime,std::max(LandDefendTime,c.landStep)):DefendTime;}
// r50: a land-only defence at the end of its budget goes on for LandDefendStep more (at most DefendTime in all) while its last
// step cut the own Dark land by LandDefendGain or more, Dark units stand within DefendReach of the area (none within HuntKeep of
// a live fire centre, as hold() counts them) or it is still on its way there (marching, or its army body farther than
// StageDistance from its hold cell); logged as OFFENCE_DEFEND "extended: ...". False: the defence ends ("defence time").
template<class Ops> bool extend(Campaign&c,uint32_t t,Ops&ops,const Fire&f,const std::vector<Soldier>&m){
 if(c.style!=Style::Defend||!c.landOnly)return false;uint32_t limit=defendBudget(c);if(limit>=DefendTime)return false;
 Intrusion in;ops.intrusion(c,in);int units=0;
 for(auto&p:in.fighters)if(!closer(p.first,p.second,f,HuntKeep)&&distance(p.first,p.second,c.frontX,c.frontY)<=DefendReach)units++;
 for(auto&p:in.gardeners)if(!closer(p.first,p.second,f,HuntKeep)&&distance(p.first,p.second,c.frontX,c.frontY)<=DefendReach)units++;
 Army a=body(m);bool walking=c.phase==Phase::March||(a.count&&distance(a.x,a.y,c.stageX,c.stageY)>StageDistance);
 int gain=c.landOwnAt>=0?c.landOwnAt-in.own:0;c.threatOwn=in.own;c.threatRise=in.rise;c.threatUnits=units;
 if(gain<LandDefendGain&&!units&&!walking)return false;
 c.landStep=std::min(DefendTime,limit+LandDefendStep);c.landOwnAt=in.own;
 c.reason=gain>=LandDefendGain?"extended: land won back":units?"extended: units near the area":"extended: still on the way";c.defendWhy=c.reason;c.defendNote=t?t:1;ops.report(Event::Defend,c);
 return true;
}
// r50 (field pid 18584: a raid held 12 of 12 gardeners while own Dark rose 1,967 -> 3,567, a sweep 3,253 -> 4,636): while the
// player's own Dark land is rising or above SeriousCells and some of it is reclaimable from home (within LaunchReach, outside
// the reclaim keeps), an attack takes HomeGuard gardeners fewer than its style wants (never fewer than MinGardeners) and
// leaves HomeGuard free gardeners at home when it can, so the reclaim pass (every phase) works on that land meanwhile. No
// recall: only Dark fighters in the territory recall an attack (r37); recalling for Dark Gardeners or a rise would stop
// attacks in the middle all the time (r38).
constexpr int HomeGuard=4;
inline int guardedWant(int want,bool guard){return guard?std::max(std::min(want,MinGardeners),want-HomeGuard):want;}
template<class Ops> bool homeGuard(const Campaign&c,Ops&ops,const Strength&s,const std::vector<Target>&all,const Fire&f){
 Intrusion in;ops.intrusion(c,in);if(in.own<=0||(in.rise<=0&&in.own<SeriousCells))return false;
 std::vector<Keep> keep;keeps(c,ops,all,f,keep);int dx=0,dy=0;return ops.ownDark(c,s.homeX,s.homeY,LaunchReach,keep,dx,dy);
}
inline const char*guardName(Style s){static const char*names[]={"raid with a home guard","assault with a home guard","pincer with a home guard","sweep with a home guard","hunt with a home guard","allied with a home guard","defend"};return names[(int)s];}
// The defence in place (phase Assault of a 'defend' campaign).
template<class Ops> void hold(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m);
template<class Ops> void decide(Campaign&c,uint32_t t,Ops&ops){
 Config cfg=ops.config(c.player);
 if(cfg.ok&&defend(c,t,ops))return; /* r33: the own land first */
 if(!cfg.ok||cfg.mode<=0||t<=c.nextCheck||t<firstAttack(cfg.delay)||t<c.cooldownUntil)return;
 // F and Str are zeroed by the attack agent's reset (load, end of a native
 // attack) until CAIAgentSquads writes them again: not known yet, no failure.
 Strength s=ops.strength(c);if(s.ok&&!s.fighters&&!s.strength){c.nextCheck=t+UnknownRetry;return;}
 if(!s.ok){decline(c,t,ops,"no sector");return;}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);ops.paths(c,s.homeX,s.homeY,all); /* r37: targets by the walk from home */
 // Another computer called for an assault: join it without waiting for chance (never a target the fire now locks).
 Target call;bool allied=ops.call(c,t,call)&&call.uid!=c.joinedUid;
 if(allied){allied=false;for(auto&b:all)if(b.id==call.id&&b.uid==call.uid&&b.reachable&&!locked(b,f)){call=b;allied=true;}}
 std::vector<Soldier> free;ops.available(c,s.homeX,s.homeY,free);int avail=(int)free.size(),army=avail-std::max(HomeReserve,avail/6);
 if(army<MinArmy){c.value=avail;decline(c,t,ops,"no spare soldiers");return;}
 if(!allied&&army<AssaultMin&&c.random.bits16()>=c.chance){decline(c,t,ops,"chance");return;} /* r37: an army fit for an assault never waits for chance */
 std::vector<Gardener> spare;ops.gardeners(c,spare);int gfree=0;auto mustering=[&](const Gardener&g){return distance(g.x,g.y,s.homeX,s.homeY)<=LaunchReach;}; /* r33: the army musters at home */
 for(auto&g:spare)if(!benched(c,g.id,g.uid,t)&&mustering(g))gfree++;
 if(gfree<MinGardeners&&(!c.callHomeAt||t-c.callHomeAt>=CallHomeEvery)){ /* r37: gardeners stranded beyond LaunchReach (greening the Dark border where a campaign dropped them; field: 14 alive, 46,000 ticks of "no gardeners") walk home, the nearest first */
  std::vector<const Gardener*> away;for(auto&g:spare)if(!benched(c,g.id,g.uid,t)&&!mustering(g))away.push_back(&g);
  std::sort(away.begin(),away.end(),[&](const Gardener*a,const Gardener*b){return distance(a->x,a->y,s.homeX,s.homeY)<distance(b->x,b->y,s.homeX,s.homeY);});
  int called=0;for(auto*g:away){if(called>=CallHome)break;if(inFire(g->x,g->y,f)||!legClear(g->x,g->y,s.homeX,s.homeY,f,GardenerKeep))continue;ops.sendGardener(c,*g,s.homeX,s.homeY);called++;}
  if(called){c.callHomeAt=t?t:1;Campaign note=c;note.value=called;note.versus=(int)away.size();note.goalX=s.homeX;note.goalY=s.homeY;note.reason="gardeners called home";ops.report(Event::Reclaim,note);}}
 bool homeGuarded=gfree>=MinGardeners&&homeGuard(c,ops,s,all,f); /* r50: own Dark rising or large near home: a home guard stays out */
 int cap=gardenersWanted(avail);auto wantOf=[&](Style st,int take){return guardedWant(std::min(cap,gardenersFor(st,take)),homeGuarded);}; /* r32: gardeners grow with the soldiers a style takes */
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
   if(gfree>=SweepGardeners&&ops.darkNear(c,s.homeX,s.homeY,SweepSearch,keep,fx,fy))w.weight=any<0?5:1;} /* r37: buildings first */
  // Hunt: the army goes for the largest group of Dark fighters well outside the fire.
  {Option&h=o[n++];h.style=Style::Hunt;h.take=huntSize(army);h.want=wantOf(Style::Hunt,h.take);std::vector<std::pair<int,int>> fs;ops.darkFighters(c,fs);
   if(cluster(fs,f,s.homeX,s.homeY,hx,hy,hn)&&hn>=HuntMin&&ops.reach(c,hx,hy)&&weightOf(h.take)>=3*hn&&legClear(s.homeX,s.homeY,hx,hy,f,legKeep(h.take))){h.weight=any>=0?1:2+(distance(hx,hy,s.homeX,s.homeY)<=50?2:0);h.target=-3;}} /* r37: buildings first (field: five hunts in a row, two lost to the fire) */
 }
 int k=chooseStyle(c,o,n);
 if(k<0){if(any<0&&(!c.targetsNote||t-c.targetsNote>=TargetsNote)){c.targetsNote=t?t:1;ops.listTargets(c,all,f,t);} /* r38: why nothing can be attacked */
  decline(c,t,ops,gfree<MinGardeners?"no gardeners":lockedOnly?"farms only inside the fire defence":any<0?"no reachable Dark building":"too weak for every attack");return;}
 Option pick=o[k];free.resize(std::max(0,std::min(pick.take,avail)));
 c.style=pick.style;c.raid=light(c);c.split=pick.style==Style::Pincer;c.hunting=pick.style==Style::Hunt;c.escort=pick.style==Style::Allied&&!pick.want;c.chain=0;c.quietSince=0;c.lastFollow=0;c.lures=0;c.vias=0;c.viaAt=0;
 if(pick.style==Style::Allied){c.target=call;c.joinedUid=call.uid;}
 else if(pick.style==Style::Sweep){c.target=Target{};c.target.doorX=c.frontX=fx;c.target.doorY=c.frontY=fy;c.frontSince=t;}
 else if(pick.style==Style::Hunt){c.target=Target{};c.target.doorX=hx;c.target.doorY=hy;c.value=hn;}
 else{c.target=all[pick.target];if(pick.style==Style::Raid){c.versus=c.target.defence;}}
 int got=ops.raise(c,free);if(got<=0){decline(c,t,ops,"squad not created");return;}
 c.gardeners=0;c.want=pick.want;if(pick.style!=Style::Hunt)assign(c,ops,s.homeX,s.homeY,homeGuarded?std::min(pick.want,std::max(MinGardeners,gfree-HomeGuard)):pick.want,t,mustering); /* a hunt takes its gardeners at the hand-over; r50: the home guard stays */
 c.initial=(uint16_t)got;c.started=t;c.targetSince=t;c.deferredSince=0;c.nextTry=0;c.lastStyle=pick.style;c.reason=homeGuarded&&pick.want>0?guardName(pick.style):styleName(pick.style);ops.report(pick.style==Style::Allied?Event::Join:Event::Launch,c);
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
// r33 (field, pid 13612: no OFFENCE_RECLAIM in 95,000 ticks while the owned Dark cells rose to 5,805): own territory is
// searched within ReclaimRadius of the gardener and, from home, within LaunchReach of home (r33 review: farther own land,
// such as the field's 194 hex out, is the defence campaign's: a convoy with soldiers), DefendPerPass gardeners
// per pass while the sector defends, a pass that sends nobody says why (OFFENCE_RECLAIM value=0, every ReclaimNote
// ticks while the player owns Dark cells), and a free gardener still parked (WATCH) beyond GatherHome of home with no
// work and no Dark fighter within ThreatRadius is sent home (at most two per pass).
template<class Ops> void reclaim(Campaign&c,uint32_t t,Ops&ops){
 Config cfg=ops.config(c.player);if(!cfg.ok)return;Strength s=ops.strength(c);if(!s.ok)return;
 std::vector<Gardener> gs;ops.gardeners(c,gs);if(gs.empty())return;
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);Campaign note=c;note.value=0;note.versus=0;
 std::vector<Keep> keep;bool keepReady=false;
 auto keepNow=[&](){if(keepReady)return;keepReady=true;keeps(c,ops,all,f,keep);std::vector<std::pair<int,int>> fighters;ops.darkFighters(c,fighters);for(auto&q:fighters)keep.push_back({q.first,q.second,DefenceRadius});};
 std::vector<std::pair<int,int>> parked;for(auto&b:all)if((b.kind==Temple||b.kind==Fortress)&&ops.parked(b))parked.push_back({b.x,b.y});
 int out=0,bench=0,back=0;std::vector<Pulled> freed;
 for(auto&g:gs){
  if(inFire(g.x,g.y,f)||(g.aimX>=0&&inFire(g.aimX,g.aimY,f))){int r=recall(c,g,t);if(!r)continue;int x=s.homeX,y=s.homeY;freed.push_back(pulledOf(g,r==2,true)); /* r36 */
   if(r==1){keepNow();int dx=0,dy=0;if(ops.darkNear(c,g.x,g.y,ReclaimRadius,keep,dx,dy)){x=dx;y=dy;}}else bench++;
   if(sendOut(c,ops,g,f,x,y))out++;continue;}
  if(g.work==Work::Idle||(g.work==Work::Busy&&distance(g.centreX,g.centreY,s.homeX,s.homeY)<=2))continue;
  bool parkedBy=false;for(auto&p:parked)if(distance(g.x,g.y,p.first,p.second)<=ParkedKeep)parkedBy=true;
  if(parkedBy&&sendOut(c,ops,g,f,s.homeX,s.homeY))back++;}
 if(out){note.value=out;note.versus=bench;note.goalX=note.goalY=-1;note.reason="gardener out of the fire defence";ops.report(Event::Fire,note);}
 if(!freed.empty()){std::vector<Soldier> none;ops.pulled(c,f,none,freed,-1,-1);}
 if(back){note.value=back;note.versus=0;note.reason="gardener called back";ops.report(Event::Reclaim,note);}
 /* Staggered by player and sector, so the computers do not all search on the same tick. */
 uint32_t phase=(uint32_t)(c.player*7+c.sector*3);if((t/Period+phase)%(ReclaimEvery/Period))return;c.lastReclaim=t;
 bool border=cfg.mode>0&&t>=ReclaimStart;keepNow();int sent=0,own=0;
 int failed=0;
 size_t start=(size_t)((t/ReclaimEvery)%gs.size());
 int per=(c.style==Style::Defend&&c.phase!=Phase::Idle)?DefendPerPass:ReclaimPerPass,idle=0;std::vector<uint16_t> given;
 auto stuckOf=[&](const Gardener&g){return g.work==Work::Idle||(g.work==Work::Searching&&g.misses>SearchMisses);};
 for(auto&g:gs)if(stuckOf(g)&&!benched(c,g.id,g.uid,t)&&!inFire(g.x,g.y,f))idle++;
 for(size_t k=0;k<gs.size();k++){const Gardener&g=gs[(start+k)%gs.size()];if(sent>=per||failed>=per)break;bool stuck=stuckOf(g);if(!stuck||benched(c,g.id,g.uid,t)||inFire(g.x,g.y,f))continue;
  int dx=0,dy=0;bool homeSide=distance(g.x,g.y,s.homeX,s.homeY)<=LaunchReach; /* r33 review: never alone across the map */
  bool mine=ops.ownDark(c,g.x,g.y,ReclaimRadius,keep,dx,dy)||(homeSide&&ops.ownDark(c,s.homeX,s.homeY,LaunchReach,keep,dx,dy));
  if(!mine&&(!border||!ops.darkNear(c,g.x,g.y,ReclaimRadius,keep,dx,dy)||distance(dx,dy,s.homeX,s.homeY)>ReclaimHome)){failed++;continue;}
  if(ops.defenders(dx,dy,DefenceRadius,false))continue;
  if(!legClear(g.x,g.y,dx,dy,f,GardenerKeep)){failed++;continue;} /* never across the fire */
  ops.sendGardener(c,g,dx,dy);sent++;if(mine)own++;given.push_back(g.id);keep.push_back({dx,dy,SpreadKeep});note.goalX=dx;note.goalY=dy;}
 if(sent){note.value=sent;note.versus=own;note.reason=own?"own territory":"border";ops.report(Event::Reclaim,note);}
 else if(!c.reclaimNote||t-c.reclaimNote>=ReclaimNote){Intrusion in;ops.intrusion(c,in);
  if(in.own>0){c.reclaimNote=t?t:1;note.value=0;note.versus=idle;note.goalX=note.goalY=-1;note.reason=idle?"none sent: no Dark cell in reach":"none sent: no idle free gardener";ops.report(Event::Reclaim,note);}}
 /* r33: free gardeners still parked in the field (a campaign ended or a load dropped it) walk home when it is safe;
    r33 review: an idle one beyond LaunchReach with no own Dark cell within ReclaimRadius too (no campaign musters it there),
    unless a running defence of this sector stands within PickReach of it (hold() adopts it). */
 std::vector<std::pair<int,int>> foes;bool foesRead=false;int home=0;bool defending=c.style==Style::Defend&&c.phase!=Phase::Idle;
 for(auto&g:gs){if(home>=2)break;int d=distance(g.x,g.y,s.homeX,s.homeY);if(g.work!=Work::Idle||(g.escorted?d<=GatherHome:d<=LaunchReach)||benched(c,g.id,g.uid,t)||std::find(given.begin(),given.end(),g.id)!=given.end())continue;
  if(defending&&distance(g.x,g.y,c.frontX,c.frontY)<=PickReach)continue;
  int qx=0,qy=0;if(!g.escorted&&ops.ownDark(c,g.x,g.y,ReclaimRadius,keep,qx,qy))continue;
  if(!foesRead){foesRead=true;ops.darkFighters(c,foes);}bool danger=false;for(auto&q:foes)if(distance(q.first,q.second,g.x,g.y)<=ThreatRadius)danger=true;
  if(!danger&&sendOut(c,ops,g,f,s.homeX,s.homeY))home++;}
 if(home){note.value=home;note.versus=0;note.goalX=s.homeX;note.goalY=s.homeY;note.reason="parked gardener sent home";ops.report(Event::Reclaim,note);}
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
  {std::vector<Pulled> who;for(auto&s:fresh){Pulled q;q.id=s.id;q.x=s.x;q.y=s.y;who.push_back(q);}ops.pulled(c,f,m,who,way?ex:-1,way?ey:-1);} /* r36: before replanAt changes the leg */
  if(c.phase==Phase::March||c.phase==Phase::Return)c.replanAt=t+GuardResend; /* the stored waypoints led here: a new way once they are out */
  uint32_t sinceFire=c.fireAt?t-c.fireAt:GuardResend;
  if(h.count>=std::max(LureMin,(int)m.size()/10)&&sinceFire>=GuardResend){c.fireAt=t?t:1; /* a new extraction episode (r38: stragglers alone do not count) */
   bool front=c.phase==Phase::Assault&&!c.target.id&&!c.hunting; /* a sweep's escort at its front */
   uint32_t key=((uint32_t)c.phase<<28)^(c.target.id?c.target.uid:front?(0x4000000u|((uint32_t)c.frontX<<13)|(uint32_t)c.frontY):(c.hunting?1u:2u));
   if(c.lureKey!=key||!c.lureSince||t-c.lureSince>LureWindow){c.lureKey=key;c.lureSince=t?t:1;c.lures=0;}
   if(++c.lures>=LureLimit){c.lures=0;
    if(c.phase==Phase::Return){c.replanAt=0;c.homeless=true;c.retryAt=t+ReorderEvery;c.vias=0;c.viaAt=0;return true;} /* hold outside, plan again later */
    if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="lured toward the fire defence";ops.report(Event::Avoid,c);retarget(c,t,ops,m);}
    else startReturn(c,t,ops,m,"lured toward the fire defence");
    return true;}}}
 std::vector<Gardener> gs;living(c,ops,gs);int out=0,bench=0;Strength s{};bool home=false;std::vector<Pulled> who;
 for(int i=std::min((int)gs.size(),c.gardeners)-1;i>=0;i--){const Gardener&g=gs[i];
  if(!inFire(g.x,g.y,f)&&!(g.aimX>=0&&inFire(g.aimX,g.aimY,f)))continue;
  int r=recall(c,g,t);if(!r)continue;who.push_back(pulledOf(g,r==2,false,c.gcx[i]>=0?c.gcx[i]:c.gdx[i],c.gcx[i]>=0?c.gcy[i]:c.gdy[i])); /* r36 */
  if(r==2){if(!home){s=ops.strength(c);home=true;}dropGardener(c,i);if(sendOut(c,ops,g,f,s.ok?s.homeX:-1,s.ok?s.homeY:-1))out++;bench++;continue;}
  int x=-1,y=-1;bool cell=false;
  if(c.gcx[i]>=0&&c.target.id){RingCell rc;rc.x=c.gcx[i];rc.y=c.gcy[i];rc.dark=true;int bx=0,by=0;if(ops.beside(c.target,rc,bx,by)&&!closer(bx,by,f,GardenerKeep)&&legClear(g.x,g.y,bx,by,f,GardenerKeep)){x=bx;y=by;cell=true;}}
  if(x<0&&c.orderAt&&!closer(c.goalX,c.goalY,f,GardenerKeep)&&legClear(g.x,g.y,c.goalX,c.goalY,f,GardenerKeep)){x=c.goalX;y=c.goalY;}
  if(sendOut(c,ops,g,f,x,y))out++;c.gsent[i]=t;if(!cell){c.gcx[i]=-1;c.gcy[i]=-1;}}
 if(out||bench){note.value=out;note.versus=bench;note.goalX=note.goalY=-1;note.reason="gardener out of the fire defence";ops.report(Event::Fire,note);}
 if(!who.empty())ops.pulled(c,f,m,who,-1,-1);
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
 std::vector<Soldier> core;Army a=body(m,&core);if(c.split)core=m; /* a pincer: the nearest soldier of either half */
 std::vector<Gardener> gs;living(c,ops,gs);int want=c.escort?0:c.want; /* living() counts c.lost */
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
    others wait parked (WATCH: job 0xB0 greens nothing) at the rear point behind the screen (r33 review: r32 sent
    them home alone across the map and the final gather walked them back alone) and get ring cells again as the
    allowance allows; they go home with the army when the campaign ends. At
    most HoldMax ticks per target, so continuous recruitment cannot make the citadel unbreakable. */
 bool parked=(c.target.kind==Temple||c.target.kind==Fortress)&&dark<=ParkedDark&&ops.parked(c.target);
 if(parked&&!c.parkedSince)c.parkedSince=t; /* the budget counts per target (reset with a new target only) */
 bool parkedNow=parked&&t-c.parkedSince<HoldMax,hold=parkedNow&&dark<=2;
 if(hold!=c.holding){c.holding=hold;c.reason=hold?"settlers inside":"";if(hold)ops.report(Event::Deferred,c);}
 /* r32: until then the gardeners wait behind the screen: one without a ring cell that strayed beyond EscortRadius
    from the army is called back to the rear point (r33: greening stray Dark land too, measured to the nearest soldier). */
 /* r32 review: facing a threat the rear point follows it (away from the threat behind the army's cell), and a
    waiting gardener nearer the threat than that cell is called back too. */
 if(!secure){int rx=c.rearX>=0?c.rearX:a.x,ry=c.rearX>=0?c.rearY:a.y,tx=0,ty=0;bool th=c.orderAt&&!c.split&&threat(c,ops,f,tx,ty);int screenGap=th?distance(c.goalX,c.goalY,tx,ty):0;
  if(th){rearPoint(c,ops,f,c.goalX,c.goalY,a.x,a.y,true,tx,ty,rx,ry);c.rearX=rx;c.rearY=ry;}
  gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&g){return c.gcx[i]<0&&(reachOf(core,g.x,g.y)>EscortRadius||(th&&distance(g.x,g.y,tx,ty)<=screenGap))&&t-c.gsent[i]>=GardenerResend;},[&](int px,int py){return !th||distance(px,py,tx,ty)>screenGap;});}
 if(!secure&&!parkedNow)return;
 /* Ring cells: unoccupied first, then farthest from the fire, then nearest to the army. */
 std::vector<std::pair<int,int>> foes;ops.darkFighters(c,foes);auto defended=[&](int x,int y){for(auto&q:foes)if(distance(x,y,q.first,q.second)<=DefenceRadius)return true;return false;};
 std::vector<int> cells;for(int i=0;i<(int)ring.size();i++)if(ring[i].dark&&!closer(ring[i].x,ring[i].y,f,GardenerKeep))cells.push_back(i);
 std::sort(cells.begin(),cells.end(),[&](int p,int q){if(ring[p].occupied!=ring[q].occupied)return !ring[p].occupied;int ep=clear2(ring[p].x,ring[p].y,f),eq=clear2(ring[q].x,ring[q].y,f);if(ep!=eq)return ep>eq;return distance(ring[p].x,ring[p].y,a.x,a.y)<distance(ring[q].x,ring[q].y,a.x,a.y);});
 int allow=parkedNow?((c.style==Style::Allied||!ops.leads(c,c.target))?0:std::max(0,dark-2)):MaxGardeners,engaged=0; /* one campaign per parked citadel */
 std::vector<int> surplus; /* r33 review: beyond the allowance, parked behind the screen */
 for(int i=0;i<(int)gs.size()&&i<c.gardeners;i++){const Gardener&g=gs[i];
  int mine=c.gcx[i]>=0?ringIndex(ring,c.gcx[i],c.gcy[i]):-1;bool assigned=mine>=0&&ring[mine].dark;
  int held=g.reservedX>=0?ringIndex(ring,g.reservedX,g.reservedY):-1;bool working=held>=0&&ring[held].dark;
  bool nearby=assigned||working;if(!nearby&&g.work==Work::Working)for(auto&r:ring)if(r.dark&&distance(r.x,r.y,g.x,g.y)<=2){nearby=true;break;}
  if(parkedNow){ /* within the allowance a gardener works on; every other one waits parked at the rear point (a working one at once) */
   if(nearby&&engaged<allow)engaged++;
   else if(nearby||engaged>=allow){surplus.push_back(i);c.gcx[i]=c.gcy[i]=-1;if(!g.escorted){c.gsent[i]=0;c.gdx[i]=c.gdy[i]=-1;}continue;}} /* not parked yet: stopped now */
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
    if(c.orderAt&&!closer(c.goalX,c.goalY,f,GardenerKeep)&&legClear(g.x,g.y,c.goalX,c.goalY,f,GardenerKeep)){ops.park(c,g,c.goalX,c.goalY);c.gdx[i]=c.goalX;c.gdy[i]=c.goalY;c.gsent[i]=t?t:1;}break;} /* r33: parked there */
   work(c,t,ops,i,g,x,y);c.gcx[i]=ring[k].x;c.gcy[i]=ring[k].y;if(parkedNow&&!nearby)engaged++;break;}
 }
 /* r33: a gardener without a Dark ring cell (none left for it, or its cell turned green) that strayed beyond EscortRadius
    from the nearest soldier (greening stray Dark land or not) waits behind the screen again. */
 if(parkedNow&&!surplus.empty()){int rx=c.rearX>=0?c.rearX:a.x,ry=c.rearX>=0?c.rearY:a.y;
  gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&){return std::find(surplus.begin(),surplus.end(),i)!=surplus.end();});}
 if(!parkedNow){int rx=c.rearX>=0?c.rearX:a.x,ry=c.rearX>=0?c.rearY:a.y;living(c,ops,gs);
  gather(c,t,ops,gs,f,rx,ry,[&](int i,const Gardener&g){int mine=c.gcx[i]>=0?ringIndex(ring,c.gcx[i],c.gcy[i]):-1;int held=g.reservedX>=0?ringIndex(ring,g.reservedX,g.reservedY):-1;
   return !(mine>=0&&ring[mine].dark)&&!(held>=0&&ring[held].dark)&&reachOf(core,g.x,g.y)>EscortRadius&&t-c.gsent[i]>=GardenerResend;});}
}
// A march plans again from where the army stands (after a guard extraction, a late or bent leg): as one
// army (a pincer's flank legs no longer hold); without a safe way the target is avoided and the army goes home.
template<class Ops> void remarch(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);auto stand=[&](int x,int y,int pass){return ops.stand(c,x,y,pass);};Army a=summary(m);
 c.split=false;c.replanAt=0;
 if(plan(c,f,a.x,a.y,c.stageX,c.stageY,groupSize(c,m),stand)){c.since=t;order(c,t,ops,m);return;}
 if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="no way outside the fire defence";ops.report(Event::Avoid,c);}
 c.beyondFire=across(a.x,a.y,c.stageX,c.stageY,f);startReturn(c,t,ops,m,"no way outside the fire defence");
}
// r37: a leg is late when its budget (legBudget of its length, counted from c.since) is spent or the army body came
// no closer than nearBest-1 to the leg end for NoProgress ticks; the record restarts with every new leg end.
// r38: length and progress are the walked steps (ops.walkSteps: the native path; field pid 3832, First Dawn: the walk from
// the Viking base loops south-east around the lake, the hex distance to the goal grew for 1,500 ticks and every march
// failed "target not reached"), the hex distance only where no walk is known.
template<class Ops> bool stalled(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m,int sx,int sy,int gx,int gy){
 int len=ops.walkSteps(c,sx,sy,gx,gy);uint32_t budget=legBudget(std::max(distance(sx,sy,gx,gy),len));if(m.empty())return t-c.since>budget;
 Army b=body(m);int w=ops.walkSteps(c,b.x,b.y,gx,gy);int d=w>0?w:distance(b.x,b.y,gx,gy);
 if(c.nearX!=gx||c.nearY!=gy||!c.nearAt){c.nearX=gx;c.nearY=gy;c.nearBest=d;c.nearAt=t?t:1;}
 else if(d<c.nearBest-1){c.nearBest=d;c.nearAt=t?t:1;}
 return t-c.since>budget||t-c.nearAt>NoProgress;
}
// March: through the waypoints, then wait until 70% of the army stands at the
// staging cell (each half at its own cell for a pincer), or late (r37: the
// leg's budget or no progress for NoProgress), then assault. A late or re-sent leg is checked from where the army stands; when it
// is not clear (or the guard pulled soldiers out) the march plans again.
// r35: a waypoint ends when the army is there (r32 waited for the convoy at every leg end); the staging cell waits
// for the gardeners (within convoyLimit() of the army body, of the first half for a pincer) at most StageWait ticks
// (gathered()); the convoy looks every ConvoyEvery ticks; a campaign whose gardeners all died goes home.
template<class Ops> void march(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.replanAt){if(t<c.replanAt)return;remarch(c,t,ops,m);return;}
 /* r32: all died on the way: the target is avoided, the army goes home. All left behind: convoy() sends it home. */
 if(c.gardeners){std::vector<Gardener> gs;living(c,ops,gs);if(!c.gardeners&&!c.hunting&&!c.escort){
  if(c.target.id){avoid(c,c.target.uid,t,LostAvoid);c.reason="gardeners lost";ops.report(Event::Avoid,c);}startReturn(c,t,ops,m,"no gardeners left");return;}}
 int lsx=0,lsy=0;legFrom(c,lsx,lsy);int lgx=walking(c)?c.vx[c.viaAt]:c.stageX,lgy=walking(c)?c.vy[c.viaAt]:c.stageY;
 bool late=stalled(c,t,ops,m,lsx,lsy,lgx,lgy),step=convoying(c)?t-c.convoyAt>=ConvoyEvery:t-c.lastOrder>=2*ReorderEvery; /* r37: by progress and the leg's budget; r38: walked */
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 if(c.split){std::vector<Soldier> a,b;halves(m,a,b);bool arrived=within(a,c.stageX,c.stageY,10)*10>=(int)a.size()*7&&within(b,c.stage2X,c.stage2Y,10)*10>=(int)b.size()*7&&gathered(c,t,ops,a.empty()?b:a);
  Army sa=summary(a),sb=summary(b);bool clear=(a.empty()||legClear(sa.x,sa.y,c.stageX,c.stageY,f,legKeep(std::max(1,sa.count))))&&(b.empty()||legClear(sb.x,sb.y,c.stage2X,c.stage2Y,f,legKeep(std::max(1,sb.count))));
  if(arrived){c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);order(c,t,ops,m);}
  else if((late||t-c.lastOrder>=2*ReorderEvery)&&!clear)remarch(c,t,ops,m);
  else if(late){c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);order(c,t,ops,m);}
  else if(step)order(c,t,ops,m);
  return;}
 bool via=walking(c);int gx=via?c.vx[c.viaAt]:c.stageX,gy=via?c.vy[c.viaAt]:c.stageY,there=within(m,gx,gy,10);
 bool arrived=there*10>=(int)m.size()*7&&(via||gathered(c,t,ops,m));Army a=summary(m);int keep=legKeep(std::max(1,groupSize(c,m)));
 if(via&&arrived){c.viaAt++;c.since=t;order(c,t,ops,m);return;}
 if(via&&late){remarch(c,t,ops,m);return;}
 if(arrived||late){
  if(late&&!arrived&&c.target.id&&!legClear(a.x,a.y,c.target.doorX,c.target.doorY,f,keep)){remarch(c,t,ops,m);return;} /* never straight to the door across the fire */
  /* r33: still far from the staging cell when late (field: three marches that never crossed a river): failed; r37: the next target from here */
  if(late&&!arrived&&c.target.id&&distance(body(m).x,body(m).y,gx,gy)>2*StageDistance){failed(c,c.target.uid,t);c.reason="target not reached";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}
  // Re-check the defence at the staging cell: too strong now -> another target.
  if(c.target.id&&c.raid){int now=ops.defenders(c.target.doorX,c.target.doorY,RaidRadius,true),limit=defenceLimit(ops.config(c.player),summary(m).weight);
   if(now*2>limit*3){c.value=limit;c.versus=now;avoid(c,c.target.uid,t);ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}}
  c.phase=Phase::Assault;c.since=t;ops.report(Event::Assault,c);
  if((c.style==Style::Sweep||c.style==Style::Defend)&&!c.target.id){c.lastOrder=0;return;}
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
  if(holdPoint(c,ops,f,c.frontX,c.frontY,a.x,a.y,hk,hx,hy)||(!closer(hx,hy,f,hk)&&ops.stand(c,hx,hy,1))){send(c,t,ops,m,hx,hy,false);c.rearX=hx;c.rearY=hy;}} /* r33 review: never an unchecked cell */
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
  work(c,t,ops,j,g,cx,cy);c.gcx[j]=cx;c.gcy[j]=cy;taken.push_back({cx,cy,SpreadKeep});}
}
// r33: the defence in place (see defend()). Every DefendLook ticks: the Dark units near the area and the Dark fighters'
// weight near the army are read again; the soldiers' cell (stageX/stageY) is next to the nearest Dark group (native
// combat engages within 13), else beside own Dark land near the army, else the next area; longer moves (beyond
// StageDistance) are convoy steps from where the move began (legX/legY), shorter ones plain orders. Gardeners: an own
// Dark cell within EscortRadius of the army body each (FORWARD: they green around it natively), none within DefendDanger
// of a Dark fighter; one that strayed beyond EscortRadius of the nearest soldier or stopped searching gets a new cell, the
// others wait parked behind the army (rear point RearStep beyond it, away from the units).
template<class Ops> void hold(Campaign&c,uint32_t t,Ops&ops,const std::vector<Soldier>&m){
 if(c.phase==Phase::March){march(c,t,ops,m);return;}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 std::vector<Gardener> gs;living(c,ops,gs);strays(c,ops,gs);std::vector<Soldier> core;Army a=body(m,&core);int n=groupSize(c,m),hk=std::max(soldierKeep(n),HuntKeep);
 /* r33 review: free gardeners near the army join the defence (those greening the intrusion alone, or a defence launched
    with none at home), at most up to its number, every 5*GardenerResend ticks, never across or out of the fire. */
 if((int)gs.size()<c.want&&(!c.lastTopUp||t-c.lastTopUp>=GardenerResend*5)){c.lastTopUp=t?t:1;int before=c.gardeners;
  assign(c,ops,a.x,a.y,c.want,t,[&](const Gardener&g){return reachOf(core,g.x,g.y)<=PickReach&&!inFire(g.x,g.y,f)&&legClear(g.x,g.y,a.x,a.y,f,GardenerKeep);});
  if(c.gardeners>before){living(c,ops,gs);c.value=c.gardeners;ops.report(Event::Gardeners,c);}}
 std::vector<Keep> keep;keeps(c,ops,all,f,keep);frontKeeps(f,keep);
 if(!c.lastFollow||t-c.lastFollow>=DefendLook){bool first=!c.lastFollow;c.lastFollow=t?t:1;
  Intrusion in;ops.intrusion(c,in);c.threatOwn=in.own;c.threatRise=in.rise;
  std::vector<std::pair<int,int>> units;int count=0,danger=0;
  /* r33 review: the danger counts the same fighters as assess()/defendSize() (none within HuntKeep of a live fire centre:
     guards at the Temple sent every defence near the Dark base home on arrival, and it launched again DefendPause later) */
  for(auto&p:in.fighters){if(closer(p.first,p.second,f,HuntKeep))continue;if(distance(p.first,p.second,a.x,a.y)<=ThreatRadius)danger+=2;if(distance(p.first,p.second,c.frontX,c.frontY)<=DefendReach){units.push_back(p);count++;}}
  for(auto&p:in.gardeners)if(!closer(p.first,p.second,f,HuntKeep)&&distance(p.first,p.second,c.frontX,c.frontY)<=DefendReach)units.push_back(p);
  c.threatUnits=(int)units.size();c.threatFighters=count;
  if(danger>summary(m).weight){startReturn(c,t,ops,m,"threat too strong");return;}
  int ux=0,uy=0,un=0,lx=0,ly=0;
  bool chase=!units.empty()&&cluster(units,f,a.x,a.y,ux,uy,un)&&ops.reach(c,ux,uy)&&legClear(a.x,a.y,ux,uy,f,legKeep(n));
  bool land=!gs.empty()&&ops.ownDark(c,a.x,a.y,EscortRadius,keep,lx,ly);
  if(!chase&&!land){int dx=0,dy=0; /* this area is clean: the next one (the own Dark cell nearest the area), none left: home */
   if(!gs.empty()&&ops.ownDark(c,c.frontX,c.frontY,OwnRadius,keep,dx,dy)&&ops.reach(c,dx,dy)){
    if(distance(dx,dy,c.frontX,c.frontY)>=2){c.frontX=c.target.doorX=dx;c.frontY=c.target.doorY=dy;c.frontSince=t;c.reason="next area";ops.report(Event::Front,c);}lx=dx;ly=dy;}
   else{startReturn(c,t,ops,m,gs.empty()?"no gardeners left":"land clean");return;}}
  int px=chase?ux:lx,py=chase?uy:ly,hx=px,hy=py;
  if(!holdPoint(c,ops,f,px,py,a.x,a.y,hk,hx,hy)&&(closer(hx,hy,f,hk)||!ops.stand(c,hx,hy,1))){startReturn(c,t,ops,m,"no safe cell near the intrusion");return;} /* r33 review: the fallback cell is standable too */
  if(first||distance(hx,hy,c.stageX,c.stageY)>=3){c.stageX=hx;c.stageY=hy;c.legX=a.x;c.legY=a.y;c.vias=0;c.viaAt=0;c.waitSince=0;}
  if(chase&&distance(hx,hy,ux,uy)>0){int len=distance(hx,hy,ux,uy);c.rearX=hx+(int)lround((double)(hx-ux)*RearStep/len);c.rearY=hy+(int)lround((double)(hy-uy)*RearStep/len);}else{c.rearX=hx;c.rearY=hy;}
 }
 int hx=c.stageX,hy=c.stageY;
 if(!gs.empty()&&distance(a.x,a.y,hx,hy)>StageDistance){ /* a longer move: the convoy (the gardeners parked with the army) */
  if(!c.convoyAt||t-c.convoyAt>=ConvoyEvery){if(!convoy(c,t,ops,m,f,c.legX,c.legY,hx,hy,false)){c.beyondFire=across(a.x,a.y,hx,hy,f);startReturn(c,t,ops,m,"no way outside the fire defence");return;}} /* r50: beyondFire */
  return;}
 c.waitSince=0;c.waited=0;
 if((!c.lastOrder||distance(c.goalX,c.goalY,hx,hy)>=3||(t-c.lastOrder>=ReorderEvery&&within(m,hx,hy,SiegeRadius)<needed(m)))&&legClear(a.x,a.y,hx,hy,f,legKeep(n)))send(c,t,ops,m,hx,hy,false);
 if(gs.empty())return;
 std::vector<std::pair<int,int>> foes;ops.darkFighters(c,foes);
 std::vector<Keep> taken=keep;for(auto&q:foes)taken.push_back({q.first,q.second,DefendDanger});for(int j=0;j<c.gardeners;j++)if(c.gcx[j]>=0)taken.push_back({c.gcx[j],c.gcy[j],SpreadKeep});
 std::vector<int> wait;
 for(int j=0;j<(int)gs.size()&&j<c.gardeners;j++){const Gardener&g=gs[j];
  bool stuck=g.work==Work::Idle||(g.work==Work::Searching&&g.misses>SearchMisses),away=reachOf(core,g.x,g.y)>EscortRadius;
  /* its cell turned green and it holds no reserved cell: a new one (it may keep greening stray cells far from the army) */
  int qx=0,qy=0;std::vector<Keep> none;bool done=c.gcx[j]>=0&&g.reservedX<0&&c.gsent[j]&&t-c.gsent[j]>=2*GardenerResend&&!ops.ownDark(c,c.gcx[j],c.gcy[j],0,none,qx,qy);
  if(c.gcx[j]>=0&&!stuck&&!away&&!done&&!g.escorted)continue; /* greening near the army */
  if(c.gsent[j]&&t-c.gsent[j]<GardenerResend){if(c.gcx[j]<0)wait.push_back(j);continue;}
  int cx=0,cy=0;
  if(ops.ownDark(c,a.x,a.y,EscortRadius,taken,cx,cy)&&reachOf(core,cx,cy)<=EscortRadius&&legClear(g.x,g.y,cx,cy,f,GardenerKeep)){work(c,t,ops,j,g,cx,cy);c.gcx[j]=cx;c.gcy[j]=cy;taken.push_back({cx,cy,SpreadKeep});continue;}
  c.gcx[j]=c.gcy[j]=-1;wait.push_back(j);}
 if(!wait.empty())gather(c,t,ops,gs,f,c.rearX>=0?c.rearX:a.x,c.rearX>=0?c.rearY:a.y,[&](int i,const Gardener&){return std::find(wait.begin(),wait.end(),i)!=wait.end();});
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
// r33: the campaign ends at home: its gardeners within GatherHome of home get home as their work centre (FORWARD:
// they green Dark land around it natively); any farther one stays parked and reclaim() walks it home when safe.
template<class Ops> void release(Campaign&c,Ops&ops,const Strength&s){
 std::vector<Gardener> gs;living(c,ops,gs);if(s.ok)for(auto&g:gs)if(distance(g.x,g.y,s.homeX,s.homeY)<=GatherHome)ops.sendGardener(c,g,s.homeX,s.homeY);c.gardeners=0;
}
// One director step for one sector, every Period ticks.
template<class Ops> void step(Campaign&c,uint32_t t,Ops&ops){
 if(!c.armed){c.armed=true;c.random.state^=(uint32_t)c.player*0x01000193u+(uint32_t)c.sector*0x9e3779b9u+t;toIdle(c,t);c.cooldownUntil=0;}
 if(!ops.sector(c)){if(c.phase!=Phase::Idle){c.reason="sector gone";ops.report(Event::Lost,c);toIdle(c,t);}return;}
 reclaim(c,t,ops);
 if(c.phase==Phase::Idle){decide(c,t,ops);return;}
 std::vector<Soldier> m;
 if(!ops.members(c,m)){c.reason="squad vanished";ops.report(Event::Lost,c);toIdle(c,t);return;}
 if(!c.checked&&t>=c.orderAt+IgnoreCheck){c.checked=true;std::vector<Soldier> a,b;halves(m,a,b);Army s=body(c.split?a:m);if(s.count&&distance(s.x,s.y,c.orderX,c.orderY)<2&&distance(s.x,s.y,c.goalX,c.goalY)>ConvoyLead-3){c.reason="order not executed";ops.report(Event::Ignored,c);
  /* r33: IgnoreLimit ignored orders on one target fail it (field: 15 on one unreachable farm) */
  if(c.target.id&&c.phase!=Phase::Return&&++c.ignoredOrders>=IgnoreLimit){failed(c,c.target.uid,t);c.reason="orders not executed";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;}}}
 std::vector<Target> all;ops.targets(c,all);Fire f=fire(c,ops,all);
 if(guard(c,t,ops,m,f))return;
 if(c.phase==Phase::Return){
  Strength s=ops.strength(c);bool hot=false;for(auto&u:m)if(inFire(u.x,u.y,f))hot=true;
  int home=s.ok?within(m,s.homeX,s.homeY,12):(int)m.size();
  bool done=m.empty()||!s.ok||(!walking(c)&&home*10>=(int)m.size()*8&&together(c,ops,m))||t-c.since>HomeTimeout; /* r32: home with its gardeners (r35: they follow; at home the army waits for them, HomeTimeout at most) */
  /* Never hand soldiers back inside the fire (reservoirs never move them): the guard walks them out first.
     Only a squad that still has a member within FireLine after HotHold ticks (on green land, no way out) is released. */
  if(done&&(!hot||t-c.since>HotHold)){
   /* An incomplete give-back keeps the squad and retries; after 5 tries the next load adopts it. */
   if(ops.dissolve(c)){release(c,ops,s);ops.report(Event::Dissolved,c);toIdle(c,t);}else if(++c.dissolveTries>=5){release(c,ops,s);c.reason="give-back incomplete";ops.report(Event::Lost,c);toIdle(c,t);}
   return;}
  if(m.empty()||!s.ok)return;
  if(c.replanAt){if(t>=c.replanAt){c.replanAt=0;homeward(c,t,ops,m,f,s);}return;} /* the guard's way out first, then a new way home */
  if(c.homeless){if(t>=c.retryAt)homeward(c,t,ops,m,f,s);return;} /* holding outside the fire */
  bool step=convoying(c)&&t-c.convoyAt>=ConvoyEvery; /* r32: the way home is a convoy too */
  if(walking(c)){int gx=c.vx[c.viaAt],gy=c.vy[c.viaAt];bool there=within(m,gx,gy,10)*10>=(int)m.size()*7; /* r35: no wait at a waypoint */
   if(there){c.viaAt++;int x=walking(c)?c.vx[c.viaAt]:s.homeX,y=walking(c)?c.vy[c.viaAt]:s.homeY;advance(c,t,ops,m,f,x,y,true);}
   else if(t-c.orderAt>ViaTimeout||t-c.lastOrder>=2*ReorderEvery)homeward(c,t,ops,m,f,s); /* late or re-sent: a way from where the army stands */
   else if(step)advance(c,t,ops,m,f,gx,gy,true);
   return;}
  if(t-c.lastOrder>=2*ReorderEvery)homeward(c,t,ops,m,f,s); /* never a straight leg home re-sent through the fire */
  else if(step)advance(c,t,ops,m,f,s.homeX,s.homeY,true);
  return;
 }
 if(m.empty()){startReturn(c,t,ops,m,"army lost");return;}
 if((int)m.size()*100<(int)c.initial*((light(c)||c.style==Style::Defend)?50:30)){startReturn(c,t,ops,m,"army beaten");return;}
 if(t-c.started>(c.style==Style::Sweep?SweepTime:c.style==Style::Defend?defendBudget(c):CampaignTimeout)&&!extend(c,t,ops,f,m)){startReturn(c,t,ops,m,c.style==Style::Defend?"defence time":"campaign time");return;} /* r37: a land-only defence ends after LandDefendTime; r50: unless it is winning (extend()) */
 if(answer(c,t,ops,m))return; /* r33: a serious intrusion recalls the army */
 if(c.style==Style::Defend){hold(c,t,ops,m);return;}
 if(c.style==Style::Hunt&&c.hunting){hunt(c,t,ops,m);return;}
 if(c.style==Style::Sweep&&!c.target.id){sweep(c,t,ops,m);return;}
 if(!c.target.id||!ops.alive(c.target)){
  if(c.target.id){c.chain++;if(!c.razed){c.reason="ring green";ops.report(Event::Collapsed,c);}}
  if(c.escort&&c.target.id){startReturn(c,t,ops,m,"allied target fell");return;}
  if(c.style==Style::Sweep){resume(c,t);return;}
  ops.report(Event::Retarget,c);retarget(c,t,ops,m,c.want>0&&!c.gardeners&&!c.escort);return;} /* r32: an adopted army takes gardeners near it and marches with them */
 if(t-c.targetSince>targetBudget(c.targetHex)){failed(c,c.target.uid,t);if(c.style==Style::Sweep){resume(c,t);return;}c.reason="target not reached";ops.report(Event::Avoid,c);retarget(c,t,ops,m);return;} /* r33: counted, LostAvoid then LongAvoid; r37: budget by distance, then the next target from here */
 if(c.target.kind!=Farm){for(auto&b:all)if(b.kind==Farm&&b.reachable&&!locked(b,f)&&!avoided(c,b.uid,t)){if(c.escort){startReturn(c,t,ops,m,"allied escort released");return;}c.reason="farm first";ops.report(Event::Retarget,c);retarget(c,t,ops,m);return;}}
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
// r50: ShovelBatch 6 (was 4): after a mass loss the order tops up six missing shovels at a time; recruitment takes each
// one as it is made, so the output cannot fill with them.
constexpr int ShovelBatch=6,ShovelContinuous=100;
// r46: a toolsmith's output piles (one per tool) share a capacity of OutputCapacity goods (CProductionBuildingRole
// sub_14CDC0: it makes nothing while they hold 8 or more), and a tool nobody takes stays there (field pid 2476: six
// pickaxes and a hammer the AI had ordered sat in the output for 120,000 ticks; two tools were made in that time, no
// shovel, and the gardeners died out). An order for a tool that already lies in an output pile is surplus and is
// cleared by the plugin's tool watch; never the shovel order, never a continuous order (100).
// r48: the AI sends some tool orders once (pid 33752: PICKAXE x12 at tick 1,800 for its unstaffed mines, never again);
// clearing such a batch after the first tool left three iron mines without a worker for the whole game, and with no
// iron ore the kingdom made no bars, no tools and no weapons. An order is surplus only when the output is nearly full
// (OutputNearFull of OutputCapacity) and at least ToolSurplus of that tool already lie there: the clog is stopped, a
// batch that workers keep taking is not.
constexpr int OutputCapacity=8,ToolSurplus=2,OutputNearFull=6;constexpr uint32_t ToolWatchEvery=10;
inline bool surplusOrder(int good,int lying,int order,int outSum){return good!=31&&lying>=ToolSurplus&&outSum>=OutputNearFull&&order>0&&order<ShovelContinuous;}
inline bool clogged(int outSum){return outSum>=OutputCapacity;}
// r49: shovel priority (user request). While the plugin owes shovels and the output holds DropAt or more goods, the most
// numerous non-shovel tool loses one unreserved piece (the pile's own removal 0x16F8D0, which keeps the economy's stock
// counter and the pile role consistent), so the next round can make a shovel. One piece per DropEvery ticks; never a
// reserved good, never a shovel. Returns the output index to drop from, -1 for none.
// r50: only a pile with DropKeep (2) or more lying pieces gives one, so a pile is never emptied (field pid 18584: the r49
// drop took the last axe and the last hammer). A free shovel lying in the output no longer stops the drop: the drop runs
// only while shovels are owed, i.e. while free shovels are at or below ShovelReserve, so recruitment cannot use that
// shovel and only a second one gets through (pid 31460 ended with the output full and one shovel lying there for good).
constexpr int DropAt=7,DropKeep=2;constexpr uint32_t DropEvery=250;
template<class P> inline int dropCandidate(const P*out,int n,int outSum){
 if(outSum<DropAt)return -1;
 int best=-1,most=0;for(int i=0;i<n;i++){int free=out[i].count-out[i].reserved;if(out[i].good!=31&&out[i].count>=DropKeep&&free>most){most=free;best=i;}}
 return best;
}
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
// r33: a restore is pointless while the home economy holds no iron bar (field, pid 13612: 1,443 restores between ticks
// 16,084 and 76,510 with the iron stock at 0 and the order stuck at 4): the watch still follows the byte, nothing is put
// back (the 250-tick evaluation orders again once iron is in stock). iron < 0: unknown, restored as before.
inline int shovelRestoreWhile(ShovelWatch&w,int now,int iron){int add=shovelRestore(w,now);return iron==0?0:add;}
// After the 250-tick evaluation: remember the plugin's part of the order
// (ordered: the byte after it, target: shovelTarget) while shovels are still
// wanted, otherwise forget it so nothing is put back.
inline void shovelNoted(ShovelWatch&w,bool wanted,int ordered,int target){
 if(!wanted||ordered<=0||ordered>=ShovelContinuous||target<=0){w=ShovelWatch{};return;}
 w.owed=std::min(ordered,target);w.seen=ordered;
}
}
