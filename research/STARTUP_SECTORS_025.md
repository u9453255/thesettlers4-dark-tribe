# Failed 0.2.4 startup and 0.2.5 repair

## Observed failure and preservation

The user tested combined 0.2.4 on 2026-10-12: map loading completed, followed by a crash about one second later; an immediate victory sound was also reported. Neither the winning player nor the native/scripted trigger of that sound has been observed by the agent.

Trace021 (14:40:05) and Trace022 (14:40:36) agree: exception C0000005, runtime base 00E80000, instruction 00FB4BFE / RVA **0x134BFE**, read **0x345**, EAX=9, ECX=0. The corresponding native logs show initialization/courier creation at tick 10 and the exception at tick 31 with operation=idle, in two separate PIDs. This differs from Fortress Trace020 (RVA 0xD428A, read 0x12E). A first-chance log alone is not proof of an unhandled crash; the user outcome plus matching game traces establish these failed sessions.

Before modifying source/package files, 111 relevant files were copied and SHA256 indexed under ignored `evidence/failed-024-20261012-1440`: traces, activation/helper/runtime logs, source/reports, generated and installed data, executable and saves/sidecars. Original files remain. Private game data/disassembly stay ignored.

## Pinned executable evidence

Executable SHA256 remains `f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b`. `tools/inspect-startup-sectors.py` reads that executable on disk, refuses other hashes and repeats these checks:

- RTTI for the failing callback is **CBarrackRole**. Its recruitment tick obtains a sector at the building entrance, reads the economic-sector ID from the 20-byte sector record, then dereferences its economy pointer. The observed null pointer and unit index 9 produce exactly `0 + 9 + 0x33C = 0x345` at RVA 0x134BFE.
- Landscape loader RVA 0x20D690 copies chunk 6 **byte 1** into the initial WORD sector/owner map (0x20D71F..727). Byte 0 spawns objects; byte 2 feeds native flags; byte 3 feeds resource information. Clearing all four bytes is not an object-only operation.
- Sector initializer RVA 0x1B1EA0 seeds records 1 and 2 with owner nibbles 1 and 2 (0x21/0x42). Subsequent native partitioning retains the owner nibble and creates ordinary owned economic sectors before the map buildings load. This establishes the initial owner namespace without inventing a native address or patching runtime memory.
- `DefaultGameEndCheck` also consults native game-mode ending callbacks, beyond the custom Lua building counts. Arming the Lua check does not make it safe to call the default check while both custom bases remain alive. It does not establish which branch caused the user's sound.

The public [S4ModApi landscape implementation](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CS4Landscape.cpp) corroborates the sector map and owner access. [DefaultGameEndCheck documentation](https://docs.settlers-united.com/s4-lua-api-de/library-functions/game/game.defaultgameendcheck) describes native win/loss evaluation; pinned executable inspection supplies the exact branch evidence.

## Confirmed generated-map defects and fix

0.2.4 swapped player/building/unit owners but retained source ordinary-land owner **1**, assigning the Viking settlement's original land to the Dark human. Every added building was placed on unowned source land. The sole new ordinary barracks at **(96,442)** and its original XML entrance had initial owner **0**. The traced role matches that sole regular barracks, although the trace lacks a decoded building identity: attributing its specific coordinates is an inference, not a captured runtime entity.

Footprint clearing additionally erased 503 source owned tiles and native flags/resources. Tower radius and grass-connectivity checks proved layout geometry; they did not establish a valid initial native economy. That previous acceptance assumption is superseded.

Revision 2 remaps the original ordinary owner 1 to Viking owner 2, seeds initial non-Dark land within the four actual starting towers' original XML radii, and retains every original flag/resource byte. Object clearing changes byte 0 only. New terrain remains flattened as before. Ten Viking building entrances must all have owner 2; tests reject the actual barracks entrance label 0 and wrong label 1. Four occupied towers, six economy buildings, 68 loose units/40 carriers, original materials and the original AI remain. No timed resources/waves or removal of barracks/recruitment.

The actual Lua check returns while both custom bases live; it calls native default ending only after explicitly declaring a custom loser. Mocked engine tests verify startup-zero, both-alive, extra Fortress/towers and each last-base loss. Native game ending/winner still require user observation. Original chunks 3 and 10 are retained; unknown metadata is not speculatively flipped.

Read-only `START_SECTOR` diagnostics run once on game ticks after plugin initialization. They record each Viking building identity/type/entrance/owner/sector/economy and pointer presence for the user's next run. Rendering/mouse callbacks do not access Lua or wait on the economy lock. No native sector/player state is patched.

## Launcher refusal during repair

The user attempted activation before the repair package was complete and reported `Unsupported native manifest`, followed by launch.ps1 line 9 refusing startup. That exact loader branch rejects an unsupported manifest version or executable hash. The pinned hash stayed unchanged; source moved from 0.2.4 to 0.2.5 before its rebuild manifest, establishing the version transition as the supported explanation. The exact user-time files were not captured at that instant; the earlier failed snapshot contains the 0.2.4 manifest and the later transition snapshot records the now-rebuilt files. No claim is made that the late snapshot reproduces the failure.

Final runtime manifest, DLL marker, loader, launcher, installer/staged manifest, V025 helper mutex and startup-status-025/readiness labels are coherent at **0.2.5**. File-only verification passes. Regression tests explicitly refuse a disposable 0.2.4 manifest and changed DLL, then accept the matching package. No hash/build guard is bypassed. The earlier frontend readiness suggestion did not establish a completed package.

## Validation and evidence boundary

Passed offline: compatibility generation; scenario generation/rejection/metadata preservation; actual Lua through Lupa with mocked APIs; 12 map round trips; pinned sector/crash/end-branch inspection; x86 native build and economy/queue/placement/panel/aircraft models; staging; file-only loader; disposable non-game startup fixture and manifest refusal tests; temporary installer upgrade/refusal/original restoration. Reports are regenerated for 0.2.5. Existing Fortress and aircraft fixes, safety/hash/entity/transaction/save guards and sidecar schema 2 remain.

No game launch/control/capture, live game process inspection, game injection or live installation was performed by the agent. **This repairs confirmed source/map defects; startup crash resolution, absence of early ending, native AI economy/attacks and the correct final winner are not yet verified gameplay.** Start a NEW 0.2.5 map and perform the startup gate in TESTING.md before the other acceptance tests. Failed 0.2.4 saves retain their old map.
