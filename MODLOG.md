# Experiment journal

## Repository cleanup for other users (2026-10-13, still 0.2.15)

- The user asked to make the repository usable by other people. No plugin, scenario or pin change; release markers stay 0.2.15.
- The game folder is configurable: `DARK_TRIBE_GAME_DIR`, then `local-config.json` (git-ignored, written by `tools/setup.py`), then the old `C:\Projects\thesettlers4` if present (`tools/project_paths.py`, `tools/project-paths.cjs`). The launcher takes the game path from `runtime/manifest.json` and finds Python on PATH or through `DARK_TRIBE_PYTHON` (`tools/find-python.ps1`). The native sprite tests read the game folder from the environment.
- New `tools/setup.py` (hash-pinned compiler and S4ModApi downloads, Python helpers from `tools/requirements.txt`, game pin report) and `tools/workflow.py` (the whole offline workflow).
- Fresh-checkout fixes found by a scratch simulation with a copied original game folder: `install.py` creates `backups/`; the pin generator creates `work/native-generated`; tools read the original `buildingInfo.xml` from the game when no install backup exists; the bundled API comes from the SDK before the first build; `stage-release.py` and `verify-offline.py` handle a checkout that was never installed (first-install test instead of the v0.1 upgrade test).
- Removed one-off investigation scripts, early native probes, live-process readers and research notes/reports about replaced designs (0.1 prototype and plan, 0.2.1 crash note, 0.2.13 requirement status, stale combined/courier/runtime JSON). Their history remains in this journal and in Git.
- Offline workflow passed on this machine and in the fresh-checkout simulation; no game was started, inspected or modified.

## 0.2.15 - version bump (2026-10-13)

- The user asked to bump the version and commit/push. All current release, runtime, loader, helper, status, installer, scenario and HD window-class markers move to 0.2.15. Behaviour equals 0.2.14 r11 (field rounds listed under 0.2.14). Offline workflow re-run; no agent game testing.

- Field round r12: a finished Fortress waited ~1300 ticks on an idle unit standing on its footprint. Own idle units on a reserved site are now ordered to the nearest free tile outside it (every 20 ticks, `SITE_EVICT`); mod moves are ignored by the send callback.

- Field round r13: r12 eviction never fired (blockers filtered out). Eviction now covers own landed aircraft, ignores stale entries of hidden settlers and logs every skip; GROW_BLOCK names the blocker. The HD panel re-attaches when the game window is replaced and logs every visibility change and draw failure (menu loss cause not yet proven).

- Field round r14: Fortress delays were wild Dark-land critters (class 0x80, owner 0, types 2/3) idling on the footprint. Wild animals no longer block completion except on the anchor tile.

- Round r23: markers on newly darkened land investigated (scan matches the native Dark validator exactly); PLACEMENT_SCAN diagnostics log why Dark spots in view are rejected.
- Round r22: the Dark UI (mana section, panel, markers, input) is hidden outside a running map; it no longer stays on the main menu after leaving a game.
- Round r21: Manacopter Hall panel redesigned like the Manacopter and unit panels (queue cells with progress and times, summary row, action rows incl. Demolish Hall).
- Round r20 (three parallel agents): Dark mana section replaces the native resource panel; construction page without sub-tabs; Manacopter panel in the unit-panel style with keep-this-aircraft; at most one Dark Temple and one Dark Fortress; player-language map briefing.
- Round r19: development leftovers removed from what the player sees (version/status notices, hints, coordinates, log paths, unit panel extras); construction page shows four large building tiles only; failure notices disappear after 6 s.
- Round r18 (three parallel agents): right-drag camera pan works with a Manacopter-only selection (a click still flies); no Dark menu text crosses a frame or is truncated (checked per glyph in tests); Temple/Fortress recruitment works like the native barracks cluster (select a unit, then infinite/0/+5/-5/+1/-1).
- Round r17: unit portraits resolved through the engine's animation table (Gardener, Shaman and Thrower were wrong in r16; enslaved settler and Manacopter master added) and drawn at native portrait size.
- Round r16: the Dark menus are drawn with the game's own GUI and Dark Tribe sprites (read from the installed Gfx files, hash-pinned, drawn fallback on mismatch), in the native menu layouts and with the native GUI fonts. The Manacopter tab moved into the construction page.
- Round r15 (three parallel agents): Manacopter markers show real health; the Dark menu is restyled to the native S4 panel look and fully covers the native content area; a Dark unit selection panel replaces the Roman unit menus (filter, board, return to base, deselect, keep fighters/workers).

## 0.2.14 - in-place native construction (2026-10-12)

