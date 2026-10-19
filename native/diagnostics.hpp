#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <atomic>
#include "diagnostics-model.hpp"
// Crash and freeze diagnostics, Windows part (round r26; notes in
// research/CRASH_DIAGNOSTICS_0215.md). The exception logger and the freeze
// watchdog run while the game may be crashing or hung, possibly with its heap
// corrupted or locked. They therefore never allocate and never take the
// loader lock: lines go through a log handle opened at startup, memory is read
// with ReadProcessMemory after VirtualQuery, and module names come from the
// loader list read without its lock.
namespace dtdiag {
inline HANDLE logFile=INVALID_HANDLE_VALUE;
inline const std::atomic<uint32_t>*tickSource=nullptr;
inline const std::atomic<const char*>*operationSource=nullptr;
inline uintptr_t image=0;
inline char dumpDir[MAX_PATH]={};

// Same prefix as the plugin's log(): "<ms> pid=<pid> tick=<plugin tick> ".
inline void write(const Text&body){
 if(logFile==INVALID_HANDLE_VALUE)return;
 Text line;line.dec(GetTickCount()).put(" pid=").dec(GetCurrentProcessId()).put(" tick=").dec(tickSource?tickSource->load():0).ch(' ').put(body.s);
 if(line.n>sizeof(line.s)-2)line.n=sizeof(line.s)-2;
 line.s[line.n++]='\n';DWORD done=0;WriteFile(logFile,line.s,line.n,&done,nullptr);
}

// Reads that fail instead of faulting. Whole 1 KB blocks are cached; a block
// never crosses a page, and guard/no-access pages are refused before reading.
struct SafeRead {
 uint8_t block[1024];uint32_t at=0,size=0;
 static bool readablePage(uint32_t a){MEMORY_BASIC_INFORMATION m{};return VirtualQuery((void*)(uintptr_t)a,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS));}
 void reset(){size=0;}
 bool operator()(uint32_t a,void*out,uint32_t n){
  if(!n)return true;if(!a||a+n<a)return false;
  if(size&&a>=at&&a+n<=at+size){memcpy(out,block+(a-at),n);return true;}
  uint32_t base=a&~1023u;SIZE_T got=0;
  if(a+n<=base+1024){
   if(!readablePage(base))return false;
   if(!ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)base,block,1024,&got)||got!=1024)return false;
   at=base;size=1024;memcpy(out,block+(a-base),n);return true;
  }
  if(!readablePage(a)||!readablePage(a+n-1))return false;
  return ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)a,out,n,&got)&&got==n;
 }
};

struct Module {uint32_t base=0,size=0;char name[24]={};};
// Loaded modules from PEB->Ldr->InLoadOrderModuleList (x86 layout: PEB+0x0C
// Ldr, Ldr+0x0C list head; entry DllBase +0x18, SizeOfImage +0x20,
// BaseDllName +0x2C {Length, MaximumLength, Buffer +0x30}).
inline int modules(SafeRead&read,Module*out,Span*spans,int max){
 uint32_t teb=(uint32_t)(uintptr_t)NtCurrentTeb(),peb=0,ldr=0,link=0;
 if(!read(teb+0x30,&peb,4)||!read(peb+0x0C,&ldr,4))return 0;
 uint32_t head=ldr+0x0C;if(!read(head,&link,4))return 0;
 int n=0;
 for(int guard=0;link&&link!=head&&n<max&&guard<1024;guard++){
  uint32_t e[13];if(!read(link,e,sizeof(e)))break;
  Module&m=out[n];m.base=e[6];m.size=e[8];
  uint32_t chars=(e[11]&0xffff)/2;if(chars>sizeof(m.name)-1)chars=sizeof(m.name)-1;
  wchar_t w[sizeof(m.name)]={};if(!chars||!read(e[12],w,chars*2))chars=0;
  for(uint32_t i=0;i<chars;i++)m.name[i]=w[i]>32&&w[i]<127?(char)w[i]:'?';m.name[chars]=0;
  if(m.base&&m.size){spans[n]={m.base,m.size};n++;}
  link=e[0];
 }
 return n;
}
inline void where(Text&t,uint32_t a,const Module*mods,int count){
 for(int i=0;i<count;i++)if(a-mods[i].base<mods[i].size){t.put(mods[i].name[0]?mods[i].name:"module").put("+0x").hex(a-mods[i].base,0);return;}
 t.put("0x").hex(a);
}
inline void frames(Text&t,const uint32_t*f,int n,const Module*mods,int count){for(int i=0;i<n;i++){if(i)t.ch(' ');where(t,f[i],mods,count);}}

