// Original Dark Tribe scenario plugin. S4ModApi interface header: MIT, nyfrk.
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <atomic>
#include "S4ModApi.h"
#include "economy.hpp"
#include "queue-model.hpp"
#include "panel.hpp"
#include "hd-overlay.hpp"
#include "file-hash.hpp"
#include "aircraft-model.hpp"
#include "interaction-model.hpp"
#include "fast-memory.hpp"
#include "placement-model.hpp"
#include "placement-native.hpp"
#include "construction-model.hpp"
#include "construction-native.hpp"
#include "offence-native.hpp"
#include "scenario-model.hpp"
#include "map-size.hpp"
#include "footprints.hpp"
#include "diagnostics.hpp"
static S4API api=nullptr;static uintptr_t image=0;static HMODULE ownModule=0;
static SRWLOCK stateLock=SRWLOCK_INIT,commandLock=SRWLOCK_INIT,ioLock=SRWLOCK_INIT;
using HDBool=BOOL(__stdcall*)();static HDBool hdIsActive=nullptr,hdMouseBlocked=nullptr;
static std::atomic<bool> hdActive(false),hdBlocked(false),sceneMatches(false);
static std::atomic<int> hdPanelStatus(0);
static dt::State state{};static std::atomic<bool> running(false),fatal(false);static bool initialized=false;
static dtplacement::Hints placementHints;static dtconstruction::Native nativeConstruction;
static thread_local bool incomeBypass=false;static bool newMap=true;static std::atomic<int> pillar(0);
static std::atomic<const char*> operation("idle");static char root[MAX_PATH]={},notice[160]="";static std::atomic<uint32_t> nativeTick(0);
struct Command {int type=0,arg=0;uint16_t entity=0,x=0,y=0,x2=0,y2=0,cw=0,ch=0;uint32_t identity=0;int amount=0; /* RecruitOrder: native order amount. */std::vector<dt::UnitRef> units;Command(int t=0,int a=0,uint16_t e=0,uint16_t px=0,uint16_t py=0,uint32_t uid=0):type(t),arg(a),entity(e),x(px),y(py),identity(uid){} };
static std::vector<Command> commands;
static std::vector<dt::UnitRef> cachedSelection,cachedFleet;
static std::atomic<uint32_t> spriteFrame(0);static SRWLOCK spriteLock=SRWLOCK_INIT;static std::vector<dt::SpriteHit> spriteHits;
struct Boarding {dt::UnitRef craft,unit;uint32_t expires=0,lastOrder=0;};
static std::vector<Boarding> boarding;
using View=dtui::Data;
// Recruitment unit chosen in the Temple/Fortress panel. UI state: written by
// panel input (mouse/HD threads) and bound to the building it was chosen on,
// packed as identity<<32 | entity<<8 | row. The tick clears it when another
// building is selected; frames and input apply it without a tick round-trip.
static std::atomic<uint64_t> recruitChoice(0);
static uint8_t recruitPick(const dtui::Data&v){uint64_t c=recruitChoice.load();return v.selected&&(uint16_t)(c>>8)==v.selected&&(uint32_t)(c>>32)==v.selectedIdentity?(uint8_t)(c&3):0;}
static void chooseRecruit(const dtui::Data&v,int row){recruitChoice.store(((uint64_t)v.selectedIdentity<<32)|((uint64_t)v.selected<<8)|(uint64_t)(row&3));}
static View view{};static std::atomic<bool> buildOverride(false),ordinarySelectionPending(false);static uint16_t lastSelection=0,chosenAircraft=0;static uint32_t chosenIdentity=0;static bool destinationControls=false;static int draftX=0,draftY=0;static uint32_t draftIdentity=0;
// Aircraft group chosen with clicks/boxes (native selection cannot hold Manacopters).
// Tick-owned; cachedPick is the copy read by input callbacks under the shared lock.
static std::vector<dt::UnitRef> fleetPick,cachedPick;static std::atomic<int> pressX(-1),pressY(-1);
// Unload targeting: Unload arms it, the next map left-click picks the spot.
// Jobs fly each aircraft to the nearest valid spot and unload once landed.
struct UnloadJob {dt::UnitRef craft;int x=0,y=0;uint32_t expires=0,landedSince=0,retries=0;};
static std::vector<UnloadJob> unloadJobs;static std::vector<dt::UnitRef> unloadGroup;static std::atomic<bool> unloadTargeting(false);static std::atomic<int> mouseX(-1),mouseY(-1);
using LuaBlock=void(__cdecl*)();using LuaGlobal=uint32_t(__cdecl*)(const char*);using LuaNumber=double(__cdecl*)(uint32_t);
static LuaBlock luaBegin=nullptr,luaEnd=nullptr;static LuaGlobal luaGlobal=nullptr;static LuaNumber luaNumber=nullptr;
// Lua access occurs exclusively in the game tick callback, never in rendering/input.
static double luaConstant(const char*name){luaBegin();double value=luaNumber(luaGlobal(name));luaEnd();return value;}
static bool scenarioVersion(){return luaConstant("DT_Version")==200;}

static bool readable(const void*p,size_t n){return dtmem::readable((uintptr_t)p,n);}
template<class T> static T peek(uintptr_t a){T v{};if(readable((void*)a,sizeof(v)))memcpy(&v,(void*)a,sizeof(v));return v;}
static BYTE* entity(uint16_t id){BYTE*p=image?((BYTE**)(image+0xe9bc38))[id]:nullptr;return p&&readable(p,92)&&peek<uint16_t>((uintptr_t)p+8)==id?p:nullptr;}
static uint32_t identity(uint16_t id){auto*p=entity(id);return p?peek<uint32_t>((uintptr_t)p+4):0;}
static uint16_t kind(uint16_t id){auto*p=entity(id);return p?peek<uint16_t>((uintptr_t)p+12):0;}
static bool owned(uint16_t id){auto*p=entity(id);return p&&p[32]==0x13&&p[33]>0;}
static BYTE* role(uint16_t id){auto*p=entity(id);return p?peek<BYTE*>((uintptr_t)p+(p[10]==8?80:88)):nullptr;}
static const char* rtti(uintptr_t vt){uintptr_t col=peek<uintptr_t>(vt-4),type=peek<uintptr_t>(col+12);auto*p=(char*)(type+8);return readable(p,96)?p:"";}
static bool isSite(uint16_t id){auto*r=role(id);auto n=rtti(peek<uintptr_t>((uintptr_t)r));return strstr(n,"BuildingSiteRole")||strstr(n,"BuildUpRole");}
static bool aircraft(uint16_t id){auto*p=entity(id);return p&&p[10]==0x40&&readable(p,0x88)&&owned(id)&&strstr(rtti(peek<uintptr_t>((uintptr_t)p)),"CManakopter@@");}
static uint16_t selected(){uintptr_t a=peek<uintptr_t>(image+0x10865a4),b=peek<uintptr_t>(image+0x10865a8);return b==a+2?peek<uint16_t>(a):0;}
static uint32_t mana(){return peek<uint32_t>(image+0x109b7fc+0x8c)^0x27356334;}
// Side length of the running map (0 without a supported map); see map-size.hpp.
static int side(){return dtmap::size(image);}
// Accepted scenario layouts (including the legacy First Dawn save layout) are in scenario-model.hpp.
static void log(const char*s);
// Scene fingerprint; each failed part sets one bit so transitions can be logged.
static unsigned signatureMask(){if(!image)return 1;uintptr_t p=peek<uintptr_t>(image+0xe968a0);unsigned m=0;
 if(peek<uint32_t>(image+0xe9482c)!=1)m|=2;if(!p||!readable((void*)p,0x33c)){m|=8;return m;}
 /* One readable check for the whole setup block, then plain copies of its per-player arrays. */
 dt::Setup s;s.size=peek<uint32_t>(image+0xd6921c);memcpy(&s.players,(void*)(p+0x338),4);
 memcpy(s.slot,(void*)(p+0x90),sizeof(s.slot));memcpy(s.race,(void*)(p+0x18c),sizeof(s.race));memcpy(s.x,(void*)(p+0xfc),sizeof(s.x));memcpy(s.y,(void*)(p+0x120),sizeof(s.y));
 m|=dt::sceneMask(s);if(!api->IsCurrentlyOnScreen(S4_SCREEN_INGAME))m|=512;return m;}
static std::atomic<unsigned> lastSceneMask(0xffffffff);
static bool signature(){unsigned m=signatureMask();if(m!=lastSceneMask.exchange(m)&&nativeTick.load()){char line[120];snprintf(line,sizeof(line),"SCENE_STATE mask=%03x (2 player,4 map,8 data,16 players,32 slots,64 races,128 start,256 opponent,512 not-ingame)",m);log(line);}return m==0;}
// The game's own screen state ("in a running map"), refreshed by the frame
// callback (every frame, menus included) and by ticks. Ticks stop in the menus,
// so tick-cached scene state alone stays stale after leaving a map; frames do not.
static std::atomic<bool> frameIngame{false};static std::atomic<uint64_t> frameAt{0},tickAt{0};
static bool inGame(){uint64_t now=GetTickCount64(),f=frameAt.load(),t=tickAt.load();
 if(f&&now-f<1000)return frameIngame.load(); /* frames are delivered: they know the current screen */
 return t&&now-t<3000; /* no frames (renderer without the hook): only while ticks run */}
static std::atomic<uint64_t> noticeAt{0};constexpr uint64_t NoticeMs=6000;
static void say(const char*s){strncpy(notice,s,sizeof(notice)-1);notice[sizeof(notice)-1]=0;noticeAt=s[0]?GetTickCount64():0;} /* failures only; shown for NoticeMs */
static void log(const char*s){char path[MAX_PATH];snprintf(path,sizeof(path),"%s/evidence/v02-runtime.log",root);FILE*f=fopen(path,"a");if(f){fprintf(f,"%lu pid=%lu tick=%u %s\n",GetTickCount(),GetCurrentProcessId(),nativeTick.load(),s);fclose(f);}}
// First-chance faults and fatal exceptions with a stack; heap-free (diagnostics.hpp).
static LONG WINAPI exceptionTrace(EXCEPTION_POINTERS*p){dtdiag::exception(p);return EXCEPTION_CONTINUE_SEARCH;}
static bool changeMana(int amount){uint32_t before=mana(),after=0;if(!dt::manaTarget(before,amount,after))return false;incomeBypass=true;BOOL ok=api->MagicIncreaseMana(amount,1);incomeBypass=false;if(!ok||mana()!=after){fatal=true;running=false;log("Native mana transaction failed; economy disabled");return false;}return true;}
static bool enqueue(Command c){if(!TryAcquireSRWLockExclusive(&commandLock))return false;bool ok=commands.size()<128;if(ok)commands.push_back(std::move(c));ReleaseSRWLockExclusive(&commandLock);return ok;}
static bool constructionMenu(){return api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_FOUNDATION)||api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_METAL)||api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_FOOD)||api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_MISC)||api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_MISC_DECOSUB)||api->IsCurrentlyOnScreen(S4_MENU_BUILDINGS_MILITARY);}
// Suppress only synthetic human Dark catch-up credits; native farm income is untouched.
using AddMana=uint32_t(__fastcall*)(int,int);static AddMana originalIncome=nullptr;
// This entry is AI catch-up mana, not mushroom production. The human Dark
// economy must not mint catch-up resources when its mana bank is empty.
static uint32_t __fastcall income(int player,int amount){if(running.load()&&!incomeBypass&&player==1&&amount>0)return peek<uint32_t>(image+0x109b7fc+0x8c);return originalIncome(player,amount);}
// r41 (user: a landed Manacopter healed to full almost instantly, enemies could not kill it): CManakopter's update
// (RVA 0x16B4E0) reschedules a landed aircraft every 10 ticks (0x16B55E) and, when the cell's flag byte has the Dark
// bit 0x04, stores DTManakopterHitpoints (BYTE at RVA 0xDCDB24, 250) into its hit points +0x21 (0x16B58F..0x16B597):
// full health every 10 ticks on Dark land. The user wants no healing at all, so the store (88 47 21 at 0x16B594) is
// replaced by three NOPs at startup; the constructor (0x16AF74) and the damage path (0x16B490) still write +0x21.
// The sequence is pinned (the je over it, the load with the relocated setting address, the store, the jmp after it);
// a page already patched (NOPs in place) passes too.
static bool installNoHealing(){BYTE expected[19]={0x0f,0x84,0x8b,0x01,0x00,0x00,0xa0,0,0,0,0,0x88,0x47,0x21,0xe9,0x7e,0x01,0x00,0x00};uint32_t setting=(uint32_t)(image+0xdcdb24);memcpy(expected+7,&setting,4);
 BYTE*at=(BYTE*)(image+0x16b589);bool patched=at[11]==0x90&&at[12]==0x90&&at[13]==0x90;if(memcmp(at,expected,11)||memcmp(at+14,expected+14,5)||(!patched&&memcmp(at+11,expected+11,3)))return false;if(patched)return true;
 DWORD old=0;if(!VirtualProtect(at+11,3,PAGE_EXECUTE_READWRITE,&old))return false;at[11]=0x90;at[12]=0x90;at[13]=0x90;VirtualProtect(at+11,3,old,&old);FlushInstructionCache(GetCurrentProcess(),at+11,3);return true;}
// r43 (user: Iron Kingdom's computer drafted every carrier into soldiers until nobody transported or built; the
// reserve must scale with the kingdom). The AI's CPeopleManagerTask (0x2566D0) sets its home economy's carrier
// reserve every pass through CEcoManager slot +0x138 (0x51C10: word eco+0x20 = clamp(value, 5, min(999, carriers))) as
// clamp(min(7*pendingTasks + 10, carriers - 10), 5, 80), and the barracks (0x134D5A) drafts a carrier only while
// carriers > busy + reserve; the cap carriers - 10 follows every draft down to 5 (field: reserve 8 -> 6 -> 5).
// The one call site (0x2567B8, call [eax+0x138]) is redirected to reserveHook, which raises the value to
// dtoffence::carrierKeep(completed buildings of the player; table 0x10EF624 + (p*0x53 + type)*8, incremented by
// 0x1362F0 on completion) and calls the native setter. It runs inside CAIMain::Tick on the game thread: heap-free,
// log-free, no plugin state but the per-player values the tick logs as CARRIER_RESERVE.
struct ReserveSeen{int native,keep,set,buildings,room,builders,diggers;};static ReserveSeen reserveSeen[9]={},reserveLogged[9]={};static int gardenerShort[9]={}; /* r47: gardeners the plugin still wants, set by offenceRecruit */
static int kingdomBuildings(int p){if(p<1||p>8)return 0;int n=0;const int32_t*row=(const int32_t*)(image+0x10ef624+(uintptr_t)p*0x298);for(int t=0;t<83;t++){int c=row[t*2];if(c>0&&c<10000&&dtoffence::economyBuilding(t))n+=c;}return n;} /* r45: economy buildings only */
// r47: builders (settler 3) and diggers (2) come from Statistics 0xDABE30 + p*0x1128 + 4 + 4*type, the carrier total from
// the economy's sector (CEcoManager +4 = sector index into AllEcoSectors 0x106C8A4, word +0x26). With room the reserve is
// at most carriers - room, and the sector's +0x1E byte (CEcoManager slot +0x13C reads it, +0x140 stores argument - current,
// clamped 0..99; the people manager passes 12 right before the reserve) is raised to the room so the barracks waits.
static int __fastcall reserveHook(BYTE*eco,void*,int value){using SetReserve=int(__thiscall*)(BYTE*,int);using GetHold=int(__thiscall*)(BYTE*);using SetHold=void(__thiscall*)(BYTE*,int);int set=value;int p=eco?*(const int32_t*)(eco+8):0;
 if(running.load()&&p>=1&&p<=8){int b=kingdomBuildings(p);const int32_t*st=(const int32_t*)(image+0xdabe30+(uintptr_t)p*0x1128+4);int builders=st[3],diggers=st[2];if(builders<0||builders>10000)builders=0;if(diggers<0||diggers>10000)diggers=0;
  uint32_t idx=*(const uint32_t*)(eco+4);uintptr_t sector=idx<0x4000?*(const uintptr_t*)(image+0x106c8a4+4*idx):0;int carriers=sector?*(const uint16_t*)(sector+0x26):0;
  int room=dtoffence::specialistRoom(builders,diggers,gardenerShort[p]);set=dtoffence::carrierReserve(value,b,carriers,room);
  if(room>0){int hold=((GetHold)(image+0x51c70))(eco);if(hold<room)((SetHold)(image+0x51c80))(eco,room+hold);}
  reserveSeen[p]=ReserveSeen{value,dtoffence::carrierKeep(b),set,b,room,builders,diggers};}
 return ((SetReserve)(image+0x51c10))(eco,set);}
static bool installReserveHook(){BYTE expected[22]={0x8b,0x43,0x28,0x57,0x8b,0x40,0x24,0x8b,0x48,0x18,0x8b,0x01,0xff,0x90,0x38,0x01,0x00,0x00,0x5f,0x5e,0x5b,0xc3};BYTE setter[11]={0x55,0x8b,0xec,0x8b,0x41,0x04,0x56,0x57,0x8b,0x7d,0x08};BYTE*at=(BYTE*)(image+0x2567ac);
 bool patched=at[12]==0xe8&&at[17]==0x90;if(memcmp(at,expected,12)||memcmp(at+18,expected+18,4)||(!patched&&memcmp(at+12,expected+12,6))||memcmp((void*)(image+0x51c10),setter,sizeof(setter)))return false;if(patched)return true;
 DWORD old=0;if(!VirtualProtect(at+12,6,PAGE_EXECUTE_READWRITE,&old))return false;int32_t rel=(int32_t)((uintptr_t)&reserveHook-(uintptr_t)(at+17));at[12]=0xe8;memcpy(at+13,&rel,4);at[17]=0x90;VirtualProtect(at+12,6,old,&old);FlushInstructionCache(GetCurrentProcess(),at+12,6);return true;}
// r51 (crash fix; field: pids 28292 on r23, 9960 on r44 and 27908 on r50, game traces Trace071/105/113, each about 35 s after
// the game started): the Ubisoft services SDK linked into S4_Main.exe (ubiservices client-sdk; worker thread entry
// 0x434160 -> job runner 0x45F100/0x45FBA0 -> job step 0x453C60) sometimes runs a job whose step sequence holds no step
// method. The step runner 0x468520 then asserts "m_pStepMethod != nullptr || m_pStepMethodEx != nullptr" and, with no
// assert callback installed (dword 0x122A6CC), the handler 0x341660 writes to 0xDEADBAAD and the game dies; without the
// assert it would call the null step (0x468669). Not Dark Tribe code. The runner holds a reference to the job, which is
// Running (3), and 0x453C60 steps it again while it stays Running (0x45426D), so the guard parks it instead: the broken
// branch (0x46854C, reached only by falling through both null checks) calls the job's own state setter 0x4CC550 with
// Waiting (1) (a legal Running -> Waiting transition, validator 0x4F7F20) and leaves through the function's epilogue
// (0x46866B: cookie check, pop esi, ret 4); the job runner then re-queues the job (0x45FBD5) and a later wake parks it
// again. The normal step path is unchanged. The parked job's vtable and a count go to two plugin globals (the patch
// writes them on the services thread; the tick logs UBISERVICES_JOB_PARKED). Pinned: the function's prologue (with the
// relocated cookie), both null checks, the replaced branch (with its relocated message), the epilogue and the setter's
// prologue and store; a page this module already patched passes; anything else fails closed (code 18).
static std::atomic<uint32_t> ubiParked{0},ubiParkedVtable{0};static uint32_t ubiParkedLogged=0;static uint64_t ubiParkedLogAt=0;
static bool installUbiStepGuard(){BYTE*fn=(BYTE*)(image+0x468520);BYTE*setter=(BYTE*)(image+0x4cc550);uint32_t cookie=(uint32_t)(image+0xd62444),text=(uint32_t)(image+0xb20358);
 BYTE head[16]={0x55,0x8b,0xec,0x83,0xec,0x58,0xa1,0,0,0,0,0x33,0xc5,0x89,0x45,0xfc};memcpy(head+7,&cookie,4);
 BYTE decide[28]={0x8b,0x45,0x08,0x56,0x8b,0xf1,0x89,0x45,0xd0,0x83,0x3e,0x00,0x0f,0x85,0xa8,0x00,0x00,0x00,0x83,0x7e,0x08,0x00,0x0f,0x85,0x9e,0x00,0x00,0x00};
 BYTE broken[33]={0x6a,0x13,0x68,0,0,0,0,0x8d,0x4d,0xe4,0xc7,0x45,0xf8,0x0f,0x00,0x00,0x00,0xc7,0x45,0xf4,0x00,0x00,0x00,0x00,0xc6,0x45,0xe4,0x00,0xe8,0xf3,0x58,0xbc,0xff};memcpy(broken+3,&text,4);
 BYTE tail[17]={0x8b,0x4d,0xfc,0x33,0xcd,0x5e,0xe8,0x2f,0x8c,0x4e,0x00,0x8b,0xe5,0x5d,0xc2,0x04,0x00};
 BYTE setHead[35]={0x55,0x8b,0xec,0x81,0xec,0xdc,0x00,0x00,0x00,0xa1,0,0,0,0,0x33,0xc5,0x89,0x45,0xfc,0x56,0x57,0x8b,0x7d,0x08,0x8b,0xf1,0x57,0xff,0x76,0x0c,0xe8,0xad,0xb9,0x02,0x00};memcpy(setHead+10,&cookie,4);
 BYTE setTail[21]={0x8b,0x4d,0xfc,0x89,0x7e,0x0c,0x33,0xcd,0x5f,0x5e,0xe8,0x7e,0x4b,0x48,0x00,0x8b,0xe5,0x5d,0xc2,0x04,0x00};
 /* mov ecx,[ebp+8] (the job); mov eax,[ecx]; mov [&ubiParkedVtable],eax; lock inc dword [&ubiParked]; push 1 (Waiting); call 0x4CC550; jmp 0x46866B; 4 x nop */
 BYTE patch[33]={0x8b,0x4d,0x08,0x8b,0x01,0xa3,0,0,0,0,0xf0,0xff,0x05,0,0,0,0,0x6a,0x01,0xe8,0xec,0x3f,0x06,0x00,0xe9,0x02,0x01,0x00,0x00,0x90,0x90,0x90,0x90};
 uint32_t vt=(uint32_t)(uintptr_t)&ubiParkedVtable,count=(uint32_t)(uintptr_t)&ubiParked;memcpy(patch+6,&vt,4);memcpy(patch+13,&count,4);
 if(memcmp(fn,head,16)||memcmp(fn+0x10,decide,28)||memcmp(fn+0x14b,tail,17)||memcmp(setter,setHead,35)||memcmp(setter+0x1c8,setTail,21))return false;
 BYTE*at=fn+0x2c;if(!memcmp(at,patch,33))return true;if(memcmp(at,broken,33))return false;
 DWORD old=0;if(!VirtualProtect(at,33,PAGE_EXECUTE_READWRITE,&old))return false;memcpy(at,patch,33);VirtualProtect(at,33,old,&old);FlushInstructionCache(GetCurrentProcess(),at,33);return true;}
// r51: one UBISERVICES_JOB_PARKED line when the guard parked a job (the first one, then at most once a minute while it repeats).
static void ubiServicesNote(){uint32_t n=ubiParked.load();if(n==ubiParkedLogged)return;uint64_t now=GetTickCount64();if(ubiParkedLogged&&now-ubiParkedLogAt<60000)return;ubiParkedLogged=n;ubiParkedLogAt=now;
 uint32_t vt=ubiParkedVtable.load();bool inside=vt>=(uint32_t)image&&vt<(uint32_t)image+0x1400000u;char msg[300];
 snprintf(msg,sizeof(msg),"UBISERVICES_JOB_PARKED count=%u vtable=%08x rva=%06x class=%s (a Ubisoft services job without a step method was parked instead of crashing the game)",n,vt,inside?(unsigned)(vt-(uint32_t)image):0u,inside?rtti(vt):"");log(msg);}