- PID 23844 log: every larger-building refund was a ground entity (walking unit) entering the footprint after mod mode-1 cancellation, before BuildingsAdd. The 0.2.13 flag correction did not address this lifecycle defect.
- Successful construction now keeps the native site and completes it in place; no deletion/recreation. Freeze site phase 1, detach ordinary requests (Temple/Fortress), reserve once, draw 30 ticks of progress, then mirror native completion: Farm/Hall phase-8 grow sequence; Temple/Fortress phase-5 pair with an economy, otherwise native pile removal + phase 8. Units are waited for (native phase 7); static blockers refund once.
- Corrected the unfinished draft (direct 0x1362F0 call skipped piles, entity-map anchor and site flags), a relocation-truncating pin and three mid-instruction pins. New tools/inspect-construction.py; construction-tests rewritten for the in-place lifecycle. 36 signature regions.
- Field fix after user test (PID 31592): Farms paid, grew and were then refunded as "static obstruction". The completion check re-tested flag bits on the reserved cells, and the keep-clear bit 0x40 around newly created objects (e.g. planted mushrooms, RVA 0x163510) tripped it. Reserved cells are now checked for occupancy only, as native phase 7 does. GROW_BLOCK logs the reason before any refund. Same version number; rebuilt and restaged DLL.
- Field fix r3 (PID 1696). Start refusals ("terrain or static flags" for every Fortress/Temple, "ground object" for some Farms/Halls) came from re-testing placement flags that native site init itself changes. The start check now trusts the native validator; only a building on solid cells is a hard blocker. Removed the unknown-building-within-32 refusal. Per-tick read cache removes the VirtualQuery-per-read lag (new memory fixture). Moving units onto a landed Manacopter now boards them, independent of HD sprite blits; boarding steps are logged.
- Field round r4 (PID 29972). Temple/Fortress rise is now drawn: the end of native phase 4 is mirrored so the site is anchored in the building layer, and growth is 4/6/8/10 s. Manacopter group selection works by click, Shift/Ctrl-click and drag box through the pinned projection, independent of HD sprites. Group right-click flight, Return, Fly and Unload apply to every selected aircraft. Each aircraft has radius-15 sight. Fixed the r3 command-20 collision with the panel Destination toggle.
- Field round r5. Fixed the growing-site click crash: the native site panel and pause dereference a null economy on Dark land. Growing sites now get a panel-safe info copy, piles are removed and flag 0x1000 is cleared. Manacopter picking is no longer tied to the Hall being selected, and picked aircraft get the native selected flag. Boarding now mirrors the native arrival (mode 0xA, scheduler removal), so passengers leave the map. Terrain around aircraft is revealed with the native fog-cell reveal.
- Field round r6. Aircraft sight now uses the native temporary vision circle (RVA 0x1AE100, radius 25, every tick), like settlers, so terrain fades back when the aircraft leaves. Removed the persistent slot-7 reveal and the forced land fog bits, which caused permanent reveal and black blocks.
- Field round r7. Right-click flight now works for an aircraft-only selection: the mouse callback handles it and the native projection resolves it on the tick (previously the game sent no move order after a single-click pick). Pick projection is fitted locally (works near map edges). Destination menu removed.
- Field round r8. Picked Manacopters get a native-style selection bracket drawn by the mod (HD click-through marker windows; GDI on the classic renderer), positioned by inverting the native projection. Native brackets cannot be enabled: they come from the settler draw method, which aircraft lack. Only flag 0x200 is mirrored, not 0x400.
- Round r9: targeted unload. Unload arms a cursor-badge targeting mode; a left-click picks the place and each aircraft flies to the nearest spot with a free landing tile and exits, then unloads natively. Right-click cancels.
- Round r10: "Load nearby" boards the closest eligible units within 12 tiles into the selected landed aircraft, filling each up to its free seats (works for groups).
- Round r11: Load nearby radius doubled to 24 tiles on user request.
- Full offline workflow passed (build, 11 native fixtures, inspectors, map/Lua, 12 round trips, staging, startup fixture, verify-offline, loader file checks). Gameplay, unit evacuation and the Temple/Fortress economy path are unverified; user tests in TESTING.md. Research: research/CONSTRUCTION_INPLACE_0214.md.

## 0.2.13 - construction occupancy and automatic green circles (2026-10-12)

