#pragma once
#include "economy.hpp"
#include "placement-model.hpp"
#include "blockade-model.hpp"
namespace dt {
constexpr int FoundationCancelMode=1; // Refused/cancelled work only, never successful construction.
constexpr uint32_t GrowthTicks=30; // Minimum; see growthTicks().
// Visible native rise, longer for larger buildings (game ticks, ~10 per second).
inline uint32_t growthTicks(uint16_t kind){switch(kind){case 49:return 40;case 80:return 60;case 50:return 80;case 51:return 100;default:return GrowthTicks;}}
constexpr uint8_t NativeGrowth=1; // Previously zeroed schema-2 reserved byte.
inline bool foundationIdentity(uint32_t uid){return uid&&!(uid&0x60000000);}
enum class SiteSpace {Clear,MobileBlocked,StaticBlocked,Invalid};
enum class ConstructionResult {Waiting,Complete,Retired,Refunded,Fatal};
enum class ConstructionEvent {Unpaid,Started,StartFailed,Complete,Lost,LegacyCancelled,Blocked,UnitWait,Duplicate,FireBlocked};
// One Dark Temple (50) and one Dark Fortress (51) per player. A kind exists
// while the player owns a completed one (not marked for deletion), or while a
// construction job of that kind will become one (unpaid or growing). Refunded
// or refused jobs and destroyed buildings do not count, so a lost base can be
// rebuilt. Counts are rebuilt from scratch every tick.
inline int limitSlot(uint16_t kind){return kind==50?0:kind==51?1:-1;}
enum LimitState:uint8_t {LimitFree=0,LimitBuilt=1,LimitBuilding=2};
struct BuildingLimit {
 uint16_t built[2]={},building[2]={};
 void countBuilt(uint16_t kind){int s=limitSlot(kind);if(s>=0&&built[s]<0xffff)built[s]++;}
 void countBuilding(uint16_t kind){int s=limitSlot(kind);if(s>=0&&building[s]<0xffff)building[s]++;}
 bool exists(uint16_t kind)const{int s=limitSlot(kind);return s>=0&&(built[s]||building[s]);}
 uint8_t state(uint16_t kind)const{int s=limitSlot(kind);return s<0?LimitFree:built[s]?LimitBuilt:building[s]?LimitBuilding:LimitFree;}
};
// Jobs already recorded (from earlier ticks or a matched save) that will become a building.
inline void countJobs(BuildingLimit&l,const State&s){for(uint32_t i=0;i<s.constructionCount&&i<MaxJobs;i++)if(!s.construction[i].refunded)l.countBuilding(s.construction[i].kind);}
inline const char*limitNotice(uint16_t kind){return kind==50?"You already have a Dark Temple.":kind==51?"You already have a Dark Fortress.":"";}
// Refuse a new, unpaid native site: native mode-1 cancellation, nothing
// charged. The record stays until the site's identity disappears, so the same
// site is never rediscovered, charged or counted.
template<class Ops> ConstructionResult refuseConstruction(Construction&j,Ops&ops,ConstructionEvent reason){
 j.removed=1;j.refunded=1;j.cancelRequested=1;
 ops.remove(j,FoundationCancelMode);ops.report(reason,j);return ConstructionResult::Retired;
}
inline uint32_t growthWork(const Construction&j,uint32_t tick,uint32_t total){
 uint32_t span=growthTicks(j.kind),elapsed=tick>=j.readyAt?span:span-std::min(span,j.readyAt-tick);
 return total?std::min(total-1,(uint32_t)((uint64_t)total*elapsed/span)):0;
}
template<class Ops> ConstructionResult updateConstruction(Construction&j,uint32_t tick,Ops&ops){
 if(j.refunded)return ops.sameSite(j)?ConstructionResult::Waiting:ConstructionResult::Retired;
 auto refund=[&](ConstructionEvent reason){
  if(!ops.changeMana((int)j.cost))return ConstructionResult::Fatal;
  j.refunded=1;
  if(ops.foundation(j)){j.cancelRequested=1;ops.remove(j,FoundationCancelMode);}
  ops.report(reason,j);return ConstructionResult::Refunded;
 };
 // Old paid conversion jobs released their site. Refund once, never recreate
 // them over units or attempt to reconstruct altered native bookkeeping.
 if(j.removed&&j.pad!=NativeGrowth)return refund(ConstructionEvent::LegacyCancelled);
 if(!j.removed){
  if(!ops.foundation(j))return ConstructionResult::Retired;
  if(!ops.darkLand(j)||ops.mana()<j.cost)return refuseConstruction(j,ops,ConstructionEvent::Unpaid);
  if(!ops.changeMana(-(int)j.cost))return ConstructionResult::Fatal;
  j.removed=1;j.pad=NativeGrowth;j.readyAt=tick+growthTicks(j.kind);
  if(!ops.begin(j))return refund(ConstructionEvent::StartFailed);
  ops.report(ConstructionEvent::Started,j);
 }
 if(ops.completed(j)){ops.report(ConstructionEvent::Complete,j);return ConstructionResult::Complete;}
 if(!ops.foundation(j))return refund(ConstructionEvent::Lost);
 if(!ops.progress(j,tick))return ConstructionResult::Fatal;
 if(tick<j.readyAt)return ConstructionResult::Waiting;
 auto space=ops.space(j);
 if(space==SiteSpace::Invalid)return ConstructionResult::Fatal;
 if(space==SiteSpace::StaticBlocked)return refund(ConstructionEvent::Blocked);
 // Idle units do not leave reserved cells by themselves (field log: a Fortress
 // next to the base waited ~1300 ticks on one unit). Order own units off.
 if(space==SiteSpace::MobileBlocked){if((tick-j.readyAt)%20==0)ops.evict(j);if((tick-j.readyAt)%100==0)ops.report(ConstructionEvent::UnitWait,j);return ConstructionResult::Waiting;}
 if(!ops.finish(j)||!ops.completed(j))return ConstructionResult::Fatal;
 ops.report(ConstructionEvent::Complete,j);return ConstructionResult::Complete;
}
// A native site found this tick becomes job j. A second Temple/Fortress
// (another one is built or will be) is refused before payment; so is a site
// the player cannot fund or that is not on Dark land (refused at discovery,
// before future income). Only a job that will become a building is counted,
// so of two outlines placed in the same tick the first is kept.
// r30 blockade: a Farm inside the fire field of an own Temple/Fortress, or a
// Temple/Fortress whose field would hold an own Farm, is refused the same way
// (mode 1 before payment, event FireBlocked). A kept site joins the blockade.
template<class Ops> ConstructionResult discoverConstruction(Construction&j,uint32_t tick,Ops&ops,BuildingLimit&limit,FireBlockade*fire=nullptr){
 ConstructionResult r=ConstructionResult::Waiting;
 if(limit.exists(j.kind))r=refuseConstruction(j,ops,ConstructionEvent::Duplicate);
 else if(fire&&fire->blocked(j.kind,j.x,j.y))r=refuseConstruction(j,ops,ConstructionEvent::FireBlocked);
 else if(ops.mana()<j.cost||!ops.darkLand(j))r=updateConstruction(j,tick,ops);
 if(!j.refunded){limit.countBuilding(j.kind);if(fire)fire->add(j.kind,j.x,j.y);}
 return r;
}
}
