**Integration update, 2026-10-12:** included in the primary combined 0.2.4 package with the other fixes. The isolated-candidate notes below record the earlier investigation. Rebuilt/staged artifacts and final user checks belong to the primary project; gameplay remains unverified.

# Separated Viking opponent candidate — scenario revision 1

2026-10-12. Implemented in isolated worktree bf9b against commit 8afd2c2. Main project matched that commit and was clean at intake; a separate helicopter change is being integrated there. This candidate is not a separate numbered release. Merge the authored changes, rebuild the combined native plugin and scenario, then restage using the coordinated release version. Do not replace the main runtime with this worktree's base-0.2.2 DLL/manifest.

## Confirmed map/configuration defects and preserved evidence

The pinned original dark03.map hash is 237a1df871a31818e00496a8b1b802cb414902136657aeb5e10561c11ec854fb. Swapping its two player records left the new Viking computer start at (261,279), inside the central Dark region. The Viking small tower is actually at (75,401), and its woodcutter at (56,404). Four Viking military units remained at x259..273/y286..294 inside the player's area; five other Viking troops remained at a remote southern outpost. The generation step moved neither set of units nor the start coordinate. AI inactivity caused specifically by this mismatch remains a hypothesis until the user test.

The original tower was **not missing or empty**: record byte 6 is 1, byte 7 is 1 and bytes 8..15 are zero. Static inspection of the pinned executable's map-building loader (RVA 0x13d940, garrison branch 0x13d9e2..0x13da1c, creation helper 0x13dab0) establishes that byte 6 enables garrison loading and bytes 7..15 are counts for settler types 29..37. It therefore starts with one swordsman. Tower creation/territory in the user's failing game has not been observed by this agent. Private disassembly remains under work/native-analysis/opponent.

The unchanged 83-byte chunk 10 is not established as an AI-build toggle. Its per-player pattern correlates with victory targets in original scripts. Do not label it a disabled AI flag or flip it speculatively. Original team configuration, goods stacks and this metadata are preserved byte-for-byte.

## Implementation

- Set the Viking start to its original tower (75,401); relocate nine troops to individually free, normal-grass cells in its external settlement. All Viking map entities are x<140, outside the original Dark-region bounds x169..437. Existing Dark building/unit records are unchanged except their original owner swap.
- Retain original occupied small tower, reinforce to two swordsmen/one bowman, add three occupied large towers (three swordsmen/three bowmen each). Initial native XML influence radii are checked as 20/30. Six economic buildings include the original woodcutter plus forester, sawmill, stonecutter, small residence and barracks. All economic building anchors are within the conservative hex-distance influence of an initial tower. Original XML door positions connect to the base in a terrain-only grass flood fill; object blocking, slopes and actual engine territory/pathfinding remain unverified.
- Add 24 starting carriers (40 total carriers; 68 loose Viking units total, excluding tower garrisons). Preserve original starting tools, 36 boards and 28 stone. Native AI is responsible for all later production, delivery, construction and recruitment. No recurring resource grants, scripted building orders or artificial army waves.
- New footprints use the original Viking block/build/dig masks with a two-cell halo, avoid original buildings/units/Dark land, and clear/flatten only the prepared footprints. Trees, stones and ores outside them remain original. No game XML definitions change.
- Configure original AI AttackMode2 on all difficulties, AttackDelay20/15/15 minutes and finite army limits through first-new-game initialization. Parameter semantics are documented in [AI.SetPlayerVar](https://docs.settlers-united.com/s4-lua-api-de/library-functions/ai/ai.setplayervar); shipped single-player scripts supply the same API precedent. These parameters do not promise an attack at exactly those times.
- Arm the scripted victory condition only after both a Dark base and a Viking military building have been observed. Initial missing/zero counts warn once and return without declaring a winner or calling DefaultGameEndCheck. Once armed, the last enemy military building or last Dark base decides the victor. This prevents that specific initialization-zero path; the actual cause of the user's immediate ending is not established.
- Preserve exact legacy start (261,279) and new start (75,401) in helper/native fingerprint checks; mixed or other coordinates are refused. Executable/race/player-count/slot/host checks and Lua DT_Version200 remain. Existing matched saves retain their old map/opponent configuration; a new game is required for this map fix. Sidecar schema and all economy/save/identity guards remain unchanged.

## Offline evidence and limits

Passed: node tools/build-scenario.cjs; node tools/test-scenario.cjs (including five rejected broken maps); python tools/test-scenario.py (actual Lua in Lupa with mocked engine APIs, not native Lua engine); node tools/verify-maps.cjs (12 source map round trips); python tools/build-native.py; python tools/stage-release.py; python tools/verify-offline.py (disposable installer upgrade/refusal/restore); python tools/test-startup.py (disposable original non-game loader fixture).

No game launch, process inspection, injection into the game, input, capture or installation into the real game was performed. Native AI development, military territory, traversable building entrances, material production/access, first battle and absence of an immediate game ending require the user's TESTING.md checks. This patch does not establish a second-farm crash fix.

## 0.2.4 user failure / superseded territory assumption

The user tested the combined package and it crashed at tick 31. The former radius/grass tests proved geometry only: occupied map-load towers were insufficient evidence that new barracks had an economy. Chunk 6 byte 1 remained owner 1 after swapping players, new buildings had owner label 0, and footprint clearing erased native metadata. Revision 2 corrects initial ownership and retains flags/resources. Default end checks no longer run with both bases alive. See [0.2.5 analysis](STARTUP_SECTORS_025.md); actual gameplay effectiveness and reported sound trigger remain unconfirmed.

## 0.2.6 prepared-terrain correction

Owner 2 initialization remains. Preserve original resources and non-prepared flags, but clear obsolete 0x43 blocking bits in explicitly flattened/object-cleared footprints and the authored Dark clearing. Preserving stale flags there caused the 0.2.5 Farm-only placement regression; see PLACEMENT_COURIERS_026.md. Native runtime collision and halo guards remain.

## r31: clean Dark start (First Dawn)

`tools/build-scenario.cjs` removes the Dark start farm at 337,282 (Euclid 18.3 from the fire centre of the Temple at 340,300; FarmLock 33, `tools/citadel-fire.cjs` = `native/citadel-fire.hpp`) after the opponent is prepared. It also clears the flags baked under it (0x08/0x01/0x02 on its 52 footprint cells). The other 7 start farms, every other Dark record, the Viking layout and all other map bytes are unchanged (`tools/test-scenario.cjs`). In r30 this farm was crushed at the start (`FARM_FIRE_REMOVED 337,282 reason=start`); on the new map that line no longer appears.