- User reports Farms complete but all three larger buildings stall, with green dots appearing only where the cursor moves. PID 29560 logs prove paid mode-1 cancellation/identity disappearance followed by footprint timeouts, without CREATE. Preserve user save/source/log evidence; file-only saved Hall body/margin is empty, while other saved anchors contain real units.
- Replace the walking-sector IsOccupied test with the pinned native BYTE collision flags and exact Dark terrain acceptance. Retain all original masks, physical margin and entity/building collision checks, settling, payment/refund and sidecar layout. Add BUILD_BLOCK reason/tile/flags/entity diagnostics. The earlier live rejected sector value was not logged; gameplay effectiveness remains unverified.
- Native background hints filter ordinary territorial ownership and skip Dark land, while cursor checks add only local points. Add tick-only native viewport validation/cache filling for all four buildings on selection/pan/resize/zoom, with 8,192 cells per tick and automatic ten-tick occupancy refresh. Reuse existing sparse nodes and traverse each row linearly; never change ownership or fake separately rendered valid points.
- Keep executable/API/HD guards and add thirteen locally generated relocation-aware native signature regions. Ten original non-game native checks cover all real masks/terrain/flags, no-hover viewport refresh/bounds and cache reuse. Repeat required file/ABI/loader/map/Lua/round-trip/disposable-startup/temporary-installer checks. No agent game testing, process inspection, injection or live installation; user acceptance is in TESTING.md.
- Advance current runtime/helper/status/installer/scenario labels together to 0.2.13. Older DLLs, native farmer/Manacopter behavior, backups and user save pairs remain. Stage for user activation only. Research: research/CONSTRUCTION_HINTS_0213.md.

## 0.2.12 - correct HD startup hash crash (2026-10-12)

- User launches the in-progress 0.2.11 and crashes at the menu. Preserve its source/DLL/manifest/logs and Trace037/038/039 with hashes. PID 1756 confirms API creation but no callback readiness; Trace039 faults at zero on tick 0.
- Confirm hashFile calls null save-hook original createW before startup initializes it. Replace it with a shared SHA256 helper using this module's Windows imports; save hashing and strict HD pins remain. Add a ninth original non-game native fixture for pre-hook hashing, known/multi-block vectors, changed bytes, locked/missing refusal and closed handles, plus source-order checks.
- Keep the owned cached HD panel, construction/mana/native Farmer/Manacopter fixes and exact save layout. Advance every active release marker to 0.2.12, preserve failed versioned DLL and originals; user activation only. Code and offline verification complete; startup crash resolution and menu/controls remain user acceptance. See research/HD_STARTUP_HASH_0212.md and TESTING.md.

## 0.2.11 - HD menu display repair (2026-10-12)

- User screenshot shows the ordinary grid after SU installation. Existing PID 14776 logs confirm callbacks and Dark economy initialization; HD log confirms DX11 and Dark rendering. Original menu draws only on the legacy DirectDraw surface. Static Manager metadata confirms a separate windowed DX11 swapchain and status-export ABI.
- Add an owned, non-activating layered panel using the same renderer and cached tick snapshots, with shared DPI/resize drawing and hit mapping. Input only queues existing commands; native display remains when HD is off. Preserve economy/construction/aircraft/sidecar guards and native Farmer delivery. No courier/inventory UI returns.
- Pin the exact HD ASI/Manager files; validate loaded Manager hash/exports. Readiness requires successful panel-window creation in the DLL worker. No game/render/input callback waits. Preserve the installed patch, graphics/registry settings, backups and saves.
- Eight offline native checks include a hidden original window fixture for drawing, owner/style/focus contract, DPI mapping, transparent areas and disabled input. Inspect an offscreen HD preview and repeat required map/Lua/package/loader/temporary-installer checks. All gameplay remains user acceptance; no agent game launch, control, capture, process inspection, injection or live install. Details: research/HD_MENU_DISPLAY_0211.md.

## 0.2.10 — correct native foundation cancellation (2026-10-12)

- User reports all Dark construction fails. Preserved existing 0.2.9 logs showing repeated charge/refund/recharge with no CREATE. Corrected my earlier mode-0 interpretation: CBuilding +0x10 only cleans up; mode 1 clears footprints and schedules removal through RVA 0x16D030. The old API BOOL was Lua dispatch success, not deletion proof.
- All four building types use guarded native cancellation mode 1 with the same strict costs, settling, native identity disappearance, original masks/halo and one-completion limit. Skip native UID deletion/free bits. A refunded/refused still-present foundation remains deduplicated; no second payment/refund or late free creation.
- Shared construction controller has disposable offline regressions for all four types, deferred removal, actual occupancy, successful Lua calls with retained entities, rejection before future income, legacy paid-job recovery without payment, recycled IDs and transaction failure. Existing schema-2 reserved bytes store checked refund/cancellation flags without changing layout.
- Add ID/identity/type/location/mode/phase diagnostics, correct current docs and mark previous deletion claims superseded. Retain native farmer economy, courier removal and Manacopter controls. Release/build/loader/helper markers advance together to 0.2.10. No in-game testing or live activation by agent; actual construction/crash resolution remains user acceptance.

