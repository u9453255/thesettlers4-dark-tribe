#include "placement-hints.hpp"
#include "placement-model.hpp"
#include "footprints.hpp"
#include <assert.h>
#include <set>
#include <utility>
#include <stdio.h>
#include <map>
struct Hints {std::set<std::pair<int,int>> visited;int type=0;void point(int x,int y,int kind){visited.emplace(x,y);type=kind;}};
struct Cache {
 struct Node {unsigned x;uintptr_t next;uint8_t value;};std::map<uintptr_t,Node> nodes;uintptr_t head=1;unsigned allocated=0,reads=0,rows=0;
 Cache(){nodes[1]={576,0,0};}
 bool row(int,uintptr_t&out){rows++;out=head;return true;}
 bool node(uintptr_t address,unsigned&x,uintptr_t&next){reads++;auto n=nodes.find(address);if(n==nodes.end())return false;x=n->second.x;next=n->second.next;return true;}
 bool store(uintptr_t address,uint8_t value){nodes.at(address).value=value;return true;}
 void insert(int x,int,uint8_t value){uintptr_t id=nodes.size()+1;auto*link=&head;while(nodes.at(*link).x<(unsigned)x)link=&nodes.at(*link).next;nodes[id]={(unsigned)x,*link,value};*link=id;allocated++;}
};
int main(){
 for(int kind:{49,50,51,80}){
  dt::HintScan scan;Hints native;auto box=dt::clipHints({20,30,60,50},576);assert(scan.begin(box,kind,123,10));
  assert(scan.update(native)==800&&scan.complete&&native.visited.size()==800&&native.type==kind); // No hover event.
  assert(!scan.begin(box,kind,123,11)&&scan.update(native)==0);
  assert(scan.begin({60,30,100,50},kind,123,12));native.visited.clear();assert(scan.update(native)==800&&native.visited.count({99,49}));
  assert(scan.begin(box,kind,124,13));assert(scan.grid==124); // Native cache replaced.
  scan.update(native);assert(!scan.begin(box,kind,124,23));assert(!scan.complete); // Automatic occupancy refresh.
 }
 dt::HintScan large;Hints native;assert(large.begin({16,16,200,200},80,1,1));assert(large.update(native)==8192&&!large.complete);while(!large.complete)large.update(native);assert(native.visited.size()==184*184);
 large.begin({16,16,30,30},1,1,2);assert(large.complete&&large.kind==0);
 auto edge=dt::clipHints({-20,-5,900,800},576);assert(edge.left==16&&edge.top==16&&edge.right==560&&edge.bottom==560);
 assert(dt::hintValue(-1,10)==0&&dt::hintValue(0,10)==0&&dt::hintValue(1,10)==10&&dt::hintValue(8,10)==17&&dt::hintValue(1,74)==74);
 for(unsigned terrain=0;terrain<256;terrain++)assert(dt::darkBuildTerrain(terrain)==(terrain!=21&&((terrain&240)==16||(terrain&240)==64||(terrain&240)==144||terrain==48)));
 assert(dt::darkBuildCell(true,28,4,0)); // Unwalkable road/zero walking sector is still buildable.
 assert(dt::darkBuildCell(true,144,4,0));
 for(unsigned flags=0;flags<256;flags++)assert(dt::darkBuildFlags(flags)==((flags&95)==4));
 assert(!dt::darkBuildCell(true,28,6,0)&&!dt::darkBuildCell(true,28,4,123)&&!dt::darkBuildCell(false,28,4,0));
 assert(!dt::darkBuildCell(true,32,4,0)&&!dt::darkBuildCell(true,80,4,0)&&!dt::darkBuildCell(true,21,4,0));
 // Exercise the original full body + margin of every Dark building, with a
 // zero walking sector. Real construction flags/objects still refuse a site.
 for(int kind:{49,50,51,80}){
  auto*fp=dt::footprint(kind);assert(fp&&fp->count);int x=396,y=312;
  assert(dt::clearBuilding(fp->cells,fp->count,x,y,[](int,int,bool body){return body?dt::darkBuildCell(true,28,4,0):dt::darkHaloCell(true,28,4,0);}));
  int bx=x+fp->cells[0].x,by=y+fp->cells[0].y;
  assert(!dt::clearBuilding(fp->cells,fp->count,x,y,[&](int a,int b,bool body){unsigned flags=a==bx&&b==by?68:4;return body?dt::darkBuildCell(true,28,flags,0):dt::darkHaloCell(true,28,flags,0);}));
  assert(!dt::clearBuilding(fp->cells,fp->count,x,y,[&](int a,int b,bool body){unsigned id=a==bx&&b==by?1064:0;return body?dt::darkBuildCell(true,28,4,id):dt::darkHaloCell(true,28,4,id);}));
  assert(dt::darkHaloCell(true,28,68,0)); // Native outline-only flag outside body.
  assert(!dt::darkHaloCell(true,28,69,0)); // Physical blocker still refused.
 }
 Cache cache;dt::HintRow row;
 for(int x=16;x<200;x++)assert(dt::storeHint(row,x,20,(x%2)?10:0,0,cache));
 assert(cache.allocated==92&&cache.rows==1);auto allocated=cache.allocated;cache.reads=0;row={};
 for(int x=16;x<200;x++)assert(dt::storeHint(row,x,20,(x%2)?11:0,0,cache));
 assert(cache.allocated==allocated&&cache.reads<400);row={}; // Linear reuse, not a fresh row search per tile.
 for(int x=16;x<200;x++)assert(dt::storeHint(row,x,20,0,0,cache));
 assert(cache.allocated==allocated);for(auto&n:cache.nodes)assert(n.second.value==0);
 Cache broken;broken.head=0;row={};assert(!dt::storeHint(row,20,20,10,0,broken));
 puts("PASS: all four viewport hint scans without hover; pan/cache/occupancy refresh, bounded batches, sparse cache reuse and original Dark masks/terrain/collision flags");
}