static bool installIncomeHook(){BYTE expected[9]={0x8d,0x41,0xff,0x3b,0x05,0,0,0,0};uint32_t compare=(uint32_t)(image+0xe94828);memcpy(expected+5,&compare,4);BYTE*at=(BYTE*)(image+0x36380);if(memcmp(at,expected,9))return false;BYTE*gateway=(BYTE*)VirtualAlloc(nullptr,32,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!gateway)return false;memcpy(gateway,at,9);gateway[9]=0xe9;*(int32_t*)(gateway+10)=(int32_t)((uintptr_t)(at+9)-(uintptr_t)(gateway+14));originalIncome=(AddMana)gateway;DWORD old;if(!VirtualProtect(at,9,PAGE_EXECUTE_READWRITE,&old))return false;at[0]=0xe9;*(int32_t*)(at+1)=(int32_t)((uintptr_t)income-(uintptr_t)(at+5));memset(at+5,0x90,4);VirtualProtect(at,9,old,&old);FlushInstructionCache(GetCurrentProcess(),at,9);return true;}
// Keep the pinned direct native farm-income path intact. No trampoline or interception.
static bool nativeFarmIncomeIntact(){BYTE expected[]={0x55,0x8b,0xec,0x8b,0x4d,0x08};return !memcmp((void*)(image+0x389c0),expected,sizeof(expected));}
static bool nativeConstructionIntact(){
 BYTE expected[]={0x83,0xe8,0x00,0x74,0x16,0x83,0xe8,0x01,0x74,0x0c,0x83,0xe8,0x01,0x75,0x06,0x50,0xe8,0x8c,0x0b,0xf7,0xff,0xc3,0xe9,0x66,0x0a,0xf7,0xff,0x8b,0x01,0xff,0x60,0x10};
 return !memcmp((void*)(image+0x1c5edf),expected,sizeof(expected));
}
static void position(uint16_t id,int&x,int&y){auto*p=entity(id);x=p?peek<uint16_t>((uintptr_t)p+24):0;y=p?peek<uint16_t>((uintptr_t)p+26):0;}
static bool freePoint(int x,int y){int n=side();if(!n||x<1||y<1||x>=n-1||y>=n-1)return false;BYTE*ground=peek<BYTE*>(image+0x11630dc);return readable(ground,(size_t)n*n*2)&&!peek<uint16_t>((uintptr_t)ground+2*(y*n+x))&&!api->LandscapeIsOccupied(x,y)&&!api->LandscapeGetObject(x,y)&&api->LandscapeGetType(x,y)>=16&&api->LandscapeGetType(x,y)<96;}
static bool exitPoint(uint16_t id,int&x,int&y){position(id,x,y);int dx=kind(id)==49?3:-1,dy=kind(id)==49?1:8;int cx=x+dx,cy=y+dy;for(int radius=0;radius<=10;radius++)for(int iy=-radius;iy<=radius;iy++)for(int ix=-radius;ix<=radius;ix++)if((abs(ix)==radius||abs(iy)==radius)&&freePoint(cx+ix,cy+iy)){x=cx+ix;y=cy+iy;return true;}return false;}
// Mod-issued moves must not be read as player orders by the send callback.
static std::atomic<bool> internalSend(false);
static void move(uint16_t id,int x,int y){internalSend=true;api->SendWarriors(x,y,S4_MOVEMENT_FORWARD,&id,1,1);internalSend=false;dtmem::invalidate();}
// Not destroyed: the native removal (RVA 0x16D030) marks the UID and flag
// 0x4000000 at once, but the entity stays readable until the deferred delete.
static bool standing(uint16_t id){auto*p=entity(id);return p&&dt::foundationIdentity(identity(id))&&!(peek<uint32_t>((uintptr_t)p+0x14)&0x4000000);}
static uint16_t nearestBase(int x,int y){uint16_t out=0;int best=0x7fffffff;for(uint32_t id=1;id<65536;id++){auto*p=entity((uint16_t)id);if(!p||p[10]!=8||!owned((uint16_t)id)||!standing((uint16_t)id)||isSite((uint16_t)id)||(kind((uint16_t)id)!=50&&kind((uint16_t)id)!=51))continue;int a,b;position((uint16_t)id,a,b);int d=abs(x-a)+abs(y-b);if(d<best){best=d;out=(uint16_t)id;}}return out;}
static bool knownSite(uint16_t id){for(uint32_t i=0;i<state.constructionCount;i++)if(state.construction[i].site==id&&state.construction[i].identity==identity(id))return true;return false;}
static bool landed(uint16_t id);static void cancelBoarding(uint16_t craft);static bool flight(uint16_t id,uint32_t uid,int x,int y);
struct ConstructionOps {
 bool sameSite(const dt::Construction&j){return identity(j.site)==j.identity;}
 bool foundation(const dt::Construction&j){auto*p=entity(j.site);return p&&p[10]==8&&p[32]==0x13&&dt::foundationIdentity(identity(j.site))&&sameSite(j)&&kind(j.site)==j.kind&&isSite(j.site)&&peek<uint16_t>((uintptr_t)p+24)==j.x&&peek<uint16_t>((uintptr_t)p+26)==j.y;}
 bool darkLand(const dt::Construction&j){return api->LandscapeIsDarkLand(j.x,j.y);}
 uint32_t mana(){return ::mana();}
 bool changeMana(int amount){return ::changeMana(amount);}
 bool remove(const dt::Construction&j,int mode){
  if(!foundation(j))return false;
  operation="BuildingsDelete cancel foundation";BOOL accepted=api->BuildingsDelete(j.site,mode);operation="idle";dtmem::invalidate();
  char msg[220];snprintf(msg,sizeof(msg),"BUILD_CANCEL site=%u identity=%08lx kind=%u mode=%d luaAccepted=%u identityAfter=%08lx x=%u y=%u",j.site,(unsigned long)j.identity,j.kind,mode,(unsigned)accepted,(unsigned long)identity(j.site),j.x,j.y);log(msg);
  return accepted!=FALSE;
 }
 bool begin(const dt::Construction&j){operation="native construction reservation";bool okay=nativeConstruction.begin(j);operation="idle";dtmem::invalidate();return okay;}
 bool progress(const dt::Construction&j,uint32_t t){return nativeConstruction.progress(j,t);}
 bool completed(const dt::Construction&j){return nativeConstruction.completed(j);}
 dtconstruction::Native::Block lastBlock; // Captured at the decision, before any refund/cancellation.
 bool start=false; // r30: the new-map tick; a farm site refused by the fire blockade is a start-map farm (no notice).
 dt::SiteSpace space(const dt::Construction&j){lastBlock={};return nativeConstruction.space(j,true,&lastBlock);}
 // Order own idle units off the reserved building (+1 tile margin) to the
 // nearest free tile, as a player would. Never moves other players' units,
 // hidden/boarded units or aircraft.
 void evict(const dt::Construction&j){
  std::vector<uint16_t> ids;nativeConstruction.blockers(j,ids);auto*body=dt::placementFootprint(j.kind);if(ids.empty()||!body)return;
  auto keep=dt::reservation(body->cells,body->count,j.x,j.y);
  for(uint16_t id:ids){
   BYTE*p=entity(id);if(!p)continue;int ux,uy;position(id,ux,uy);const char*skip=nullptr;
   bool craft=aircraft(id);
   if(!owned(id))skip="foreign unit (waits)";else if(p[10]!=1&&!craft)skip="not a settler";else if(craft&&!landed(id))skip="aircraft in flight";
   int tx=-1,ty=-1;
   if(!skip)for(int r=craft?4:1;r<=20&&tx<0;r++)for(int dy=-r;dy<=r&&tx<0;dy++)for(int dx=-r;dx<=r&&tx<0;dx++){
    if(abs(dx)!=r&&abs(dy)!=r)continue;int x=ux+dx,y=uy+dy;if(dt::contains(keep,x,y)||!freePoint(x,y))continue;tx=x;ty=y;}
   if(!skip&&tx<0)skip="no free tile outside the site";
   char line[180];
   if(skip){snprintf(line,sizeof(line),"SITE_EVICT_SKIP site=%u unit=%u cls=%u at=%d,%d reason=%s",j.site,id,p[10],ux,uy,skip);log(line);continue;}
   if(craft){cancelBoarding(id);flight(id,identity(id),tx,ty);}else move(id,tx,ty);
   snprintf(line,sizeof(line),"SITE_EVICT site=%u kind=%u unit=%u%s from=%d,%d to=%d,%d",j.site,j.kind,id,craft?" (aircraft)":"",ux,uy,tx,ty);log(line);
  }
 }
 bool finish(const dt::Construction&j){operation="native in-place construction completion";bool okay=nativeConstruction.finish(j);operation="idle";dtmem::invalidate();return okay;}
 void report(dt::ConstructionEvent event,const dt::Construction&j){
  const char*label="";
  switch(event){
   case dt::ConstructionEvent::Unpaid:label="unpaid site refused; no future funding";say("Not enough mana, or no open Dark land there.");break;
   case dt::ConstructionEvent::Started:label="paid once; original site retained; native rise started";say("");break;
   case dt::ConstructionEvent::StartFailed:label="native growth preparation refused; refunded once";say("The building could not be started. Mana refunded.");break;
   case dt::ConstructionEvent::Complete:label="native completion; same site and identity";say("");break;
   case dt::ConstructionEvent::Lost:label="original site lost; refunded once";say("Building site lost. Mana refunded.");break;
   case dt::ConstructionEvent::LegacyCancelled:label="old conversion retired; refunded once; no recreation";say("Construction refunded. Place the building again.");break;
   case dt::ConstructionEvent::Blocked:label="static obstruction; refunded once";say("Building site blocked. Mana refunded.");break;
   case dt::ConstructionEvent::UnitWait:label="waiting for units to clear native blocking shape; no timeout refund";say("Waiting for units to leave the building site.");break;
   case dt::ConstructionEvent::Duplicate:label="second Temple/Fortress refused before payment (one of each); not charged";say(dt::limitNotice(j.kind));break;
   case dt::ConstructionEvent::FireBlocked:label=start?"start-map site inside the citadel fire field; cancelled before payment":"inside the citadel fire field (Farm vs Temple/Fortress); refused before payment";if(start){char line[160];snprintf(line,sizeof(line),"FARM_FIRE_REMOVED %u,%u reason=start site inside the citadel fire field site=%u kind=%u",j.x,j.y,j.site,j.kind);log(line);}else say(dt::fireNotice(j.kind));break;
  }
  dtconstruction::Native::Block why=lastBlock;
  if(event==dt::ConstructionEvent::Blocked||event==dt::ConstructionEvent::UnitWait||event==dt::ConstructionEvent::StartFailed){if(event==dt::ConstructionEvent::StartFailed)why=nativeConstruction.lastStart();BYTE*e=why.id?entity(why.id):nullptr;char msg[260];snprintf(msg,sizeof(msg),"GROW_BLOCK kind=%u reason=%s tile=%d,%d entity=%u cls=%u owner=%02x flags=%08lx type=%u aircraft=%d",j.kind,why.reason,why.x,why.y,why.id,e?e[10]:0,e?e[32]:0,e?(unsigned long)peek<uint32_t>((uintptr_t)e+0x14):0ul,e?kind(why.id):0,(int)(why.id&&aircraft(why.id)));log(msg);}
  char msg[256];snprintf(msg,sizeof(msg),"BUILD_STATE site=%u identity=%08lx kind=%u x=%u y=%u readyAt=%u identityNow=%08lx refunded=%u growth=%u %s",j.site,(unsigned long)j.identity,j.kind,j.x,j.y,j.readyAt,(unsigned long)identity(j.site),j.refunded,j.pad,label);log(msg);
 }
};
// One Dark Temple and one Dark Fortress (dt::BuildingLimit). Recounted on every
// tick by construction() within its existing discovery scan (no extra scan) and
// read by command 1 and the construction tiles.
static dt::BuildingLimit buildingLimit;
static_assert(dtui::TileBuilt==dt::LimitBuilt&&dtui::TileBuilding==dt::LimitBuilding,"tile states follow the limit model");
// r30 citadel fire blockade (blockade-model.hpp, citadel-fire.hpp): own
// Temple/Fortress and Mushroom Farm positions, completed or site, rebuilt every
// tick inside the same discovery scan. Discovery refuses blocked sites; the
// placement hints never mark a blocked anchor. Transient, never saved.
static dt::FireBlockade fireBlockade;
// Set by initialize() on a new map only (never on a restored save): the next
// construction() pass removes start-map farms inside a citadel fire field once.
static bool fireStartPending=false;
// r30 review: set by initialize() on a restored save. A save made before r30
// (no blockade yet) can hold a farm inside a citadel fire field, which the
// computers never take (offence locks it) while it keeps the fire on, so the
// Temple/Fortress could never fall. The next construction() pass removes such
// farms once, silently, logged with reason=load. An r30 save holds none (the
// blockade refuses them), so the pass is idempotent without a schema change.
static bool fireLoadPending=false;
// Start-map farms inside a fire field: the player's own demolition path (Lua
// Buildings.CrushBuilding -> mode-1 removal 0x136960 -> Kill 0x16D030(id,0)
// -> CBuilding slot 4 0x1361D0 -> 0x13C680, which decrements the ready-farm
// counter at once). Attacker 0 and farm byte +0x25 = 0: no computer is credited.
static bool fireSweep(std::vector<uint16_t>&farms,dt::FireBlockade&fire,bool load){
 bool removed=false;
 for(auto id:farms){int x,y;position(id,x,y);auto*by=fire.farmBlocker(x,y);if(!by){fire.add(49,x,y);continue;}
  int bx=by->x,by2=by->y;operation=load?"BuildingsCrush loaded farm in citadel fire":"BuildingsCrush start farm in citadel fire";BOOL accepted=api->BuildingsCrush(id);operation="idle";dtmem::invalidate();bool gone=!standing(id);if(!gone)fire.add(49,x,y);removed=removed||gone;
  char line[200];snprintf(line,sizeof(line),"FARM_FIRE_REMOVED %d,%d reason=%s farm inside the citadel fire field farm=%u citadel=%d,%d crushAccepted=%u gone=%u",x,y,load?"load":"start",id,bx,by2,(unsigned)accepted,(unsigned)gone);log(line);}
 return removed;
}
static void construction(uint32_t t){
 // r34: the site panel wrapper (dtconstruction::PanelGuard) runs inside the native input loop and only counts; it is logged here.
 {static uint32_t seenViews=0,seenSkips=0;uint32_t v=dtconstruction::PanelGuard::views.load(),k=dtconstruction::PanelGuard::skips.load();if(v!=seenViews||k!=seenSkips){seenViews=v;seenSkips=k;char msg[200];snprintf(msg,sizeof(msg),"SITE_PANEL_GUARD views=%lu skips=%lu site=%lu kind=%lu panel of a site without an economy shown with a transient safe view",(unsigned long)v,(unsigned long)k,(unsigned long)dtconstruction::PanelGuard::lastSite.load(),(unsigned long)dtconstruction::PanelGuard::lastKind.load());log(msg);}}
 ConstructionOps ops;dt::BuildingLimit limit;dt::countJobs(limit,state);uint16_t fresh[64];unsigned freshCount=0;
 dt::FireBlockade fire;std::vector<uint16_t> farms;bool start=fireStartPending,load=fireLoadPending&&!start;fireStartPending=false;fireLoadPending=false;ops.start=start;
 for(uint32_t id=1;id<65536;id++){
  auto*p=entity((uint16_t)id);if(!p||p[10]!=8||p[32]!=0x13||!dt::foundationIdentity(identity((uint16_t)id)))continue;
  // Completed Temple/Fortress. Native removal (demolish/Lua crush, delete
  // modes 1/2, destruction) marks the UID 0x20000000 and flag 0x4000000 at
  // once (RVA 0x16D030), so a lost base stops counting on the next tick.
  if(!isSite((uint16_t)id)){uint16_t k=kind((uint16_t)id);if(dt::limitSlot(k)>=0&&!(peek<uint32_t>((uintptr_t)p+0x14)&0x4000000))limit.countBuilt(k);if((k==49||dt::citadelKind(k))&&standing((uint16_t)id)){int x,y;position((uint16_t)id,x,y);if(k==49)farms.push_back((uint16_t)id);else fire.add(k,x,y);}continue;}
  if(!knownSite((uint16_t)id)&&freshCount<64)fresh[freshCount++]=(uint16_t)id; // Any further ones are found on the next tick.
 }
 // Recorded jobs that will become buildings (growing sites) join the blockade.
 for(uint32_t i=0;i<state.constructionCount&&i<dt::MaxJobs;i++)if(!state.construction[i].refunded)fire.add(state.construction[i].kind,state.construction[i].x,state.construction[i].y);
 // Completed farms block new Temple/Fortress sites, except on the new-map tick
 // and the first tick after a load, where a citadel wins and the farms inside
 // its field are removed instead.
 if(!start&&!load)for(auto id:farms){int x,y;position(id,x,y);fire.add(49,x,y);}
 auto discover=[&](uint16_t id){
  uint16_t type=kind(id);uint32_t cost=dt::buildCost(type);int x,y;position(id,x,y);
  if(!cost||state.constructionCount>=dt::MaxJobs){
   api->BuildingsDelete(id,dt::FoundationCancelMode);dtmem::invalidate();say(cost?"Too many buildings are being built at once.":"Only Dark Tribe buildings can be constructed.");return;
  }
  auto&j=state.construction[state.constructionCount++];j={};j.site=id;j.identity=identity(id);j.kind=type;j.cost=cost;j.x=(uint16_t)x;j.y=(uint16_t)y;
  // A second Temple/Fortress (SU repeated placement, the last-building hotkey,
  // two quick outlines) is cancelled with mode 1 before payment; unfunded sites
  // are refused as before; so is a site inside the citadel fire blockade.
  dt::discoverConstruction(j,t,ops,limit,&fire);
 };
 // Temple/Fortress sites first: of a farm and a citadel placed in the same
 // tick the citadel is kept and the farm refused.
 for(unsigned f=0;f<freshCount;f++)if(dt::citadelKind(kind(fresh[f])))discover(fresh[f]);
 if(start||load)fireSweep(farms,fire,load);
 for(unsigned f=0;f<freshCount;f++)if(!dt::citadelKind(kind(fresh[f])))discover(fresh[f]);
 // Taken before this tick's job updates: a job refunded below still counts
 // until the next tick, so the limit is never briefly too permissive.
 buildingLimit=limit;
 fire.normalize();if(fire!=fireBlockade){fireBlockade=fire;placementHints.reset();}placementHints.blockWith(&fireBlockade);
 for(uint32_t i=0;i<state.constructionCount;){
  auto&j=state.construction[i];auto result=dt::updateConstruction(j,t,ops);
  if(result==dt::ConstructionResult::Fatal){fatal=true;running=false;log("Construction transaction or native identity check failed; plugin disabled");return;}
  bool erase=result==dt::ConstructionResult::Complete||result==dt::ConstructionResult::Retired||result==dt::ConstructionResult::Refunded;
  // An API/Lua success is not proof of deletion. Retain a refunded/refused
  // identity until it disappears so discovery cannot charge it again.
  if(j.refunded&&ops.sameSite(j))erase=false;
  if(erase){bool converted=j.removed;state.construction[i]=state.construction[--state.constructionCount];if(converted)return;}else i++;
 }
}
static void aircraftJobs(uint32_t t){for(uint32_t i=0;i<state.aircraftCount;){auto&j=state.aircraft[i];bool erase=false;if(!owned(j.hall)||identity(j.hall)!=j.identity){changeMana(300);say("Manacopter Hall lost. Mana refunded.");erase=true;}else if(t>=j.readyAt){int x,y;if(exitPoint(j.hall,x,y)){uint32_t before=peek<uint32_t>(image+0x113fc98),uid=identity((uint16_t)before);operation="DarkTribeAddManakopter";api->DarkTribeAddManakopter(x,y,1);operation="idle";dtmem::invalidate();uint32_t id=peek<uint32_t>(image+0x113fc98);if(id&&id<65536&&aircraft((uint16_t)id)&&(id!=before||identity((uint16_t)id)!=uid)){say("Manacopter ready.");erase=true;}else{changeMana(300);say("The Manacopter could not be built. Mana refunded.");erase=true;}}}if(erase)state.aircraft[i]=state.aircraft[--state.aircraftCount];else i++;}}
// Pinned Lua FlyTo uses the last-created global, never the native selection.
// Tick-only scoped rebinding retains the native scheduler and transport state.
static bool flight(uint16_t id,uint32_t uid,int x,int y){
 if(!aircraft(id)||!dt::flightAllowed(uid,identity(id),x,y,side()))return false;
 auto*slot=(uint32_t*)(image+0x113fc98);if(!readable(slot,4))return false;
 dt::ScopedAircraft target(*slot,id);operation="DarkTribeFlyTo";
 BOOL ok=api->DarkTribeFlyTo(x,y);operation="idle";dtmem::invalidate();
 auto*p=entity(id);bool accepted=ok&&p&&identity(id)==uid&&peek<uint16_t>((uintptr_t)p+0x48)==x&&peek<uint16_t>((uintptr_t)p+0x4a)==y;
 char line[160];snprintf(line,sizeof(line),"FLIGHT id=%u identity=%u x=%d y=%d accepted=%d",id,uid,x,y,accepted);log(line);
 return accepted;
}
static uint32_t passengers(uint16_t id){auto*p=entity(id);uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58),b=peek<uintptr_t>((uintptr_t)p+0x5c);return b>=a&&(b-a)%12==0&&(b-a)/12<=4096?(b-a)/12:0;}
static bool passengerStorage(uint16_t id){auto*p=entity(id);uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58),b=peek<uintptr_t>((uintptr_t)p+0x5c);return b>=a&&(b-a)%12==0&&(b-a)/12<=4096&&(a==b||readable((void*)a,b-a));}
static bool landed(uint16_t id){auto*p=entity(id);return aircraft(id)&&!(peek<uint16_t>((uintptr_t)p+0x4c)&1)&&peek<int>((uintptr_t)p+0x78)==-10&&peek<int>((uintptr_t)p+0x7c)==-50;}
static dt::UnitRef unitRef(uint16_t id){return {id,kind(id),identity(id)};}
static bool eligiblePassenger(dt::UnitRef unit){
 auto*p=entity(unit.id);return p&&p[10]==1&&owned(unit.id)&&dt::sameEntity(unit,unitRef(unit.id))&&dt::boardable(kind(unit.id))&&!(peek<uint32_t>((uintptr_t)p+0x14)&(0x8000|0x4000000));
}
static void cancelBoarding(uint16_t craft){boarding.erase(std::remove_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return o.craft.id==craft;}),boarding.end());}
static void requestBoarding(const Command&c,uint32_t t){
 if(!aircraft(c.entity)||identity(c.entity)!=c.identity||!landed(c.entity)||!passengerStorage(c.entity)){auto*q=entity(c.entity);char line[200];snprintf(line,sizeof(line),"BOARD_REFUSED craft=%u aircraft=%d identityMatch=%d landed=%d storage=%d state=%04x z1=%d z2=%d",c.entity,(int)aircraft(c.entity),(int)(identity(c.entity)==c.identity),(int)landed(c.entity),(int)passengerStorage(c.entity),q?peek<uint16_t>((uintptr_t)q+0x4c):0,q?peek<int>((uintptr_t)q+0x78):0,q?peek<int>((uintptr_t)q+0x7c):0);log(line);say("Units can only board a landed Manacopter.");return;}
 uint32_t queued=0;dt::UnitRef craft=unitRef(c.entity);
 std::vector<dt::UnitRef> selectedUnits=c.units;
 if(selectedUnits.empty()){WORD ids[100]{};SIZE_T count=0;if(api->GetSelection(ids,100,&count))for(SIZE_T i=0;i<std::min<SIZE_T>(count,100);i++)selectedUnits.push_back(unitRef(ids[i]));}
 for(auto unit:selectedUnits){
  if(!eligiblePassenger(unit))continue;
  boarding.erase(std::remove_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.unit,unit);}),boarding.end());
  uint32_t waiting=(uint32_t)std::count_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.craft,craft);});
  if(passengers(c.entity)+waiting>=6||boarding.size()>=128)continue;
  boarding.push_back({craft,unit,t+600,0});queued++;
 }
 {char line[140];snprintf(line,sizeof(line),"BOARD_QUEUE craft=%u selected=%u queued=%u passengers=%u",c.entity,(unsigned)selectedUnits.size(),queued,passengers(c.entity));log(line);}
 say(queued?"":"Only Warriors, Throwers, Gardeners and Shamans can board, and only while seats are free.");
}
static void boardingOrders(uint32_t t){
 for(size_t i=0;i<boarding.size();){
  auto&o=boarding[i];bool remove=!aircraft(o.craft.id)||!dt::sameEntity(o.craft,unitRef(o.craft.id))||!eligiblePassenger(o.unit)||t>=o.expires||!landed(o.craft.id)||!passengerStorage(o.craft.id)||passengers(o.craft.id)>=6;
  if(!remove){
   int x,y,a,b;position(o.craft.id,x,y);position(o.unit.id,a,b);
   if(dt::nearBoarding(a,b,x,y)){
    using Board=bool(__thiscall*)(BYTE*,uint32_t);uint32_t before=passengers(o.craft.id);operation="Manakopter board selected";
    // Mirror the native "enter entity" arrival (military role slot 10, job
    // state 24, RVA 0x189CD3): mode +0x42 = 0xA and removal from the per-tick
    // scheduler bucket +0x30 (RVA 0x16D720). Without this the unit kept
    // walking/drawing on the map while also listed as a passenger.
    BYTE*u=entity(o.unit.id);void*scheduler=peek<void*>(image+0x10540d4);uint32_t bucket=u?peek<uint32_t>((uintptr_t)u+0x30):0xffffffff;BYTE mode=u?u[0x42]:0;
    bool hidden=false;if(u&&scheduler&&bucket<=0x1f){using Remove=void(__thiscall*)(void*,uint32_t,uint32_t);u[0x42]=0xa;((Remove)(image+0x16d720))(scheduler,bucket,o.unit.id);hidden=true;dtmem::invalidate();}
    bool ok=hidden&&((Board)(image+0x16b890))(entity(o.craft.id),o.unit.id);operation="idle";dtmem::invalidate();
    if(hidden&&(!ok||passengers(o.craft.id)!=before+1)){
     // Refused: restore exactly as the settler message handler re-adds a unit (RVA 0x132EC4).
     using Add=uint32_t(__stdcall*)(uint32_t,uint32_t);BYTE*w=entity(o.unit.id);if(w){w[0x42]=mode;*(uint32_t*)(w+0x30)=((Add)(image+0x16d5d0))(1,o.unit.id);}dtmem::invalidate();
    }
    {char line[120];snprintf(line,sizeof(line),"BOARD_CALL craft=%u unit=%u accepted=%d passengers=%u->%u",o.craft.id,o.unit.id,(int)ok,before,passengers(o.craft.id));log(line);}
    if(ok&&passengers(o.craft.id)==before+1){SIZE_T removed=0;WORD id=o.unit.id;api->RemoveSelection(&id,1,&removed);say("");remove=true;}
   }else if(!o.lastOrder||t-o.lastOrder>=40){
    bool ordered=false;for(int radius=2;radius<=5&&!ordered;radius++)for(int dy=-radius;dy<=radius&&!ordered;dy++)for(int dx=-radius;dx<=radius&&!ordered;dx++)if((abs(dx)==radius||abs(dy)==radius)&&freePoint(x+dx,y+dy)){move(o.unit.id,x+dx,y+dy);ordered=true;}o.lastOrder=t;
   }
  }
  if(remove)boarding.erase(boarding.begin()+i);else i++;
 }
}
// r39 (user: Dark Gardeners dropped from a Manacopter were "invincible" and oddly selectable): the native unload
// (RVA 0x16B990) pops each passenger and posts it two entity events, 0x19 (leave at the exit tile) and 0x11/0xD (back
// to work), handled by the role's event slot (CSettler slot 27 0x1818D0 -> role +0x38). The soldier roles
// (CHJBRole 0x189F70, case 0x18A020; Shamans forward to it) and the computer Gardener (CGardenerRole 0x179D1A) handle
// 0x19 by placing the unit at the nearest free cell of a land sector around the exit tile (0x182A40: ground map and
// position +0x18, the type's animation), re-indexing it (0x1BB560) and clearing the hidden flag (+0x14 &= ~0x8000,
// |= 0x2100). The Dark Gardener role (CDarkGardenerRole, vtable 0xC5AD04, slot 14 0x174F10) knows only 0xD and 0x11
// and treats 0x19 as "put me back on the scheduler": the unit kept the hidden flag, had no ground cell (clicks on
// its cell found nothing) and no spatial index entry (no enemy and no computer defence could target it), while its
// 0x11 event still gave it work. The mod does the 0x19 part for Dark Gardeners right after the native unload.
static void placeUnloaded(uint16_t craft,const std::vector<std::pair<uint32_t,uint16_t>>&riders,int ax,int ay,int ex,int ey){
 using Place=BYTE(__stdcall*)(uint32_t,int,int);using Index=void(__thiscall*)(BYTE*);int placed=0,left=0;
 for(auto&r:riders){BYTE*u=entity(r.second);if(!u||peek<uint32_t>((uintptr_t)u+4)!=r.first||u[10]!=1)continue;
  uintptr_t role=peek<uintptr_t>((uintptr_t)u+0x58);if(!role||peek<uintptr_t>(role)!=image+0xc5ad04)continue; /* Dark Gardener role only */
  if(!(peek<uint32_t>((uintptr_t)u+0x14)&0x8000))continue;
  operation="Manakopter gardener placement";BYTE ok=((Place)(image+0x182a40))(r.second,ex,ey);if(!ok)ok=((Place)(image+0x182a40))(r.second,ax,ay);
  if(ok){((Index)(image+0x1bb560))(u);uint32_t bits=peek<uint32_t>((uintptr_t)u+0x14);*(uint32_t*)(u+0x14)=(bits&~0x8000u)|0x2100u;placed++;}else left++;
  operation="idle";dtmem::invalidate();}
 if(placed||left){char line[150];snprintf(line,sizeof(line),"PASSENGER_PLACED craft=%u gardeners=%d unplaced=%d exit=%d,%d",craft,placed,left,ex,ey);log(line);}
}
static void transport(const Command&c){
 if(!aircraft(c.entity)||identity(c.entity)!=c.identity||!landed(c.entity)||!passengerStorage(c.entity)){say("The Manacopter must be landed to unload.");return;}
 cancelBoarding(c.entity);
 auto*p=entity(c.entity);int x,y;position(c.entity,x,y);
 if(!passengers(c.entity)){say("No passengers to unload.");return;}
 const int dx[]={2,2,2,1,0,-1},dy[]={0,1,2,2,2,1};
 for(int i=0;i<6;i++)if(!freePoint(x+dx[i],y+dy[i])){say("No room to unload here. Land on open ground.");return;}
 // Passengers boarded by older builds were never taken off the per-tick
 // scheduler. Native unload re-adds every passenger (settler message handler,
 // RVA 0x132EC4), so normalise such entries first to avoid double scheduling.
 {uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58),b=peek<uintptr_t>((uintptr_t)p+0x5c);void*scheduler=peek<void*>(image+0x10540d4);using Remove=void(__thiscall*)(void*,uint32_t,uint32_t);
  for(uintptr_t r=a;scheduler&&r+12<=b&&readable((void*)r,12);r+=12){uint32_t uid=peek<uint32_t>(r),id=peek<uint32_t>(r+4);BYTE*u=id&&id<65536?entity((uint16_t)id):nullptr;
   if(!u||peek<uint32_t>((uintptr_t)u+4)!=uid||u[0x42]==0xa)continue;uint32_t bucket=peek<uint32_t>((uintptr_t)u+0x30);if(bucket>0x1f)continue;
   u[0x42]=0xa;((Remove)(image+0x16d720))(scheduler,bucket,id);dtmem::invalidate();char line[100];snprintf(line,sizeof(line),"PASSENGER_NORMALISED craft=%u unit=%u",c.entity,id);log(line);}}
 /* r39: the passengers and the exit record before the native unload empties the vector */
 std::vector<std::pair<uint32_t,uint16_t>> riders;{uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58),b=peek<uintptr_t>((uintptr_t)p+0x5c);for(uintptr_t r=a;r+12<=b&&readable((void*)r,12);r+=12){uint32_t uid=peek<uint32_t>(r),id=peek<uint32_t>(r+4);if(id&&id<65536)riders.push_back({uid,(uint16_t)id});}}
 int ex=x,ey=y;{int k=dt::unloadExit(p[0x4e]);if(k>=0){ex=x+(signed char)peek<BYTE>(image+0xd69988+4*k);ey=y+(signed char)peek<BYTE>(image+0xd69989+4*k);}}
 using Unload=void(__thiscall*)(BYTE*);operation="Manakopter unload";((Unload)(image+0x16b990))(p);operation="idle";dtmem::invalidate();
 placeUnloaded(c.entity,riders,x,y,ex,ey);
 say(passengers(c.entity)?"Some passengers could not get out.":"");
 char line[120];snprintf(line,sizeof(line),"TRANSPORT id=%u identity=%u action=16 passengers=%u",c.entity,c.identity,passengers(c.entity));log(line);
}
static const char* passengerName(uint16_t k){switch(k){case 29:return "Warrior";case 32:return "Thrower";case 52:return "Gardener";case 54:return "Shaman";case 53:return "Farmer";default:return "Settler";}}
static void passengerView(View&v){
 if(!passengerStorage(v.selected))return;
 auto*p=entity(v.selected);uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58);
 v.passengers=passengers(v.selected);
 for(uint32_t i=0;i<std::min(v.passengers,6u);i++){
  uint32_t uid=peek<uint32_t>(a+i*12),id=peek<uint32_t>(a+i*12+4),k=peek<uint32_t>(a+i*12+8);
  bool valid=id&&id<65536&&uid&&identity((uint16_t)id)==uid&&owned((uint16_t)id)&&kind((uint16_t)id)==k;
  if(valid)snprintf(v.passengerLines[i],sizeof(v.passengerLines[i]),"%s #%u",passengerName((uint16_t)k),id);else snprintf(v.passengerLines[i],sizeof(v.passengerLines[i]),"Unavailable #%u",id);
  v.passengerKind[i]=valid?(uint16_t)k:0;
 }
 v.boarding=(uint32_t)std::count_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return o.craft.id==v.selected&&o.craft.identity==v.selectedIdentity;});
}
static void setPick(std::vector<dt::UnitRef> next){fleetPick=std::move(next);chosenAircraft=fleetPick.empty()?0:fleetPick[0].id;chosenIdentity=fleetPick.empty()?0:fleetPick[0].identity;draftIdentity=0;if(!fleetPick.empty())buildOverride=false;}
static bool picked(dt::UnitRef u){for(auto&o:fleetPick)if(dt::sameEntity(o,u))return true;return false;}
// The aircraft an order for entity `id` applies to: the whole group when it is
// part of the group, otherwise just that aircraft.
static std::vector<dt::UnitRef> orderGroup(uint16_t id,uint32_t uid){dt::UnitRef one{id,kind(id),uid};if(picked(one))return fleetPick;return {one};}
// Resolve a map click/box against aircraft on the tick, with the native
// screen->map projection sampled now (RVA 0x259910, pinned via placement signatures).
static void pickAircraft(const Command&c){
 int placing=peek<int>(image+0x1086588);if(placing>=4&&placing<=6)return; // Native building placement owns the click.
 int w=peek<int>(image+0xd691ec),h=peek<int>(image+0xd691f0);if(w<64||h<64||w>16384||h>16384)return;
 double kx=c.cw>16?(double)w/c.cw:1.0,ky=c.ch>16?(double)h/c.ch:1.0;int x0=(int)(c.x*kx),y0=(int)(c.y*ky),x1=(int)(c.x2*kx),y1=(int)(c.y2*ky);
 using Project=int(__stdcall*)(int,int,int*,int*);auto project=(Project)(image+0x259910);
 // Fit the projection locally around the click/box centre: quarter-screen
 // samples fail near map edges, where the native projection clamps.
 int cx=std::clamp((x0+x1)/2,48,w-49),cy=std::clamp((y0+y1)/2,48,h-49);
 int qx[4]={cx-40,cx+40,cx-40,cx+40},qy[4]={cy-40,cy-40,cy+40,cy+40},m[4][2]{};for(int i=0;i<4;i++)project(qx[i],qy[i],&m[i][0],&m[i][1]);
 auto t=dt::fitAffine(qx[0],qy[0],qx[1],qy[1],qx[2],qy[2],m[0],m[1],m[2]);bool additive=c.arg!=0;
 if(!dt::affineAgrees(t,qx[3],qy[3],m[3])){log("AIRCRAFT_PICK projection not affine here; pick ignored");return;}
 std::vector<dt::UnitRef> hits;double best=1e18;dt::UnitRef nearest{};bool box=dt::boxSelect(x0,y0,x1,y1);
 for(auto craft:cachedFleet){if(!aircraft(craft.id)||identity(craft.id)!=craft.identity)continue;int ax,ay;position(craft.id,ax,ay);auto b=dt::spriteBox(t,ax,ay);if(!dt::pickHit(b,x0,y0,x1,y1))continue;
  if(box)hits.push_back(craft);else{double cx=(b.left+b.right)/2.0,cy=(b.top+b.bottom)/2.0,d=(cx-x1)*(cx-x1)+(cy-y1)*(cy-y1);if(d<best){best=d;nearest=craft;}}}
 if(!box&&nearest.id)hits.push_back(nearest);
 if(hits.empty()){if(!additive&&!fleetPick.empty())setPick({});return;}
 std::vector<dt::UnitRef> next=additive?fleetPick:std::vector<dt::UnitRef>{};
 for(auto h:hits){auto it=std::find_if(next.begin(),next.end(),[&](const dt::UnitRef&o){return dt::sameEntity(o,h);});if(it!=next.end()){if(additive&&!box)next.erase(it);}else next.push_back(h);}
 setPick(next);if(!box){api->ClearSelection();lastSelection=0;}
 char line[120];snprintf(line,sizeof(line),"AIRCRAFT_PICK box=%d additive=%d hits=%u group=%u",(int)box,(int)additive,(unsigned)hits.size(),(unsigned)fleetPick.size());log(line);
 say(fleetPick.size()>1?"":fleetPick.empty()?"":"Manacopter selected. Right-click to fly; Shift/Ctrl-click or drag a box to add more.");
}
// Right-click destination for an aircraft-only selection, resolved on the tick
// with the exact native projection (RVA 0x259910) of the clicked pixel.
static bool screenToMap(const Command&c,int&mx,int&my){
 int w=peek<int>(image+0xd691ec),h=peek<int>(image+0xd691f0);if(w<64||h<64||w>16384||h>16384)return false;
 double kx=c.cw>16?(double)w/c.cw:1.0,ky=c.ch>16?(double)h/c.ch:1.0;
 using Project=int(__stdcall*)(int,int,int*,int*);auto project=(Project)(image+0x259910);
 mx=0;my=0;project((int)(c.x*kx),(int)(c.y*ky),&mx,&my);int n=side();return mx>1&&my>1&&mx<n-1&&my<n-1;
}
// Selection brackets for picked aircraft. The native bracket comes from the
// settler draw method (CSettler slot 5, RVA 0x181370: flag 0x200 fills the draw
// descriptor at RVA 0xE9ADFC); the Manacopter draw path never does, so the
// mod draws its own bracket at the aircraft's solved screen position.
static void aircraftMarkers(View&v){
 int w=peek<int>(image+0xd691ec),h=peek<int>(image+0xd691f0);if(w<64||h<64||w>16384||h>16384)return;
 using Project=int(__stdcall*)(int,int,int*,int*);auto native=(Project)(image+0x259910);
 auto project=[&](int x,int y,int out[2]){out[0]=-1000;out[1]=-1000;native(x,y,&out[0],&out[1]);};
 v.renderW=(uint16_t)w;v.renderH=(uint16_t)h;
 // Health: BYTE +0x21 against the native maximum (dt::aircraftMaxHp, RVA 0x16AF34).
 int maxHp=dt::aircraftMaxHp(peek<int32_t>(image+dt::ManakopterHitpointsSetting));
 for(auto craft:fleetPick){
  if(v.markCount>=12)break;BYTE*q=entity(craft.id);if(!q)continue;int ax,ay;position(craft.id,ax,ay);
  int sx=-1,sy=-1;if(!dt::solveScreen(project,w,h,ax,ay,sx,sy))continue;
  int up[2],down[2];project(sx,sy-24,up);project(sx,sy+24,down);double tile=(down[1]!=up[1])?48.0/fabs((double)(down[1]-up[1])):16.0;tile=std::clamp(tile,4.0,64.0);
  // Sprite offsets +0x78/+0x7C are -10/-50 when landed (altitude while flying).
  int ox=peek<int>((uintptr_t)q+0x78),oy=peek<int>((uintptr_t)q+0x7c);double k=tile/16.0;
  int mx=sx+(int)(std::clamp(ox,-200,200)*k),my=sy+(int)((std::clamp(oy,-400,200)-30)*k);
  if(mx<-40||my<-40||mx>w+40||my>h+40)continue;
  v.markX[v.markCount]=(int16_t)mx;v.markY[v.markCount]=(int16_t)my;v.markHp[v.markCount]=(uint8_t)std::max(1,dt::healthPercent(peek<uint8_t>((uintptr_t)q+0x21),maxHp));v.markCount++;
 }
}
// Passenger types aboard `craft`: entries whose unit identity still matches (as passengerView).
static void riderKinds(uint16_t craft,std::vector<dtunits::Member>&out){
 if(!passengerStorage(craft))return;uintptr_t a=peek<uintptr_t>((uintptr_t)entity(craft)+0x58);uint32_t n=std::min(passengers(craft),64u);
 for(uint32_t i=0;i<n;i++){uint32_t uid=peek<uint32_t>(a+i*12),id=peek<uint32_t>(a+i*12+4),k=peek<uint32_t>(a+i*12+8);
  if(id&&id<65536&&uid&&identity((uint16_t)id)==uid&&owned((uint16_t)id)&&kind((uint16_t)id)==k)out.push_back({dtunits::portraitKind((uint16_t)k),100});}
}
// Manacopter panel grid (tick): the first six aircraft of the pick with their
// health (BYTE +0x21 against dt::aircraftMaxHp, as aircraftMarkers), passengers
// and landed state; the whole pick's average health, passengers and unloadable
// state; and its passengers by type (most numerous first). Shown numbers are
// capped at 99 per aircraft and 999 per type (an aircraft has six seats).
static void aircraftPanelView(View&v){
 int maxHp=dt::aircraftMaxHp(peek<int32_t>(image+dt::ManakopterHitpointsSetting));uint32_t hpSum=0,hpCount=0;std::vector<dtunits::Member> cargo;
 v.group=(uint32_t)fleetPick.size();
 for(auto u:fleetPick){BYTE*q=entity(u.id);if(!q)continue;uint32_t n=passengers(u.id);bool down=landed(u.id);uint8_t hp=(uint8_t)std::max(1,dt::healthPercent(peek<uint8_t>((uintptr_t)q+0x21),maxHp));
  v.groupPassengers+=n;if(n&&down)v.groupUnload=1;hpSum+=hp;hpCount++;
  if(v.airShown<6){int i=v.airShown++;v.airId[i]=u.id;v.airIdentity[i]=u.identity;v.airHp[i]=hp;v.airLoad[i]=(uint8_t)std::min(n,99u);v.airLanded[i]=down;}
  riderKinds(u.id,cargo);}
 v.groupHp=(uint8_t)(hpCount?(hpSum+hpCount/2)/hpCount:v.aircraftHp);
 auto s=dtunits::summarize(cargo.data(),cargo.size());v.riderTypes=s.types;for(int i=0;i<4;i++){v.riderKind[i]=s.kind[i];v.riderCount[i]=std::min<uint16_t>(s.count[i],999);}
}
// Aircraft panel: keep only the clicked aircraft of the pick (like the unit
// portraits, command 40). The panel sends the entity and identity it showed.
static void keepAircraft(const Command&c){
 dt::UnitRef u{c.entity,kind(c.entity),c.identity};if(!aircraft(c.entity)||identity(c.entity)!=c.identity||!picked(u))return;
 unsigned before=(unsigned)fleetPick.size();setPick({u});
 /* An unload waiting for its map click continues for this aircraft only (none left ends it on this tick). */
 unloadGroup.erase(std::remove_if(unloadGroup.begin(),unloadGroup.end(),[&](const dt::UnitRef&o){return !dt::sameEntity(o,u);}),unloadGroup.end());
 char line[100];snprintf(line,sizeof(line),"AIRCRAFT_PANEL action=%d keep=%u group=%u",dtui::AircraftKeep,c.entity,before);log(line);say("");
}
static bool unloadFree(int x,int y){return freePoint(x,y);}
static void dropUnloadJob(uint16_t craft){unloadJobs.erase(std::remove_if(unloadJobs.begin(),unloadJobs.end(),[&](const UnloadJob&j){return j.craft.id==craft;}),unloadJobs.end());}
// Fly (or unload in place) towards the nearest valid spot to (tx,ty).
static bool startUnload(dt::UnitRef craft,int tx,int ty,uint32_t t,const int(*taken)[2],int takenCount,int&ox,int&oy){
 int ax,ay;position(craft.id,ax,ay);auto free=[&](int x,int y){return (x==ax&&y==ay)||unloadFree(x,y);}; // its own tile counts as free
 if(!dt::nearestUnloadSpot(free,tx,ty,24,taken,takenCount,ox,oy))return false;
 dropUnloadJob(craft.id);cancelBoarding(craft.id);
 if(!(landed(craft.id)&&ax==ox&&ay==oy)&&!flight(craft.id,craft.identity,ox,oy))return false;
 unloadJobs.push_back({craft,ox,oy,t+1500,0,0});
 char line[140];snprintf(line,sizeof(line),"UNLOAD_TARGET craft=%u click=%d,%d spot=%d,%d",craft.id,tx,ty,ox,oy);log(line);return true;
}
// Per tick: once an aircraft with a job has landed and settled, unload it with
// the native routine; if its landing spot is blocked, retarget to the nearest.
static void unloadJobsTick(uint32_t t){
 for(size_t i=0;i<unloadJobs.size();){
  auto&j=unloadJobs[i];bool drop=false;uint16_t id=j.craft.id;
  if(!aircraft(id)||identity(id)!=j.craft.identity||!passengers(id))drop=true;
  else if(t>=j.expires){say("Unload cancelled: no reachable landing spot.");drop=true;}
  else if(landed(id)){
   if(!j.landedSince)j.landedSince=t;
   if(t-j.landedSince>=3){
    int ax,ay;position(id,ax,ay);
    bool exits=true;for(int e=0;e<6;e++)if(!unloadFree(ax+dt::UnloadExitX[e],ay+dt::UnloadExitY[e]))exits=false;
    if(exits){Command c{16,0,id,0,0,j.craft.identity};transport(c);drop=true;}
    else if(++j.retries<=3){int ox,oy;dt::UnitRef craft=j.craft;uint32_t keep=j.retries;unloadJobs.erase(unloadJobs.begin()+i);if(startUnload(craft,ax,ay,t,nullptr,0,ox,oy)){unloadJobs.back().retries=keep;}else say("Unload cancelled: no clear landing spot nearby.");continue;}
    else{say("Unload cancelled: no clear landing spot nearby.");drop=true;}
   }
  }else j.landedSince=0;
  if(drop)unloadJobs.erase(unloadJobs.begin()+i);else i++;
 }
}
// Load nearby: the closest eligible units within LoadRadius tiles of the
// selected landed aircraft walk over and board through the existing native
// boarding orders, up to each aircraft's free seats (6 minus passengers and
// units already approaching it).
static void loadNearby(const Command&c,uint32_t t){
 std::vector<dt::UnitRef> crafts;std::vector<dt::LoadCraft> slots;unsigned flying=0;
 for(auto u:orderGroup(c.entity,c.identity)){
  if(!aircraft(u.id)||identity(u.id)!=u.identity||!passengerStorage(u.id))continue;
  if(!landed(u.id)){flying++;continue;}
  int waiting=(int)std::count_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.craft,u);});
  int ax,ay;position(u.id,ax,ay);crafts.push_back(u);slots.push_back({ax,ay,std::max(0,6-(int)passengers(u.id)-waiting)});
 }
 if(crafts.empty()){say(flying?"Land the aircraft first, then load nearby units.":"No selected aircraft can take passengers.");return;}
 std::vector<dt::UnitRef> candidates;std::vector<dt::LoadUnit> spots;
 for(uint32_t id=1;id<65536;id++){
  auto*p=entity((uint16_t)id);if(!p||p[10]!=1)continue;dt::UnitRef u=unitRef((uint16_t)id);if(!eligiblePassenger(u))continue;
  if(std::any_of(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.unit,u);}))continue;
  int x,y;position((uint16_t)id,x,y);bool inRange=false;for(auto&s:slots)if(abs(x-s.x)<=dt::LoadRadius&&abs(y-s.y)<=dt::LoadRadius){inRange=true;break;}
  if(inRange){candidates.push_back(u);spots.push_back({x,y});}
 }
 auto plan=dt::assignNearby(slots,candidates.size()?spots:std::vector<dt::LoadUnit>{});unsigned queued=0;
 for(auto&a:plan){if(boarding.size()>=128)break;boarding.push_back({crafts[a.second],candidates[a.first],t+600,0});queued++;}
 char line[120];snprintf(line,sizeof(line),"LOAD_NEARBY aircraft=%u candidates=%u queued=%u flying=%u",(unsigned)crafts.size(),(unsigned)candidates.size(),queued,flying);log(line);
 say(queued?"":"No units that can board are nearby, or no seats are free.");
}
// Settler max health: the settler XML 'health' byte (loader RVA 0x183D5C) at
// +2 of 36-byte records indexed race*0x46+type at RVA 0x1146930. Spawning
// copies it to entity +0x21 (RVA 0x180480/0x18048A); the native selection
// bracket draws 7*hp/max and skips max 0 (CSettler slot 5, RVA 0x181398).
// The same reads are checked here once; on mismatch the panel shows 100%.
static bool unitHealthPinned(){
 static int pinned=0;if(pinned)return pinned>0;
 uint32_t field=(uint32_t)(image+0x1146932);BYTE draw[8]={0x0f,0xb6,0x34,0x85},spawn[7]={0x8a,0x04,0x85},store[3]={0x88,0x47,0x21};memcpy(draw+4,&field,4);memcpy(spawn+3,&field,4);
 bool ok=readable((void*)(image+0x180480),16)&&readable((void*)(image+0x181398),8)&&!memcmp((void*)(image+0x181398),draw,8)&&!memcmp((void*)(image+0x180480),spawn,7)&&!memcmp((void*)(image+0x18048a),store,3);
 pinned=ok?1:-1;if(!ok)log("UNIT_PANEL settler health table pins mismatch; health shown as 100%");return ok;
}
static int unitHealth(const BYTE*p){
 if(!unitHealthPinned())return 100;uint32_t race=p[0x20]&15,type=peek<uint16_t>((uintptr_t)p+0xc);if(race>4||type>=0x46)return 100;
 return dtunits::healthPercent(p[0x21],peek<BYTE>(image+0x1146930+(race*0x46+type)*36+2));
}
// Own, living, visible Dark settler (not inside a building or aircraft).
static bool darkUnit(uint16_t id){auto*p=entity(id);return p&&p[10]==1&&owned(id)&&!(peek<uint32_t>((uintptr_t)p+0x14)&0x8000);}
// Dark unit selection panel (agent C): fill view.unit* from the native selection
// when no aircraft/building detail is shown; commands 40..49 belong to it.
static void unitPanelView(View&v){
 if(v.isAircraft||v.site||cachedSelection.empty())return;
 if(v.selected){auto*s=entity(v.selected);if(s&&s[10]!=1)return;} // A building/site panel has priority.
 dtunits::Member members[100];size_t n=0;
 for(auto u:cachedSelection){if(n>=100)break;if(!darkUnit(u.id)||identity(u.id)!=u.identity)continue;auto*p=entity(u.id);members[n++]={kind(u.id),(uint8_t)unitHealth(p)};}
 if(!n)return;
 auto s=dtunits::summarize(members,n);
 v.unitPanel=true;v.unitTypes=s.types;v.unitTotal=s.total;v.unitMilitary=s.military;
 memcpy(v.unitKind,s.kind,sizeof(v.unitKind));memcpy(v.unitCount,s.count,sizeof(v.unitCount));memcpy(v.unitHealth,s.health,sizeof(v.unitHealth));
}
// The current native selection, read on the tick (never the panel's copy).
static void darkSelection(std::vector<uint16_t>&all,std::vector<uint16_t>&units){
 all.clear();units.clear();WORD ids[100]{};SIZE_T count=0;if(!api->GetSelection(ids,100,&count))return;
 for(SIZE_T i=0;i<std::min<SIZE_T>(count,100);i++){all.push_back(ids[i]);if(darkUnit(ids[i]))units.push_back(ids[i]);}
}
template<class Keep> static void keepSelection(Keep keep,const char*what,int code){
 std::vector<uint16_t> all,units;darkSelection(all,units);std::vector<WORD> drop;unsigned kept=0;uint16_t last=0;
 for(auto id:all){bool k=std::find(units.begin(),units.end(),id)!=units.end()&&keep(kind(id));if(k){kept++;last=kind(id);}else drop.push_back(id);}
 char line[140];
 if(!kept){snprintf(line,sizeof(line),"No %s left in the selection.",what);say(line);return;}
 SIZE_T removed=0;if(!drop.empty()){api->RemoveSelection(drop.data(),drop.size(),&removed);dtmem::invalidate();}
 snprintf(line,sizeof(line),"UNIT_PANEL action=%d keep=%s selected=%u kept=%u removed=%u",code,what,(unsigned)all.size(),kept,(unsigned)removed);log(line);
 (void)last;say("");
}
static void unitBoard(uint32_t t){
 std::vector<uint16_t> all,units;darkSelection(all,units);std::vector<dt::UnitRef> candidates;std::vector<dt::LoadUnit> spots;
 for(auto id:units){auto u=unitRef(id);if(!eligiblePassenger(u))continue;int x,y;position(id,x,y);candidates.push_back(u);spots.push_back({x,y});}
 if(candidates.empty()){say("Only Warriors, Throwers, Gardeners and Shamans can board a Manacopter.");return;}
 auto inReach=[&](int ax,int ay){for(auto&s:spots){long dx=s.x-ax,dy=s.y-ay;if(dx*dx+dy*dy<=(long)dt::LoadRadius*dt::LoadRadius)return true;}return false;};
 std::vector<dt::UnitRef> crafts;unsigned flying=0;
 for(auto craft:cachedFleet){if(!aircraft(craft.id)||identity(craft.id)!=craft.identity||!passengerStorage(craft.id))continue;int ax,ay;position(craft.id,ax,ay);if(!inReach(ax,ay))continue;if(!landed(craft.id)){flying++;continue;}crafts.push_back(craft);}
 if(crafts.empty()){say(flying?"Nearby Manacopters are flying. Land one first, then board.":"No landed Manacopter within 24 tiles of the selection.");return;}
 // Re-queue the selected units only: their older boarding orders are replaced.
 for(auto u:candidates)boarding.erase(std::remove_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.unit,u);}),boarding.end());
 std::vector<dt::LoadCraft> slots;int seats=0;
 for(auto craft:crafts){int waiting=(int)std::count_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.craft,craft);});int ax,ay;position(craft.id,ax,ay);int open=std::max(0,6-(int)passengers(craft.id)-waiting);seats+=open;slots.push_back({ax,ay,open});}
 auto plan=dt::assignNearby(slots,spots);unsigned queued=0;
 for(auto&a:plan){if(boarding.size()>=128)break;boarding.push_back({crafts[a.second],candidates[a.first],t+600,0});queued++;}
 char line[160];snprintf(line,sizeof(line),"UNIT_PANEL action=41 board candidates=%u aircraft=%u seats=%d queued=%u flying=%u",(unsigned)candidates.size(),(unsigned)crafts.size(),seats,queued,flying);log(line);
 if(!queued){say("The nearby Manacopters are full.");return;}
 unsigned left=(unsigned)candidates.size()-queued;
 if(left)snprintf(line,sizeof(line),"%u unit%s found no free seat.",left,left==1?"":"s");
 else line[0]=0;
 say(line);
}
static void unitCommand(const Command&c,uint32_t t){
 switch(c.type){
  case dtui::UnitKeepType:{char what[32];snprintf(what,sizeof(what),"%ss",dtunits::name((uint16_t)c.arg));keepSelection([&](uint16_t k){return k==(uint16_t)c.arg;},what,40);break;}
  case dtui::UnitBoard:unitBoard(t);break;
  default:break;
 }
}
static void nextAircraft(){uint16_t first=0,next=0;for(uint32_t id=1;id<65536;id++)if(aircraft((uint16_t)id)){if(!first)first=(uint16_t)id;if(id>chosenAircraft&&!next)next=(uint16_t)id;}{uint16_t n=next?next:first;if(n)setPick({unitRef(n)});else setPick({});}buildOverride=false;if(chosenAircraft){api->ClearSelection();lastSelection=0;int x,y;position(chosenAircraft,x,y);api->MapSetScreenPos(x,y);}say(chosenAircraft?"":"No Manacopter ready. Build one at a Manacopter Hall.");}
static uint32_t* templeQueues(uint16_t id){auto*p=entity(id);if(!p||p[10]!=8||!owned(id)||!standing(id)||isSite(id)||(kind(id)!=50&&kind(id)!=51))return nullptr;BYTE*r=role(id);if(!readable(r,0x198)||!strstr(rtti(peek<uintptr_t>((uintptr_t)r)),"CDarkTempleRole"))return nullptr;return (uint32_t*)(r+0x180);}
// Temple/Fortress control cluster: one native order for one unit type (+1, +5,
// -1, -5, 0 = clear, 100 = continuous), applied on the tick to the Dark Temple
// role queue with the native semantics (queue-model.hpp). templeQueues checks
// the building (owned, completed, Temple/Fortress role); the farm-worker queue
// is never touched.
static void recruitOrder(const Command&c){
 uint32_t*q=templeQueues(c.entity);int index=dt::unitQueue(c.arg),row=-1;for(int r=0;r<4;r++)if(dtui::RecruitUnits[r]==c.arg)row=r;
 if(!q||index<0||row<0){say("Recruitment is not available for this building yet.");return;}
 uint32_t before=q[index];dt::orderQueue(q,c.arg,c.amount);uint32_t after=q[index];
 say(""); /* the cell count and faded controls show the result, as in the native barracks */char line[120];snprintf(line,sizeof(line),"RECRUIT_ORDER building=%u unit=%d amount=%d queue=%u->%u",c.entity,c.arg,c.amount,before,after);log(line);
}
static void actions(std::vector<Command>&todo,uint32_t t){for(auto&c:todo){dtmem::invalidate();if(c.type==1){if(buildingLimit.exists((uint16_t)c.arg)){say(dt::limitNotice((uint16_t)c.arg));continue;}if(!dt::affordable(mana(),dt::buildCost((uint16_t)c.arg),state.constructionCount,dt::MaxJobs)){say("Not enough mana for this building.");continue;}if(api->StartBuildingPlacement((S4_BUILDING_ENUM)c.arg)){placementHints.reset();say("");}else say("Placement could not start. Open the construction menu again.");continue;}if(c.type==17){requestBoarding(c,t);continue;}if(c.type==16&&c.arg==0){std::vector<dt::UnitRef> armed;for(auto u:orderGroup(c.entity,c.identity))if(aircraft(u.id)&&identity(u.id)==u.identity&&passengers(u.id))armed.push_back(u);
   if(armed.empty()){say("No passengers on board.");continue;}unloadGroup=armed;unloadTargeting=true;say("");continue;}
  if(c.type==34){unloadTargeting=false;int mx,my;if(!screenToMap(c,mx,my)){say("Unload target is outside the map.");unloadGroup.clear();continue;}
   int taken[12][2];int count=0;unsigned started=0;for(size_t i=0;i<unloadGroup.size()&&i<12;i++){auto u=unloadGroup[i];if(!aircraft(u.id)||identity(u.id)!=u.identity||!passengers(u.id))continue;int ox,oy;if(startUnload(u,mx,my,t,taken,count,ox,oy)){taken[count][0]=ox;taken[count][1]=oy;count++;started++;}}
   unloadGroup.clear();say(started?"":"No clear landing spot near that place.");continue;}
  if(c.type==35){unloadTargeting=false;unloadGroup.clear();say("");continue;}
  if(c.type==16){auto group=orderGroup(c.entity,c.identity);if(group.size()==1){transport(c);continue;}unsigned emptied=0,tried=0;for(auto u:group){if(!aircraft(u.id)||identity(u.id)!=u.identity||!landed(u.id)||!passengers(u.id))continue;tried++;Command one{16,0,u.id,0,0,u.identity};transport(one);if(!passengers(u.id))emptied++;}char msg[100];snprintf(msg,sizeof(msg),"Only %u of %u Manacopters could unload.",emptied,tried);say(!tried?"No landed Manacopter has passengers.":emptied<tried?msg:"");continue;}if(c.type==31){pickAircraft(c);continue;}if(c.type>=40&&c.type<50){unitCommand(c,t);continue;}if(c.type==36){loadNearby(c,t);continue;}if(c.type==18){if(aircraft(c.entity)&&identity(c.entity)==c.identity){api->ClearSelection();lastSelection=0;dt::UnitRef u{c.entity,kind(c.entity),c.identity};if(c.arg){auto next=fleetPick;auto it=std::find_if(next.begin(),next.end(),[&](const dt::UnitRef&o){return dt::sameEntity(o,u);});if(it!=next.end())next.erase(it);else next.push_back(u);setPick(next);}else setPick({u});buildOverride=false;say("");}continue;}if(c.type==30){dt::UnitRef best{};int bestDistance=1<<30;for(auto craft:cachedFleet){if(!aircraft(craft.id)||identity(craft.id)!=craft.identity||!landed(craft.id))continue;int ax,ay;position(craft.id,ax,ay);if(!dt::boardingTarget(c.x,c.y,ax,ay))continue;int d=abs(c.x-ax)+abs(c.y-ay);if(d<bestDistance){bestDistance=d;best=craft;}}
   if(best.id){char line[120];snprintf(line,sizeof(line),"BOARD_ORDER craft=%u target=%u,%u units=%u",best.id,c.x,c.y,(unsigned)c.units.size());log(line);Command b{17,0,best.id,0,0,best.identity};b.units=c.units;requestBoarding(b,t);continue;}
   c.type=19;}
  if(c.type==19){for(auto unit:c.units)boarding.erase(std::remove_if(boarding.begin(),boarding.end(),[&](const Boarding&o){return dt::sameEntity(o.unit,unit);}),boarding.end());continue;}if(c.type==11){nextAircraft();continue;}if(c.type==13){setPick({});say("");continue;}if(c.type==dtui::AircraftKeep){keepAircraft(c);continue;}
 if(c.type==20){if(chosenAircraft==c.entity&&chosenIdentity==c.identity)destinationControls=!destinationControls;continue;}if(c.type==14){if(aircraft(c.entity)&&identity(c.entity)==c.identity&&chosenAircraft==c.entity){draftX=std::clamp((int)c.x+(c.arg==1?-10:c.arg==2?10:0),1,std::max(side()-2,1));draftY=std::clamp((int)c.y+(c.arg==3?-10:c.arg==4?10:0),1,std::max(side()-2,1));draftIdentity=c.identity;}continue;}if(c.type==33){int mx,my;if(!screenToMap(c,mx,my)||fleetPick.empty()){say("Flight target is outside the map.");continue;}c.type=32;c.x=(uint16_t)mx;c.y=(uint16_t)my;}
  if(c.type==6||c.type==12||c.type==32){auto group=c.type==32?fleetPick:orderGroup(c.entity,c.identity);int x=c.x,y=c.y;if(c.type==12&&!group.empty()){int a,b;position(group[0].id,a,b);uint16_t base=nearestBase(a,b);if(!base||!exitPoint(base,x,y)){say("No free landing spot at your base.");continue;}}unsigned accepted=0;for(size_t i=0;i<group.size();i++){int tx=x,ty=y;if(group.size()>1)dt::groupTarget((int)i,x,y,side(),tx,ty);cancelBoarding(group[i].id);dropUnloadJob(group[i].id);if(flight(group[i].id,group[i].identity,tx,ty))accepted++;}say(group.empty()?"No Manacopter selected.":accepted<group.size()?"Some Manacopters could not fly there.":"");continue;}
if(!owned(c.entity)||isSite(c.entity)||(c.identity&&identity(c.entity)!=c.identity))continue;uint16_t k=kind(c.entity);if(c.type==dtui::RecruitOrder&&(k==50||k==51)){recruitOrder(c);}else if(c.type==8&&(k==50||k==51)){dt::clearManualQueues(templeQueues(c.entity));say("");}else if((c.type==9||c.type==10)&&k==80){uint32_t refund=dt::cancelAircraft(state,c.entity,identity(c.entity),c.type==10);if(refund)changeMana((int)refund);say(refund?"":"No aircraft waiting at this Hall.");}else if(c.type==3&&k==80){if(mana()<300||state.aircraftCount>=dt::MaxJobs){say("A Manacopter needs 300 mana.");continue;}if(!changeMana(-300))return;uint32_t ready=dt::nextAircraftTime(state,c.entity,identity(c.entity),t);auto&j=state.aircraft[state.aircraftCount++];j.hall=c.entity;j.identity=identity(c.entity);j.readyAt=ready;say("");}else if(c.type==5){api->BuildingsCrush(c.entity);say("");}}}
// Pair custom state to the exact engine save, rather than merely to its filename.
struct Snapshot {char magic[8]={'D','T','S','A','V','E','2',0};BYTE hash[32]{};dt::State data{};uint32_t pending=0; /* Legacy diverted harvest; always zero in new saves. */};
static Snapshot pendingLoad{};static bool hasPendingLoad=false;
using CreateA=HANDLE(WINAPI*)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);using CreateW=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);using Close=BOOL(WINAPI*)(HANDLE);using Write=BOOL(WINAPI*)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static CreateA createA=nullptr;static CreateW createW=nullptr;static Close closeHandle=nullptr;static Write writeFile=nullptr;
struct Saving {HANDLE handle;std::wstring path;Snapshot snap;bool wrote=false;};static std::vector<Saving> saving;
static bool hashFile(const std::wstring&p,BYTE out[32]){return dtio::hashFile(p,out);}
static std::string hexHash(const BYTE*p){static const char digits[]="0123456789abcdef";std::string s;for(int i=0;i<32;i++){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
static std::string sidecar(const BYTE*p){return std::string(root)+"/save-states/"+hexHash(p)+".dts";}
static bool savePath(const std::wstring&p){if(p.size()<4)return false;std::wstring s=p.substr(p.size()-4);return _wcsicmp(s.c_str(),L".sav")==0;}
static void observedOpen(HANDLE handle,const std::wstring&p,DWORD access){if(handle==INVALID_HANDLE_VALUE||!savePath(p))return;if(access&GENERIC_WRITE){Snapshot snap;AcquireSRWLockShared(&stateLock);bool ready=initialized&&!fatal.load();snap.data=state;ReleaseSRWLockShared(&stateLock);if(ready&&dt::valid(snap.data)){AcquireSRWLockExclusive(&ioLock);saving.push_back({handle,p,snap});ReleaseSRWLockExclusive(&ioLock);}}if(access&GENERIC_READ){BYTE hash[32];if(!hashFile(p,hash))return;FILE*f=fopen(sidecar(hash).c_str(),"rb");if(!f)return;Snapshot snap;bool ok=fread(&snap,1,sizeof(snap),f)==sizeof(snap)&&memcmp(snap.magic,"DTSAVE2",7)==0&&!memcmp(snap.hash,hash,32)&&dt::valid(snap.data)&&snap.pending<=1000000;fclose(f);if(ok){AcquireSRWLockExclusive(&ioLock);pendingLoad=snap;hasPendingLoad=true;ReleaseSRWLockExclusive(&ioLock);}}}
static HANDLE WINAPI openA(LPCSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES q,DWORD d,DWORD f,HANDLE t){HANDLE h=createA(p,a,s,q,d,f,t);if(p){int n=MultiByteToWideChar(CP_ACP,0,p,-1,nullptr,0);if(n>0){std::wstring w(n,L'\0');MultiByteToWideChar(CP_ACP,0,p,-1,&w[0],n);w.resize(n-1);observedOpen(h,w,a);}}return h;}
static HANDLE WINAPI openW(LPCWSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES q,DWORD d,DWORD f,HANDLE t){HANDLE h=createW(p,a,s,q,d,f,t);if(p)observedOpen(h,p,a);return h;}
static BOOL WINAPI observedWrite(HANDLE h,LPCVOID p,DWORD n,LPDWORD written,LPOVERLAPPED over){BOOL ok=writeFile(h,p,n,written,over);if(ok&&n){AcquireSRWLockExclusive(&ioLock);for(auto&v:saving)if(v.handle==h)v.wrote=true;ReleaseSRWLockExclusive(&ioLock);}return ok;}
static BOOL WINAPI closeFile(HANDLE h){Saving found{};bool have=false;AcquireSRWLockExclusive(&ioLock);for(size_t i=0;i<saving.size();i++)if(saving[i].handle==h){found=saving[i];saving.erase(saving.begin()+i);have=true;break;}ReleaseSRWLockExclusive(&ioLock);BOOL ok=closeHandle(h);if(have&&found.wrote&&ok&&hashFile(found.path,found.snap.hash)){std::string path=sidecar(found.snap.hash),temp=path+".tmp";HANDLE file=createA(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD wrote=0;bool saved=file!=INVALID_HANDLE_VALUE&&WriteFile(file,&found.snap,sizeof(found.snap),&wrote,nullptr)&&wrote==sizeof(found.snap)&&FlushFileBuffers(file);if(file!=INVALID_HANDLE_VALUE)closeHandle(file);if(saved)saved=MoveFileExA(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;log(saved?"Save and construction/aircraft sidecar committed":"Save sidecar could not be written");}return ok;}
static bool iat(uintptr_t rva,void*hook,void**original){void**at=(void**)(image+rva);if(!*original)*original=*at;DWORD old;if(!VirtualProtect(at,4,PAGE_READWRITE,&old))return false;*at=hook;VirtualProtect(at,4,old,&old);return true;}
// Computer offence against the Dark Tribe (r27/r28, research/AI_OFFENCE_0215.md).
// The native AI only targets towers/castles, so every computer sector AI gets
// a director (offence-model.hpp) that uses the AI's own config, soldiers,
// gardeners and strength values: a native state-6 squad holds the borrowed
// soldiers, the plugin moves them, and the AI's gardeners green the ring
// around a Dark building so it collapses natively (the plugin's native destroy
// is only the fallback for an all-green ring). r28: six attack styles, short
// rests between campaigns, an assault calls the other computer enemies, and
// free gardeners green Dark land in the computer's own territory and on the
// Dark border. Tick-only and transient: mod squads restored from a save are
// adopted again on the first tick.
static dtoffence::Native nativeOffence;static dtoffence::Campaign campaigns[9][8];static bool offenceAdopted=false;
static const char*offenceReason[9][8];static uint32_t offenceDeclines[9][8];static bool offenceReleased=false;static uint32_t offenceRecruitAt[9],offenceOrderAt[9],offenceRecruitNote[9];static int offenceOrderAlive[9];static const char*offenceRazeNote="";static bool offenceNoEconomy[9];static const char*offenceRecruitWhy[9];static const char*offenceToolsWhy[9];static uint32_t offenceToolsNote[9];
// r29 review: the plugin's part of each computer's shovel order, watched every tick (transient, never in dt::State).
static dtoffence::ShovelWatch offenceShovelWatch[9];static uintptr_t offenceShovelEco[9];static uint32_t offenceShovelRestores[9],offenceShovelRestoreNote[9];
// r33: the home economy's iron-bar stock at the last 250-tick evaluation (-1 unknown): no shovel restore while it is 0.
static int offenceIron[9]={-1,-1,-1,-1,-1,-1,-1,-1,-1};static uint32_t offenceIronNote[9];
// An open call for allies: an assault on a Dark building that the other computer enemies join (transient).
struct OffenceCall {dtoffence::Target target{};uint32_t until=0;uint8_t caller=0;};static OffenceCall offenceCall;
// r30: the citadel fire as last logged (OFFENCE_FIRE_STATE on change) and the last native-soldier census note per player (transient).
static char offenceFireNote[320];static uint32_t offenceFireNativeAt[9];
static uint32_t offenceSmithNote[9]={},toolSurplusNote[9]={},toolSurplusCount[9]={}; /* r46: toolsmith diagnostics and surplus-order watch throttles */
static bool shovelsOwed[9]={};static uint32_t toolDropAt[9]={},toolDrops[9]={}; /* r49: shovel priority: owed shovels (offenceTools), last drop tick and count */
static uint32_t offenceFarNote[9]={}; /* r50: last OFFENCE_GARDENER_FAR listing per player */
// r36: last logged route per campaign (offenceRouteNote), transient.
struct RouteSeen {int phase=-1,legX=-1,legY=-1,vias=-1,endX=-1,endY=-1,vx[dtoffence::MaxVias]={},vy[dtoffence::MaxVias]={};uint32_t replanAt=0,same=0;bool homeless=false;};
static RouteSeen offenceRoutes[9][8];
static void offenceReset(){offenceAdopted=false;offenceReleased=false;for(auto&row:campaigns)for(auto&c:row)c=dtoffence::Campaign{};for(auto&row:offenceRoutes)for(auto&r:row)r=RouteSeen{};memset(offenceReason,0,sizeof(offenceReason));memset(offenceDeclines,0,sizeof(offenceDeclines));memset(offenceRecruitAt,0,sizeof(offenceRecruitAt));memset(offenceNoEconomy,0,sizeof(offenceNoEconomy));memset(offenceOrderAt,0,sizeof(offenceOrderAt));memset(offenceOrderAlive,0,sizeof(offenceOrderAlive));memset(offenceRecruitNote,0,sizeof(offenceRecruitNote));memset(offenceRecruitWhy,0,sizeof(offenceRecruitWhy));memset(offenceToolsWhy,0,sizeof(offenceToolsWhy));memset(offenceToolsNote,0,sizeof(offenceToolsNote));for(auto&w:offenceShovelWatch)w=dtoffence::ShovelWatch{};for(auto&c:toolSurplusCount)c=0;for(auto&n:toolSurplusNote)n=0;for(auto&n:offenceSmithNote)n=0;memset(shovelsOwed,0,sizeof(shovelsOwed));memset(toolDropAt,0,sizeof(toolDropAt));memset(toolDrops,0,sizeof(toolDrops));memset(offenceFarNote,0,sizeof(offenceFarNote));memset(offenceShovelEco,0,sizeof(offenceShovelEco));memset(offenceShovelRestores,0,sizeof(offenceShovelRestores));memset(offenceShovelRestoreNote,0,sizeof(offenceShovelRestoreNote));for(auto&v:offenceIron)v=-1;memset(offenceIronNote,0,sizeof(offenceIronNote));offenceCall=OffenceCall{};offenceRazeNote="";nativeOffence.resetTerritory();offenceFireNote[0]=0;memset(offenceFireNativeAt,0,sizeof(offenceFireNativeAt));}
static const char*offenceEvent(dtoffence::Event e){static const char*names[]={"DECLINE","LAUNCH","STAGE","ASSAULT","SIEGE","DESTROY","DEFER","RETARGET","RETURN","DISSOLVE","LOST","ADOPT","ORDER_IGNORED","AVOID","COLLAPSE","GREENING","GARDENERS","RECLAIM","FRONT","HUNT","JOIN","CALL","FIRE","CONVOY","DEFEND","DEFEND_FIRE"};return names[(int)e];}
// Gardeners held by any campaign of the player (a gardener serves one army at a time).
static bool offenceHeld(uint8_t p,uint16_t id){for(auto&c:campaigns[p])for(int i=0;i<c.gardeners;i++)if(c.gid[i]==id)return true;return false;}
// r36 diagnostics (fire guard and routes, log only): Euclidean clearance of a segment from the live fire centres (-1: no
// fire), and the cells of the straight hex line a->b off the land region (rocks, water) or with a building/blocked flag.
static double offenceSegFire(const dtoffence::Fire&f,int ax,int ay,int bx,int by){if(!f.on||f.z.empty())return -1;double b=1e18;for(auto&z:f.z)b=std::min(b,dtoffence::seg2(ax,ay,bx,by,z.x,z.y));return sqrt(b);}
static double offenceFireAt(const dtoffence::Fire&f,int x,int y){int d=dtoffence::clear2(x,y,f);return d>=dtoffence::NoFire?-1.0:sqrt((double)d);}
static void offenceLegLand(int ax,int ay,int bx,int by,int wanted,int&off,int&built){off=built=0;int steps=dtoffence::distance(ax,ay,bx,by);if(steps>2000)return;
 for(int k=0;k<=steps;k++){int x=steps?ax+(int)lround((double)(bx-ax)*k/steps):ax,y=steps?ay+(int)lround((double)(by-ay)*k/steps):ay;if(nativeOffence.region(x,y)!=wanted)off++;else if(nativeOffence.flags(x,y)&0x09)built++;}}
struct OffenceOps {
 int region=0,homeX=0,homeY=0;uint32_t now=0; /* r33: the sector home and the tick, for the targets' walking detour */
 bool sector(const dtoffence::Campaign&c){return nativeOffence.sector(c.player,c.sector)!=0;}
 dtoffence::Config config(uint8_t p){return nativeOffence.config(p);}
 dtoffence::Strength strength(const dtoffence::Campaign&c){return nativeOffence.strength(c.player,c.sector);}
 void targets(const dtoffence::Campaign&,std::vector<dtoffence::Target>&out){nativeOffence.targets(region,out,homeX,homeY,now);}
 bool alive(const dtoffence::Target&t){return nativeOffence.alive(t);}
 int defenders(int x,int y,int radius,bool weighted){return nativeOffence.defenders(x,y,radius,weighted);}
 void available(const dtoffence::Campaign&c,int x,int y,std::vector<dtoffence::Soldier>&out){nativeOffence.available(c.player,c.sector,x,y,out);}
 int raise(dtoffence::Campaign&c,const std::vector<dtoffence::Soldier>&chosen){operation="offence squad";int n=nativeOffence.raise(c.player,c.sector,chosen,c.squad);operation="idle";
  if(n<=0){char msg[160];snprintf(msg,sizeof(msg),"OFFENCE_RAISE_FAILED player=%u sector=%u wanted=%u reason=%s",c.player,c.sector,(unsigned)chosen.size(),nativeOffence.reason);log(msg);}return n;}
 bool members(const dtoffence::Campaign&c,std::vector<dtoffence::Soldier>&out){return nativeOffence.members(c.player,c.sector,c.squad,out);}
 // The native group order (event 0x1396 -> CGroupMgr 0x16A800) has no owner
 // test. Soldiers always FORWARD, also on the way home: WATCH would leave them
 // passive there. r33: escorted gardeners WATCH (park): the order copies the mode
 // into the event unchanged (0x16A9E3) and the gardener's handler pushes job 0xB0
 // (walk, then wait) without touching its work centre (0x179B90..0x179B99), so it
 // never greens its way off; every release is a FORWARD order. Destinations never
 // have flag 0x01, so no soldier is garrisoned. At most 100 units per order (formation table, 0x16A109).
 void order(const dtoffence::Campaign&c,std::vector<WORD>&ids,int x,int y,const char*what,S4_MOVEMENT_ENUM mode=S4_MOVEMENT_FORWARD){
  int n=side();if(!n||x<2||y<2||x>=n-2||y>=n-2||ids.empty())return;
  for(size_t at=0;at<ids.size();at+=100){SIZE_T count=std::min<size_t>(100,ids.size()-at);internalSend=true;BOOL ok=api->SendWarriors(x,y,mode,ids.data()+at,count,1);internalSend=false;dtmem::invalidate();
   if(!ok){char msg[140];snprintf(msg,sizeof(msg),"OFFENCE_ORDER_REFUSED player=%u sector=%u units=%u at=%d,%d what=%s",c.player,c.sector,(unsigned)count,x,y,what);log(msg);}}
 }
 void move(const dtoffence::Campaign&c,const std::vector<dtoffence::Soldier>&m,int x,int y,bool home){std::vector<WORD> ids;for(auto&s:m)if(nativeOffence.movable(s.id,s.uid))ids.push_back(s.id);order(c,ids,x,y,home?"army home":"army");}
 bool stand(const dtoffence::Campaign&,int x,int y,int pass){return nativeOffence.stand(x,y,pass,region);}
 bool free(int x,int y){return nativeOffence.free(x,y);}
 bool destroy(const dtoffence::Campaign&c,const dtoffence::Target&t){
  operation="offence native destroy";bool ok=nativeOffence.destroy(c.player,t);operation="idle";
  char msg[200];snprintf(msg,sizeof(msg),"OFFENCE_RAZE player=%u sector=%u target=%u kind=%u identity=%08lx at=%d,%d result=%s",c.player,c.sector,t.id,t.kind,(unsigned long)t.uid,t.x,t.y,ok?"destroyed":nativeOffence.reason);
  if(ok||strcmp(offenceRazeNote,nativeOffence.reason)){log(msg);offenceRazeNote=ok?"":nativeOffence.reason;}return ok; /* a refusal is logged once while it lasts */
 }
 bool dissolve(dtoffence::Campaign&c){operation="offence squad dissolve";bool ok=nativeOffence.dissolve(c.player,c.sector,c.squad);operation="idle";
  if(!ok){char msg[120];snprintf(msg,sizeof(msg),"OFFENCE_DISSOLVE_INCOMPLETE player=%u sector=%u",c.player,c.sector);log(msg);}else c.squad=0;return ok;}
 void gardeners(const dtoffence::Campaign&c,std::vector<dtoffence::Gardener>&out){std::vector<dtoffence::Gardener> all;nativeOffence.gardeners(c.player,region,all);out.clear();for(auto&g:all)if(!offenceHeld(c.player,g.id))out.push_back(g);}
 bool gardener(const dtoffence::Campaign&c,uint16_t id,uint32_t uid,dtoffence::Gardener&g){return nativeOffence.gardener(c.player,id,uid,g);}
 // One gardener per order: its destination becomes its work centre.
 void sendGardener(const dtoffence::Campaign&c,const dtoffence::Gardener&g,int x,int y){std::vector<WORD> ids;if(nativeOffence.movable(g.id,g.uid))ids.push_back(g.id);order(c,ids,x,y,"gardener");}
 // r33: one gardener per order, WATCH: it walks there and waits (job 0xB0), its work centre unchanged.
 void park(const dtoffence::Campaign&c,const dtoffence::Gardener&g,int x,int y){std::vector<WORD> ids;if(nativeOffence.movable(g.id,g.uid))ids.push_back(g.id);order(c,ids,x,y,"gardener parked",S4_MOVEMENT_WATCH);}
 // r33: Dark Gardeners and fighters in or near the player's territory, its owned Dark cells and their rise.
 void intrusion(const dtoffence::Campaign&c,dtoffence::Intrusion&in){nativeOffence.intrusion(c.player,region,in);}
 void ring(const dtoffence::Target&t,std::vector<dtoffence::RingCell>&out){nativeOffence.ring(t,out);}
 bool beside(const dtoffence::Target&t,const dtoffence::RingCell&r,int&x,int&y){return nativeOffence.beside(t,r,x,y);}
 // Soldiers standing on a Dark ring cell keep it Dark (the gardener's cell test skips occupied cells): move them out (at most 12 per pass), never into the citadel fire (r30).
 void clearFootprint(const dtoffence::Campaign&c,const dtoffence::Target&t,const std::vector<dtoffence::Soldier>&m,const dtoffence::Fire&f){
  std::vector<dtoffence::RingCell> ring;nativeOffence.ring(t,ring);int moved=0;auto cool=[&](int px,int py){return !dtoffence::inFire(px,py,f);};
  for(auto&s:m){if(moved>=12)break;bool blocking=false;for(auto&r:ring)if(r.dark&&r.x==s.x&&r.y==s.y)blocking=true;int x=0,y=0;if(!blocking||!nativeOffence.outside(t,s.x,s.y,cool,x,y))continue;std::vector<WORD> ids;if(nativeOffence.movable(s.id,s.uid))ids.push_back(s.id);order(c,ids,x,y,"off the ring");moved++;}
 }
 bool darkNear(const dtoffence::Campaign&,int x,int y,int radius,const std::vector<dtoffence::Keep>&keep,int&dx,int&dy){return nativeOffence.darkNear(x,y,radius,keep,region,dx,dy);}
 // Dark cells inside the computer's own territory (it cannot build there).
 bool ownDark(const dtoffence::Campaign&c,int x,int y,int radius,const std::vector<dtoffence::Keep>&keep,int&dx,int&dy){return nativeOffence.ownDark(c.player,x,y,radius,keep,region,dx,dy);}
 void darkFighters(const dtoffence::Campaign&,std::vector<std::pair<int,int>>&out){nativeOffence.fighters(out);}
 bool reach(const dtoffence::Campaign&,int x,int y){return nativeOffence.reach(x,y,region);}
 // r38: walking steps between two points of the land region (bridge field per goal, cached), -1 none, 0 unknown.
 int walkSteps(const dtoffence::Campaign&,int fx,int fy,int tx,int ty){operation="offence walk field";int s=nativeOffence.walkSteps(fx,fy,tx,ty,region);operation="idle";return s;}
 // r38 diagnostics: why no building can be attacked (every target with its reachability, walked path, detour and lock).
 void listTargets(const dtoffence::Campaign&c,const std::vector<dtoffence::Target>&all,const dtoffence::Fire&f,uint32_t t){
  std::string line;char head[120];snprintf(head,sizeof(head),"OFFENCE_TARGETS player=%u sector=%u fire=%d buildings=%u",c.player,c.sector,f.on?1:0,(unsigned)all.size());line=head;int shown=0;
  for(auto&b:all){if(shown++>=12)break;char u[160];snprintf(u,sizeof(u)," [kind=%u id=%u door=%d,%d reach=%d path=%d detour=%d locked=%d avoided=%d region=%d]",b.kind,b.id,b.doorX,b.doorY,b.reachable?1:0,b.path,b.detour,dtoffence::locked(b,f)?1:0,dtoffence::avoided(c,b.uid,t)?1:0,nativeOffence.region(b.doorX,b.doorY));line+=u;}
  if(all.size()>12){char more[32];snprintf(more,sizeof(more)," +%u more",(unsigned)(all.size()-12));line+=more;}
  char tail[64];snprintf(tail,sizeof(tail)," homeRegion=%d",region);line+=tail;log(line.c_str());
 }
 // r37: every target's walking path from (x,y) (one breadth-first search, limit = the longest detourLimit among them, at
 // most twice the map side); a target without a walk from here within the limit is not reachable for this choice.
 void paths(const dtoffence::Campaign&,int x,int y,std::vector<dtoffence::Target>&all){int limit=0;for(auto&t:all)if(t.reachable)limit=std::max(limit,dtoffence::detourLimit(dtoffence::distance(x,y,t.doorX,t.doorY)));
  int n=side();limit=std::min(limit,2*std::max(1,n));operation="offence path search";nativeOffence.walkAll(x,y,region,limit,all);operation="idle";for(auto&t:all)if(t.path<0)t.reachable=false;}
 // One campaign per parked citadel: the first (player, sector) of those besieging it works its ring.
 bool leads(const dtoffence::Campaign&c,const dtoffence::Target&t){for(uint8_t p=2;p<=8;p++)for(int i=0;i<8;i++){auto&o=campaigns[p][i];if(&o==&c||o.phase!=dtoffence::Phase::Assault||o.target.id!=t.id||o.target.uid!=t.uid)continue;if(p*8+i<c.player*8+c.sector)return false;}return true;}
 bool call(const dtoffence::Campaign&c,uint32_t t,dtoffence::Target&out){if(!offenceCall.until||t>=offenceCall.until||offenceCall.caller==c.player)return false;out=offenceCall.target;return true;}
 // Opens a call when another computer is an enemy of the Dark player too.
 bool announce(const dtoffence::Campaign&c,uint32_t t,const dtoffence::Target&target){bool others=false;for(uint8_t p=2;p<=8;p++)if(p!=c.player&&nativeOffence.enemyOfDark(p)&&nativeOffence.playerAI(p))others=true;
  if(!others)return false;offenceCall.target=target;offenceCall.until=t+dtoffence::CallTicks;offenceCall.caller=c.player;return true;}
 // The human's paid or pending Dark construction sites with their kind: gardeners keep away (greening would
 // collapse them); r30: a Farm site turns the citadel fire on, a Temple/Fortress site adds a fire centre.
 void sites(const dtoffence::Campaign&,std::vector<dtoffence::Site>&out){for(uint32_t i=0;i<state.constructionCount&&i<dt::MaxJobs;i++)if(!state.construction[i].refunded){dtoffence::Site s;s.x=state.construction[i].x;s.y=state.construction[i].y;s.kind=state.construction[i].kind;out.push_back(s);}}
 // r30: the native ready-farm counter of the Dark player (the citadel fire's own input), -1 unreadable.
 int readyFarms(){return nativeOffence.readyFarms();}
 bool spare(const dtoffence::Target&t,int x,int y){return nativeOffence.spare(t,x,y);}
 bool parked(const dtoffence::Target&t){return nativeOffence.parked(t);}
 void report(dtoffence::Event e,const dtoffence::Campaign&c){
  if(e==dtoffence::Event::Decline){uint32_t&n=offenceDeclines[c.player][c.sector];const char*&last=offenceReason[c.player][c.sector];bool fresh=last!=c.reason;last=c.reason;if(!fresh&&(++n%20))return;if(fresh)n=0;}
  /* r30: units=soldiers or gardeners ordered out, benched=gardeners dropped and sent home, to=the cell ordered (exit point). */
  if(e==dtoffence::Event::Fire){char note[300];snprintf(note,sizeof(note),"OFFENCE_FIRE player=%u sector=%u phase=%d style=%s target=%u kind=%u door=%d,%d units=%d benched=%d to=%d,%d reason=%s",c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),c.target.id,c.target.kind,c.target.doorX,c.target.doorY,c.value,c.versus,c.goalX,c.goalY,c.reason?c.reason:"");log(note);return;}
  /* r32: the convoy (rate-limited in the model): gap=farthest held gardener from the army centre (rally: distance to the gardeners' centre), limit=the gap above which the army holds (rally: RallyGap), left=gardeners left behind, wait=ticks the army held, at=army centre (rally: rally cell). */
  /* r33: the defence: start, recall, end (phase 3) or a declined one; units=Dark units in or near the territory near the area,
     fighters among them, own=owned Dark cells, rise=their growth in the last territory pass, area=the defended point. */
  if(e==dtoffence::Event::Defend){char note[360];snprintf(note,sizeof(note),"OFFENCE_DEFEND player=%u sector=%u phase=%d army=%u gardeners=%d units=%d fighters=%d own=%d rise=%d area=%d,%d landAt=%d,%d beyondFire=%d reason=%s",c.player,c.sector,(int)c.phase,(unsigned)c.initial,c.gardeners,c.threatUnits,c.threatFighters,c.threatOwn,c.threatRise,c.frontX,c.frontY,c.threatLandX,c.threatLandY,c.beyondFire?1:0,c.reason?c.reason:"");log(note);return;}
  /* r50: landAt=the land point a defence look weighed against the units (-1,-1 none; area=landAt: the defence goes to the land),
     beyondFire=1 when the straight way to the area it gave up on runs within FireLine of a fire centre. OFFENCE_DEFEND_FIRE: the
     Dark units a defence look left out within HuntKeep of a live fire centre (fighters, gardeners), nearest=the smallest
     Euclidean distance of one from its fire centre (centre=), outer=the largest (the unit nearest the HuntKeep line, at=),
     area=what the look chose, threat=what it made of the rest (none, land, units, serious). */
  if(e==dtoffence::Event::DefendFire){const dtoffence::FireDrop&d=c.fireDrop;char note[360];snprintf(note,sizeof(note),"OFFENCE_DEFEND_FIRE player=%u sector=%u phase=%d dropped=%d fighters=%d gardeners=%d nearest=%.1f centre=%d,%d outer=%.1f at=%d,%d units=%d own=%d rise=%d area=%d,%d threat=%s",c.player,c.sector,(int)c.phase,d.count,d.fighters,d.gardeners,d.nearest,d.cx,d.cy,d.outer,d.ax,d.ay,c.threatUnits,c.threatOwn,c.threatRise,c.frontX,c.frontY,c.reason?c.reason:"");log(note);return;}
  if(e==dtoffence::Event::Convoy){char note[300];snprintf(note,sizeof(note),"OFFENCE_CONVOY player=%u sector=%u phase=%d style=%s target=%u kind=%u door=%d,%d army=%u gardeners=%d gap=%d limit=%d left=%d wait=%u at=%d,%d reason=%s",c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),c.target.id,c.target.kind,c.target.doorX,c.target.doorY,(unsigned)c.initial,c.gardeners,c.gap,c.versus,c.value,c.waited,c.goalX,c.goalY,c.reason?c.reason:"");log(note);return;}
  /* r32: gap=farthest held gardener from the army centre at the last convoy look, wait=ticks the army has held for its gardeners. */
  char msg[400];snprintf(msg,sizeof(msg),"OFFENCE_%s player=%u sector=%u phase=%d style=%s raid=%u target=%u kind=%u door=%d,%d army=%u gardeners=%d ring=%u/%d value=%d versus=%d chance=%u gap=%d wait=%u reason=%s",
   offenceEvent(e),c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),(unsigned)c.raid,c.target.id,c.target.kind,c.target.doorX,c.target.doorY,(unsigned)c.initial,c.gardeners,c.progress,c.ringSize,c.value,c.versus,c.chance,c.gap,c.waited,c.reason?c.reason:"");log(msg);
 }
 /* r36 diagnostics: every unit the fire guard turned back, with the leg the campaign had ordered. leg=via k/n (k = index of
    the waypoint being walked to, n = waypoints; k=n: the last leg), from=leg start, to=the army's order cell, prev=start of
    the previous leg, legFire=clearance of the leg segment from the fire (Euclid tiles), legOff/legBuilt=cells of the straight
    leg off the land region (rocks, water) / with a building or blocked flag, age=ticks since the order, body/bodyFire=army
    body, exit=cell the soldiers were sent to. Per unit: id@x,y, fire=its clearance, off/prevOff=distance to the current /
    previous leg segment; gardeners also aim=native job target (aimFire), work (0 idle, 1 busy, 2 searching, 3 working),
    esc=WATCH-parked, cell=its campaign cell, centre=work centre, bench=1 when this recall benched it. kind=free-gardeners:
    reclaim's free gardeners (no leg). At most 8 units per line. */
 void pulled(const dtoffence::Campaign&c,const dtoffence::Fire&f,const std::vector<dtoffence::Soldier>&m,const std::vector<dtoffence::Pulled>&who,int ex,int ey){
  if(who.empty())return;bool free=who[0].free,gardeners=who[0].gardener;bool leg=!free&&c.orderAt!=0;
  int ox=0,oy=0;dtoffence::legFrom(c,ox,oy);int gx=leg?c.goalX:-1,gy=leg?c.goalY:-1,px=-1,py=-1;
  if(leg){if(c.viaAt>=2&&c.viaAt-2<c.vias){px=c.vx[c.viaAt-2];py=c.vy[c.viaAt-2];}else if(c.viaAt==1){px=c.legX;py=c.legY;}}
  int off=0,built=0;if(leg)offenceLegLand(ox,oy,gx,gy,region,off,built);dtoffence::Army b=m.empty()?dtoffence::Army{}:dtoffence::body(m);
  char head[460];snprintf(head,sizeof(head),"OFFENCE_FIRE_PULL player=%u sector=%u phase=%d style=%s kind=%s leg=via %d/%d from=%d,%d to=%d,%d prev=%d,%d legFire=%.1f legOff=%d legBuilt=%d age=%u body=%d,%d bodyFire=%.1f exit=%d,%d units=%u",
   c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),free?"free-gardeners":gardeners?"gardeners":"soldiers",leg?c.viaAt:-1,leg?c.vias:-1,leg?ox:-1,leg?oy:-1,gx,gy,px,py,leg?offenceSegFire(f,ox,oy,gx,gy):-1.0,off,built,leg?now-c.orderAt:0u,
   m.empty()?-1:b.x,m.empty()?-1:b.y,m.empty()?-1.0:offenceFireAt(f,b.x,b.y),ex,ey,(unsigned)who.size());
  std::string line=head;int shown=0;
  for(auto&q:who){if(shown++>=8)break;char u[260];double uo=leg?sqrt(dtoffence::seg2(ox,oy,gx,gy,q.x,q.y)):-1.0,po=px>=0?sqrt(dtoffence::seg2(px,py,ox,oy,q.x,q.y)):-1.0;
   if(!q.gardener)snprintf(u,sizeof(u)," [%u@%d,%d fire=%.1f off=%.1f prevOff=%.1f]",q.id,q.x,q.y,offenceFireAt(f,q.x,q.y),uo,po);
   else snprintf(u,sizeof(u)," [%u@%d,%d fire=%.1f off=%.1f prevOff=%.1f aim=%d,%d aimFire=%.1f work=%d esc=%d cell=%d,%d centre=%d,%d bench=%d]",q.id,q.x,q.y,offenceFireAt(f,q.x,q.y),uo,po,q.aimX,q.aimY,q.aimX>=0?offenceFireAt(f,q.aimX,q.aimY):-1.0,(int)q.work,(int)q.escorted,q.cellX,q.cellY,q.centreX,q.centreY,(int)q.benched);
   line+=u;}
  if(who.size()>8){char more[32];snprintf(more,sizeof(more)," +%u more",(unsigned)(who.size()-8));line+=more;}
  log(line.c_str());
 }
};
// The plugin stopped at runtime (fatal) or a save cannot be continued: hand
// every mod squad back to its AI, otherwise those soldiers would stand idle
// for the rest of the game. Tick only; native calls only when pins verified.
static bool offencePinned=false;
static void offenceStopLocked(){if(!offencePinned||offenceReleased)return;offenceReleased=true;operation="offence release";int n=nativeOffence.releaseAll();operation="idle";dtmem::invalidate();
 for(auto&row:campaigns)for(auto&c:row)c=dtoffence::Campaign{};char msg[96];snprintf(msg,sizeof(msg),"OFFENCE_RELEASE squads=%d (plugin stopped)",n);log(msg);}