// Main thread, learned from the first tick (ticks always run on it).
inline std::atomic<DWORD> mainThreadId{0};inline HANDLE mainThread=nullptr;inline std::atomic<uint32_t> mainStackTop{0};
inline std::atomic<uint64_t> beatAt{0};inline std::atomic<bool> beatIngame{false};
// Called by the tick (ingame=true), frame and map-init callbacks; only beats on
// the main thread count. Lock-free.
inline void beat(bool ingame,bool tick){
 DWORD id=GetCurrentThreadId(),main=mainThreadId.load(std::memory_order_acquire);
 if(!main){
  if(!tick)return;HANDLE h=nullptr;
  if(!DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&h,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_LIMITED_INFORMATION,FALSE,0))return;
  mainThread=h;mainStackTop=(uint32_t)(uintptr_t)((NT_TIB*)NtCurrentTeb())->StackBase;mainThreadId.store(id,std::memory_order_release);main=id;
 }
 if(id!=main)return;
 beatIngame.store(ingame,std::memory_order_relaxed);beatAt.store(GetTickCount64(),std::memory_order_release);
}

// Exception logging (vectored handler, first chance). Fatal codes: up to 8,
// each with a stack. Other faults: up to 32 lines as before, a stack for each
// new address (up to 16). One thread at a time; buffers are static because a
// stack overflow leaves little stack.
inline std::atomic<bool> inHandler{false};inline std::atomic<int> faultLines{0},fatalLines{0};
inline uint32_t faultSeen[16];inline int faultSeenCount=0;
inline SafeRead exRead;inline Module exMods[256];inline Span exSpans[256];inline uint32_t exFrames[24];inline Text exLine,exStack;
inline void exception(EXCEPTION_POINTERS*p){
 const EXCEPTION_RECORD*r=p->ExceptionRecord;uint32_t code=r->ExceptionCode;bool fatal=fatalCode(code);
 if(!fatal&&!faultCode(code))return;
 if(fatal?fatalLines.load()>=8:faultLines.load()>=32)return;
 if(inHandler.exchange(true))return;
 (fatal?fatalLines:faultLines)++;
 uint32_t at=(uint32_t)(uintptr_t)r->ExceptionAddress;DWORD thread=GetCurrentThreadId();bool onMain=thread==mainThreadId.load();
 const char*operation=operationSource?operationSource->load():"idle";
 exRead.reset();int count=modules(exRead,exMods,exSpans,256);
 Text&t=exLine;t.clear();
 if(fatal)t.put("FATAL_EXCEPTION code=").hex(code).put(" (").put(codeName(code)).put(") address=").hex(at);
 else t.put("FIRST_CHANCE code=").hex(code).put(" address=").hex(at).put(" gameRva=").hex(at-(uint32_t)image).put(" operation=").put(operation).put(" (").put(codeName(code)).put(")"); // prefix unchanged since 0.2.x
 t.put(" at=");where(t,at,exMods,count);t.put(" thread=").dec(thread).put(onMain?" main":"");
 if(code==0xC0000005u&&r->NumberParameters>=2){uint32_t k=(uint32_t)r->ExceptionInformation[0];t.put(" access=").put(k==0?"read":k==1?"write":k==8?"execute":"other").put(" target=").hex((uint32_t)r->ExceptionInformation[1]);}
 if(fatal)t.put(" operation=").put(operation);
 write(t);
 bool fresh=fatal;
 if(!fatal){fresh=true;for(int i=0;i<faultSeenCount;i++)if(faultSeen[i]==at)fresh=false;if(fresh&&faultSeenCount<16)faultSeen[faultSeenCount++]=at;else fresh=false;}
 if(fresh){
  int n=0;const char*source="scan";
  if(code==0xC0000374u&&r->NumberParameters>=1){
   uint32_t info=(uint32_t)r->ExceptionInformation[0],h[5];
   if(exRead(info,h,sizeof(h))&&h[0]>=1&&h[0]<=16&&h[1]>=0xB8&&h[1]<=0x1000){
    Text&f=exStack;f.clear();f.put("HEAP_FAILURE type=").dec(h[2]).put(" (").put(heapFailure(h[2])).put(") heap=").hex(h[3]).put(" block=").hex(h[4]);write(f);
    for(int i=0;i<24;i++){uint32_t v=0;if(!exRead(info+0x38+4*i,&v,4)||!inside(exSpans,count,v))break;exFrames[n++]=v;}
    if(n)source="heap-failure";
   }
  }
  if(!n)n=scanStack(exRead,at,(uint32_t)p->ContextRecord->Esp,(uint32_t)(uintptr_t)((NT_TIB*)NtCurrentTeb())->StackBase,exSpans,count,exFrames,24);
  Text&s=exStack;s.clear();s.put("STACK source=").put(source).put(" frames=");frames(s,exFrames,n,exMods,count);write(s);
 }
 inHandler=false;
}

