// Disposable non-game x86 process for tools/test-diagnostics.py: exercises the
// crash and freeze diagnostics (diagnostics.hpp) end to end.
//   diagnostics-fixture <dir> fault|heap|overflow|freeze
// Writes <dir>\evidence\v02-runtime.log (and a freeze dump) like the plugin.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diagnostics.hpp"
static std::atomic<uint32_t> tick{0};static std::atomic<const char*> operation{"fixture"};
static LONG WINAPI first(EXCEPTION_POINTERS*p){dtdiag::exception(p);return EXCEPTION_CONTINUE_SEARCH;}
// Runs after the logger: resumes the fixture's own RaiseException calls and
// ends the process for fatal codes before Windows Error Reporting starts.
static LONG WINAPI settle(EXCEPTION_POINTERS*p){
 DWORD c=p->ExceptionRecord->ExceptionCode;
 if(c==0xC0000374||c==0xC00000FD)TerminateProcess(GetCurrentProcess(),c&0xffff);
 if(c==0xC0000005||c==0xE06D7363)return EXCEPTION_CONTINUE_EXECUTION;
 return EXCEPTION_CONTINUE_SEARCH;
}
static void note(const char*name,const void*fn){dtdiag::Text t;t.put("FIXTURE ").put(name).put("=0x").hex((uint32_t)((uintptr_t)fn-(uintptr_t)GetModuleHandleW(nullptr)),0);dtdiag::write(t);}
__attribute__((noinline)) static void hangHere(uint64_t ms){
 uint64_t start=GetTickCount64();
 for(;;){for(volatile int i=0;i<2000000;i=i+1){}if(GetTickCount64()-start>=ms)break;}
}
static volatile int depthLimit=0x7fffffff; // never reached: the stack overflows first
__attribute__((noinline)) static int recurse(int depth){volatile char pad[4096];pad[0]=(char)depth;pad[4095]=pad[0];if(depth>=depthLimit)return 0;return recurse(depth+1)+pad[1];}
static void pump(uint64_t ms){
 uint64_t until=GetTickCount64()+ms;
 while(GetTickCount64()<until){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}tick++;dtdiag::beat(true,true);Sleep(50);}
}
int main(int argc,char**argv){
 if(argc<3)return 2;
 char evidence[MAX_PATH];snprintf(evidence,sizeof(evidence),"%s\\evidence",argv[1]);CreateDirectoryA(argv[1],nullptr);CreateDirectoryA(evidence,nullptr);
 bool dumps=false;if(!dtdiag::start(argv[1],(uintptr_t)GetModuleHandleW(nullptr),&tick,&operation,dumps))return 3;
 AddVectoredExceptionHandler(1,first);AddVectoredExceptionHandler(0,settle);
 atexit([]{dtdiag::processExit(true);}); // the plugin uses DllMain process detach
 dtdiag::beat(true,true);
 const char*mode=argv[2];
 if(!strcmp(mode,"fault")){
  ULONG_PTR info[2]={0,0x1234};RaiseException(0xC0000005,0,2,info);RaiseException(0xC0000005,0,2,info);
  RaiseException(0xE06D7363,0,0,nullptr);
  return 0;
 }
 if(!strcmp(mode,"heap")){
  HeapSetInformation(nullptr,HeapEnableTerminationOnCorruption,nullptr,0);
  HANDLE heap=HeapCreate(0,0,0);void*block=HeapAlloc(heap,0,64);HeapFree(heap,0,block);HeapFree(heap,0,block);
  return 4; // not reached when the double free is detected
 }
 if(!strcmp(mode,"overflow")){note("recurse",(const void*)&recurse);return recurse(0)==7?5:6;}
 if(!strcmp(mode,"freeze")){
  WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"DarkTribeDiagnosticsFixture";RegisterClassW(&wc);
  HWND w=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"Dark Tribe diagnostics fixture",WS_POPUP,-12000,-12000,400,300,nullptr,nullptr,wc.hInstance,nullptr);
  if(!w)return 7;ShowWindow(w,SW_SHOWNOACTIVATE);
  note("hangHere",(const void*)&hangHere);
  pump(2500);hangHere(13500);pump(2500);
  DestroyWindow(w);return 0;
 }
 return 2;
}
