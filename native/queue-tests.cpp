#include "queue-model.hpp"
#include <assert.h>
#include <stdio.h>
int main(){uint32_t q[6]={0,7,0,0,0,0};assert(!dt::adjustQueue(q,40,1));assert(dt::adjustQueue(q,29,1)&&q[4]==1);assert(dt::adjustQueue(q,29,-1)&&q[4]==0);assert(!dt::adjustQueue(q,29,-1)&&q[4]==0);for(int i=0;i<150;i++)dt::adjustQueue(q,32,1);assert(q[5]==99);dt::adjustQueue(q,52,1);dt::adjustQueue(q,54,1);assert(dt::clearManualQueues(q)==101&&q[1]==7&&q[2]==0&&q[3]==0&&q[4]==0&&q[5]==0);q[4]=100;assert(!dt::adjustQueue(q,29,1)&&q[4]==100);assert(dt::adjustQueue(q,29,-1)&&q[4]==99);assert(dt::clearManualQueues(q)==99&&q[1]==7);dt::State s;uint32_t a=dt::nextAircraftTime(s,5,9,100);assert(a==400);s.aircraft[s.aircraftCount++]={9,a,5};uint32_t b=dt::nextAircraftTime(s,5,9,110);assert(b==700);s.aircraft[s.aircraftCount++]={9,b,5};s.aircraft[s.aircraftCount++]={10,450,6};assert(dt::cancelAircraft(s,5,9,false)==300&&s.aircraftCount==2);assert(dt::nextAircraftTime(s,5,9,120)==700);assert(dt::cancelAircraft(s,5,9,true)==300&&s.aircraftCount==1);assert(dt::cancelAircraft(s,5,9,true)==0&&s.aircraftCount==1);assert(dt::cancelAircraft(s,6,99,true)==0);assert(dt::valid(s));
 // Native control orders (barracks cluster): +1, +5, -1, -5, 0 = clear, 100 = infinite.
 {using dt::queueAfter;
  assert(queueAfter(0,1)==1&&queueAfter(0,5)==5&&queueAfter(12,5)==17&&queueAfter(96,5)==99&&queueAfter(98,1)==99&&queueAfter(99,1)==99&&queueAfter(99,5)==99);
  assert(queueAfter(7,-5)==2&&queueAfter(3,-5)==0&&queueAfter(1,-1)==0&&queueAfter(0,-1)==0&&queueAfter(0,-5)==0);
  assert(queueAfter(0,dt::OrderInfinite)==100&&queueAfter(42,dt::OrderInfinite)==100&&queueAfter(100,dt::OrderInfinite)==100&&queueAfter(42,250)==100);
  assert(queueAfter(100,1)==100&&queueAfter(100,5)==100&&queueAfter(100,-1)==99&&queueAfter(100,-5)==95&&queueAfter(100,-100)==0&&queueAfter(250,-1)==99&&queueAfter(250,1)==100);
  assert(queueAfter(42,dt::OrderClear)==0&&queueAfter(100,dt::OrderClear)==0&&queueAfter(0,dt::OrderClear)==0);
  // Controls are disabled exactly where the order would change nothing.
  assert(!dt::canOrder(99,1)&&!dt::canOrder(99,5)&&dt::canOrder(98,5)&&!dt::canOrder(0,-1)&&!dt::canOrder(0,-5)&&!dt::canOrder(0,dt::OrderClear)&&dt::canOrder(1,dt::OrderClear));
  assert(!dt::canOrder(100,1)&&!dt::canOrder(100,5)&&!dt::canOrder(100,dt::OrderInfinite)&&dt::canOrder(100,-1)&&dt::canOrder(100,dt::OrderClear)&&dt::canOrder(0,dt::OrderInfinite));
  assert(dt::infiniteQueue(100)&&dt::infiniteQueue(101)&&!dt::infiniteQueue(99));
  for(uint32_t n=0;n<=100;n++)for(int amount:{1,5,-1,-5,0,100}){uint32_t r=queueAfter(n,amount);assert(r<=100&&(r==100)==(amount==100||(n==100&&amount>0)));}}
 // orderQueue: each unit's own queue; the automatic Mushroom Farmer queue (index 1) is never touched.
 {uint32_t r[6]={0,9,0,0,0,0};
  assert(dt::orderQueue(r,29,5)&&r[4]==5&&dt::orderQueue(r,32,dt::OrderInfinite)&&r[5]==100&&dt::orderQueue(r,52,1)&&r[2]==1&&dt::orderQueue(r,54,5)&&r[3]==5);
  assert(!dt::orderQueue(r,32,5)&&r[5]==100&&dt::orderQueue(r,32,-5)&&r[5]==95&&dt::orderQueue(r,29,-1)&&r[4]==4&&dt::orderQueue(r,29,dt::OrderClear)&&r[4]==0&&!dt::orderQueue(r,29,dt::OrderClear));
  assert(!dt::orderQueue(r,53,5)&&!dt::orderQueue(r,53,dt::OrderClear)&&!dt::orderQueue(r,53,dt::OrderInfinite)&&!dt::orderQueue(r,40,1)&&!dt::orderQueue(nullptr,29,1)&&r[1]==9&&r[0]==0);
  assert(dt::clearManualQueues(r)==95+1+5&&r[1]==9&&!r[2]&&!r[3]&&!r[4]&&!r[5]);}
 puts("PASS: queue counts, immediate add/cancel, native +-1/+-5/clear/infinite orders, bounds, protected farm workers, aircraft scheduling/refunds and generation guards");}
