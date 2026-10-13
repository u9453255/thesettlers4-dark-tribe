#include "placement-model.hpp"
#include <assert.h>
#include <stdio.h>
int main(){dt::WorldCell footprint[]={{0,0},{1,0}};auto r=dt::reservation(footprint,2,100,100);assert(dt::clearReservation(r,[](int,int){return false;}));assert(!dt::clearReservation(r,[](int x,int y){return x==100&&y==100;}));assert(!dt::clearReservation(r,[](int x,int y){return x==102&&y==101;}));auto edge=dt::reservation(footprint,2,1,1);assert(!dt::clearReservation(edge,[](int,int){return false;}));auto second=dt::reservation(footprint,2,101,101);bool overlaps=false;for(auto&c:second)overlaps|=dt::contains(r,c.x,c.y);assert(overlaps);assert(!dt::contains(r,110,110));puts("PASS: empty footprint, unit/object occupancy, safety margin, map edges and adjacent footprint overlap");}