- User installed Settlers United during repair. Proved its executable differs only by the LAA bit; add a second exact whole-file pin with original normalized hash required, without changing the installed file. Inspect local SU/HD/API versions and public sources using game-research-websearch; record sources, availability limits and future opportunities in research/SETTLERS_UNITED_0210.md.
- Compare all 170 public API methods and required local GUID/Lua exports. Select the exact installed SU 2.3.2 API to share one instance; retain pinned 2.3.0 fallback. Add unknown executable/API, missing update guard and manifest mismatch refusals. Offline checks only; patch/settings/saves preserved and runtime compatibility remains human acceptance.

## 0.2.9 — remove redundant courier economy (2026-10-12)

- User correction: the native Mushroom Farmer already carries mushrooms to base. Remove all custom Shaman hiring, farm registration, stock/cargo, harvest routing, periodic courier movement and delivery mana credits. Keep native worker provisioning and ordinary manual Shaman recruitment.
- Leave direct native farm income at RVA 0x389C0 untouched with a read-only signature check. Keep only synthetic human Dark AI catch-up suppression and checked construction/Hall cost/refund transactions. No resource timer added.
- Remove economic stock/cargo counters and Hire courier controls. Farm panel explains native workers. Keep actual Manacopter passengers/boarding/unload, now labeled Passengers; former delivery Shamans are ordinary selectable/boardable units.
- Retain exact schema-2 layout and save hashes. Validate and retire old mod inventory without a bank credit; discard legacy pending diverted harvest. Preserve native units/bank and paid jobs; no original save/sidecar is rewritten until a new user save.
- Versioned DLL and all build/loader/helper/readiness markers advance to 0.2.9. Code/docs/offline checks only; game testing and activation remain the user's work. Evidence: research/NATIVE_FARM_ECONOMY_029.md.

## 0.2.8 — construction costs, native income and selected Manacopter transport (2026-10-12)

- User reports Temple/Fortress non-completion, unaffordable placement and excessive Farm mana. Confirmed static defects: unpaid foundations wait for future income; delete mode 2 creates obstruction objects; the old income hook targets AI catch-up rather than direct native income. Their complete relationship to user outcomes remains unverified.
- Reject insufficient mana in the menu and on each foundation tick, charge once, remove verified foundations in native mode 0, retain disappearance/30-tick/footprint/entity/identity/refund safeguards. Native positive production moves to conserved Farm stock; suppress human Dark AI catch-up grants. Native mana transactions retain exact balance checks and gain overflow/underflow preflight. API negative changes already use DecreaseMana; discarded the initial contrary hypothesis.
- Add drawn-sprite aircraft click selection, native right-click flight, real passenger type/ID roster and Unload. Selected Warriors/Throwers/Gardeners/ordinary Shamans approach a landed craft and board within six coordinates; six reservations/passengers maximum. Protect couriers/Farmers, recycled identities and hidden units; cancel approach on new orders/flight/unload and expire after 600 ticks. Ground occupancy now guards actual native unloading tiles. Approach orders are transient across reload.
- Build versioned DarkTribe-0.2.8.dll because the canonical older DLL was locked. Retain that file; loader verifies only the explicit new release files. No process inspection/control, live install, game launch or game injection. Pinned source/API/file analysis and disposable offline checks only.
- Update 0.2.8 package/helper/status/readiness labels together, retaining scenario contract 200 and sidecar schema 2. Original backups and saves preserved. Updated MAIN/HANDOFF/TESTING/research; actual construction, mana behavior, selection, boarding/pathfinding and crash resolution require the user tests.


## 0.2.6 — prepared terrain flags and explicit couriers (2026-10-12)

- User reports 0.2.5 runs but only Farm placement works. Actual local masks reproduce it: stale blocking flags in the already flattened/cleared Dark patch reject Temple/Fortress/Hall. Clear obsolete 0x43 flags only in explicitly prepared cells; preserve owner/resources/other flags and runtime safety checks. All four real masks pass 391 tested anchors each offline.
- User requests no automatically Farm-created Shamans. Register initial/new Farms without couriers; conserve production in stock until explicit existing Hire courier (5 mana) action. Preserve native automatic Mushroom Farmers, ordinary manual Shamans, prior saved carriers/cargo and schema 2. No timer resources or teleportation.
- Added pinned native placement evidence and all-four-mask map fixtures. Native models include waiting-stock/no-courier and legacy cargo restoration; shared Farm panel states that stock waits and hiring is explicit.
- All package/readiness markers move to 0.2.6. A NEW map and TESTING.md checks are required. Runtime placement, manual courier movement and AI development remain unverified by agent. Details: research/PLACEMENT_COURIERS_026.md.


