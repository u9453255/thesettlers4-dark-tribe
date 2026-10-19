#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include "diagnostics-model.hpp"
using namespace dtdiag;
// Fake address space: a few byte ranges; reads outside them fail like an unreadable page.
struct Memory {
 struct Range {uint32_t base;uint8_t bytes[512];uint32_t size;};
 Range ranges[4];int count=0;
 Range&add(uint32_t base,uint32_t size){Range&r=ranges[count++];r.base=base;r.size=size;memset(r.bytes,0x90,sizeof(r.bytes));return r;}
 void put(uint32_t a,const uint8_t*b,uint32_t n){for(int i=0;i<count;i++)if(a>=ranges[i].base&&a+n<=ranges[i].base+ranges[i].size){memcpy(ranges[i].bytes+(a-ranges[i].base),b,n);return;}assert(false);}
 void put32(uint32_t a,uint32_t v){put(a,(const uint8_t*)&v,4);}
 bool operator()(uint32_t a,void*out,uint32_t n)const{for(int i=0;i<count;i++)if(a>=ranges[i].base&&a+n<=ranges[i].base+ranges[i].size){memcpy(out,ranges[i].bytes+(a-ranges[i].base),n);return true;}return false;}
};
static bool call(std::initializer_list<int> tail){uint8_t b[7];memset(b,0x90,7);int k=7-(int)tail.size();for(int v:tail)b[k++]=(uint8_t)v;return callBefore(b);}
int main(){
 // Heap-free text.
 {Text t;t.put("FREEZE ").hex(0x1bd257,0).put(" ").hex(0x2a).put(" ").dec(0).put(" ").dec(18446744073709551615ull);assert(!strcmp(t.s,"FREEZE 1bd257 0000002a 0 18446744073709551615"));
  Text u;for(int i=0;i<100;i++)u.put("0123456789");assert(u.n==sizeof(u.s)-1&&u.s[u.n]==0);u.ch('x').hex(1);assert(u.n==sizeof(u.s)-1);}
 // Call encodings ending right before a return address.
 assert(call({0xE8,1,2,3,4}));                  // call rel32
 assert(call({0xFF,0x15,0xF0,0x50,0xF1,0x00}));  // call [abs32]
 assert(call({0xFF,0xD0}));                      // call eax
 assert(call({0xFF,0x10}));                      // call [eax]
 assert(call({0xFF,0x50,0x08}));                 // call [eax+8]
 assert(call({0xFF,0x14,0x24}));                 // call [esp]
 assert(call({0xFF,0x54,0x24,0x08}));            // call [esp+8]
 assert(call({0xFF,0x90,1,2,3,4}));              // call [eax+disp32]
 assert(call({0xFF,0x94,0x24,1,2,3,4}));         // call [esp+disp32]
 assert(call({0xFF,0x14,0x85,1,2,3,4}));         // call [disp32+eax*4]
 assert(!call({0xFF,0x25,1,2,3,4}));             // jmp [abs32]
 assert(!call({0xFF,0x50}));                     // truncated call [eax+8]
 assert(!call({0x55,0x8b,0xec,0x83,0xec,0x10,0x53}));
 assert(indirectCallLength(0xD0,0)==2&&indirectCallLength(0x15,0)==6&&indirectCallLength(0x14,0x25)==7&&indirectCallLength(0x14,0x24)==3&&indirectCallLength(0x20,0)==0);
 // Stack scan: return addresses after calls inside modules, in stack order, no consecutive duplicates.
 {Memory m;m.add(0x00400000,512);m.add(0x77000000,512);m.add(0x00001000,256);
  const uint8_t rel[]={0xE8,0,0,0,0},abs[]={0xFF,0x15,0,0,0,0};
  m.put(0x00400100,rel,5);   // return 0x400105
  m.put(0x7700001A,abs,6);   // return 0x77000020
  m.put32(0x1004,0x12345678);m.put32(0x1010,0x00400105);m.put32(0x1020,0x00400110);m.put32(0x1030,0x77000020);m.put32(0x1034,0x77000020);m.put32(0x1040,0x00400105);
  Span spans[]={{0x00400000,512},{0x77000000,512}};uint32_t f[8];
  int n=scanStack(m,0x00400050,0x1002,0x1100,spans,2,f,8);
  assert(n==4&&f[0]==0x00400050&&f[1]==0x00400105&&f[2]==0x77000020&&f[3]==0x00400105);
  assert(scanStack(m,0x00400050,0x1000,0x1100,spans,2,f,2)==2);   // frame limit
  assert(scanStack(m,0x00400050,0x1000,0x1020,spans,2,f,8)==2);   // stops at the stack top
  assert(scanStack(m,0x00400050,0x1200,0x1100,spans,2,f,8)==1);   // esp above top: eip only
  assert(scanStack(m,0x00400050,0x1000,0x1100,spans,1,f,8)==2);   // second module unknown: its frame is skipped, the repeat then dropped
  assert(scanStack(m,0x00400050,0x0F00,0x1100,spans,2,f,8,0x20)==1); // byte limit; unreadable words skipped
 }
 // Heap failure names and exception classes.
 assert(!strcmp(heapFailure(8),"double free (block not busy)")&&!strcmp(heapFailure(3),"entry corruption")&&!strcmp(heapFailure(99),"unrecognised"));
 assert(fatalCode(0xC0000374)&&fatalCode(0xC00000FD)&&fatalCode(0xC0000017)&&!fatalCode(0xC0000005));
 assert(faultCode(0xC0000005)&&faultCode(0xC0000094)&&faultCode(0xC000001D));
 for(uint32_t c:{0xE06D7363u,0xE0434352u,0x80000003u,0x40010006u,0x406D1388u})assert(!fatalCode(c)&&!faultCode(c)); // C++/CLR exceptions, breakpoints, debug output, thread names
 assert(!strcmp(codeName(0xC0000374),"heap corruption")&&!strcmp(codeName(0xC0000005),"access violation"));
 // Watchdog: menus, pauses and responsive windows are not freezes.
 {Watch w;
  assert(step(w,50000,0,true,true,true,true)==Event::None);           // no beat yet
  assert(step(w,50000,1000,false,true,true,true)==Event::None);       // last beat in the menus
  assert(step(w,10999,1000,true,true,true,true)==Event::None);        // 9.999 s
  assert(step(w,11000,1000,true,true,false,true)==Event::None);       // window still responding (pause, modal loop)
  assert(step(w,11000,1000,true,false,false,false)==Event::None);     // unknown window, game in the background
  assert(step(w,11000,1000,true,true,true,false)==Event::Freeze&&w.stalled&&w.samples==1);
  assert(wantDump(w,Event::Freeze,true,0));w.dumped=true;
  assert(step(w,20000,1000,true,true,true,true)==Event::None);
  assert(step(w,31000,1000,true,true,true,true)==Event::Sample&&w.samples==2);
  assert(!wantDump(w,Event::Sample,true,1));                          // already dumped this freeze
  assert(step(w,90000,1000,true,true,true,true)==Event::None);        // two samples per freeze
  assert(step(w,91000,90500,true,true,false,true)==Event::Recovered&&!w.stalled&&w.lastStall==89500);
  assert(step(w,92000,90500,true,true,false,true)==Event::None);
 }
 {Watch w;  // unknown window: the foreground game counts; the dump waits for the second sample
  assert(step(w,12000,1000,true,false,false,true)==Event::Freeze);
  assert(!wantDump(w,Event::Freeze,false,0));
  assert(step(w,31000,1000,true,false,false,true)==Event::Sample&&wantDump(w,Event::Sample,false,0));
  assert(!wantDump(w,Event::Sample,false,MaxDumps));                  // per-process limit
  assert(step(w,500,1000,true,false,false,true)==Event::None);        // clock before the beat
 }
 {Watch w;assert(step(w,500,1000,true,true,true,true)==Event::None);}
 puts("diagnostics tests passed");
}
