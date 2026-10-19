#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "fast-memory.hpp"
namespace dtmap {
// Side length of the running map. RVA 0x1054138 is the row stride of the landscape (0x11630D4),
// flag (0x11630D8) and ground-entity (0x11630DC) arrays as the native Dark validator (0x13EBC0)
// and its neighbourhood pass (0x13DC90) index them; the view setup (0x114B90 -> 0x2589D0) copies
// it to 0xD6921C next to the view's landscape pointer 0xD69220 (tools/inspect-placement.py).
// Both must agree and be the size of a supported scenario; anything else reads as 0 (no map).
constexpr uint32_t Sizes[]={512,576};
constexpr int MaxSize=576;
inline bool supported(uint32_t n){for(auto s:Sizes)if(n==s)return true;return false;}
inline int size(uintptr_t image){
 uint32_t stride=0,view=0;if(!image||!dtmem::readable(image+0x1054138,4)||!dtmem::readable(image+0xd6921c,4))return 0;
 memcpy(&stride,(void*)(image+0x1054138),4);memcpy(&view,(void*)(image+0xd6921c),4);
 return stride==view&&supported(stride)?(int)stride:0;
}
}
