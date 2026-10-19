#pragma once
#include <stdint.h>
namespace dt {
// Generated scenarios (tools/scenarios.py keeps the same list; verify-offline checks both agree).
// Player 1 is the human Dark Tribe and the only Dark player; every other player is a computer.
struct Start {uint32_t race,x,y;};
struct Scenario {uint32_t size,players;Start start[4];};
constexpr Scenario Scenarios[]={
 {576,2,{{3,277,214},{1,75,401}}},                         // First Dawn
 {576,2,{{3,277,214},{1,261,279}}},                        // First Dawn, unchanged 0.1 layout of paired old saves
 {576,2,{{3,378,134},{1,475,353}}},                        // Iron Kingdom
 {512,4,{{3,303,423},{0,98,353},{1,367,116},{2,260,263}}}, // The Alliance
};
// The engine's session setup (RVA 0xE968A0): player count (+0x338) and nine-entry per-player arrays
// of slot type (+0x90; 0 human, 1 computer), race (+0x18C) and start x/y (+0xFC/+0x120); the map
// size is the view size (RVA 0xD6921C).
constexpr uint32_t MaxPlayers=9;
struct Setup {uint32_t size=0,players=0,slot[MaxPlayers]{},race[MaxPlayers]{},x[MaxPlayers]{},y[MaxPlayers]{};};
// Scene fingerprint bits: 4 map size, 16 player count, 32 slot types, 64 races, 128 no scenario
// starts its Dark player here, 256 the other players' starts do not belong to that scenario.
// With several scenarios sharing the Dark start, a bit is set only if every one of them fails it.
inline unsigned scenarioMask(const Scenario&c,const Setup&s){
 unsigned m=0;if(s.size!=c.size)m|=4;if(s.players!=c.players)m|=16;
 for(uint32_t i=0;i<c.players&&i<MaxPlayers;i++){
  if(s.slot[i]!=(i?1u:0u))m|=32;if(s.race[i]!=c.start[i].race)m|=64;
  if(i&&(s.x[i]!=c.start[i].x||s.y[i]!=c.start[i].y))m|=256;
 }
 return m;
}
inline unsigned sceneMask(const Setup&s){
 unsigned m=128|256;bool known=false;
 for(auto&c:Scenarios)if(c.start[0].x==s.x[0]&&c.start[0].y==s.y[0]){unsigned k=scenarioMask(c,s);m=known?(m&k):k;known=true;}
 return m;
}
}
