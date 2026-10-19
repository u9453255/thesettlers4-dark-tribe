#include <assert.h>
#include <stdio.h>
#include "scenario-model.hpp"
static dt::Setup setup(const dt::Scenario&c){
 dt::Setup s;s.size=c.size;s.players=c.players;
 for(uint32_t i=0;i<c.players;i++){s.slot[i]=i?1:0;s.race[i]=c.start[i].race;s.x[i]=c.start[i].x;s.y[i]=c.start[i].y;}
 return s;
}
int main(){
 // Every listed scenario is accepted, including the legacy First Dawn save layout.
 for(auto&c:dt::Scenarios)assert(dt::sceneMask(setup(c))==0);
 const auto&first=dt::Scenarios[0],&legacy=dt::Scenarios[1],&iron=dt::Scenarios[2],&alliance=dt::Scenarios[3];
 // Mixed layouts: a known Dark start with another scenario's opponent start.
 {auto s=setup(first);s.x[1]=iron.start[1].x;s.y[1]=iron.start[1].y;assert(dt::sceneMask(s)==256);}
 {auto s=setup(iron);s.x[1]=legacy.start[1].x;s.y[1]=legacy.start[1].y;assert(dt::sceneMask(s)==256);}
 // Each part of the fingerprint.
 {auto s=setup(alliance);s.size=576;assert(dt::sceneMask(s)==4);}
 {auto s=setup(iron);s.size=512;assert(dt::sceneMask(s)==4);}
 {auto s=setup(alliance);s.players=2;assert(dt::sceneMask(s)==16);}
 {auto s=setup(first);s.players=4;assert(dt::sceneMask(s)==16);}
 {auto s=setup(alliance);s.slot[3]=0;assert(dt::sceneMask(s)==32);}   // a second human
 {auto s=setup(alliance);s.slot[0]=1;assert(dt::sceneMask(s)==32);}   // the Dark player as a computer
 {auto s=setup(alliance);s.race[2]=0;assert(dt::sceneMask(s)==64);}
 {auto s=setup(alliance);s.x[3]=261;assert(dt::sceneMask(s)==256);}
 // Unknown maps, including the unchanged campaign sources (their player 1 is not the Dark Tribe).
 {dt::Setup s;s.size=512;s.players=4;s.x[0]=260;s.y[0]=263;assert(dt::sceneMask(s)==(128|256));}
 {dt::Setup s;s.size=576;s.players=4;s.x[0]=310;s.y[0]=205;assert(dt::sceneMask(s)==(128|256));}
 {dt::Setup s;assert(dt::sceneMask(s)==(128|256));}
 puts("scenario fingerprint tests passed");
}
