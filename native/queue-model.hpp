#pragma once
#include "economy.hpp"
namespace dt {
// Dark Temple role queues: six DWORDs at role+0x180, saved and loaded by the
// role itself. Index -> settler type (table at RVA 0x101A230): 1 Mushroom
// Farmer (automatic, never changed here), 2 Gardener, 3 Shaman, 4 Warrior,
// 5 Thrower. Native facts (tools/inspect-recruitment.py):
// - recruitment (RVA 0x145760) decrements a count only while it is below 100,
//   so 100 recruits continuously while mana lasts;
// - the role's order handler (RVA 0x145D60) clamps finite orders to 0..99 and
//   sets 100 for an order of 100 or more;
// - the native barracks controls (RVA 0xAF67F) send +100 (infinite), 0 (clear),
//   +5, -5, +1 and -1 for the selected unit type; below infinite, a decrease
//   leaves a finite count (100-1 = 99), as in the barracks.
constexpr uint32_t QueueFull=99,QueueInfinite=100;
constexpr int OrderClear=0,OrderInfinite=100;
inline int unitQueue(int unit){switch(unit){case 29:return 4;case 32:return 5;case 52:return 2;case 54:return 3;default:return -1;}}
inline bool infiniteQueue(uint32_t n){return n>=QueueInfinite;}
// The count after one native control order on a queue holding n.
inline uint32_t queueAfter(uint32_t n,int amount){
 if(amount==OrderClear)return 0;
 if(amount>=OrderInfinite||(infiniteQueue(n)&&amount>0))return QueueInfinite;
 int64_t v=(int64_t)std::min(n,QueueInfinite)+amount;
 return (uint32_t)std::max<int64_t>(0,std::min<int64_t>(QueueFull,v));
}
// True when the order changes the queue (otherwise its control is disabled).
inline bool canOrder(uint32_t n,int amount){return queueAfter(n,amount)!=n;}
inline bool orderQueue(uint32_t*q,int unit,int amount){int index=unitQueue(unit);if(!q||index<0)return false;uint32_t before=q[index];q[index]=queueAfter(before,amount);return before!=q[index];}
inline bool adjustQueue(uint32_t*q,int unit,int delta){return delta&&orderQueue(q,unit,delta);}
inline uint32_t clearManualQueues(uint32_t*q){if(!q)return 0;uint32_t total=0;for(int i=2;i<=5;i++){total+=q[i];q[i]=0;}return total;}
inline uint32_t nextAircraftTime(const State&s,uint16_t hall,uint32_t identity,uint32_t now){uint32_t end=now;for(uint32_t i=0;i<s.aircraftCount;i++)if(s.aircraft[i].hall==hall&&s.aircraft[i].identity==identity)end=std::max(end,s.aircraft[i].readyAt);return end+300;}
inline uint32_t cancelAircraft(State&s,uint16_t hall,uint32_t identity,bool all){uint32_t refund=0;for(;;){int chosen=-1;for(uint32_t i=0;i<s.aircraftCount;i++)if(s.aircraft[i].hall==hall&&s.aircraft[i].identity==identity&&(chosen<0||s.aircraft[i].readyAt>s.aircraft[chosen].readyAt))chosen=(int)i;if(chosen<0)break;s.aircraft[chosen]=s.aircraft[--s.aircraftCount];refund+=300;if(!all)break;}return refund;}
}