// r29: shovels for the gardeners at the computer's own toolsmith. The native
// AI never orders shovels for gardeners (only for its diggers, 0x249835), and
// its building manager can also clear or lower the shovel order (0x249770
// sends 10-2*diggers: 0 at its 5-digger steady state, negative above). Every
// 250 ticks offenceTools tops the home economy's native shovel order up
// through 0xD6A90, as the toolsmith menu does, and remembers its own part of
// it (offenceShovelWatch, transient; the order byte itself is saved with the
// economy). Every tick offenceShovelGuard puts that part back after a clear.
// The plugin hook (0xE9B18) runs before the net loop dispatches the AI's 0x13A9
// event (0xE9B5E) and before the entity update, so a clear can cost the
// toolsmith one tick, not the whole 250-tick window. Logs each new order, a
// changed refusal (then every 6,000 ticks) and the first restore (then every
// 6,000 ticks, with the count); the normal states stay quiet.
// r46 diagnostics: the toolsmith's production gate as the engine reads it (research/AI_OFFENCE_0215.md Round r46), logged
// every 3,000 ticks while shovels are owed: state, worker, flags, every output pile (good:count/reserved, b while busy),
// their total against the capacity of 8, the input piles, the eco's tool orders and the round-robin index.
static void pileText(char*out,size_t n,const dtoffence::Native::ToolPile*list,int count){size_t at=0;out[0]=0;for(int i=0;i<count&&at<n;i++){int w=snprintf(out+at,n-at,"%s%d:%d/%d%s",i?",":"",list[i].good,list[i].count,list[i].reserved,list[i].busy?"b":"");if(w<0)break;at+=(size_t)w;}}
static void offenceSmithLog(uint8_t p,uintptr_t eco,const char*why){
 auto k=nativeOffence.toolsmith(p,eco);char out[200],in[100],ord[200];pileText(out,sizeof(out),k.out,k.outs);pileText(in,sizeof(in),k.in,k.ins);
 {size_t at=0;ord[0]=0;for(int i=0;i<k.tools&&at<sizeof(ord);i++){int w=snprintf(ord+at,sizeof(ord)-at,"%s%d:%d",i?",":"",k.goods[i],k.orders[i]);if(w<0)break;at+=(size_t)w;}}
 char msg[700];snprintf(msg,sizeof(msg),"OFFENCE_TOOLSMITH player=%u found=%d id=%u state=%d worker=%d flags=%x out=%s sum=%d/%d in=%s orders=%s product=%d last=%d clogged=%d reason=%s",(unsigned)p,k.found?1:0,(unsigned)k.id,k.state,k.worker,(unsigned)k.flags,out,k.outSum,dtoffence::OutputCapacity,in,ord,k.product,k.last,dtoffence::clogged(k.outSum)?1:0,why);log(msg);
}
static void offenceTools(uint8_t p,uint32_t t,const dtoffence::Native::Supply&supply,int wanted){
 auto tools=nativeOffence.tools(p,supply.eco);const char*why="";
 int amount=dtoffence::shovelOrder(supply.alive,supply.pending,supply.shovels,tools.ordered,tools.toolsmiths,&why,wanted);
 int target=dtoffence::shovelTarget(supply.alive,supply.pending,supply.shovels,wanted);auto&watch=offenceShovelWatch[p];offenceShovelEco[p]=supply.eco;
 shovelsOwed[p]=amount>0||(strcmp(why,"enough gardeners")&&strcmp(why,"spare shovels")); /* r49 */
 if(shovelsOwed[p]&&t-offenceSmithNote[p]>=3000){offenceSmithNote[p]=t;offenceSmithLog(p,supply.eco,amount>0?"ordering":why);} /* r46 */
 if(amount<=0){dtoffence::shovelNoted(watch,!strcmp(why,"already ordered"),tools.ordered,target);
  if(strcmp(why,"enough gardeners")&&strcmp(why,"spare shovels")&&(why!=offenceToolsWhy[p]||t-offenceToolsNote[p]>=6000)){offenceToolsWhy[p]=why;offenceToolsNote[p]=t;
   char msg[260];snprintf(msg,sizeof(msg),"OFFENCE_TOOLS player=%u toolsmiths=%d otherEconomy=%d order=%d->%d shovels=%d gardeners=%d pending=%d restores=%u reason=%s",p,tools.toolsmiths,tools.elsewhere,tools.ordered,tools.ordered,supply.shovels,supply.alive,supply.pending,offenceShovelRestores[p],why);log(msg);}
  return;}
 operation="offence shovel order";bool ok=nativeOffence.orderShovels(tools,supply.eco,amount);operation="idle";int after=nativeOffence.shovelOrdered(supply.eco);
 dtoffence::shovelNoted(watch,ok,after,target);
 const char*result=ok?"ordered":"order refused";if(!ok&&result==offenceToolsWhy[p]&&t-offenceToolsNote[p]<6000)return;offenceToolsWhy[p]=result;offenceToolsNote[p]=t;
 char msg[260];snprintf(msg,sizeof(msg),"OFFENCE_TOOLS player=%u toolsmiths=%d otherEconomy=%d order=%d->%d shovels=%d gardeners=%d pending=%d restores=%u reason=%s",p,tools.toolsmiths,tools.elsewhere,tools.ordered,after,supply.shovels,supply.alive,supply.pending,offenceShovelRestores[p],result);log(msg);
}
// Every tick, before the entity update: when the shovel order fell by two or
// more since the last look (a native clear, see dtoffence::shovelRestore), put
// back the plugin's part through 0xD6A90. Only while something is owed, on the
// same live economy and with a ready toolsmith there; otherwise forget it.
// r46: the toolsmith's output piles share a capacity of 8 goods (CProductionBuildingRole::sub_14CDC0); tools the AI
// ordered but nobody takes (pid 2476: six pickaxes and a hammer) fill it and then nothing is made, shovels included.
// Every ToolWatchEvery ticks, on the tick before the entity update: once the output is nearly full, clear the order of
// every non-shovel tool of which two or more already lie there (dtoffence::surplusOrder, r48: a batch order the AI sends
// only once, such as PICKAXE x12 for its mines, must run on while workers take the tools). Never the shovel order, never
// a continuous order. Logs the first clear, then every 6,000 ticks with the count.
static void offenceToolWatch(uint8_t p,uint32_t t){
 if(t%dtoffence::ToolWatchEvery)return;uintptr_t eco=offenceShovelEco[p];if(!eco)return;auto k=nativeOffence.toolsmith(p,eco);if(!k.found)return;
 for(int i=0;i<k.outs;i++){int good=k.out[i].good,order=-1;for(int j=0;j<k.tools;j++)if(k.goods[j]==good)order=k.orders[j];
  if(!dtoffence::surplusOrder(good,k.out[i].count,order,k.outSum))continue;operation="offence tool order clear";int before=nativeOffence.clearOrder(eco,good);operation="idle";toolSurplusCount[p]++;
  if(toolSurplusCount[p]==1||t-toolSurplusNote[p]>=6000){toolSurplusNote[p]=t;char msg[200];snprintf(msg,sizeof(msg),"OFFENCE_TOOL_SURPLUS player=%u good=%d lying=%d order=%d->0 clears=%u outSum=%d/%d",(unsigned)p,good,k.out[i].count,before,toolSurplusCount[p],k.outSum,dtoffence::OutputCapacity);log(msg);}}
 /* r49: shovel priority: with shovels owed and the output at DropAt, destroy one unreserved piece of the most numerous other
    tool through the pile's own removal, at most one per DropEvery ticks; r50: only from a pile with DropKeep or more lying
    pieces (never the last one), and a free shovel lying there no longer stops it (recruitment refuses it at the reserve). */
 if(shovelsOwed[p]&&t-toolDropAt[p]>=dtoffence::DropEvery){int i=dtoffence::dropCandidate(k.out,k.outs,k.outSum);
  if(i>=0){operation="offence tool drop";int after=nativeOffence.dropFromPile(k.out[i].id);operation="idle";
   if(after>=0){toolDropAt[p]=t;toolDrops[p]++;char msg[220];snprintf(msg,sizeof(msg),"OFFENCE_TOOL_DROP player=%u good=%d lying=%d->%d reserved=%d outSum=%d/%d drops=%u",(unsigned)p,k.out[i].good,k.out[i].count,after,k.out[i].reserved,k.outSum,dtoffence::OutputCapacity,toolDrops[p]);log(msg);}}}
}
static void offenceShovelGuard(uint8_t p,uint32_t t){
 auto&watch=offenceShovelWatch[p];if(watch.owed<=0)return;uintptr_t eco=offenceShovelEco[p];
 if(!nativeOffence.economyLive(p,eco)){watch=dtoffence::ShovelWatch{};return;}
 int before=nativeOffence.shovelOrdered(eco);int add=dtoffence::shovelRestoreWhile(watch,before,offenceIron[p]);
 /* r33: a clear while no iron bar is in stock is left alone (the field: 1,443 restores of an order no toolsmith could serve) */
 if(add<=0&&offenceIron[p]==0&&watch.seen==0&&before==0&&watch.owed>0&&(!offenceIronNote[p]||t-offenceIronNote[p]>=6000)){offenceIronNote[p]=t?t:1;char msg[160];snprintf(msg,sizeof(msg),"OFFENCE_TOOLS player=%u order=%d owed=%d restores=%u reason=no restore while no iron bar is in stock",p,before,watch.owed,offenceShovelRestores[p]);log(msg);}
 if(add<=0)return;
 auto tools=nativeOffence.tools(p,eco);
 operation="offence shovel restore";bool ok=nativeOffence.orderShovels(tools,eco,add);operation="idle";int after=nativeOffence.shovelOrdered(eco);
 if(ok){watch.seen=after;offenceShovelRestores[p]++;}else watch=dtoffence::ShovelWatch{};
 if(ok&&offenceShovelRestores[p]>1&&t-offenceShovelRestoreNote[p]<6000)return;offenceShovelRestoreNote[p]=t;
 char msg[220];snprintf(msg,sizeof(msg),"OFFENCE_TOOLS player=%u toolsmiths=%d order=%d->%d owed=%d restores=%u reason=%s",p,tools.toolsmiths,before,after,watch.owed,offenceShovelRestores[p],ok?"restored after a native clear":"restore refused");log(msg);
}
// r50 diagnostics (field pid 18584: the ledger counted 40 gardeners while recruitment saw 18..19 within LaunchReach of home):
// the player's gardeners in any land region beyond LaunchReach of home. Every 3,000 ticks one OFFENCE_GARDENER_FAR line per
// such gardener (at most 40): position, raw job word (0x32 idle, 0xAE walking to its cell, 0xAF greening, 0xB0 WATCH),
// work state (0 idle, 1 busy, 2 searching, 3 working), WATCH flag, distance, whether it stands in the home land region and
// the campaign holding it (sector:style/phase, or none). Returns how many there are (OFFENCE_RECRUIT far=). Log only.
static int offenceFarGardeners(uint8_t p,uint32_t t,const dtoffence::Strength&home){
 std::vector<dtoffence::Gardener> all;nativeOffence.gardeners(p,0,all);int distant=0;bool note=!offenceFarNote[p]||t-offenceFarNote[p]>=3000;int shown=0;
 for(auto&g:all){int d=dtoffence::distance(g.x,g.y,home.homeX,home.homeY);if(d<=dtoffence::LaunchReach)continue;distant++;if(!note||shown>=40)continue;shown++;
  char held[48]="none";for(int i=0;i<8;i++){auto&c=campaigns[p][i];for(int k=0;k<c.gardeners;k++)if(c.gid[k]==g.id&&c.guid[k]==g.uid)snprintf(held,sizeof(held),"%d:%s/%d",i,dtoffence::styleName(c.style),(int)c.phase);}
  char msg[240];snprintf(msg,sizeof(msg),"OFFENCE_GARDENER_FAR player=%u id=%u at=%d,%d job=0x%x work=%d esc=%d distance=%d homeRegion=%d held=%s",(unsigned)p,(unsigned)g.id,g.x,g.y,(unsigned)g.job,(int)g.work,g.escorted?1:0,d,nativeOffence.region(g.x,g.y)==home.region?1:0,held);log(msg);}
 if(note)offenceFarNote[p]=t?t:1;
 return distant;
}
// Native gardener recruitment per computer player, every 250 ticks: one
// order at a time through the player's own economy (carrier + shovel).
static void offenceRecruit(uint8_t p,uint32_t t){
 if(offenceRecruitAt[p]&&t-offenceRecruitAt[p]<250)return;offenceRecruitAt[p]=t;
 dtoffence::Config cfg=nativeOffence.config(p);if(!cfg.ok||cfg.mode<=0)return;
 dtoffence::Strength home{};for(int i=0;i<nativeOffence.sectorCount(p)&&!home.ok;i++)home=nativeOffence.strength(p,i);if(!home.ok)return;
 auto supply=nativeOffence.supply(p,home.homeX,home.homeY);
 if(!supply.eco){if(!offenceNoEconomy[p]){offenceNoEconomy[p]=true;char msg[120];snprintf(msg,sizeof(msg),"OFFENCE_RECRUIT player=%u no economy at home %d,%d",p,home.homeX,home.homeY);log(msg);}return;}
 /* r32: a large computer keeps more gardeners: GardenersWanted plus one per 10 soldiers beyond 40 (the free soldiers of its
    sectors plus those its armies took), at most 20 (dtoffence::gardenersWanted); the shovel orders follow the same number. */
 int soldiers=0;for(int i=0;i<nativeOffence.sectorCount(p);i++){std::vector<dtoffence::Soldier> free;nativeOffence.available(p,i,home.homeX,home.homeY,free);soldiers+=(int)free.size();}
 for(auto&c:campaigns[p])if(c.phase!=dtoffence::Phase::Idle)soldiers+=c.initial;
 int wanted=dtoffence::gardenersWanted(soldiers);
 /* r37: only gardeners within LaunchReach of home count as alive for recruitment (field: 14 alive, all stranded far away,
    none replaced, 46,000 ticks of "no gardeners"); stranded ones are called home by the decision, and at most 2*wanted exist. */
 {std::vector<dtoffence::Gardener> all;nativeOffence.gardeners(p,home.region,all);int close=0;for(auto&g:all)if(dtoffence::distance(g.x,g.y,home.homeX,home.homeY)<=dtoffence::LaunchReach)close++;
  supply.alive=std::max(close,supply.alive-wanted);}
 int distant=offenceFarGardeners(p,t,home); /* r50 diagnostics */
 gardenerShort[p]=std::max(0,wanted-supply.alive-supply.pending); /* r47: room in the carrier reserve for the gardeners still missing */
 offenceIron[p]=nativeOffence.stock(supply.eco,20); /* r33: IRONBAR (20), as the census reads it */
 offenceTools(p,t,supply,wanted);
 /* Carriers walking to a shovel are invisible (not in the per-player lists):
    an order counts as coming until a gardener more is alive or 1,200 ticks passed. */
 int coming=offenceOrderAt[p]&&t-offenceOrderAt[p]<1200&&supply.alive<=offenceOrderAlive[p]?1:0;const char*why="";
 if(!dtoffence::recruitOrder(supply.alive,supply.pending,coming,supply.shovels,supply.carriers,&why,wanted)){
  /* Why no gardener is ordered (no shovels, no carriers): on a new reason, then every 6,000 ticks. */
  if(strcmp(why,"enough gardeners")&&strcmp(why,"order on its way")&&(why!=offenceRecruitWhy[p]||t-offenceRecruitNote[p]>=6000)){offenceRecruitWhy[p]=why;offenceRecruitNote[p]=t;
   char msg[240];snprintf(msg,sizeof(msg),"OFFENCE_RECRUIT_WAIT player=%u gardeners=%d wanted=%d pending=%d shovels=%d carriers=%u far=%d reason=%s",p,supply.alive,wanted,supply.pending,supply.shovels,(unsigned)supply.carriers,distant,why);log(msg);}
  return;}
 operation="offence gardener recruitment";bool ok=nativeOffence.recruit(supply);operation="idle";if(ok){offenceOrderAt[p]=t;offenceOrderAlive[p]=supply.alive;}
 char msg[240];snprintf(msg,sizeof(msg),"OFFENCE_RECRUIT player=%u gardeners=%d wanted=%d soldiers=%d pending=%d shovels=%d far=%d ordered=%u",p,supply.alive,wanted,soldiers,supply.pending,supply.shovels,distant,(unsigned)ok);log(msg);
}
// r30: the citadel fire every director step. OFFENCE_FIRE_STATE when it changes (the native ready-farm
// counter, the listed ready farms, Farm sites, on/off, the fire centres), and OFFENCE_FIRE_NATIVE (log
// only, per player at most every 300 ticks) for soldiers of the native AI (not in a mod squad) standing
// within FireLine on walkable Dark land: the plugin does not order soldiers the native agents hold.
static void offenceFire(OffenceOps&ops,uint32_t t){
 std::vector<dtoffence::Target> all;nativeOffence.targets(0,all);dtoffence::Campaign none;dtoffence::Fire f=dtoffence::fire(none,ops,all);
 char msg[320];int at=snprintf(msg,sizeof(msg),"OFFENCE_FIRE_STATE ready=%d listed=%d farmSites=%d on=%u centres=",f.ready,f.listed,f.farmSites,(unsigned)f.on);
 for(auto&z:f.z)if(at>0&&at<(int)sizeof(msg)-16)at+=snprintf(msg+at,sizeof(msg)-at,"%d,%d;",z.x,z.y);
 if(strcmp(msg,offenceFireNote)){memcpy(offenceFireNote,msg,sizeof(offenceFireNote));offenceFireNote[sizeof(offenceFireNote)-1]=0;log(msg);}
 if(!f.on||f.z.empty())return;
 for(uint8_t p=2;p<=8;p++){if(offenceFireNativeAt[p]&&t-offenceFireNativeAt[p]<300)continue;if(!nativeOffence.playerAI(p))continue;
  std::vector<uintptr_t> own;for(auto&c:campaigns[p])if(c.squad)own.push_back(c.squad);int x=0,y=0,n=nativeOffence.fireCensus(p,f,own,x,y);if(!n)continue;offenceFireNativeAt[p]=t;
  char note[160];snprintf(note,sizeof(note),"OFFENCE_FIRE_NATIVE player=%u soldiers=%d at=%d,%d (native AI soldiers within the fire line, not ordered by the plugin)",p,n,x,y);log(note);}
}
// r36 diagnostics: OFFENCE_ROUTE when a marching or homeward campaign's stored route changes (a plan), and when a replan
// (after the fire guard) or a held way home gave the same route again (same=how often in a row). from=route start,
// end=staging cell or home, army=initial soldiers, keep=legKeep(army), vias=waypoints with their fire clearance, legs=per leg
// clearance/cells off the land region/cells with a building flag (see OFFENCE_FIRE_PULL).
static void offenceRouteNote(const dtoffence::Campaign&c,uint32_t t,OffenceOps&ops){
 RouteSeen&r=offenceRoutes[c.player][c.sector&7];bool walk=c.phase==dtoffence::Phase::March||c.phase==dtoffence::Phase::Return;
 if(!walk){r=RouteSeen{};return;}
 int ex=c.phase==dtoffence::Phase::Return?ops.homeX:c.stageX,ey=c.phase==dtoffence::Phase::Return?ops.homeY:c.stageY;
 bool changed=r.phase!=(int)c.phase||r.legX!=c.legX||r.legY!=c.legY||r.vias!=c.vias||r.endX!=ex||r.endY!=ey;for(int i=0;i<c.vias&&i<dtoffence::MaxVias&&!changed;i++)if(r.vx[i]!=c.vx[i]||r.vy[i]!=c.vy[i])changed=true;
 bool replanned=r.replanAt&&!c.replanAt,held=!r.homeless&&c.homeless;r.replanAt=c.replanAt;r.homeless=c.homeless;
 if(!changed&&!replanned&&!held)return;
 char msg[900];
 if(!changed){r.same++;snprintf(msg,sizeof(msg),"OFFENCE_ROUTE player=%u sector=%u phase=%d style=%s from=%d,%d end=%d,%d vias=%d same=%u reason=%s",c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),c.legX,c.legY,ex,ey,c.vias,r.same,held?"way home held outside the fire, same route":"replanned after the fire guard, same route");log(msg);return;}
 r.phase=(int)c.phase;r.legX=c.legX;r.legY=c.legY;r.vias=c.vias;r.endX=ex;r.endY=ey;for(int i=0;i<c.vias&&i<dtoffence::MaxVias;i++){r.vx[i]=c.vx[i];r.vy[i]=c.vy[i];}r.same=0;
 std::vector<dtoffence::Target> all;ops.targets(c,all);dtoffence::Fire f=dtoffence::fire(c,ops,all);int army=std::max(1,(int)c.initial);
 std::string vias,legs;int ax=c.legX,ay=c.legY;
 for(int i=0;i<=c.vias&&i<=dtoffence::MaxVias;i++){int bx=i<c.vias?c.vx[i]:ex,by=i<c.vias?c.vy[i]:ey;char part[96];
  if(i<c.vias){snprintf(part,sizeof(part),"%s%d,%d(%.1f)",vias.empty()?"":";",bx,by,offenceFireAt(f,bx,by));vias+=part;}
  int off=0,built=0;offenceLegLand(ax,ay,bx,by,ops.region,off,built);snprintf(part,sizeof(part),"%s%.1f/%d/%d",legs.empty()?"":";",offenceSegFire(f,ax,ay,bx,by),off,built);legs+=part;ax=bx;ay=by;}
 snprintf(msg,sizeof(msg),"OFFENCE_ROUTE player=%u sector=%u phase=%d style=%s from=%d,%d end=%d,%d army=%d keep=%d fire=%d vias=%s legs=%s reason=%s",c.player,c.sector,(int)c.phase,dtoffence::styleName(c.style),c.legX,c.legY,ex,ey,army,dtoffence::legKeep(army),f.on?1:0,vias.empty()?"none":vias.c_str(),legs.c_str(),replanned?"replanned after the fire guard":"planned");log(msg);(void)t;
}
// r44: computer economy telemetry (research/ECONOMY_TELEMETRY_0215.md): one CSV row per computer every
// dtledger::EcoEvery ticks in evidence/v02-economy.csv (the header once per new file; every process appends, the ms
// and pid columns match the runtime log), read by tools/economy-report.py.
static bool ledgerChecked=false;
static void ledgerWrite(const dtledger::Ledger&l,uint32_t t){char path[MAX_PATH];snprintf(path,sizeof(path),"%s/evidence/v02-economy.csv",root);
 if(!ledgerChecked){ledgerChecked=true;char head[1200]="";FILE*r=fopen(path,"r");if(r){if(!fgets(head,sizeof(head),r))head[0]=0;fclose(r);size_t n=strlen(head);while(n&&(head[n-1]=='\n'||head[n-1]=='\r'))head[--n]=0;
  if(n&&strcmp(head,dtledger::csvHeader())){char old[MAX_PATH];snprintf(old,sizeof(old),"%s/evidence/v02-economy-%lu.csv",root,(unsigned long)GetTickCount());rename(path,old);log("Economy telemetry columns changed; the old file was renamed");}}} /* r46: a file with another header is set aside, never mixed */
 FILE*f=fopen(path,"a");if(!f)return;fseek(f,0,SEEK_END);if(ftell(f)==0)fprintf(f,"%s\n",dtledger::csvHeader());char row[2600];dtledger::csvRow(l,GetTickCount(),GetCurrentProcessId(),t,row,sizeof(row));fprintf(f,"%s\n",row);fclose(f);}
