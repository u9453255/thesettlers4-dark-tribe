# HD startup hash correction - 0.2.12

2026-10-12. The user reports a crash on reaching the main menu while launching the in-progress 0.2.11 HD menu repair. The agent read existing files only: Trace037/038/039, evidence/v02-runtime.log, session-helper.jsonl, startup-status-0211.json, runtime/active.json and HD's s4hd-log.txt. No game launch, control, capture, live process inspection, injection or installation was performed.

## Evidence and confirmed defect

Trace039 records exception c0000005 at address 00000000 at 18:32:35. PID 1756 logs Native initialization started and S4ModApi interface ready at tick 0, followed immediately by the address-zero first chance. There is no owned-panel initialization or native callback readiness. PID 27348 shows the same startup sequence. The helper records failed native readiness and disappearing processes. HD's log reports completed managed initialization at 18:32:34.143 and switching to the original render state at 18:32:35.312. The old game-folder Trace.txt is unrelated to this run and was excluded.

The 0.2.11 startup source verifies the loaded HD Manager's hash before initializing game save hooks. Its hashFile implementation calls createW and closeHandle, both initialized to nullptr and assigned from the game's import table only later. With this HD Manager present, the first hash call therefore jumps to address zero. This is a confirmed code defect matching the trace and missing readiness. The repeated game RVA 0x2445D2 first-chance entries follow the first exception; they are not treated as a separate primary fault. The game's trace contains no full native symbol stack, and a fixed code path is not gameplay verification.

Failed source, DLL, runtime/active manifests, helper/runtime/readiness logs and traces were copied without altering their originals into ignored evidence/failed-0211-20261012-1832 with a SHA256 manifest. The original failed versioned DLL remains intact. No private traces/binaries/game-derived code are committed.

## Correction and offline regression

native/file-hash.hpp hashes through this module's own CreateFileW, ReadFile and CloseHandle imports. Those imports are ready when the DLL startup worker runs and do not depend on the game's hooked IAT. The same shared helper supports HD verification and saved-game sidecar hashing; the exact whole-file HD pin, saved-game SHA256 pairing and flush/rename rules remain. Hash failure still disables the plugin. No file hash guard is bypassed and no game/API/HD file is replaced.

native/file-hash-tests.cpp is an original, disposable x86 program using the actual helper before any game/API/save-hook initialization. It checks known empty-file and abc SHA256 vectors, changed bytes, 65,537 bytes across buffer boundaries, locked/missing-file refusal and successful deletion after hashing. No game/HD/API module is loaded. tools/inspect-hd-rendering.py also checks that native startup calls the shared helper before the hook originals are assigned and that the helper never calls those originals. This is the ninth native offline check. Earlier eight UI/economy checks alone did not cover this defect.

The retained HD pane uses cached tick snapshots, a process-owned non-activating layered window, shared DPI/hit mapping and queued intent; its implementation/evidence is in HD_MENU_DISPLAY_0211.md. Construction cancellation/cost/refund safeguards, native Mushroom Farmer economy, ordinary Shaman recruitment, selected Manacopter movement/passengers/boarding/unload, scenario fingerprint and exact schema-2 sidecar layout remain.

## Release and user acceptance

The final offline build passed all nine native checks. Required generated map/Lua fixtures, twelve byte-identical map round trips, file/metadata/hash/loader refusal checks, disposable original startup fixture and temporary installer upgrade/refusal/restore also passed. Reports are pinned in combined-release-checks.json; they do not establish gameplay success.

All active package/DLL/loader/helper/scenario/readiness markers are 0.2.12, startup-status-0212.json and V0212SessionHelper. Staging does not activate the release or write the game; runtime/active.json still records the user's last installed release until their launcher. Original saves/sidecars/backups and installed SU/HD files/settings are retained. A new map is recommended for construction/economy acceptance; this startup/display fix alone does not invalidate a matched schema-2 save pair.

Follow TESTING.md. Close the game, run Launch Dark Tribe.cmd and remain at the menu until Dark Tribe 0.2.12 ready. For one PID expect HD manager hash verified before save hooks, HD panel owned window initialized, Pinned HD renderer: independent cached panel enabled and Version 0.2.12 native callbacks installed. Then verify the Dark menus and controls. Offline success establishes the corrected hashing path and package consistency; actual startup crash resolution, HD menu visibility/input/focus and gameplay remain unverified until the user runs these checks.
