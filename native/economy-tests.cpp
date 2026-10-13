#include "economy.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
int main(){
 // Exact schema-2 layout is kept for old paired saves, not new farm accounting.
 static_assert(sizeof(dt::Farm)==44,"Legacy Farm layout changed");
 static_assert(sizeof(dt::Construction)==24&&sizeof(dt::Aircraft)==12,"Legacy job layout changed");
 static_assert(sizeof(dt::State)==15920&&offsetof(dt::State,farms)==48&&offsetof(dt::State,construction)==11312&&offsetof(dt::State,aircraft)==14384,"Legacy State layout changed");
 dt::State s;s.tick=100;s.farmCount=3;s.harvested=200;s.delivered=150;s.lost=20;
 s.farms[0].id=1;s.farms[0].identity=101;s.farms[0].stock=13;s.farms[0].courier=3;s.farms[0].courierIdentity=103;
 s.farms[1].id=2;s.farms[1].stock=9;s.farms[1].cargo=8;s.farms[1].courier=4;s.farms[1].returning=1;
 s.farms[2].id=0; // Old destroyed-farm slot with no remaining inventory.
 s.constructionCount=1;s.construction[0]={105,130,150,5,51,406,308,1,0,0,0};
 s.aircraftCount=1;s.aircraft[0]={106,400,6,0};
 assert(dt::valid(s));dt::State loaded;memcpy(&loaded,&s,sizeof(s));
 assert(dt::valid(loaded)&&loaded.version==2&&loaded.farms[1].cargo==8);
 assert(dt::retireLegacyFarmState(loaded));
 assert(loaded.tick==100&&loaded.version==2&&!loaded.farmCount);
 assert(loaded.harvested==200&&loaded.delivered==150&&loaded.lost==50);
 for(auto &farm:loaded.farms){dt::Farm zero{};assert(!memcmp(&farm,&zero,sizeof(farm)));}
 assert(loaded.constructionCount==1&&!memcmp(loaded.construction,s.construction,sizeof(s.construction)));
 assert(loaded.aircraftCount==1&&!memcmp(loaded.aircraft,s.aircraft,sizeof(s.aircraft)));
 dt::State retired=loaded;assert(dt::retireLegacyFarmState(loaded));assert(!memcmp(&retired,&loaded,sizeof(loaded)));
 memcpy(&retired,&loaded,sizeof(loaded));assert(dt::valid(retired));
 dt::State fresh;assert(dt::retireLegacyFarmState(fresh)&&dt::valid(fresh));
 auto refused=[](dt::State broken){dt::State before=broken;assert(!dt::valid(broken));assert(!dt::retireLegacyFarmState(broken));assert(!memcmp(&before,&broken,sizeof(broken)));};
 dt::State bad=s;bad.farmCount=dt::MaxFarms+1;refused(bad);
 bad=s;bad.farms[1].cargo=9;refused(bad);
 bad=s;bad.harvested=149;refused(bad);
 bad=s;bad.lost=51;refused(bad);
 bad=s;bad.farms[0].id=0;refused(bad);
 bad=s;bad.farms[0].x=576;refused(bad);
 bad=s;bad.construction[0].cost=500;refused(bad);
 assert(dt::buildCost(49)==25&&dt::buildCost(50)==100&&dt::buildCost(51)==150&&dt::buildCost(80)==120&&dt::buildCost(40)==0);
 puts("PASS: legacy schema layout, inventory retirement, paid jobs retained, idempotent reload, invalid-state refusal and costs");
}
