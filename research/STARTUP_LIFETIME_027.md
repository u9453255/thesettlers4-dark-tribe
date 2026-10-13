# 0.2.7: helper lifetime and immediate NEW-map crash

## Preserved session evidence

User observation: NEW First Dawn crashes immediately on entering/loading the map in 0.2.6. Primary source HEAD was bd32ece7e2df47d37491673d5d922fa4a58e13bc; runtime manifest and active.json identified 0.2.6. The manifest's DLL SHA256 was 7ccd3454a93409fa8bde2d4ec500aac4bce980edd96292b795952a72680835c1. Logs, manifests and generated failed map were copied and SHA256 indexed before repairs under ignored evidence/failed-026-20261012. Backups/saves/sidecars remain untouched.

Trace023 starts at 15:04:19, correlates to the earlier 0.2.5 native log PID29212, and contains no crash exception. Trace024 starts at 15:40:03, identifies NEW canonical First Dawn, Starting Tick 0 and no saved game, and records an unhandled access violation at 15:40:14. Do not reuse the previous tick31 CBarrackRole diagnosis for this run.

The helper's latest session started at 15:39:35 and finished at 15:39:45. It records no load-start, native-ready or human assignment for this session. No startup-status-026.json exists, and the native log ends with the earlier PID29212 0.2.5 session. These absences do not prove why the game was delayed or that the user received a particular launcher message. The trace begins eighteen seconds after the helper ended.

## Pinned native fault replay

Executable SHA256 f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b. Relocated trace base 0x00E80000 and EIP 0x010236D2 give RVA 0x1A36D2. Caller return RVA 0x11437F follows a call at 0x11437A to 0x1A3630.

RVA 0x1A3663 reads the local-player global at preferred address 0x129482C. The fault path decrements its low byte, zero-extends it, adds eight times the tick difference, and fetches a pointer. Trace ECX=1, EDX=255 and EAX=263 exactly reproduce player byte zero -> decremented 255 -> index 263. For player1 the same arithmetic gives index8. The fetched pointer ESI=0x002F004D then writes at +12, exactly the reported 0x002F0059. tools/inspect-startup-player.py verifies the actual pinned instructions/caller and replays these operands from disk, without running game code.

Confirmed: this crash path has a zero local-player byte and the helper was already absent. Confirmed source defect: the helper can exit ten seconds after any transient candidate disappears, even without successful native startup. This is consistent with unassigned First Dawn startup; a gameplay test is required to establish the repaired outcome. There is no evidence here establishing that terrain flag clearing causes this signature; the placement repair is retained.

## Repair and contracts

A platform-independent SessionLifetime retains the full 180-second startup grace before readiness, even after a transient PID. Once ready, it allows 60 seconds for a restart. The launcher waits 180 seconds. Repeated inspection failures are recorded at most once per ten seconds for the same error/PID. Version, callback banner, manifest checks, readiness file and helper mutex advance together to 0.2.7 / startup-status-027.json / V027SessionHelper.

Human-slot assignment remains independent of DLL loading, under the existing pinned executable, exact scenario layout and offline host guards. No new addresses or native patches. Lua/mutations remain tick-only; cached rendering, construction masks/settling/transactions, conserved walking delivery, manual courier hiring/no automatic Shamans, automatic native Farmers, native recruitment, aircraft identity, Viking start/economy and victory guards remain unchanged. Schema2 engine-save/sidecar pairs are retained.

## Offline validation and user boundary

Startup tests replay the ten-second disappearance and new candidate at 28 seconds, assert continued assignment/loading, cover restart gaps, startup timeout, failed DLL loading and unchanged guards, and run the original disposable non-game x86 loader fixture. Pinned fault replay, map/placement/Lua/native-model/package/installer checks are required alongside the HANDOFF workflow. The preserved failed map and rebuilt map have identical terrain/object/entity chunks; only the release briefing differs.

No game launch/control/capture, live-process inspection or injection was performed. Installation is left to the user's launcher. Current installed active.json remains 0.2.6 until that launcher installs 0.2.7. Native loading, NEW-map success, construction/delivery/transport/AI/ending and save reload remain user acceptance. Follow TESTING.md, beginning with explicit 0.2.7 readiness.
