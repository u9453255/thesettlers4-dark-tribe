#include "fast-memory.hpp"
#include <assert.h>
#include <stdio.h>
static uintptr_t page=0;static bool seen=false;
static DWORD WINAPI other(void*){seen=dtmem::readable(page,4);return 0;}
int main(){
 SYSTEM_INFO info;GetSystemInfo(&info);size_t size=info.dwPageSize;
 auto*base=(BYTE*)VirtualAlloc(nullptr,size*3,MEM_RESERVE,PAGE_NOACCESS);assert(base);
 page=(uintptr_t)base;uintptr_t ro=(uintptr_t)base+size*2;
 assert(VirtualAlloc(base,size,MEM_COMMIT,PAGE_READWRITE)&&VirtualAlloc((void*)ro,size,MEM_COMMIT,PAGE_READONLY));
 // Uncached behaviour: committed, reserved-only, read-only and range ends.
 assert(dtmem::readable(page,4)&&dtmem::writable(page,4)&&!dtmem::readable(page+size,4));
 assert(dtmem::readable(ro,4)&&!dtmem::writable(ro,4)&&!dtmem::readable(page+size-2,4)&&!dtmem::readable(0,4)&&!dtmem::readable(page,0));
 {
  dtmem::Scope scope;
  assert(dtmem::readable(page,4)&&dtmem::next==1);
  for(int i=0;i<1000;i++)assert(dtmem::readable(page+i,4));
  assert(dtmem::next==1); // One syscall for the whole region during the tick.
  assert(!dtmem::readable(page+size,4)&&!dtmem::writable(ro,4)&&dtmem::readable(ro,8));
  // Another thread never uses the tick cache.
  VirtualFree(base,size,MEM_DECOMMIT);
  HANDLE h=CreateThread(nullptr,0,other,nullptr,0,nullptr);WaitForSingleObject(h,INFINITE);CloseHandle(h);assert(!seen);
  // After a native call that may free memory the cache must be invalidated.
  dtmem::invalidate();assert(!dtmem::readable(page,4));
  assert(VirtualAlloc(base,size,MEM_COMMIT,PAGE_READWRITE));dtmem::invalidate();assert(dtmem::readable(page,4));
  // Throughput: a whole entity-table style scan stays cheap within the tick.
  LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
  for(int i=0;i<65536*8;i++)dtmem::readable(page+(i&255),4);
  QueryPerformanceCounter(&b);double cached=(b.QuadPart-a.QuadPart)*1000.0/f.QuadPart;
  printf("cached 524288 checks: %.2f ms\n",cached);assert(cached<100.0);
 }
 assert(!dtmem::active&&dtmem::next==0);
 LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
 for(int i=0;i<65536;i++)dtmem::readable(page+(i&255),4);
 QueryPerformanceCounter(&b);printf("uncached 65536 checks: %.2f ms\n",(b.QuadPart-a.QuadPart)*1000.0/f.QuadPart);
 VirtualFree(base,0,MEM_RELEASE);
 puts("PASS: per-tick read cache, single query per region, invalidation after native mutation, thread isolation, write protection and scan cost");
}