## 0.2.5 — startup sector repair (2026-10-12)

- User 0.2.4 sessions failed at tick 31: Trace021/022, CBarrackRole RVA 0x134BFE, null economy pointer/read 0x345. Immediate victory sound reported; actual winner/trigger remains unconfirmed. Evidence preserved with hashes before editing.
- Correct map chunk 6 interpretation: byte 0 objects, byte 1 initial owner, byte 2 native flags, byte 3 resources. Preserve flags/resources; remap original Viking owner and seed initial four-tower settlement for player 2. Reject null/wrong-owner building entrances, including the failed barracks case.
- Call native DefaultGameEndCheck only following deliberate last-base defeat. Existing native mode/player ending state must not decide an otherwise healthy custom scenario through this script.
- Add read-only tick startup sector diagnostics and pinned executable regression evidence. Retain Fortress, aircraft, automatic farm workers, real economy, AI, guards and schema 2 save pairs.
- Offline build/check outcomes recorded in research/STARTUP_SECTORS_025.md. 0.2.5 staged for user testing; no game launch, capture, inspection or game injection by agent. A NEW game is required.


## 2026-10-11

- User authorized modding and storing all research in the sibling project folder.
- User selected a dedicated scenario as the first version.
- Created requested root directory, but its inherited Program Files ACL denies writing documents or subfolders. Owner: BUILTIN\Administrators; Users have read/execute only.
- Staging in outputs/thesettlers4-dark-tribe while awaiting the user's folder preference/access fix.
- No game data modified.
- Plugin scan misclassifies the engine because the configuration utility is managed; executable inspection confirms native x86.
- Cloned MIT Settlers.ts and LGPL S4ModApi into workspace work/ for source research; no loader installed.
- Next: validate map chunks and player records, create scenario tooling and a reversible data patch.

## First Dawn implementation
- Map source: Map/Campaign/dark03.map, SHA256 237a1df871a31818e00496a8b1b802cb414902136657aeb5e10561c11ec854fb.
- Decoded all 12 campaign maps. File and chunk checksums passed; reassembly was byte identical. Metadata recompression passed. Report: research/map-validation.json.
- Map header/chunk integers are little endian despite misleading upstream method names.
- Player records are 45 bytes, settlers 6 bytes, buildings 20 bytes. Swapped player 1/2 records and owners in settler/building chunks. Landscape/object/pile chunks remain byte identical.
- Dark race ID is 3; native farm building ID 49 and temple ID 50.
- Original scenario script authored in scenario/DarkTribe_FirstDawn.txt and embedded as chunk 16. Briefings are chunks 11/14 and tips 12/15.
- Original generator tools/build-compat.py adds 66 missing Roman placement entries to RACE_DARK; all four native Dark definitions and all other XML bytes remain unchanged. Generated XML remains private in work/generated/.
- Installed with tools/install.py. backups/install-manifest.json records exact source/destination hashes and rollback paths.
- GameData and user profile snapshots: work/um-data/backups/{settlers4-gamedata,settlers4-profile}/20261011-160554.zip.
- Capture tooling: official BtbN FFmpeg build, downloaded and hash checked by plugin; SHA256 9b8c4ba47dc47c0c005c2c6b47680c42985cd61ab0b66c8511a109e8f1a245ac.
- Ubisoft Connect initially relaunched Program Files copy. User changed official Locate installed game to C:\Projects; verified subsequent process executable path matches.
- User explicitly approved short game mouse/keyboard testing.
- Windowed test setting: Fullscreen=0; original profile preserved in backups/GameSettings-before-test.cfg. Restore after testing.
- Patched game reaches menus; playable scene not yet verified.
- Current first slice has original units, eight farms, one temple, two added shamans and periodic farm-linked reinforcements. Farm construction and recruitment buttons will be added after the scene loads reliably.
- Do not use Buildings.AddBuilding on an occupied footprint: current SU base-API docs explicitly describe crashes. A future base-game construction implementation must check occupancy and fixed slots, or use the SU safe API.

## Native session initialization and verified version 0.1

