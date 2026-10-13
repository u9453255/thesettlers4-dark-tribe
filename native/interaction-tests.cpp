#include "interaction-model.hpp"
#include "economy.hpp"
#include "aircraft-model.hpp"
#include <assert.h>
#include <stdio.h>
int main(){
 for(uint16_t kind:{49,50,51,80}){
  uint32_t cost=dt::buildCost(kind);
  assert(!dt::affordable(cost-1,cost,0,dt::MaxJobs));
  assert(dt::affordable(cost,cost,0,dt::MaxJobs));
  assert(!dt::affordable(cost,cost,dt::MaxJobs,dt::MaxJobs));
  uint32_t bank=cost*2,after=0;
  assert(dt::manaTarget(bank,-(int)cost,after));bank=after;
  assert(dt::affordable(bank,cost,1,dt::MaxJobs));
  assert(dt::manaTarget(bank,-(int)cost,after)&&after==0);bank=after;
  assert(!dt::affordable(bank,cost,2,dt::MaxJobs));
 }
 uint32_t after=0;
 assert(!dt::manaTarget(0,-1,after));assert(!dt::manaTarget(UINT32_MAX,1,after));
 assert(dt::manaTarget(1,-1,after)&&after==0);
 assert(dt::manaTarget(0,150,after)&&after==150);
 dt::UnitRef unit{4,29,400},replacement{4,29,401};
 assert(dt::sameEntity(unit,unit));assert(!dt::sameEntity(unit,replacement));
 assert(!dt::sameEntity({},{}));
 dt::SpriteHit hit{unit,200,100,240,140,1000};
 assert(dt::spriteContains(hit,200,100,1100));assert(!dt::spriteContains(hit,240,100,1100));
 assert(!dt::spriteContains(hit,220,120,1251));
 hit.stamp=UINT32_MAX-20;assert(dt::spriteContains(hit,220,120,5));
 assert(dt::nearBoarding(106,94,100,100));assert(!dt::nearBoarding(107,100,100,100));
 for(uint16_t kind:{29,32,52,54})assert(dt::boardable(kind));
 assert(!dt::boardable(53));
 // Move orders onto/next to a landed aircraft board it under any renderer.
 assert(dt::boardingTarget(100,100,100,100)&&dt::boardingTarget(104,96,100,100)&&!dt::boardingTarget(105,100,100,100)&&!dt::boardingTarget(100,95,100,100));
 // Right-button click versus camera drag (native slop: more than 4 px on either axis is a pan).
 using G=dt::RightGesture;const int cx=640,cy=360;
 {G g;g.press(100,100,1000,G::Passed,true);G r=g.release();
  assert(r.route==G::Passed&&r.aircraftOnly&&r.x==100&&r.y==100);
  assert(dt::rightClick(r,100,100,1080,cx,cy)); // press and release, no moves: click
  assert(dt::rightClick(r,104,96,1080,cx,cy)&&dt::rightClick(r,96,104,1080,cx,cy)); // 4 px on each axis is still a click
  assert(!dt::rightClick(r,105,100,1080,cx,cy)&&!dt::rightClick(r,100,95,1080,cx,cy)); // 5 px on one axis: pan
  assert(dt::rightClick(r,100,100,90000,cx,cy)); // far from the centre a long hold is still a click (as native)
  assert(g.route==G::None&&!g.aircraftOnly&&g.reach==0);} // release forgets the press
 {G g;g.press(300,200,0,G::Passed,true);g.sample(302,197);g.sample(296,204);
  assert(g.reach==4&&dt::rightClick(g,300,200,50,cx,cy)); // jitter inside the slop: click
  g.sample(330,200);g.sample(301,200);
  assert(g.reach==30&&!dt::rightClick(g,300,200,50,cx,cy));} // drag that returns to the start: pan
 {G g;g.press(300,200,0,G::Passed,true);g.sample(300,190); // 10 px
  g.press(50,60,10,G::Consumed,false); // a new press resets everything
  assert(g.reach==0&&g.route==G::Consumed&&!g.aircraftOnly&&g.x==50&&g.y==60&&g.time==10);
  assert(g.release().route==G::Consumed);}
 {G g;g.sample(500,500);assert(g.reach==0); // moves without a press are ignored
  G none=g.release();assert(none.route==G::None&&!dt::rightClick(none,0,0,0,cx,cy));} // a lone release is never a click
 // LegacyMouse parks the cursor at the render centre while panning: near the
 // centre only a short hold is a click.
 {G g;g.press(cx+3,cy-2,5000,G::Passed,true);
  assert(dt::rightClick(g,cx+3,cy-2,5000+dt::WarpClickMs,cx,cy));
  assert(!dt::rightClick(g,cx,cy,5000+dt::WarpClickMs+1,cx,cy));
  assert(dt::rightClick(g,cx+3,cy-2,90000,0,0)); // centre unknown: slop rule only
  g.press(cx+dt::WarpZone,cy-dt::WarpZone,5000,G::Passed,true);assert(!dt::rightClick(g,cx+dt::WarpZone,cy-dt::WarpZone,90000,cx,cy)); // zone edge, long hold
  g.press(cx+dt::WarpZone+1,cy,5000,G::Passed,true);assert(dt::rightClick(g,cx+dt::WarpZone+1,cy,90000,cx,cy)); // outside the zone
  g.press(cx,cy,UINT32_MAX-50,G::Passed,true);assert(dt::rightClick(g,cx,cy,100,cx,cy)); // 151 ms across the tick-count wrap
  assert(!dt::rightClick(g,cx,cy,dt::WarpClickMs,cx,cy));}
 puts("PASS: all construction cost boundaries, sequential spend/refund bounds, selection identities, sprite expiry, boarding proximity, move-to-aircraft boarding targets, protected workers and right-button click/pan classification");
}
