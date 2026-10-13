# Version 0.2 native findings

Historical findings below describe earlier implementations. Current contracts are in MAIN.md and NATIVE_FARM_ECONOMY_029.md. The direct native farm credit is RVA 0x389C0 and remains untouched in 0.2.9. RVA 0x36380 is a generic addition entry used by AI catch-up; only positive active human Dark catch-up is suppressed. All custom courier/stock/cargo attribution is removed. Construction uses strict current affordability, native cancellation mode 1 and a 30-tick settling delay. Mode 0 only cleans up and does not schedule entity removal; the previous interpretation was wrong. See CONSTRUCTION_CANCELLATION_0210.md. Selected Manacopter flight/passenger controls supersede the earlier transport limitations.

All addresses are RVAs for the pinned native x86 S4_Main.exe 2.50.1516.0. Private disassembly and game-derived data remain under ignored work/ and evidence/. No extracted code is committed.

## Earlier observations and boundaries

Before the user reserved further gameplay testing, limited probes confirmed S4ModApi 2.3 loading, tick/mouse/frame callbacks, native farm mana generation without timed Lua income, native Temple warrior recruitment, and DirectDraw panel rendering. They do not validate the expanded courier/construction/save implementation.

An earlier visibility probe setting low-six fog bits hid objects. The final plugin clears those bits on Dark cells; the correction is untested. One experimental process disappeared without a matching Windows Application Error, so the cause remains unknown. All final Lua access is confined to game ticks; rendering and input never call Lua.

## Layout used by original source

- Entity array E9BC38. Identity DWORD +4, pool index WORD +8, class byte +10, kind WORD +12, X/Y WORDs +24/+26, owner/race byte +32, health byte +33. Building role +80; settler role +88. Long-lived jobs validate identity as well as pool ID.
- Landscape size D6921C, pointer D69220, four bytes per tile. Dark attribute bit64; border bit128. Gameplay low six bits are fog; upper bits are preserved.
- Setup E968A0; local player E9482C. Control90, actor D8, X FC/Y120, race18C, count338. Scope: two players, human Dark277,214 and AI Viking261,279, plus the new scenario's Lua DT_Version200 marker.
- Income36380: fastcall ECX player, EDX amount. The checked nine-byte gateway copies complete LEA/CMP instructions with the relocated absolute operand. Native mana109B7FC+8C*player is XOR encoded with27356334. Deliveries/refunds bypass interception and verify the actual bank change.
- Farm role cycle DWORD +190 helps assign harvested income. Ambiguous income is divided among active farms; no timer fabricates production.
- Temple queues: six DWORDs at role+180. Queue1 MushroomFarmer,2 DarkGardener,3 Shaman,4 Warrior,5 Thrower. Native recruitment/cost handling persists. Fortress uses its native Dark Temple role; no invented tower attack routine is introduced.
- Save IAT B15318 CreateFileA, B15320 CreateFileW, B152D4 CloseHandle and B1528C WriteFile. Only handles actually written produce sidecars, including read/write opens. Exact imported names are verified offline. Custom snapshots match SHA256 of the engine .sav. Save/load timing needs actual engine verification.

## Construction and transport

The API starts the normal placement event. Own Dark foundations wait for delivered mana, are charged and removed, and become completed native buildings after ten ticks if the private footprint is free and Dark. Blocked conversions refund after another300 ticks. Footprints are generated from local original XML block masks, MSB first and offset by hotspots. Alignment and occupied borders need final testing.

Couriers are dedicated native Shaman units with conserved custom cargo, moved through native movement events. Delivery requires proximity to a base entrance. No coordinate teleport or native Roman goods pile is introduced. Native pathfinding for these couriers and entrance geometry are unverified.

Hall purchases reserve300 mana and create a native aircraft after300 ticks. Selected-aircraft right clicks invoke native DarkTribe.FlyTo, which accepts a destination rather than an explicit vehicle ID. Individual versus fleet targeting and slave transport are final-test questions. Specialist and Fortress roles are retained, not claimed verified.

## Offline verification and provenance

Inventory checks cover harvest/storage/cargo/delivery conservation, blocked delivery, repeated delivery, death losses, save-copy cargo, corrupt-state rejection and costs. Package checks validate executable hash, gateway bytes, save imports, actual API exports, x86 dependencies and hashes. Temporary-copy installer checks cover upgrade, unrelated-change refusal and rollback. Twelve map files passed byte-identical round trips. Reports are retained in this folder.

The released DLL exports Lua functions without the source linker pragmas' leading underscore; offline checks caught and fixed this mismatch. The API updater is disabled through its supported NoUpdateForS4ModApi.txt flag beside the DLL, preserving the pinned ABI/hash.

Public sources: [API release](https://github.com/nyfrk/S4ModApi/releases/tag/v2.3), [API source](https://github.com/nyfrk/S4ModApi), [compiler release](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006). Source contracts were inspected in CS4Scripting.cpp, CS4MenuEvents.cpp, CS4SendWarriors.cpp, CS4Landscape.cpp and CUpdate.cpp. Compiler download digest is recorded in compiler-download.json. No publication was requested.
