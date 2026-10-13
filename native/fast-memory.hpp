#pragma once
#include <windows.h>
#include <stdint.h>
// Readability checks used to issue one VirtualQuery syscall per read; whole
// entity-table scans made tens of thousands per tick and stalled the game.
// Within one game tick (Scope on the tick thread only), remember the regions
// already queried. invalidate() after any native call that may free memory.
// Other threads (render/mouse callbacks) always query directly.
namespace dtmem {
struct Region {uintptr_t base=0,end=0;bool ok=false,write=false;};
inline Region cache[32];inline unsigned next=0;inline DWORD owner=0;inline bool active=false;
inline bool cached(){return active&&GetCurrentThreadId()==owner;}
inline void invalidate(){if(!cached())return;for(auto&r:cache)r={};next=0;}
inline bool region(uintptr_t p,size_t n,Region&out){
 if(!p||!n||p+n<p)return false;
 bool use=cached();
 if(use)for(auto&r:cache)if(r.end&&p>=r.base&&p<r.end){out=r;return n<=r.end-p;}
 MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery((void*)p,&m,sizeof(m)))return false;
 bool ok=m.State==MEM_COMMIT&&!(m.Protect&(PAGE_GUARD|PAGE_NOACCESS));
 Region r{(uintptr_t)m.BaseAddress,(uintptr_t)m.BaseAddress+m.RegionSize,ok,ok&&(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0};
 if(use&&r.end>r.base){cache[next++%32]=r;}
 out=r;return p>=r.base&&p<r.end&&n<=r.end-p;
}
inline bool readable(uintptr_t p,size_t n){Region r;return region(p,n,r)&&r.ok;}
inline bool writable(uintptr_t p,size_t n){Region r;return region(p,n,r)&&r.write;}
struct Scope {
 Scope(){owner=GetCurrentThreadId();active=true;for(auto&r:cache)r={};next=0;}
 ~Scope(){for(auto&r:cache)r={};next=0;active=false;owner=0;}
};
}