// Process end: ExitProcess runs this; TerminateProcess (Task Manager, a
// crash handler) and fail-fast crashes do not.
inline void processExit(bool ending){
 if(logFile==INVALID_HANDLE_VALUE)return;
 Text t;t.put("PROCESS_EXIT ").put(ending?"process ending":"plugin unloaded");
 uint64_t b=beatAt.load();if(b)t.put(" lastBeatMsAgo=").dec(GetTickCount64()-b).put(beatIngame.load()?" inMap":" notInMap");
 write(t);
}

// Freeze dump: a process snapshot (VA clone, PSS_CREATE_USE_VM_ALLOCATIONS)
// written by dbghelp from the clone, so no game thread is suspended while
// dbghelp works. Best effort: if the hung thread holds the heap lock this
// may never finish; the FREEZE lines are written before it starts.
using PssCapture=DWORD(WINAPI*)(HANDLE,DWORD,DWORD,HANDLE*);using PssFree=DWORD(WINAPI*)(HANDLE,HANDLE);
struct DumpInput {ULONG processId;HANDLE process;ULONG type;};
struct DumpCallback {void*routine;void*param;};
using WriteDump=BOOL(WINAPI*)(HANDLE,DWORD,HANDLE,DWORD,void*,void*,const DumpCallback*);
inline PssCapture pssCapture=nullptr;inline PssFree pssFree=nullptr;inline WriteDump writeDump=nullptr;
inline BOOL CALLBACK dumpCallback(void*,const DumpInput*in,HRESULT*out){if(in&&out&&in->type==16)*out=S_FALSE;return TRUE;} // 16 = IsProcessSnapshotCallback
inline void dump(int index){
 Text t;
 if(!pssCapture||!pssFree||!writeDump){t.put("FREEZE_DUMP unavailable (no process snapshot or dbghelp)");write(t);return;}
 Text path;path.put(dumpDir).put("\\freeze-").dec(GetCurrentProcessId()).ch('-').dec((uint64_t)index).put(".dmp");
 HANDLE snapshot=nullptr;
 DWORD err=pssCapture(GetCurrentProcess(),0x00000001|0x00000080|0x00000100|0x20000000,CONTEXT_ALL,&snapshot); // VA clone, threads, thread contexts, VM allocations
 if(err){t.put("FREEZE_DUMP snapshot failed error=").dec(err);write(t);return;}
 HANDLE file=CreateFileA(path.s,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE){err=GetLastError();pssFree(GetCurrentProcess(),snapshot);t.put("FREEZE_DUMP file failed error=").dec(err);write(t);return;}
 DumpCallback cb{(void*)dumpCallback,nullptr};
 BOOL ok=writeDump(snapshot,GetCurrentProcessId(),file,0x1|0x20|0x40|0x1000,nullptr,nullptr,&cb); // data segments, unloaded modules, referenced memory, thread info
 err=ok?0:GetLastError();DWORD size=GetFileSize(file,nullptr);CloseHandle(file);pssFree(GetCurrentProcess(),snapshot);
 if(ok)t.put("FREEZE_DUMP written ").put(path.s).put(" bytes=").dec(size);else t.put("FREEZE_DUMP failed error=").dec(err);
 write(t);
}

