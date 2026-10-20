#pragma once
#include <stdint.h>
#include <string.h>
// r34 site selection crash fix (pure logic, tested by construction-tests.cpp).
// The native site panel (CBuildingSiteRole slot 22, RVA 0x143D10) reads the
// ordinary economy at the site entrance (AllEcoSectors[id], null on Dark land)
// for the building info's inhabitant (+0x1DE), tool (+0x1DF) and for each
// material pile (+0x1B0/2/4). A site can be clicked in the same input batch
// that created it, before any plugin tick sees it (field crash pid 8768:
// 0x143E07 on an unpaid Farm, inhabitant 53). The plugin's slot-22 wrapper
// therefore decides per call, from what the native panel itself would read:
//  - Native:   an economy exists, or nothing the panel would read needs one;
//  - SafeView: no economy but a field needs one: show a transient view (info
//              copy with +0x1DE/+0x1DF cleared, piles 0), restored after the call;
//  - Skip:     a read the native panel makes would fault, or the view cannot
//              be written/is already in use: the panel refresh is dropped.
namespace dt {
constexpr unsigned BuildingInfoSize=0x32c,InfoInhabitant=0x1de,InfoTool=0x1df;
enum class PanelCall {Native,SafeView,Skip};
struct PanelSite {
 bool readable=false,economy=false,writable=false,busy=false;
 int8_t inhabitant=0,tool=0;uint16_t piles[3]{};
};
inline bool panelNeedsEconomy(const PanelSite&s){return s.inhabitant||s.tool||s.piles[0]||s.piles[1]||s.piles[2];}
inline PanelCall panelCall(const PanelSite&s){
 if(!s.readable)return PanelCall::Skip;
 if(s.economy||!panelNeedsEconomy(s))return PanelCall::Native;
 if(!s.writable||s.busy)return PanelCall::Skip;
 return PanelCall::SafeView;
}
// The panel-safe info: the site's own building info with inhabitant and tool cleared.
inline void panelSafeInfo(uint8_t*out,const uint8_t*info){memcpy(out,info,BuildingInfoSize);out[InfoInhabitant]=0;out[InfoTool]=0;}
// Transient view around one panel call: the role's info pointer and pile ids
// are replaced for the call only and restored exactly afterwards.
template<class Call> void withPanelView(uintptr_t*info,uint16_t*piles,uintptr_t safe,Call call){
 uintptr_t savedInfo=*info;uint16_t saved[3];memcpy(saved,piles,sizeof(saved));
 *info=safe;memset(piles,0,sizeof(saved));
 call();
 *info=savedInfo;memcpy(piles,saved,sizeof(saved));
}
}
