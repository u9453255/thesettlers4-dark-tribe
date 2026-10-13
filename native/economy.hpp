#pragma once
#include <stdint.h>
#include <algorithm>
#include <type_traits>
namespace dt {
constexpr uint32_t StateVersion=2,MaxFarms=256,MaxJobs=128,CargoCapacity=8;
// Farm fields are retained only to validate and retire legacy courier sidecars.
// Native farm workers now own production, delivery and the mana credit.
struct Farm {uint32_t identity=0,stock=0,cargo=0,lastCycle=0,baseIdentity=0,courierIdentity=0;uint16_t id=0,courier=0,base=0,x=0,y=0;uint8_t returning=0,needsCourier=0,pad[2]={};uint32_t lastOrder=0;};
// The two new flags occupy zeroed reserved bytes in existing schema-2 saves.
// removed is the legacy paid flag; pad=1 identifies native in-place growth.
struct Construction {uint32_t identity=0,readyAt=0,cost=0;uint16_t site=0,kind=0,x=0,y=0;uint8_t removed=0,refunded=0,cancelRequested=0,pad=0;};
struct Aircraft {uint32_t identity=0,readyAt=0;uint16_t hall=0,pad=0;};
struct State {uint32_t version=StateVersion,tick=0,farmCount=0,constructionCount=0,aircraftCount=0;uint64_t harvested=0,delivered=0,lost=0;Farm farms[MaxFarms]{};Construction construction[MaxJobs]{};Aircraft aircraft[MaxJobs]{};};
static_assert(std::is_trivially_copyable<State>::value,"Snapshot must be serializable");
inline uint32_t buildCost(uint16_t kind){switch(kind){case 49:return 25;case 50:return 100;case 51:return 150;case 80:return 120;default:return 0;}}
inline bool valid(const State&s){if(s.version!=StateVersion||s.farmCount>MaxFarms||s.constructionCount>MaxJobs||s.aircraftCount>MaxJobs)return false;uint64_t held=0;for(uint32_t i=0;i<s.farmCount;i++){auto &f=s.farms[i];if(f.stock>1000000||f.cargo>CargoCapacity||f.returning>1||f.needsCourier>1||f.x>=576||f.y>=576||(!f.id&&(f.stock||f.cargo)))return false;held+=f.stock+f.cargo;}for(uint32_t i=0;i<s.constructionCount;i++){auto&j=s.construction[i];if(!j.site||!buildCost(j.kind)||j.cost!=buildCost(j.kind)||j.removed>1||j.refunded>1||j.cancelRequested>1||j.pad>1||(j.pad&&!j.removed)||(j.pad&&j.cancelRequested&&!j.refunded)||((j.refunded||j.cancelRequested)&&!j.removed)||j.x>=576||j.y>=576)return false;}for(uint32_t i=0;i<s.aircraftCount;i++)if(!s.aircraft[i].hall)return false;if(s.delivered>s.harvested||s.lost>s.harvested-s.delivered)return false;return s.harvested-s.delivered-s.lost==held;}
// Do not mint a refund from obsolete inventory or alter native units/bank.
// Preserve paid construction, Hall orders and historical conservation totals.
inline bool retireLegacyFarmState(State&s){
 if(!valid(s))return false;
 s.lost=s.harvested-s.delivered;
 std::fill(s.farms,s.farms+MaxFarms,Farm{});s.farmCount=0;
 return valid(s);
}
}