- Repeated imported-map crashes were isolated: stock imported Viking campaign maps also failed, while normal AllGreen and a tutorial loaded. Native initialization left localPlayer zero because the first actor UID was FFFFFFFF. Assigned only the already-human scenario slot to local host zero; no executable patch. The pinned x86 build then initialized player 1 normally.
- A second-start failure showed localPlayer remains one after leaving a game. The guard now accepts zero or one with the full scenario fingerprint. Automatic helper assignment was observed through repeated starts.
- The compatibility patch reproduced and eliminated ordinary-building placement initialization failure. Native Dark buildings remain original.
- Initial recruitment at (305,285) failed: this coordinate was unsuitable terrain. Generated a clearing and successfully spawned Dark warriors, throwers and shamans at (389,300).
- Selected eight native warriors and verified coordinate changes after a movement command. Base-game recruitment waves increased native units.
- Early read-only observation wrongly matched pool index against entity DWORD offset 4. Correct pool index is ushort offset 8. This explains undercounted later troops; corrected observations supersede early counts.
- Barracks foundation triggered recruitment and deducted two credits; rejected recruitment request retained credits. Foundation deletion can produce a native building-destroyed message.
- Occupancy guard refused a mushroom farm at (400,300) without charging. Moved expansion site to (420,320) and enlarged the prepared clearing. Native farm count rose eight to nine; an additional native Dark farm worker appeared. Native farm appearance inspected in expansion-visual.png and native-farm.png.
- Created FirstDawn_Verifi.sav using the in-game Save menu. F6 opens options/diplomacy, not quicksave. Loaded it through Main Menu > Load Game > map picture > save list. Nine farms remained; the next wave added nine warriors and two throwers. The saved once-only expansion flag also remained.
- Isolated temporary ending tests removed the Viking tower and then the Dark Temple after one minute. The unmodified victory handler produced 'You have won!' and 'You have lost!' respectively. Temporary test map removed from the game. This validates the conditions, not a full ordinary mission playthrough or balance.
- Removed the duplicate profile map; only game Map/User/00_Dark_Tribe_First_Dawn.map remains installed.
- Added singleton session helper, original-XML rebuild support, guarded restore shortcut, and a short gameplay recording with game-only audio.

### Measured UI coordinates (800 Ã— 600 client, only for repeatable local tests)
- Main: Single Player (628,325); Load Game (628,464).
- Single Player: map picture (550,420); Custom Maps (675,327); continue (694,546).
- Construction main tab (26,198); basic category (26,230); Small Residence (65,400).
- Military category (175,230); Barracks (65,285).
- Save panel (24,230) after F6; filename (85,520); Save (55,574).
- Escape confirmation Yes (57,437); statistics continue (610,548).

- Final occupancy guard checks Buildings.ALL and all 82 native building types. Fresh launcher test still built a ninth farm successfully (final-farm.json); session helper assigned the initial localPlayer-zero setup.
- Original-file restore/reinstall passed; game exited normally and the pre-test GameSettings.cfg was restored byte for byte.


## 2026-10-12: expanded version 0.2 staged

- User reserved all in-game testing. No game launch, input, capture, process inspection or injection occurred during this final implementation session.
- Added original x86 native income interception, conserved stock/cargo, walking couriers, checked delivered-mana transactions, Temple queues, all four Dark construction choices, paid foundation conversion/refunds, Hall assembly, native flight routing and Dark fog clearing.
- Exact-save-hash sidecars preserve inventory and pending work. Only written save handles create sidecars. Old/missing-state saves disable the expanded economy.
- Removed prototype timed credits/army waves and fixed farm pad. Added version marker, 80 starting mana and base-aware victory conditions.
- Built with verified portable LLVM-MinGW and released API2.3. Offline export verification caught/fixed the release's Lua symbol-name difference. Supported no-update marker pins the API.
- Standalone inventory checks, pinned hook/import/dependency checks, temporary-copy installer upgrade/refusal/rollback and twelve map round trips passed.
- Previous files preserved in backups/v01. Update manifest staged without changing installed game files; activation is through the user's launcher. Restore removes the helper's activation marker and retains saves.
- MAIN.md, TESTING.md, README.md and native findings updated. Gameplay geometry/pathfinding, visibility, aircraft/specialist effects and real save timing remain unverified. No publication.


## 2026-10-12: startup correction 0.2.1

User reported crash during map loading. Log records native-load-failed and no human assignment; native startup log absent. No Windows fault record established the exact exception. Fixed confirmed startup-order defect: guarded assignment runs independently, native loading uses a separate worker after window readiness, and startup waits for callback confirmation. Added injector exit/error details and native PID/progress logs.

