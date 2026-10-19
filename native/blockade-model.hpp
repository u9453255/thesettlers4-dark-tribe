#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
#include "citadel-fire.hpp"
// Dark construction blockade around the citadel fire (r30, user decision
// 2026-10-19: no Mushroom Farm inside the field; start-map farms inside it are
// destroyed once). Pure: positions only, no engine access.
//
// A Mushroom Farm (49) at (x,y) is blocked when dtfire::farmTooClose() holds for
// any own Temple (50) or Fortress (51), completed or site; a Temple/Fortress at
// (x,y) is blocked when any own Farm, completed or site, would then be too close
// to its fire centre. The Manacopter Hall (80) has no fire and is never blocked.
// The offence director's farm lock uses the same dtfire::FarmLock, so with the
// blockade no farm ever stands where computers may not besiege it.
namespace dt {
inline bool citadelKind(int kind){return kind==50||kind==51;}
struct FireSpot {int x=0,y=0;bool operator==(const FireSpot&o)const{return x==o.x&&y==o.y;}};
struct FireBlockade {
 std::vector<FireSpot> citadels,farms; // own Temple/Fortress and own Mushroom Farm positions (completed or site)
 void clear(){citadels.clear();farms.clear();}
 void add(int kind,int x,int y){if(citadelKind(kind))citadels.push_back({x,y});else if(kind==49)farms.push_back({x,y});}
 // The first recorded citadel whose field holds a farm at (fx,fy), or null.
 const FireSpot*farmBlocker(int fx,int fy)const{for(auto&c:citadels)if(dtfire::farmTooClose(fx,fy,c.x,c.y))return &c;return nullptr;}
 bool farmBlocked(int fx,int fy)const{return farmBlocker(fx,fy)!=nullptr;}
 // A citadel position whose field would hold any recorded farm.
 bool citadelBlocked(int bx,int by)const{for(auto&f:farms)if(dtfire::farmTooClose(f.x,f.y,bx,by))return true;return false;}
 bool blocked(int kind,int x,int y)const{return kind==49?farmBlocked(x,y):citadelKind(kind)?citadelBlocked(x,y):false;}
 // Scan order varies between ticks; sort before comparing snapshots.
 void normalize(){auto less=[](const FireSpot&a,const FireSpot&b){return a.y!=b.y?a.y<b.y:a.x<b.x;};std::sort(citadels.begin(),citadels.end(),less);std::sort(farms.begin(),farms.end(),less);}
 bool operator==(const FireBlockade&o)const{return citadels==o.citadels&&farms==o.farms;}
 bool operator!=(const FireBlockade&o)const{return !(*this==o);}
};
// Player text for a refused site (no coordinates, no internals).
inline const char*fireNotice(int kind){return kind==49?"Too close to the fire of your Temple or Fortress.":kind==50?"Too close to your Mushroom Farms. The Temple's fire needs room.":kind==51?"Too close to your Mushroom Farms. The Fortress's fire needs room.":"";}
}
