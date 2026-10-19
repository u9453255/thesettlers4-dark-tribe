#pragma once
#include <stdint.h>
#include <stddef.h>
// Crash and freeze diagnostics, pure parts (tested by diagnostics-tests.cpp).
// These run inside the exception handler and the freeze watchdog, possibly
// while the game's heap is corrupted or locked by a hung thread: no
// allocation, no CRT formatting, fixed buffers only.
namespace dtdiag {

// Heap-free text line; overlong input is cut off.
struct Text {
 char s[640];unsigned n=0;
 Text(){s[0]=0;}
 void clear(){n=0;s[0]=0;}
 // Not inlined: the optimizer would otherwise copy constant tags in vector
 // chunks and the literal log tags (checked by verify-offline) would vanish.
 __attribute__((noinline)) Text& put(const char*p){while(p&&*p&&n<sizeof(s)-1)s[n++]=*p++;s[n]=0;return *this;}
 Text& ch(char c){if(n<sizeof(s)-1){s[n++]=c;s[n]=0;}return *this;}
 // Lower-case hex; digits=0 prints no leading zeros.
 Text& hex(uint32_t v,int digits=8){char b[8];int k=0;do{b[k++]="0123456789abcdef"[v&15];v>>=4;}while(v&&k<8);while(k<digits&&k<8)b[k++]='0';while(k)ch(b[--k]);return *this;}
 Text& dec(uint64_t v){char b[20];int k=0;do{b[k++]=char('0'+v%10);v/=10;}while(v&&k<20);while(k)ch(b[--k]);return *this;}
};

// Length of an indirect call (FF /2) from its ModRM and SIB bytes; 0 if not FF /2.
inline int indirectCallLength(uint8_t modrm,uint8_t sib){
 if(((modrm>>3)&7)!=2)return 0;
 int mod=modrm>>6,rm=modrm&7;
 if(mod==3)return 2;
 if(mod==0){if(rm==5)return 6;if(rm==4)return (sib&7)==5?7:3;return 2;}
 if(mod==1)return rm==4?4:3;
 return rm==4?7:6;
}
// True when the 7 bytes before a candidate return address end with a call:
// E8 rel32, or an FF /2 indirect call whose encoded length ends exactly there.
inline bool callBefore(const uint8_t b[7]){
 if(b[2]==0xE8)return true;
 for(int k=2;k<=7;k++){int at=7-k;if(b[at]==0xFF&&indirectCallLength(b[at+1],at+2<7?b[at+2]:0)==k)return true;}
 return false;
}

struct Span {uint32_t base=0,size=0;};
inline bool inside(const Span*spans,int count,uint32_t a){for(int i=0;i<count;i++)if(a>=spans[i].base&&a-spans[i].base<spans[i].size)return true;return false;}

// Stack trace by scanning the stack from esp towards its top for values that
// point into a loaded module right after a call instruction (works through
// frame-pointer-omitted code such as the game's Lua core). Stale values from
// earlier calls can appear; consecutive duplicates are dropped. read(addr,out,n)
// must fail safely on unreadable memory. Returns the number of frames; frame 0 is eip.
template<class Read>
inline int scanStack(Read&&read,uint32_t eip,uint32_t esp,uint32_t top,const Span*spans,int count,uint32_t*out,int max,uint32_t limit=0x10000){
 int n=0;if(max<=0)return 0;out[n++]=eip;
 if(esp&3)esp=(esp+3)&~3u;
 uint32_t end=top;if(end<esp)return n;if(end-esp>limit)end=esp+limit;
 for(uint32_t a=esp;a+4<=end&&n<max;a+=4){
  uint32_t v=0;if(!read(a,&v,4))continue;
  if(v<8||!inside(spans,count,v-7)||!inside(spans,count,v-1))continue;
  uint8_t b[7];if(!read(v-7,b,7)||!callBefore(b))continue;
  if(out[n-1]!=v)out[n++]=v;
 }
 return n;
}

// ntdll HEAP_FAILURE_TYPE names (the record passed as the first parameter of
// STATUS_HEAP_CORRUPTION; type at +8, heap +0xC, block +0x10, stack +0x38).
inline const char*heapFailure(uint32_t type){
 static const char*const names[]={"internal","unknown","generic","entry corruption","multiple entries corruption","virtual block corruption","buffer overrun","buffer underrun","double free (block not busy)","invalid argument","use after free","cross-heap operation","free lists corruption","list entry corruption","LFH bitmap mismatch","segment LFH bitmap corruption","segment LFH double free","VS subsegment corruption","null heap","allocation limit","commit limit","invalid allocation type"};
 return type<sizeof(names)/sizeof(names[0])?names[type]:"unrecognised";
}

// Exception codes: fatal ones are always logged (bounded); the others are
// first-chance faults the game may handle itself (bounded, as before).
inline bool fatalCode(uint32_t c){return c==0xC0000374u||c==0xC00000FDu||c==0xC0000409u||c==0xC0000602u||c==0xC0000420u||c==0xC0000017u;}
inline bool faultCode(uint32_t c){return c==0xC0000005u||c==0xC0000094u||c==0xC000001Du||c==0xC0000096u||c==0xC0000006u||c==0xC000008Cu||c==0xC0000095u||c==0x80000002u;}
inline const char*codeName(uint32_t c){
 switch(c){case 0xC0000374u:return "heap corruption";case 0xC00000FDu:return "stack overflow";case 0xC0000409u:return "stack buffer overrun";case 0xC0000602u:return "fail fast";case 0xC0000420u:return "assertion failure";case 0xC0000017u:return "out of memory";
  case 0xC0000005u:return "access violation";case 0xC0000094u:return "integer divide by zero";case 0xC000001Du:return "illegal instruction";case 0xC0000096u:return "privileged instruction";case 0xC0000006u:return "in-page error";case 0xC000008Cu:return "array bounds exceeded";case 0xC0000095u:return "integer overflow";case 0x80000002u:return "datatype misalignment";}
 return "exception";
}

// Freeze watchdog. A beat is a game tick or a frame on the game's main thread;
// "ingame" says whether that beat came from a running map. The main thread is
// frozen when no beat came for FreezeMs while the last beat was in a map and
// its window stopped processing messages (Windows "not responding"); when the
// window is unknown, the game being in the foreground counts instead.
constexpr uint64_t FreezeMs=10000,SecondSampleMs=30000;constexpr int MaxDumps=2;
struct Watch {bool stalled=false,dumped=false;uint64_t beat=0,since=0,lastStall=0;int samples=0;};
enum class Event {None,Freeze,Sample,Recovered};
inline Event step(Watch&w,uint64_t now,uint64_t beat,bool ingame,bool windowKnown,bool hung,bool foreground){
 if(w.stalled){
  if(beat!=w.beat){w.stalled=false;w.lastStall=beat>w.beat?beat-w.beat:0;return Event::Recovered;}
  if(w.samples==1&&now>=beat&&now-beat>=SecondSampleMs){w.samples=2;return Event::Sample;}
  return Event::None;
 }
 if(!beat||!ingame||now<beat||now-beat<FreezeMs)return Event::None;
 if(windowKnown?!hung:!foreground)return Event::None;
 w.stalled=true;w.dumped=false;w.beat=beat;w.since=now;w.samples=1;return Event::Freeze;
}
// One dump per freeze, at most MaxDumps per process: at once when Windows
// reports the window as hung, otherwise at the second sample.
inline bool wantDump(const Watch&w,Event e,bool hung,int dumps){
 if(w.dumped||dumps>=MaxDumps)return false;
 return (e==Event::Freeze&&hung)||e==Event::Sample;
}
}
