#pragma once
#include <algorithm>
#include <stdint.h>
namespace dt {
struct HintBox {int left=0,top=0,right=0,bottom=0;bool operator==(const HintBox&o)const{return left==o.left&&top==o.top&&right==o.right&&bottom==o.bottom;}};
inline HintBox clipHints(HintBox box,int size){box.left=std::clamp(box.left,16,size-16);box.right=std::clamp(box.right,16,size-16);box.top=std::clamp(box.top,16,size-16);box.bottom=std::clamp(box.bottom,16,size-16);return box;}
inline bool hintKind(int kind){return kind==49||kind==50||kind==51||kind==80;}
inline uint8_t hintValue(int validity,unsigned base){return validity>0&&validity<=8?(uint8_t)(base+validity-1):0;}
struct HintRow {int y=-1;uintptr_t node=0;};
// A tick walks each sparse row once in increasing x order. Existing entries
// change in place; only missing non-default markers use the native allocator.
template<class Ops> bool storeHint(HintRow&row,int x,int y,uint8_t value,uint8_t fallback,Ops&ops){
 if(row.y!=y){if(!ops.row(y,row.node))return false;row.y=y;}
 for(unsigned n=0;n<4096;n++){
  unsigned at=0;uintptr_t next=0;if(!ops.node(row.node,at,next))return false;
  if(at>=(unsigned)x){if(at==(unsigned)x)return ops.store(row.node,value);if(value!=fallback)ops.insert(x,y,value);return true;}
  row.node=next;
 }
 return false;
}
// All eligible points in the viewport are checked without any mouse movement.
// Bounded tick batches avoid making the main thread scan an unlimited zoomed view.
struct HintScan {
 static constexpr unsigned Budget=8192;
 HintBox box{};int kind=0;uintptr_t grid=0;unsigned cursor=0;uint32_t nextRefresh=0;bool complete=true;
 void reset(){*this=HintScan{};}
 bool begin(HintBox next,int type,uintptr_t generation,uint32_t tick){
  if(!hintKind(type)||next.right<=next.left||next.bottom<=next.top||!generation){reset();return false;}
  bool changed=kind!=type||grid!=generation||!(box==next);
  if(changed||(complete&&tick>=nextRefresh)){box=next;kind=type;grid=generation;cursor=0;complete=false;nextRefresh=tick+10;}
  return changed;
 }
 template<class Ops> unsigned update(Ops&ops,unsigned budget=Budget){
  if(complete)return 0;unsigned checked=0,width=(unsigned)(box.right-box.left),total=width*(unsigned)(box.bottom-box.top);
  while(cursor<total&&checked<budget){int x=box.left+(int)(cursor%width),y=box.top+(int)(cursor/width);ops.point(x,y,kind);cursor++;checked++;}
  complete=cursor==total;return checked;
 }
};
}