// Freeze sample: suspend the main thread only to read its registers and
// stack (syscalls and plain reads, nothing that can wait on the game), resume,
// then format and log.
inline SafeRead wdRead;inline Module wdMods[256];inline Span wdSpans[256];inline uint32_t wdFrames[24];
inline void sample(const char*tag,uint64_t silent,bool known,bool hung,bool foreground){
 wdRead.reset();int count=modules(wdRead,wdMods,wdSpans,256);
 CONTEXT c{};c.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;int n=0;bool got=false;
 if(mainThread&&SuspendThread(mainThread)!=(DWORD)-1){
  if(GetThreadContext(mainThread,&c)){got=true;wdRead.reset();n=scanStack(wdRead,c.Eip,c.Esp,mainStackTop.load(),wdSpans,count,wdFrames,24);}
  ResumeThread(mainThread);
 }
 uint32_t gameTick=0;if(image)wdRead(image+0xe66b14,&gameTick,4);
 Text t;t.put(tag).put(" main thread silent for ").dec(silent).put(" ms gameTick=").dec(gameTick).put(" window=").put(known?(hung?"not-responding":"responding"):"unknown").put(foreground?" foreground":" background");
 const char*operation=operationSource?operationSource->load():"idle";t.put(" operation=").put(operation);
 if(got){t.put(" eip=");where(t,c.Eip,wdMods,count);}else t.put(" context=unavailable");
 write(t);
 if(got){Text s;s.put("STACK source=main-thread frames=");frames(s,wdFrames,n,wdMods,count);write(s);}
}
struct BestWindow {HWND h=nullptr;LONG area=0;};
inline BOOL CALLBACK pickWindow(HWND h,LPARAM p){auto*b=(BestWindow*)p;RECT r{};if(!IsWindowVisible(h)||GetWindow(h,GW_OWNER)||!GetWindowRect(h,&r))return TRUE;LONG a=(r.right-r.left)*(r.bottom-r.top);if(a>b->area){b->area=a;b->h=h;}return TRUE;}
inline DWORD WINAPI watchdog(void*){
 Watch w;int dumps=0;HWND window=nullptr;uint64_t windowAt=0;bool announced=false;
 for(;;){
  Sleep(1000);
  DWORD main=mainThreadId.load(std::memory_order_acquire);if(!main)continue;
  uint64_t now=GetTickCount64(),last=beatAt.load(std::memory_order_acquire);bool ingame=beatIngame.load(std::memory_order_relaxed);
  // Window lookup allocates, so it only runs while the game is responsive.
  if(last&&now>=last&&now-last<3000&&now-windowAt>=5000){BestWindow b;EnumThreadWindows(main,pickWindow,(LPARAM)&b);window=b.h;windowAt=now;
   if(!announced){announced=true;Text t;t.put("FREEZE_WATCH ready main thread ").dec(main).put(window?" window found":" window not found").put("; reports after ").dec(FreezeMs/1000).put(" s without ticks or frames");write(t);}}
  bool known=window&&IsWindow(window),hung=known&&IsHungAppWindow(window);
  HWND fg=GetForegroundWindow();DWORD fgPid=0;if(fg)GetWindowThreadProcessId(fg,&fgPid);bool foreground=fgPid==GetCurrentProcessId();
  Event e=step(w,now,last,ingame,known,hung,foreground);
  if(e==Event::None)continue;
  if(e==Event::Recovered){Text t;t.put("FREEZE_END main thread resumed after ").dec(w.lastStall).put(" ms");write(t);continue;}
  sample(e==Event::Freeze?"FREEZE":"FREEZE_SAMPLE",now-last,known,hung,foreground);
  if(wantDump(w,e,hung,dumps)){w.dumped=true;dump(++dumps);}
 }
}

// Startup (DLL start thread): opens the shared log for appending, resolves the
// dump functions from system DLLs and starts the watchdog. Returns false when
// the log cannot be opened (diagnostics then stay silent).
inline bool start(const char*root,uintptr_t gameImage,const std::atomic<uint32_t>*tick,const std::atomic<const char*>*operation,bool&dumps){
 image=gameImage;tickSource=tick;operationSource=operation;
 Text dir;dir.put(root).put("\\evidence");memcpy(dumpDir,dir.s,dir.n+1<sizeof(dumpDir)?dir.n+1:sizeof(dumpDir)-1);
 Text path;path.put(dir.s).put("\\v02-runtime.log");
 logFile=CreateFileA(path.s,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 HMODULE kernel=GetModuleHandleW(L"kernel32.dll");
 if(kernel){pssCapture=(PssCapture)GetProcAddress(kernel,"PssCaptureSnapshot");pssFree=(PssFree)GetProcAddress(kernel,"PssFreeSnapshot");}
 HMODULE help=LoadLibraryExW(L"dbghelp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(help)writeDump=(WriteDump)GetProcAddress(help,"MiniDumpWriteDump");
 dumps=pssCapture&&pssFree&&writeDump;
 if(logFile==INVALID_HANDLE_VALUE)return false;
 HANDLE h=CreateThread(nullptr,0,watchdog,nullptr,0,nullptr);if(h)CloseHandle(h); // default stack: dbghelp needs room
 return h!=nullptr;
}
}
