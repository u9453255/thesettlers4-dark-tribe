#pragma once
#include <windows.h>
#include "fast-memory.hpp"
#include "map-size.hpp"
#include "placement-hints.hpp"
#include "placement-model.hpp"
#include "blockade-model.hpp"
#include "placement-signatures.hpp" // Generated locally from the exact pinned executable.
#include <string.h>
namespace dtplacement {
struct Camera {int width=0,height=0;BYTE projection[52]{};int mode=0;bool operator==(const Camera&o)const{return width==o.width&&height==o.height&&mode==o.mode&&!memcmp(projection,o.projection,sizeof(projection));}};
class Hints {
public:
 struct Diag {unsigned dark=0,valid=0,flagged=0,terrain=0,footprint=0,bits[8]{},fire=0;};
private:
 uintptr_t image=0;Camera camera{};dt::HintBox box{};bool boundsReady=false;dt::HintScan scan;
 using Project=int(__stdcall*)(int,int,int*,int*);
 using Validate=int(__stdcall*)(int,int,int,int,int);
 using Set=void(__thiscall*)(void*,int,int,const BYTE*);
 static bool readable(uintptr_t address,size_t bytes){return dtmem::readable(address,bytes);}
 template<class T> static bool read(uintptr_t address,T&out){if(!readable(address,sizeof(T)))return false;memcpy(&out,(void*)address,sizeof(T));return true;}
 struct Cache {
  Hints&self;uintptr_t grid,rows;
  bool row(int y,uintptr_t&node){uintptr_t header=0;return read(rows+y*4,header)&&read(header+4,node);}
  bool node(uintptr_t address,unsigned&x,uintptr_t&next){struct Node{uint32_t x,next;BYTE value,pad[3];}data{};if(!read(address,data))return false;x=data.x;next=data.next;return true;}
  bool store(uintptr_t address,BYTE value){if(!dtmem::writable(address,12))return false;if(*(BYTE*)(address+8)!=value)*(BYTE*)(address+8)=value;return true;}
  void insert(int x,int y,BYTE value){((Set)(self.image+0xeefa0))((void*)grid,x,y,&value);}
 };
 // Why Dark anchors in view were rejected during one complete scan pass
 // (diagnostics only; the native validator decides). The Dark branch of the
 // validator (RVA 0x13EBC0) needs, on every body cell, construction flags
 // (flags & 0x5F) == 4, an accepted terrain, and acceptable slope (0x13DC90).
 struct Ops {
  Hints&self;BYTE*land;BYTE*flags;unsigned base;Cache cache;BYTE fallback;int size;dt::HintRow row{};bool ok=true;unsigned valid=0;Diag*diag=nullptr;
  void point(int x,int y,int type){
   if(!ok)return;
   unsigned index=(unsigned)(y*size+x);int result=0;
   if(land[4*index+2]&64){result=((Validate)(self.image+0x13e4e0))(x,y,1,type,0);
    /* r30: an anchor inside the citadel fire blockade is never marked (the tick refuses such a site). */
    bool fire=result>0&&self.blockade&&self.blockade->blocked(type,x,y);if(fire)result=0;
    if(diag){diag->dark++;BYTE f=flags?flags[index]:4;unsigned t=land[4*index+1];
     if(result>0)diag->valid++;else if(fire)diag->fire++;else if((f&0x5f)!=4){diag->flagged++;for(int b=0;b<8;b++)if(((f&0x5f)^4)&(1u<<b))diag->bits[b]++;}
     else if(!dt::darkBuildTerrain(t))diag->terrain++;else diag->footprint++;}}
   BYTE value=dt::hintValue(result,base);if(value)valid++;if(!dt::storeHint(row,x,y,value,fallback,cache))ok=false;
  }
 };
 Diag pass{},finished{};bool reportReady=false;int reportKind=0;dt::HintBox reportBox{};
 const dt::FireBlockade*blockade=nullptr; // tick-owned snapshot from construction(); null: no blockade
public:
 void blockWith(const dt::FireBlockade*b){blockade=b;}
 void reset(){camera={};box={};boundsReady=false;scan.reset();pass={};}
 // The rejection summary of the last complete pass (once per pass).
 bool report(Diag&out,int&kind,dt::HintBox&area){if(!reportReady)return false;reportReady=false;out=finished;kind=reportKind;area=reportBox;return true;}
 bool initialize(uintptr_t module){image=module;return dtplacement::intact(module);}
 bool update(uint32_t tick,bool&changed,unsigned&checked,unsigned&valid){
  changed=false;checked=valid=0;int mode=0,type=0;uintptr_t grid=0,land=0;
  if(!read(image+0x1086588,mode)||!read(image+0x1086548,type)||!read(image+0x105413c,grid)||!read(image+0xd69220,land))return false;
  if(mode<4||mode>6||!dt::hintKind(type)){reset();return true;}
  if(!grid||!land){reset();return true;}
  int n=dtmap::size(image);if(!n||!readable(land,(size_t)n*n*4)||!readable(grid,32))return false;
  uintptr_t rows=0;uint32_t width=0,height=0;BYTE fallback=0;
  if(!read(grid,rows)||!read(grid+0x10,width)||!read(grid+0x14,height)||!read(grid+0x1c,fallback)||width!=(uint32_t)n||height!=(uint32_t)n||!readable(rows,height*4))return false;
  Camera next;if(!read(image+0xd691ec,next.width)||!read(image+0xd691f0,next.height)||!read(image+0xd691d4,next.mode)||!read(image+0xd6920c,next.projection))return false;
  if(next.width<2||next.height<2||next.width>32768||next.height>32768)return false;
  if(!boundsReady||!(next==camera)){
   int left=0,top=0,right=0,bottom=0;auto project=(Project)(image+0x259910);
   project(1,1,&left,&top);project(next.width-2,next.height-2,&right,&bottom);
   box=dt::clipHints({left,top,right,bottom+1},n);camera=next;boundsReady=true;
  }
  changed=scan.begin(box,type,grid,tick);BYTE group=0;if(!read(image+0x109c7dd+type*0x32c,group))return false;
  uintptr_t flagMap=0;if(!read(image+0x11630d8,flagMap)||!readable(flagMap,(size_t)n*n))flagMap=0;
  if(!scan.complete&&scan.cursor==0)pass={}; /* a new pass over the view */
  Ops ops{*this,(BYTE*)land,(BYTE*)flagMap,group==7?74u:10u,{*this,grid,rows},fallback,n};ops.diag=&pass;bool before=scan.complete;checked=scan.update(ops);valid=ops.valid;
  if(!before&&scan.complete){finished=pass;reportReady=true;reportKind=type;reportBox=box;}
  return ops.ok;
 }
};
}