static void offenceTick(uint32_t t){
 nativeOffence.scanTerritory(16); /* computer-owned Dark land, a full pass every 32..36 ticks */
 for(uint8_t p=2;p<=8;p++){offenceShovelGuard(p,t);offenceToolWatch(p,t);} /* r29 review: put back a cleared shovel order before the entity update; r46: clear surplus tool orders */
 if(t%dtoffence::Period)return;OffenceOps ops;
 if(!offenceAdopted){offenceAdopted=true;
  for(uint8_t p=2;p<=8;p++)for(int i=0;i<nativeOffence.sectorCount(p);i++){std::vector<uintptr_t> found;nativeOffence.modSquads(p,i,found);
   for(size_t k=0;k<found.size();k++){auto&c=campaigns[p][i];c.player=p;c.sector=(uint8_t)i;std::vector<dtoffence::Soldier> m;nativeOffence.members(p,i,found[k],m);
    if(k==0){auto st=nativeOffence.strength(p,i);ops.region=st.region;ops.homeX=st.homeX;ops.homeY=st.homeY;ops.now=t;dtoffence::adopt(c,t,found[k],(int)m.size(),ops);}
    else{dtoffence::Campaign extra=c;extra.squad=found[k];ops.dissolve(extra);}}}}
 offenceFire(ops,t);
 for(uint8_t p=2;p<=8;p++){
  bool active=nativeOffence.enemyOfDark(p)&&nativeOffence.playerAI(p);int n=active?nativeOffence.sectorCount(p):0;
  if(active)offenceRecruit(p,t);else offenceShovelWatch[p]=dtoffence::ShovelWatch{};
  {auto&s=reserveSeen[p];auto&l=reserveLogged[p];bool held=s.native<s.keep,was=l.native<l.keep;if(s.keep&&(s.keep!=l.keep||held!=was||s.room!=l.room)){l=s;char msg[200];snprintf(msg,sizeof(msg),"CARRIER_RESERVE player=%u native=%d keep=%d buildings=%d set=%d held=%d room=%d builders=%d diggers=%d",(unsigned)p,s.native,s.keep,s.buildings,s.set,held?1:0,s.room,s.builders,s.diggers);log(msg);}} /* r43: the floor and whether it holds the AI's value up */
  if(active&&t%3000==0){int owned=nativeOffence.ownedDark(p);dtoffence::Strength home{};for(int i=0;i<n&&!home.ok;i++)home=nativeOffence.strength(p,i);
   /* What keeps this computer attacking: barracks, weapons, carriers, soldiers, gardeners, and the Dark land it owns. */
   if(home.ok){auto k=nativeOffence.census(p,home.homeX,home.homeY);char msg[360];snprintf(msg,sizeof(msg),"OFFENCE_SUPPLY player=%u barracks=%d swords=%d bows=%d raceWeapons=%d shovels=%d carriers=%d inFlight=%d reserve=%d keep=%d soldiers=%d gardeners=%d ownedDarkCells=%d economy=%u toolsmiths=%d shovelOrder=%d ironbars=%d coal=%d",
    p,k.barracks,k.swords,k.bows,k.special,k.shovels,k.carriers,k.busy,k.reserve,reserveSeen[p].keep,k.soldiers,k.gardeners,owned,(unsigned)k.eco,k.toolsmiths,k.shovelOrder,k.ironbars,k.coal);log(msg);}}
  if(active&&t%dtledger::EcoEvery==0){dtoffence::Strength home{};for(int i=0;i<n&&!home.ok;i++)home=nativeOffence.strength(p,i);{dtledger::Ledger row=nativeOffence.ledger(p,home.ok?home.homeX:-1,home.ok?home.homeY:-1,reserveSeen[p].keep);row.room=reserveSeen[p].room;ledgerWrite(row,t);}} /* r44: economy telemetry row; r50: with the specialist room */
  for(int i=0;i<8;i++){auto&c=campaigns[p][i];c.player=p;c.sector=(uint8_t)i;
   if(i>=n){if(c.phase!=dtoffence::Phase::Idle){if(!active&&nativeOffence.sector(p,i))ops.dissolve(c); /* no longer an enemy: give the soldiers back */c.reason=active?"sector gone":"not an enemy any more";ops.report(dtoffence::Event::Lost,c);dtoffence::toIdle(c,t);}continue;}
   auto st=nativeOffence.strength(p,i);ops.region=st.region;ops.homeX=st.homeX;ops.homeY=st.homeY;ops.now=t;dtoffence::step(c,t,ops);offenceRouteNote(c,t,ops);}
 }
}
// Dark-land sight marks (darkSightTick), transient: emptied on every map init.
// r42: the whole-map reveal toggle (revealTick), transient.
static bool revealAll=false,revealWas=false;static uint32_t revealLogAt=0;
static std::vector<uint8_t> sightShown;static bool sightFull=false,sightMissing=false;static uint32_t sightReturned=0,sightLogAt=0;
static HRESULT __stdcall mapInit(LPVOID,LPVOID){dtdiag::beat(false,false);running=false;placementHints.reset();AcquireSRWLockExclusive(&stateLock);initialized=false;newMap=true;offenceReset();ordinarySelectionPending=false;destinationControls=false;boarding.clear();cachedSelection.clear();cachedFleet.clear();lastSelection=0;chosenAircraft=0;chosenIdentity=0;draftIdentity=0;buildOverride=false;fleetPick.clear();cachedPick.clear();unloadJobs.clear();unloadGroup.clear();unloadTargeting=false;sightShown.clear();sightReturned=0;sightLogAt=0;revealAll=false;revealWas=false;revealLogAt=0;for(auto&r:reserveSeen)r=ReserveSeen{};for(auto&r:reserveLogged)r=ReserveSeen{};memset(gardenerShort,0,sizeof(gardenerShort));ReleaseSRWLockExclusive(&stateLock);AcquireSRWLockExclusive(&commandLock);commands.clear();ReleaseSRWLockExclusive(&commandLock);return 0;}
// Pinned 0.2.4 failure evidence: CBarrackRole needs an owned entrance economy.
// Read-only diagnostics run on ticks; they never patch game sector/player state.
static void logStartingSectors(){
 for(uint32_t id=1;id<65536;id++){auto*p=entity((uint16_t)id);if(!p||p[10]!=8||(p[32]>>4)!=2)continue;
  uint32_t door=peek<uint32_t>((uintptr_t)p+0x48);int x=door&65535,y=door>>16;if(x<0||y<0||x>=side()||y>=side())continue;
  uint32_t sector=api->LandscapeGetEcoSector(x,y),owner=api->LandscapeGetOwner(x,y);
  uint16_t economy=sector<32768?peek<uint16_t>(image+0xef1a26+sector*20):0;
  uintptr_t pool=economy?peek<uintptr_t>(image+0x106c8a4+economy*4):0;
  char msg[256];snprintf(msg,sizeof(msg),"START_SECTOR entity=%u identity=%08lx kind=%u door=%d,%d owner=%u sector=%u economy=%u economyPresent=%u role=%s",id,(unsigned long)identity((uint16_t)id),kind((uint16_t)id),x,y,owner,sector,economy,pool?1:0,rtti(peek<uintptr_t>((uintptr_t)role((uint16_t)id))));log(msg);
 }
}
static void initialize(uint32_t t){boarding.clear();offenceReset();destinationControls=false;state=dt::State{};AcquireSRWLockExclusive(&ioLock);bool restored=hasPendingLoad&&t>=pendingLoad.data.tick&&t-pendingLoad.data.tick<=5;if(restored){
 state=pendingLoad.data;
 uint64_t held=state.harvested-state.delivered-state.lost;
 if(!dt::retireLegacyFarmState(state)){fatal=true;running=false;log("Invalid legacy farm state; save refused");}
 if(held||state.harvested||pendingLoad.pending){char msg[192];snprintf(msg,sizeof(msg),"Legacy farm bookkeeping retired: held=%llu pending=%u; native bank and units unchanged",(unsigned long long)held,pendingLoad.pending);log(msg);}
}hasPendingLoad=false;ReleaseSRWLockExclusive(&ioLock);if(fatal)return;if(!restored&&(t>100||luaConstant("DT_IsNew")!=1)){fatal=true;running=false;say("This saved game cannot be continued. Start a new game of this scenario.");log("Unsupported save or missing sidecar; economy disabled");return;}state.tick=t;initialized=true;newMap=false;running=true;say(restored?"":"");fireStartPending=!restored;fireLoadPending=restored; /* r30: start-map farms in a citadel fire field on new maps; on a load, farms an older round left inside a field */log(restored?"State restored from matching save hash":"New Dark economy initialized");}
// r42: Alt+9 toggles the whole-map reveal (dt::revealPending every tick while on). The keys are polled on the tick
// (S4ModApi has no keyboard callback) and count only while this process owns the foreground window and a map runs.
static void revealTick(uint32_t t){
 HWND fg=GetForegroundWindow();DWORD pid=0;if(fg)GetWindowThreadProcessId(fg,&pid);
 bool down=pid==GetCurrentProcessId()&&inGame()&&(GetAsyncKeyState(VK_MENU)&0x8000)&&(GetAsyncKeyState('9')&0x8000);
 if(dt::pressed(down,revealWas)){revealAll=!revealAll;char msg[96];snprintf(msg,sizeof(msg),"MAP_REVEAL %s (Alt+9) tick=%u",revealAll?"on":"off",t);log(msg);}
 if(!revealAll)return;BYTE*land=peek<BYTE*>(image+0xd69220);uint32_t cells=(uint32_t)side()*side();if(!cells||!readable(land,cells*4))return;
 uintptr_t fogManager=image+0x1159d30;BYTE*fog=peek<BYTE*>(fogManager+0xc);if(peek<BYTE*>(fogManager+8)!=land||!readable(fog,cells))return;
 uint32_t n=dt::revealPending(fog,cells);if(n&&t-revealLogAt>=3000){revealLogAt=t;char msg[96];snprintf(msg,sizeof(msg),"MAP_REVEAL pending cleared cells=%u",n);log(msg);}
}
// Dark-land sight (dt::darkSight): Dark cells show visible, and cells a
// computer greened again go back to the native fog merge. The shown map is
// transient: mapInit empties it and the first pass re-checks every non-Dark
// cell once (also heals cells an older round left visible in a save).
static void darkSightTick(uint32_t t){BYTE*land=peek<BYTE*>(image+0xd69220);uint32_t cells=(uint32_t)side()*side();if(!cells||!readable(land,cells*4))return;
 uintptr_t fogManager=image+0x1159d30;BYTE*fog=peek<BYTE*>(fogManager+0xc); /* slot 2 (RVA 0x1AE600) stores the landscape at +8 and the fog byte map at +0xC */
 if(peek<BYTE*>(fogManager+8)!=land||!readable(fog,cells)){fog=nullptr;if(!sightMissing){sightMissing=true;log("DARK_SIGHT fog map unavailable; greened cells keep their fog until it returns");}}else sightMissing=false;
 if(sightShown.size()!=cells){sightShown.assign(cells,1);sightFull=true;}
 auto r=dt::darkSight(land,fog,sightShown.data(),cells);char msg[160];
 if(fog&&sightFull){sightFull=false;snprintf(msg,sizeof(msg),"DARK_SIGHT fog re-checked map=%d dark=%u handedBack=%u",side(),r.revealed,r.returned);log(msg);}else sightReturned+=r.returned;
 if(sightReturned&&t-sightLogAt>=600){snprintf(msg,sizeof(msg),"DARK_SIGHT greened cells handed back to native fog=%u dark=%u",sightReturned,r.revealed);log(msg);sightReturned=0;sightLogAt=t;}
}
static HRESULT __stdcall tick(DWORD,BOOL,BOOL){dtmem::Scope memoryScope;tickAt=GetTickCount64();dtdiag::beat(true,true);ubiServicesNote();if(hdIsActive){hdActive=hdIsActive()!=FALSE;bool blocked=hdMouseBlocked()!=FALSE;if(blocked!=hdBlocked.exchange(blocked)){log(blocked?"HD_STATE mouse blocked by HD (panel hidden)":"HD_STATE mouse unblocked");}}bool match=signature();sceneMatches=match;if(fatal||!match){if(fatal&&match&&peek<uint32_t>(image+0xe66b14)>=10){AcquireSRWLockExclusive(&stateLock);offenceStopLocked();ReleaseSRWLockExclusive(&stateLock);}running=false;return 0;}uint32_t t=peek<uint32_t>(image+0xe66b14);if(t<10)return 0;if(!scenarioVersion()){running=false;return 0;}nativeTick=t;AcquireSRWLockExclusive(&stateLock);if(!initialized||newMap){initialize(t);if(!fatal)logStartingSectors();}if(fatal){offenceStopLocked();ReleaseSRWLockExclusive(&stateLock);return 0;}running=true;if(t<state.tick){initialized=false;running=false;ReleaseSRWLockExclusive(&stateLock);return 0;}state.tick=t;std::vector<Command> todo;AcquireSRWLockExclusive(&commandLock);todo.swap(commands);ReleaseSRWLockExclusive(&commandLock);construction(t);if(fatal){ReleaseSRWLockExclusive(&stateLock);return 0;}actions(todo,t);dtmem::invalidate();if(fatal){ReleaseSRWLockExclusive(&stateLock);return 0;}bool hintsChanged=false;unsigned hintsChecked=0,hintsValid=0;operation="Dark placement hint scan";bool hintsOkay=placementHints.update(t,hintsChanged,hintsChecked,hintsValid);operation="idle";dtmem::invalidate();if(!hintsOkay){fatal=true;running=false;log("Pinned placement cache validation failed; plugin disabled");ReleaseSRWLockExclusive(&stateLock);return 0;}if(hintsChanged){char msg[140];snprintf(msg,sizeof(msg),"PLACEMENT_HINTS kind=%d checked=%u valid=%u automatic viewport scan",peek<int>(image+0x1086548),hintsChecked,hintsValid);log(msg);}{dtplacement::Hints::Diag d;int kind=0;dt::HintBox area;static uint64_t lastScanLog=0;static dtplacement::Hints::Diag lastLogged{};uint64_t now=GetTickCount64();
 if(placementHints.report(d,kind,area)&&(now-lastScanLog>=5000)&&memcmp(&d,&lastLogged,sizeof(d))){lastScanLog=now;lastLogged=d;char msg[260];
  snprintf(msg,sizeof(msg),"PLACEMENT_SCAN kind=%d view=%d,%d-%d,%d darkAnchors=%u valid=%u anchorFlags=%u (bits 01:%u 02:%u 04missing:%u 08:%u 10:%u 40:%u) terrain=%u footprintOrSlope=%u fireBlockade=%u",
   kind,area.left,area.top,area.right,area.bottom,d.dark,d.valid,d.flagged,d.bits[0],d.bits[1],d.bits[2],d.bits[3],d.bits[4],d.bits[6],d.terrain,d.footprint,d.fire);log(msg);}}boardingOrders(t);unloadJobsTick(t);offenceTick(t);if(unloadTargeting.load()&&unloadGroup.empty())unloadTargeting=false;revealTick(t);if(t%5==0){aircraftJobs(t);darkSightTick(t);}

 if(!dt::valid(state)){say("Dark Tribe stopped after an error. Please restart the game.");fatal=true;running=false;log("Resource conservation failure");}
 cachedSelection.clear();cachedFleet.clear();
 WORD ids[100]{};SIZE_T selectedCount=0;if(api->GetSelection(ids,100,&selectedCount))for(size_t i=0;i<std::min<SIZE_T>(selectedCount,100);i++)if(owned(ids[i]))cachedSelection.push_back(unitRef(ids[i]));
 for(uint32_t id=1;id<65536;id++)if(aircraft((uint16_t)id))cachedFleet.push_back(unitRef((uint16_t)id));
 // Manacopter sight = native unit vision. The fog cycle's band pass (RVA
 // 0x1AE210) gives allied settlers/buildings a circle through RVA 0x1AE100
 // (pending bits 3-5 with a soft edge), which the merge turns into visible
 // terrain/objects and lets fade back when no longer refreshed. Aircraft
 // (class 0x40) are outside its class table, so add them the same way each tick.
 {uintptr_t fog=image+0x1159d30;int local=peek<int>(image+0xe9482c);using Circle=void(__thiscall*)(void*,int,int,int);
  int n=side();if(n&&readable((void*)peek<uintptr_t>(fog+0xc),(size_t)n*n))for(auto craft:cachedFleet){BYTE*q=entity(craft.id);if(!q||(q[32]>>4)!=local)continue;int ax,ay;position(craft.id,ax,ay);if(ax<2||ay<2||ax>=n-2||ay>=n-2)continue;((Circle)(image+0x1ae100))((void*)fog,ax,ay,dt::AircraftSight);}}
 {std::vector<dt::UnitRef> live;for(auto u:fleetPick)if(aircraft(u.id)&&identity(u.id)==u.identity)live.push_back(u);fleetPick=live;chosenAircraft=live.empty()?0:live[0].id;chosenIdentity=live.empty()?0:live[0].identity;cachedPick=fleetPick;
  // Mirror the native 'selected' bit 0x200 (box select, RVA 0xED3EF) on picked
  // aircraft. Not 0x400: that bit makes Cart/Settler/Ship code refresh GUI
  // panels through virtual +0x94, which the 31-slot Manacopter vtable lacks.
  // The visible bracket is drawn by the mod (aircraftMarkers).
  for(auto craft:cachedFleet){BYTE*q=entity(craft.id);if(!q||identity(craft.id)!=craft.identity||!readable(q,0x18))continue;uint32_t&bits=*(uint32_t*)(q+0x14);bool want=picked(craft);uint32_t next=want?((bits|0x200u)&~0x400u):(bits&~0x600u);if(next!=bits)bits=next;}}
 view=View{};view.ready=true;view.buildMenu=constructionMenu();view.mana=mana();view.selected=selected();{BYTE*sel=entity(view.selected);view.kind=sel&&sel[10]==8&&owned(view.selected)&&standing(view.selected)?kind(view.selected):0;} /* building types only; settler type ids overlap (49-51) */view.selectedIdentity=identity(view.selected);
 for(int c=0;c<4;c++)view.buildLimit[c]=buildingLimit.state((uint16_t)dtui::BuildTypes[c]); /* one Temple, one Fortress: their tiles are disabled while one exists */

 uint16_t nativeSelected=view.selected;
 if(chosenAircraft){view.selected=chosenAircraft;view.selectedIdentity=chosenIdentity;view.kind=0;view.isAircraft=true;auto*p=entity(chosenAircraft);int x,y;position(chosenAircraft,x,y);view.aircraftX=x;view.aircraftY=y;view.targetX=peek<uint16_t>((uintptr_t)p+0x48);view.targetY=peek<uint16_t>((uintptr_t)p+0x4a);if(draftIdentity!=chosenIdentity){draftX=x;draftY=y;draftIdentity=chosenIdentity;destinationControls=false;}view.coordinateMode=false;view.draftX=draftX;view.draftY=draftY;view.flightActive=(peek<uint16_t>((uintptr_t)p+0x4c)&1)!=0;uintptr_t a=peek<uintptr_t>((uintptr_t)p+0x58),b=peek<uintptr_t>((uintptr_t)p+0x5c);if(b>=a&&(b-a)%12==0&&(b-a)/12<=4096)view.passengers=(b-a)/12;view.landed=landed(chosenAircraft);view.aircraftHp=(uint8_t)std::max(1,dt::healthPercent(peek<uint8_t>((uintptr_t)p+0x21),dt::aircraftMaxHp(peek<int32_t>(image+dt::ManakopterHitpointsSetting))));passengerView(view);aircraftMarkers(view);view.unloadTargeting=unloadTargeting.load();aircraftPanelView(view);}
if(view.selected&&view.selected!=lastSelection&&(view.kind==49||view.kind==50||view.kind==51||view.kind==80))buildOverride=false;lastSelection=nativeSelected;view.site=!view.isAircraft&&owned(view.selected)&&isSite(view.selected);if(!view.site&&(view.kind==50||view.kind==51)){auto*q=templeQueues(view.selected);view.recruitmentReady=q!=nullptr;if(q)memcpy(view.queues,q,sizeof(view.queues));}{uint64_t c=recruitChoice.load();if(c&&((uint16_t)(c>>8)!=view.selected||(uint32_t)(c>>32)!=view.selectedIdentity))recruitChoice.compare_exchange_strong(c,0);}view.recruitPick=recruitPick(view);if(view.kind==80){uint32_t first=0xffffffff;for(uint32_t i=0;i<state.aircraftCount;i++){auto&j=state.aircraft[i];if(j.hall==view.selected&&j.identity==view.selectedIdentity){view.aircraftCount++;first=std::min(first,j.readyAt);}}if(view.aircraftCount){uint32_t left=first>t?first-t:0;view.aircraftSeconds=(left+9)/10;view.progress=100-(std::min(left,300u)*100/300);}}unitPanelView(view);if(noticeAt.load()&&GetTickCount64()-noticeAt.load()<NoticeMs)strncpy(view.message,notice,sizeof(view.message)-1);ReleaseSRWLockExclusive(&stateLock);return 0;}
static HRESULT __stdcall frame(LPDIRECTDRAWSURFACE7 surface,INT32 width,LPVOID){spriteFrame++;frameIngame=api->IsCurrentlyOnScreen(S4_SCREEN_INGAME)!=FALSE;frameAt=GetTickCount64();dtdiag::beat(frameIngame.load(),false);if(hdActive.load())return 0;if(fatal&&sceneMatches.load()&&frameIngame.load()){HDC dc=0;if(surface&&SUCCEEDED(surface->GetDC(&dc))){dtui::failure(dc,width);dtui::manaSection(dc,width,dtui::Data{});surface->ReleaseDC(dc);}return 0;}if(!running||!frameIngame.load())return 0;View v;if(!TryAcquireSRWLockShared(&stateLock))return 0;v=view;ReleaseSRWLockShared(&stateLock);if(!v.ready)return 0;v.recruitPick=recruitPick(v);pillar=width;bool detail=dthd::detail(v);bool build=buildOverride.load()||(!detail&&v.buildMenu);HDC dc=0;if(surface&&SUCCEEDED(surface->GetDC(&dc))){if(build)dtui::houseTab(dc,width);if(build||detail)dtui::render(dc,width,v,build);dtui::manaSection(dc,width,v);dtui::markersGdi(dc,v);if(v.unloadTargeting&&mouseX.load()>=0)dtui::badgeGdi(dc,mouseX.load()+14,mouseY.load()+14);surface->ReleaseDC(dc);}return 0;}
// Cache actual drawn aircraft rectangles. No Lua, native mutation or waiting
// on the economy lock occurs in rendering or input callbacks.
static BOOL __stdcall blt(LPS4BLTPARAMS p,BOOL){
 if(!running.load()||!p||p->isFogOfWar||p->destWidth<8||p->destHeight<8)return FALSE;
 if(!TryAcquireSRWLockShared(&stateLock))return FALSE;
 dt::UnitRef craft{};for(auto unit:cachedFleet)if(unit.id==p->settlerId){craft=unit;break;}
 ReleaseSRWLockShared(&stateLock);if(!craft.id||!TryAcquireSRWLockExclusive(&spriteLock))return FALSE;
 uint32_t now=GetTickCount();spriteHits.erase(std::remove_if(spriteHits.begin(),spriteHits.end(),[&](const dt::SpriteHit&r){return now-r.stamp>250;}),spriteHits.end());
 auto it=std::find_if(spriteHits.begin(),spriteHits.end(),[&](const dt::SpriteHit&r){return dt::sameEntity(r.unit,craft);});
 dt::SpriteHit fresh{craft,p->destX,p->destY,p->destX+p->destWidth,p->destY+p->destHeight,now,spriteFrame.load()};
 if(it==spriteHits.end()){if(spriteHits.size()<128)spriteHits.push_back(fresh);}else{
  if(it->frame==fresh.frame){fresh.left=std::min(fresh.left,it->left);fresh.top=std::min(fresh.top,it->top);fresh.right=std::max(fresh.right,it->right);fresh.bottom=std::max(fresh.bottom,it->bottom);}*it=fresh;
 }
 ReleaseSRWLockExclusive(&spriteLock);return FALSE;
}
static dt::UnitRef aircraftAt(int x,int y){
 dt::UnitRef found{};if(!TryAcquireSRWLockShared(&spriteLock))return found;
 uint32_t now=GetTickCount();for(auto it=spriteHits.rbegin();it!=spriteHits.rend();++it)if(dt::spriteContains(*it,x,y,now)){found=it->unit;break;}
 ReleaseSRWLockShared(&spriteLock);return found;
}
static bool panelInput(const View&shown,int x,int y,UINT message){
 View v=shown;v.recruitPick=recruitPick(v); /* a recruitment unit chosen since the last tick */
 // The native construction tab toggles the Dark construction page.
 if(dtui::inside(x,y,dtui::TabX,dtui::TabY,dtui::TabW,dtui::TabH)){if(message==WM_LBUTTONUP){buildOverride=!buildOverride.load();enqueue({13});}return true;}
 // The mana section covers the native resource panel whenever the menus draw: its clicks never reach the game.
 if(dtui::manaArea(x,y))return v.ready;
 bool detail=dthd::detail(v);bool build=buildOverride.load()||(!detail&&v.buildMenu);
 if(!(build||detail)||!dtui::inside(x,y,dtui::BoxX,dtui::BoxY,dtui::BoxW,dtui::BoxH))return false;
 if(message==WM_LBUTTONUP){auto hit=dtui::hit(v,build,x,y);
  /* Choosing a recruitment unit is UI state only: the highlight shows on the next frame. */
  if(hit.type==dtui::RecruitSelect){chooseRecruit(v,hit.arg);return true;}
  /* Keep one aircraft: the entity and identity of the clicked cell, validated on the tick. */
  if(hit.type==dtui::AircraftKeep){if(hit.arg>=0&&hit.arg<std::min<int>(v.airShown,6))enqueue(Command{dtui::AircraftKeep,0,v.airId[hit.arg],0,0,v.airIdentity[hit.arg]});return true;}
  if(hit.type){Command c{hit.type,hit.arg,v.selected,(uint16_t)v.draftX,(uint16_t)v.draftY,v.selectedIdentity};c.amount=hit.amount;enqueue(std::move(c));}}
 return true;
}
static bool hdSnapshot(dthd::Snapshot&out){
 if(!TryAcquireSRWLockShared(&stateLock))return false;
 out={};out.data=view;ReleaseSRWLockShared(&stateLock);out.data.recruitPick=recruitPick(out.data);
 out.disabled=fatal.load();out.hdBlocked=hdBlocked.load();out.scene=sceneMatches.load();out.running=running.load();out.ingame=inGame();
 out.active=hdActive.load()&&!out.hdBlocked&&out.scene&&out.ingame&&(out.disabled||(out.running&&out.data.ready));
 out.build=buildOverride.load()||(!dthd::detail(out.data)&&out.data.buildMenu);return true;
}
static void hdInput(const dthd::Snapshot&shown,int x,int y,UINT message){
 if(!running.load()||fatal.load()||hdBlocked.load())return;
 // Capture the entity/identity actually displayed; tick validation rejects stale targets.
 panelInput(shown.data,x,y,message);
}
static void hdPanelPrepared(bool ok){hdPanelStatus=ok?1:-1;}
static DWORD WINAPI hdPanelThread(void*){dthd::Host host(ownModule,hdSnapshot,hdInput,log,hdPanelPrepared);host.run();return 0;}
// Right-button gesture, written and read only by mouse() on the game window's
// thread (S4ModApi calls the listener from the game's window procedure).
static dt::RightGesture rightGesture;
static HRESULT __stdcall mouse(DWORD,INT x,INT y,DWORD message,HWND hwnd,LPCS4UIELEMENT){
 // A right press and its release reach the game together or not at all: the
 // native right-drag camera pan keeps mouse capture (and, with LegacyMouse, a
 // hidden parked cursor) between them. See RightGesture in interaction-model.hpp.
 if(message==WM_MOUSEMOVE)rightGesture.sample(x,y); /* the pinned S4ModApi never delivers moves; harmless if one does */
 bool consumedPress=false;
 if(message==WM_RBUTTONUP){dt::RightGesture g=rightGesture.release();
  if(g.route==dt::RightGesture::Passed){
   // The game saw the press, so it also gets the release and ends its own pan.
   // After an aircraft-only press a click (no pan) flies the picked group to the
   // press point, where the native click aims. Resolved on the tick.
   if(g.aircraftOnly&&running.load()&&!buildOverride.load()){bool still=true;int cx=0,cy=0;
    if(TryAcquireSRWLockShared(&stateLock)){still=!cachedPick.empty()&&cachedSelection.empty();cx=view.renderW/2;cy=view.renderH/2;ReleaseSRWLockShared(&stateLock);}
    RECT r{};if(hwnd)GetClientRect(hwnd,&r);if(cx<32||cy<32){cx=r.right/2;cy=r.bottom/2;}
    if(still&&dt::rightClick(g,x,y,(uint32_t)GetMessageTime(),cx,cy)){Command c{33,0,0,(uint16_t)std::clamp(g.x,0,65535),(uint16_t)std::clamp(g.y,0,65535)};c.cw=(uint16_t)std::clamp((int)r.right,0,65535);c.ch=(uint16_t)std::clamp((int)r.bottom,0,65535);enqueue(std::move(c));}
   }
   mouseX=x;mouseY=y;return 0;
  }
  consumedPress=g.route==dt::RightGesture::Consumed;
 }
 bool aircraftPress=false;
 HRESULT result=[&]()->HRESULT{
  if(!running||!inGame())return 0;View v;std::vector<dt::UnitRef> units;
  if(!TryAcquireSRWLockShared(&stateLock))return !hdActive.load()&&(dtui::inside(x-pillar,y,dtui::TabX,dtui::TabY,dtui::TabW,dtui::TabH)||dtui::inside(x-pillar,y,dtui::BoxX,dtui::BoxY,dtui::BoxW,dtui::BoxH)||dtui::manaArea(x-pillar,y))?1:0;
  v=view;units=cachedSelection;ReleaseSRWLockShared(&stateLock);int left=pillar.load();
  mouseX=x;mouseY=y;
  if(!hdActive.load()&&panelInput(v,x-left,y,message))return 1;
  int menuWidth=200;if(hdActive.load()&&hwnd){RECT r{};GetClientRect(hwnd,&r);using GetDpi=UINT(WINAPI*)(HWND);auto dpi=(GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");auto geometry=dthd::Geometry::fit(r.bottom,dpi?dpi(hwnd):96);if(geometry.width)menuWidth=geometry.width;left=0;}

  // Unload targeting: the next map left-click chooses the spot (consumed);
  // right-click cancels. Resolved on the tick.
  if(unloadTargeting.load()&&(x<left||x>=left+menuWidth)&&y>=0){
   if(message==WM_LBUTTONDOWN||message==WM_RBUTTONDOWN)return 1;
   if(message==WM_LBUTTONUP){RECT r{};if(hwnd)GetClientRect(hwnd,&r);Command c{34,0,0,(uint16_t)std::clamp(x,0,65535),(uint16_t)std::clamp(y,0,65535)};c.cw=(uint16_t)std::clamp((int)r.right,0,65535);c.ch=(uint16_t)std::clamp((int)r.bottom,0,65535);enqueue(std::move(c));return 1;}
   if(message==WM_RBUTTONUP){enqueue({35});return 1;}
  }
  // Aircraft picking works whatever native menu tab is shown (the build tab
  // is the default with nothing selected). Only the custom build overlay and
  // native placement (resolved on the tick) suppress it.
  if(!buildOverride.load()&&(x<left||x>=left+menuWidth)&&y>=0){
   dt::UnitRef craft=aircraftAt(x,y);
   if(craft.id&&(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP)){
    bool additive=(GetKeyState(VK_SHIFT)<0)||(GetKeyState(VK_CONTROL)<0);
    if(message==WM_LBUTTONUP)enqueue({18,additive?1:0,craft.id,0,0,craft.identity});pressX=-1;
    if(message==WM_RBUTTONUP){Command c{17,0,craft.id,0,0,craft.identity};c.units=std::move(units);enqueue(std::move(c));}
    return 1;
   }
  }
  // Aircraft-only selection: the game sends no move order for an empty native
  // selection, so the release above turns a click into a flight. The press
  // still reaches the game, so its right-drag camera pan works as for units.
  if(!buildOverride.load()&&(x<left||x>=left+menuWidth)&&y>=0&&message==WM_RBUTTONDOWN&&TryAcquireSRWLockShared(&stateLock)){aircraftPress=!cachedPick.empty()&&cachedSelection.empty();ReleaseSRWLockShared(&stateLock);}
  // Renderer-independent aircraft picking: remember the press and resolve the
  // click/box on the tick. Native unit selection still receives the input.
  if(!buildOverride.load()&&(x<left||x>=left+menuWidth)&&y>=0){
   if(message==WM_LBUTTONDOWN){pressX=x;pressY=y;}
   if(message==WM_LBUTTONUP){RECT r{};if(hwnd)GetClientRect(hwnd,&r);int px=pressX.exchange(-1),py=pressY.load();if(px<0){px=x;py=y;}
    Command c{31,(GetKeyState(VK_SHIFT)<0)||(GetKeyState(VK_CONTROL)<0)?1:0,0,(uint16_t)std::clamp(px,0,65535),(uint16_t)std::clamp(py,0,65535)};c.x2=(uint16_t)std::clamp(x,0,65535);c.y2=(uint16_t)std::clamp(y,0,65535);c.cw=(uint16_t)std::clamp((int)r.right,0,65535);c.ch=(uint16_t)std::clamp((int)r.bottom,0,65535);enqueue(std::move(c));}
  }
  if(!hdActive.load()&&y>=181&&y<212&&message==WM_LBUTTONUP)buildOverride=false;
  return 0;
 }();
 // A press the game did not get keeps its release from the game too.
 if(message==WM_RBUTTONDOWN)rightGesture.press(x,y,(uint32_t)GetMessageTime(),result?dt::RightGesture::Consumed:dt::RightGesture::Passed,aircraftPress&&!result);
 return consumedPress?1:result;
}
static HRESULT __stdcall send(DWORD position,S4_MOVEMENT_ENUM,LPVOID){
 if(!running||buildOverride.load()||internalSend.load())return 0;View v;std::vector<dt::UnitRef> units;
 std::vector<dt::UnitRef> group;if(!TryAcquireSRWLockShared(&stateLock))return 0;v=view;units=cachedSelection;group=cachedPick;ReleaseSRWLockShared(&stateLock);
 // Selected aircraft fly to the target (spread over distinct tiles). Selected
 // ground units keep their native move order alongside.
 if(!group.empty()){bool queued=enqueue({32,0,0,(uint16_t)position,(uint16_t)(position>>16)});if(units.empty())return queued?1:0;}
 // An ordinary order either targets a landed aircraft (board it) or cancels
 // boarding for precisely those selected units. Resolved on the tick.
 if(!units.empty()){Command c{30,0,0,(uint16_t)position,(uint16_t)(position>>16)};c.units=std::move(units);enqueue(std::move(c));}
 return 0;
}
static DWORD WINAPI start(void*){char path[MAX_PATH];GetModuleFileNameA(ownModule,path,sizeof(path));std::string p(path);auto at=p.find_last_of("/\\");p=p.substr(0,at);at=p.find_last_of("/\\");p=p.substr(0,at);at=p.find_last_of("/\\");p=p.substr(0,at); // runtime/native/DarkTribe-0.2.15.dll -> project
 strncpy(root,p.c_str(),sizeof(root)-1);log("Native initialization started");image=(uintptr_t)GetModuleHandleW(L"S4_Main.exe");{bool dumps=false;bool ready=dtdiag::start(root,image,&nativeTick,&operation,dumps);log(ready?(dumps?"Crash and freeze diagnostics ready: fatal exceptions with stacks, freeze watchdog, freeze dumps":"Crash and freeze diagnostics ready: fatal exceptions with stacks, freeze watchdog; freeze dumps unavailable"):"Crash and freeze diagnostics unavailable (log handle or watchdog)");}AddVectoredExceptionHandler(1,exceptionTrace);const GUID guid={0x05104b9f,0x52d3,0x4904,{0x8d,0x6f,0xa7,0xc3,0x01,0x2e,0xab,0xdd}};S4CreateInterface(&guid,&api);if(!api){fatal=true;log("S4CreateInterface failed");return 1;}log("S4ModApi interface ready");
 HMODULE bridge=GetModuleHandleW(L"S4ModApi.dll");luaBegin=(LuaBlock)GetProcAddress(bridge,"lua_beginblock");luaEnd=(LuaBlock)GetProcAddress(bridge,"lua_endblock");luaGlobal=(LuaGlobal)GetProcAddress(bridge,"lua_getglobal");luaNumber=(LuaNumber)GetProcAddress(bridge,"lua_getnumber");if(!luaBegin||!luaEnd||!luaGlobal||!luaNumber){fatal=true;log("Scenario marker Lua exports missing");return 5;}BYTE lookup[]={0xa1,0,0,0,0,0xf2,0x0f,0x2c,0xc8,0x8b,0x34,0x85};uint32_t slot=(uint32_t)(image+0x113fc98);memcpy(lookup+1,&slot,4);if(memcmp((void*)(image+0x1cb557),lookup,sizeof(lookup))){fatal=true;log("Pinned aircraft script lookup mismatch; plugin disabled");return 6;}BYTE board[]={0x55,0x8b,0xec,0x83,0xec,0x0c,0x53,0x8b,0x5d,0x08,0x56,0x57,0x8b,0xf9,0x8b,0x34};BYTE unload[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0,0,0,0,0x64,0xa1,0,0,0,0};uint32_t seh=(uint32_t)(image+0xab9760);memcpy(unload+6,&seh,4);if(memcmp((void*)(image+0x16b890),board,sizeof(board))||memcmp((void*)(image+0x16b990),unload,sizeof(unload))){fatal=true;log("Pinned aircraft transport signatures mismatch");return 7;}if(!nativeConstructionIntact()){fatal=true;log("Pinned construction cancellation signature mismatch; plugin disabled");return 8;}{BYTE arrival[]={0x0f,0xb7,0x43,0x08,0x8b,0x0d,0,0,0,0,0x50,0xff,0x73,0x30,0xc6,0x43,0x42,0x0a,0xe8};uint32_t slotAddress=(uint32_t)(image+0x10540d4);memcpy(arrival+6,&slotAddress,4);
 BYTE readd[]={0x0f,0xb7,0x46,0x08,0x50,0x6a,0x01,0xe8};BYTE remove[]={0x55,0x8b,0xec,0x83,0xec,0x2c,0x8b,0x45,0x08,0x89,0x4d,0xfc};
 BYTE circle[]={0x55,0x8b,0xec,0x83,0xec,0x0c,0x8b,0x45,0x10,0x33,0xd2,0x53,0x8b,0x1d};BYTE band[]={0x51,0x50,0x52,0x8b,0xcb,0xe8};
 if(memcmp((void*)(image+0x1ae100),circle,sizeof(circle))||memcmp((void*)(image+0x1ae395),band,sizeof(band))||memcmp((void*)(image+0x189cd3),arrival,sizeof(arrival))||memcmp((void*)(image+0x132ebf),readd,sizeof(readd))||memcmp((void*)(image+0x16d720),remove,sizeof(remove))){fatal=true;log("Pinned passenger arrival signatures mismatch; plugin disabled");return 12;}}
 { /* r35: Dark-land sight relies on the fog merge (RVA 0x1AE3F0) skipping 8-cell groups whose fog bytes count 3 stable cycles (bits 6-7 == 0xC0), and on its step table 0xC1BD30 (0x40 count step, 0x18 explored, 0x07 current, 0x38 pending, 0xC0 counter) */
 BYTE merge[]={0xba,0x30,0xbd,0x01,0x01,0x8b,0x75,0xf4,0x8b,0x7d,0xf0,0x0f,0x6f,0x72,0x18,0x0f,0x6f,0x7a,0x20,0x8b,0x4d,0xec,0x0f,0x74,0xed,0x0f,0x6f,0x0e,0x0f,0x6f,0xd1,0x0f,0xdb,0x52,0x10,0x0f,0x6f,0xda,0x0f,0x73,0xf3,0x03,0x0f,0x6f,0xc1,0x0f,0xdb,0xc6,0x0f,0xeb,0xd3,0x0f,0x74,0xd8,0x0f,0xdb,0xcb,0x0f,0xdb,0x1a,0x0f,0xdc,0xcb,0x0f,0xdb,0xcf,0x0f,0x6f,0xdf,0x0f,0x74,0xd9,0x0f,0x7e,0xd8,0x0f,0x73,0xd3,0x20,0x0f,0x7e,0xdb,0x23,0xc3,0x0f,0xeb,0xca,0x0f,0x7f,0x0e,0x83,0xf8,0xff,0x0f,0x84,0x9d,0x00,0x00,0x00};uint32_t steps=(uint32_t)(image+0xc1bd30);memcpy(merge+1,&steps,4);BYTE table[40];const BYTE lanes[5]={0x40,0x18,0x07,0x38,0xc0};for(int i=0;i<40;i++)table[i]=lanes[i/8];
 if(memcmp((void*)(image+0x1ae438),merge,sizeof(merge))||memcmp((void*)(image+0xc1bd30),table,sizeof(table))){fatal=true;log("Pinned fog merge signature mismatch; plugin disabled");return 15;}}
 if(!nativeConstruction.initialize(image)){fatal=true;log("Pinned in-place construction signatures mismatch; plugin disabled");return 11;}if(!dtconstruction::PanelGuard::install(image)){fatal=true;log("Pinned site panel entry mismatch; plugin disabled");return 14;}if(!nativeOffence.initialize(image)){fatal=true;log("Pinned computer offence signatures mismatch; plugin disabled");return 13;}offencePinned=true;log("Pinned native rise, reservation and completion verified; reserved cells checked for occupancy only; tick read cache r3; ordinary-site rise, aircraft group and sight r4; site panel, passengers and terrain sight r5; native temporary aircraft vision r6; aircraft right-click r7; aircraft selection marker r8; targeted unload r9; load nearby r10; radius 24 r11; site eviction r12; eviction and HD panel diagnostics r13; wild animals do not block r14; native-style Dark UI and Manacopter health r15; native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23; Iron Kingdom scenario r24; The Alliance scenario and 512 maps r25; crash and freeze diagnostics r26; computer offence against the Dark Tribe r27; attack styles, re-attacks and territory greening r28; toolsmith shovel orders r29; citadel fire keep-off and farm blockade r30; clean start maps r31; scaled styles and protected convoy r32; computer defence and reclaim priority r33; site selection crash fix r34; reclaimed land fog and fast army marches r35; fire pull and route diagnostics r36; relentless and smart attacks r37; walked progress and steady sieges r38; Dark Gardeners leave the Manacopter r39; Iron Kingdom toolsmith hammers r40; no Manacopter healing r41; map reveal toggle r42; scaling carrier reserve r43; economy telemetry r44; economy-only keep and Iron Kingdom stone r45; toolsmith output watch r46; specialist room r47; tool orders until the output fills r48; shovel priority drop r49; gardener supply and land defence r50; Ubisoft services job guard r51");if(!placementHints.initialize(image)){fatal=true;log("Pinned Dark placement signatures mismatch; plugin disabled");return 10;}log("Pinned Dark placement validator and marker cache verified");if(!nativeFarmIncomeIntact()||!installIncomeHook()){fatal=true;log("Pinned income signature mismatch; plugin disabled");return 2;}if(!installNoHealing()){fatal=true;log("Pinned Manacopter healing signature mismatch; plugin disabled");return 16;}log("Manacopter healing on Dark land disabled (RVA 0x16B594)");if(!installReserveHook()){fatal=true;log("Pinned carrier reserve signature mismatch; plugin disabled");return 17;}log("Computer carrier reserve scaled with the kingdom (RVA 0x2567B8 -> 0x51C10)");if(!installUbiStepGuard()){fatal=true;log("Pinned Ubisoft services step guard signature mismatch; plugin disabled");return 18;}log("Ubisoft services job guard installed (RVA 0x46854C: a job step without a method parks the job instead of crashing the game)");log("Computer economy telemetry every 500 ticks to evidence/v02-economy.csv");
 {wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);std::wstring dir(exe);dir=dir.substr(0,dir.find_last_of(L"/\\")+1)+L"Gfx\\";char line[220]; // Read-only: pinned byte ranges of the game's own GUI/Dark sprites.
  if(dtgfx::library().load(dir))snprintf(line,sizeof(line),"Native GUI art ready: %s",dtgfx::library().why);else snprintf(line,sizeof(line),"Native GUI art unavailable (%s); drawn panel style kept",dtgfx::library().why);log(line);}
 HMODULE hd=GetModuleHandleW(L"S4HD-Manager.dll");if(hd){
  wchar_t file[MAX_PATH]{};BYTE actual[32]{};const BYTE expected[]={0x92,0x99,0xb9,0xf4,0xea,0x77,0xb6,0xb3,0x4b,0x43,0x89,0x77,0x06,0xa7,0x85,0x71,0x51,0x8c,0x77,0x60,0xe1,0x10,0x77,0x2b,0xd4,0x2d,0x86,0xd0,0x1b,0xd5,0xb0,0xa7};
  if(!GetModuleFileNameW(hd,file,MAX_PATH)||!hashFile(file,actual)||memcmp(actual,expected,32)){fatal=true;log("HD manager hash mismatch; plugin disabled");return 9;}
  log("HD manager hash verified before save hooks");
  hdIsActive=(HDBool)GetProcAddress(hd,"IsHDActive");hdMouseBlocked=(HDBool)GetProcAddress(hd,"GetMouseBlockState");
  if(!hdIsActive||!hdMouseBlocked){fatal=true;log("HD renderer status exports missing; plugin disabled");return 9;}
  HANDLE ui=CreateThread(nullptr,0,hdPanelThread,nullptr,0,nullptr);if(!ui){fatal=true;log("HD panel thread creation failed");return 9;}CloseHandle(ui);
  uint64_t deadline=GetTickCount64()+2500;while(!hdPanelStatus.load()&&GetTickCount64()<deadline)Sleep(10); // DLL startup worker only; never a game/render/input callback.
  if(hdPanelStatus.load()!=1){fatal=true;log("HD panel did not initialize; plugin disabled");return 9;}log("Pinned HD renderer: independent cached panel enabled");
 }
 createA=peek<CreateA>(image+0xb15318);createW=peek<CreateW>(image+0xb15320);closeHandle=peek<Close>(image+0xb152d4);writeFile=peek<Write>(image+0xb1528c);
 if(!iat(0xb15318,(void*)openA,(void**)&createA)||!iat(0xb15320,(void*)openW,(void**)&createW)||!iat(0xb152d4,(void*)closeFile,(void**)&closeHandle)||!iat(0xb1528c,(void*)observedWrite,(void**)&writeFile)){fatal=true;log("Save hook initialization failed");return 3;}
 S4HOOK mapHook=api->AddMapInitListener(mapInit),tickHook=api->AddTickListener(tick),frameHook=api->AddFrameListener(frame),mouseHook=api->AddMouseListener(mouse),sendHook=api->AddSettlerSendListener(send),bltHook=api->AddBltListener(blt);if(!mapHook||!tickHook||!frameHook||!mouseHook||!sendHook||!bltHook){fatal=true;log("One or more required callbacks failed to install");return 4;}log("Version 0.2.15 native callbacks installed");return 0;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*reserved){if(reason==DLL_PROCESS_ATTACH){ownModule=module;DisableThreadLibraryCalls(module);HANDLE h=CreateThread(nullptr,0,start,nullptr,0,nullptr);if(h)CloseHandle(h);}else if(reason==DLL_PROCESS_DETACH)dtdiag::processExit(reserved!=nullptr);return TRUE;}