Offline failure/success scheduling regression and actual injector fixture passed, plus package/inventory checks. No game launched or controlled. Crash resolution remains for the user's retry. Documentation updated and binaries rebuilt; data upgrade staged.


## 2026-10-12 — queue/UI and construction safety 0.2.2

- User requested visible queue counts/cancellation and graphical styling, and reported second-farm crash. Existing log had no construction/fault details. No game testing by agent.
- Added shared GDI graphical panels/icons/count badges, per-unit +/− and clear-all, Hall progress/counts/cancel/refunds, and per-farm stock/cargo/courier status. Immediate manual queue updates use the verified native counter layout; automatic worker queue retained.
- Found API occupancy gap in source: IsOccupied checks a sector word, not all entities. Added entity map/position checks, complete block/build/dig mask union, margin, actual site disappearance, three-second settling and one conversion per update. Frame/input callbacks avoid lock waits. Added phase/first-chance diagnostics without suppressing faults.
- Offline queue, placement, hit target, preview, startup and package checks passed. Visual render inspected. Current gameplay/farm crash resolution remains for the user. State schema retained; matching earlier v0.2.1 save/sidecar pairs compatible.

## 2026-10-12: future-agent handoff

Saved AGENTS.md, HANDOFF.md and research/INDEX.md in the primary mod folder. Recorded current user testing restriction, build/package workflow, native layout and queue lessons, construction/startup failure findings, save/backup contracts, evidence locations, superseded prototype findings and unresolved acceptance. Linked the handoff from MAIN.md and README.md. Documentation-only update; no game started, installed or controlled.

## 2026-10-12 — helicopter contract and controls, 0.2.3

User reports a created helicopter cannot move and has no useful task/interface. Internet research confirms original Manakopters deliver Dark troops/Shamans in raids; the original Dark Tribe is AI-oriented and no ordinary human aircraft panel is established by those sources. Settlers United documents FlyTo as targeting the craft just created by a script, not selection. Pinned offline executable analysis independently confirms the global lookup and that creation pushes no Lua return.

Implemented roster/tab selection, per-aircraft identity-checked scoped native FlyTo routing and destination verification, creation detection from the resulting native entity, cached position/target/passenger UI, coordinate controls as an independent command route, return-to-base, native boarding of existing nearby eligible squad units (mod limit six, excluding economy couriers) and guarded native unloading on the exact native exit ring. Retained all existing economy, identity, scenario, save and startup guards. No timed resource/army waves added. Version/readiness/mutex markers advanced together to 0.2.3/023; state schema unchanged.

Build, aircraft/model/panel regressions, temporary-copy installer/refusal/restore checks and disposable non-game startup fixture passed. The initial new test compile missed an initializer-list include, corrected before the final pass. Private disassembly showed the actual unload ring differs from an assumed adjacent hexagon; code now uses and offline-verifies the exact native offsets before delivery. The five-panel offscreen preview was visually inspected. No game launched, captured, controlled or inspected live. Actual native flight/landing, transport, Shaman captive behavior and save timing require the user checks in TESTING.md. This change does not diagnose or claim to resolve the second-farm crash.


## 2026-10-12: isolated Fortress research and repair candidate

- Authorized scope: Fortress original function/UI and reported construction crash; all game tests remain with the user. Primary source matched isolated checkout at `8afd2c2`. No primary/game installation changes, live-process access, game launch/control/capture/injection or publication by this task.
- Route: narrow local XML compatibility repair and cached native selection guards; preserve native Dark Temple role rather than invent Fortress production/combat. Original English/German manuals establish mana/army/Shaman/farm protection; native XML and pinned recruitment table establish Fortress role and five native recruit types. Original Dark Tribe had no documented human selection interface. Sources and evidence are in `research/FORTRESS_REPAIR.md`.
- Read existing Trace020: 13:42:12 local, unhandled access violation, game RVA `0xD428A`, ECX=0/EAX=-1, read `0x12E`, panel stack return `0x143DE0`. Static parser stores empty tool as -1 at `0x1DF`; panel zero-branch skips the invalid goods query. Fortress has the empty enum; the other three Dark definitions specify explicit zero. Corrected generator to `GOOD_NO_GOOD`, with exact permitted-change assertions, original bytes/roles/geometry retained, prior accepted-install regeneration and idempotency. Null sector origin and runtime effectiveness remain unconfirmed.
- Foundation panel no longer advertises recruitment/demolition; completed bases require the correct owned native role before exposing queues. Native role validation added at queue lookup. Automatic farm-worker recruitment, transaction/identity/save guards and tick-only mutations retained.
- Built isolated native package and generated/staged isolated data; preserved original backups. Passed pinned native analysis, ten data regressions, economy/queue/placement/panel checks, runtime/package validation, disposable installer upgrade/refusal/restore, non-game startup fixture and whitespace checks. Offscreen complete/Foundation/unavailable Fortress previews inspected; no screenshots of gameplay. Earlier second-farm crash is not declared fixed.
- Not a new independent release: all 0.2.2 readiness/version markers remain consistent pending coordinated integration. Primary release owner must integrate source, regenerate ignored data, rebuild and stage before user activation. MAIN, HANDOFF, TESTING, research index and findings updated.

# 2026-10-12 — Opponent/map candidate, isolated bf9b

Route: authored scenario data and original Lua AI settings, with only exact old/new start-coordinate fingerprints changed in helper/native plugin. Main project matched base8afd2c2 at intake; kept isolated while the helicopter agent integrated its0.2.3 change. Confirmed: swapped Viking start261,279 remained inside Dark area although original tower is75,401; four troops/leader remained central and five remote. Original tower already has one swordsman (byte6 enable, byte7 count), so user's missing-tower hypothesis is unconfirmed. Retained/strengthened it and added territorial support towers, real initial construction economy and carriers. Added guarded Lua victory arming to prevent an initial-zero-count ending path; actual reported ending/crash cause not established.

Passed scenario/map negative regressions, actual Lua with mocked APIs, exact-layout guards, native build/models, twelve map round trips, temporary installer upgrade/refusal/restore and disposable non-game loader tests. No game execution/process access/real installation. Detailed parameters, static findings and evidence limits: research/OPPONENT_START.md. User tests in TESTING.md. Candidate must be merged/rebuilt/restaged with helicopter changes; no independent release version assigned.


## 2026-10-12: primary combined release 0.2.4 ready for human tests

User explicitly authorized combining every fix and completing the primary package. Integrated Fortress commit `3613bbf`, the already-transferred helicopter 0.2.3 implementation, and finalized scenario commit `77f019b` into the primary project. Preserved helicopter routing/boarding/unloading, Fortress role/tool/site guards, new/legacy scenario fingerprint, transaction/identity/save contracts and native economy. Pre-integration file copies and merge outputs retained under ignored `work/integration`; no unrelated changes were dropped.

Native/Python/PowerShell/package/readiness/helper mutex/status/version markers synchronized to 0.2.4 / 024; Lua economy marker200 and save schema2 intentionally retained. Compatibility XML and scenario regenerated in primary ignored outputs, x86 native DLL/loader rebuilt, and upgrade hashes staged with historical accepted hashes preserved. Installed `runtime/active.json` remains the prior installation until the human launcher runs; no live installation, game process access, launch, input/capture or game injection occurred.

Passed primary scenario placement/rejection/round-trip checks, actual Lua mocked-engine startup/victory/settings/layout checks, 12 map codec round trips, native economy/queue/placement/panel/aircraft checks, Fortress pinned parser and 10 data regressions, coherent release/runtime/API/dependency checks, disposable installer upgrade/refusal/restore and original non-game startup/loader fixture. Default loader file/hash verification passed. Both final shared renderer previews inspected offline. Final report is `research/combined-release-checks.json`; MAIN/README/HANDOFF/TESTING/research index updated. Human next action: close game, primary Launch Dark Tribe.cmd, wait for Dark Tribe 0.2.4 ready, start a new First Dawn and follow TESTING.md. Native AI/pathfinding/territory/flight/transport/crash resolution remain unverified; earlier second-farm cause remains unresolved. No publication.


## Startup repair — 0.2.7

The user reports an immediate NEW-map crash in 0.2.6. Trace024 (15:40:14) faults at RVA 0x1A36D2, writing 0x002F0059 with local-player byte zero. This is a different signature from the earlier CBarrackRole failure. The helper ended at 15:39:45; the crashing trace opened at 15:40:03. No 0.2.6 native readiness or human-slot assignment is recorded for that run.

0.2.7 retains the helper for the full 180-second startup window despite transient process disappearance, then allows 60 seconds for a restart after readiness. The launcher waits up to 180 seconds. Inspection failures now retain diagnostic messages. Offline timeline regression and pinned fault/index replay pass; gameplay crash resolution remains unverified. Placement flag repair, explicit courier hiring, automatic native Farmers, Fortress, aircraft, Viking economy and all hash/identity/save guards are retained. See research/STARTUP_LIFETIME_027.md.
