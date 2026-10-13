# Human gameplay tests - Dark Tribe 0.2.15

All in-game testing belongs to you. Offline checks cover code, pinned files, disposable models, map/Lua fixtures and installer copies; they do not verify gameplay or crash resolution.

## Startup gate - verify before entering a map

1. Preserve old logs, `.sav` files and their exact matching `save-states` sidecars. Close The Settlers IV, then run `Launch Dark Tribe.cmd` in this repository.
2. Remain at the menu until **Dark Tribe 0.2.15 ready** appears (up to three minutes). If readiness fails, retain the launcher error, `evidence/session-helper.jsonl`, `evidence/v02-runtime.log` and `evidence/startup-status-0215.json`; close the game before retrying.
3. Confirm this launch logs `HD manager hash verified before save hooks`, `HD panel owned window initialized`, `Pinned HD renderer: independent cached panel enabled` and `Version 0.2.15 native callbacks installed` for the same PID. The previous 0.2.11 tick-0/address-zero crash was corrected in code; actual resolution requires this run. Failed 0.2.11 evidence remains archived.
4. Start a NEW `00_Dark_Tribe_First_Dawn` custom map, player 1 human Dark and player 2 computer Viking. Keep races unchanged. Use a client height of at least 600 pixels.
5. Observe at least two minutes before other tests: no crash or early victory/defeat. If an ending occurs, record its actual text/winner; sound alone is insufficient. Existing saves retain their old terrain, AI and bank and cannot validate the new scenario economy.

## r23 checks - test first (markers on new Dark land)

1. Reproduce the problem: on land you darkened yourself, select a Dark building to place and keep the view still over that land for about 5 seconds (no need to move the mouse), then also over your original Dark land.
2. Send the `PLACEMENT_SCAN ...` lines from `evidence/v02-runtime.log` for that time, plus a screenshot of the area. They show how many Dark spots were checked and why they were refused (blocking flags by bit, terrain, or footprint/slope).

## r22 checks

1. Play a First Dawn map, then leave it to the main menu: no mana section or other Dark element remains, and the main menu buttons in the lower left work normally. Start the map again: the Dark UI comes back.
2. Pause the game (and open the in-game menu) for more than 5 seconds: report whether the Dark panel stays visible or disappears. Log line to send if it disappears: `HD_PANEL hidden reason=inactive,not-in-game`.

## r21 checks

1. Select the Manacopter Hall: it looks like the Manacopter and unit panels. With nothing queued it says "No Manacopters queued" and "300 mana and 30 s each".
2. Build Manacopter (faded below 300 mana) a few times: one cell per queued aircraft, the first with a filling bar and its seconds, the others faded with later times; the summary shows "N queued" and "Next in N s". Cancel last and Cancel all work and fade when the queue is empty. Select Manacopter selects a ready one. Demolish Hall demolishes it.

## r20 checks

1. Below the Dark menu the native resource panel (Roman goods, beds, soldiers, percentages) never shows: the Dark mana section (crystal, value, "Mana") is there in every situation (construction page, any selection, native goods/settlers/statistics tabs, nothing selected). Clicking it does nothing. Check both under HD and, if you use it, the classic renderer.
2. Construction page: titled "Dark Buildings", no silver sub-tab row above the buildings.
3. With your starting base the Temple (or Fortress) tile is greyed and reads "Built"; clicking it does nothing. Place a Fortress: its tile reads "Building", then "Built". Try to place a second one (hold Ctrl while placing, or Alt+Space): the outline disappears and no mana is taken. Demolish a base: its tile becomes available again.
4. Select one Manacopter, a group of 2-3 and a group of 4-6: cells with picture, health bar and "n/6" like the unit panel (flying ones drawn raised), passengers by type for small groups, summary "Landed/Flying" or "N aircraft" with health, rows Unload / Load nearby units / Return to base / Next Manacopter / Deselect. Click a cell in a group: only that aircraft stays selected. Unload targeting still shows "Click map to unload" and right-click cancels.
5. Start a new map: the map briefing has no version number or launcher instructions.

## r19 checks

1. Start a new First Dawn map and load a save: no "Dark Tribe economy v0.2.15" or "initialization error" message appears; only the three intro lines on a new map.
2. Open the construction tab: four large building pictures with name and mana cost (red and greyed when unaffordable), nothing else. Each tile starts placement.
3. Select Dark units (fighters, specialists, mixed): portraits, counts, health and only "Board Manacopter".
4. Play normally: panel texts appear only when something fails (for example boarding with no landed Manacopter nearby) and vanish after about 6 seconds. Report any text that still sounds technical.

## r18 checks

1. Select only Manacopters (click or drag a box). Hold the right button and move the mouse: the camera pans like with normal units and the aircraft do NOT fly. Release. Then right-click (no movement) on the map: they fly there. Repeat with a group. Also try a short pan that starts in the middle of the screen and report if it ever sends the aircraft.
2. With normal units selected, right-drag pans and right-click moves exactly as before, also when the release happens over the Dark panel or over a Manacopter.
3. Open every Dark view (construction, Temple, Fortress, Farm, Hall, site, Manacopter single/group/unload targeting, unit panel with one and many types): no text touches or crosses a gold frame, and no text ends in "..." except long notices at the bottom.
4. Temple/Fortress: click a unit cell (the gold brackets move to it), then use up (+1), double up (+5), down (-1), double down (-5), 0 (clear this unit) and the infinity button (continuous: the count shows the infinity sign and units keep coming while mana lasts). Faded buttons do nothing. The arrow-cross button clears all manual orders. Save and load with continuous recruitment active: it stays continuous. Log: `RECRUIT_ORDER ...`.

## r17 checks - unit portraits

1. Select Gardeners (the bald figures), Shamans (red-and-white with a red staff), Mushroom Farmers (green hair), Warriors and Throwers, alone and mixed. Each portrait in the Dark unit panel, the Temple/Fortress recruitment rows and the Manacopter passenger slots must show that unit's own figure, filling its cell like the native settler portraits.
2. If enslaved settlers (hooded) or a Manacopter master can be selected, they show their own figures too.

## r16 checks - menus in the game's own art

1. In `evidence/v02-runtime.log` for this PID find `Native GUI art ready: 35 native sprites verified`. If it says `Native GUI art unavailable (...)`, the menus use the drawn style: send that line.
2. Open the construction tab (the native house tab, top left). The Dark page must look like the native build menu: tab strip with two silver sub-tabs, the four Dark buildings as real building pictures with their mana cost beside them (red when unaffordable), names below, mana at the bottom. Click each affordable building to place it. Click the second sub-tab: it selects the next Manacopter. The goods/settlers/statistics tabs must work normally.
3. Select a Temple, Fortress, Farm, Hall and a building site: native title box, building picture, bomb button (demolish, not on sites), recruitment rows with portraits and the native orange chevrons (down removes one, up or the row adds one; the arrow-cross button clears all orders).
4. Select a Manacopter (and a group): Manacopter picture, health bar beside it, passenger portraits, Unload / Load nearby / Return to base, top-right arrow = next aircraft, arrow-cross = close.
5. Select Dark units: portraits with health bars and counts like the native selection menu.
6. Compare fonts and frames with the native menus next to them (resource panel below). Send a screenshot of anything that still looks foreign.

## r15 checks - UI

1. Look at every Dark menu (construction list, Temple/Fortress recruitment, Farm, Hall, foundation, Manacopter single/group, unload targeting): it should look like a native S4 panel (moss background, gold frames, bronze buttons) and fully cover the native area, with no Roman menu pieces visible at the edges, under HD and the classic renderer. Send a screenshot of any edge that still shows Roman content.
2. Construction tab: a 2x2 grid of Dark buildings; unaffordable ones are greyed. Each cell must start placement.
3. Select Gardeners, Shamans, Warriors and Throwers (alone and mixed): the Dark unit panel replaces the Roman menu, with portraits, counts and health bars. Click a portrait (keeps that type); test Board nearest Manacopter, Return to base, Keep fighters/workers and Deselect all. Log: `UNIT_PANEL ...`.
4. Damage a Manacopter (enemy towers/soldiers): its selection bar shrinks and turns yellow, then red; it refills when the aircraft stands on Dark land.

## r14 checks - test first (Fortress activates immediately)

1. Build several Fortresses next to the main base, where the small Dark-land critters crawl. After the rise (about 10 game seconds), each must activate at once. The log shows `native completion` right after `readyAt`, with no long run of `GROW_BLOCK ... cls=128` lines.
2. Check critters near or under a finished building keep moving normally and the building can be selected and used.
3. Rarely a critter sits exactly on the building's centre tile: then `GROW_BLOCK ... reason=wild animal on building anchor` appears until it moves. Report it if that happens often.

## r13 checks

1. Build Fortresses and Temples next to the main base again. When the rise finishes, units on the site must walk off and the building must activate within a few seconds. If one still waits, keep `evidence/v02-runtime.log`: the `GROW_BLOCK ... cls= owner= flags= aircraft=` and `SITE_EVICT_SKIP ... reason=` lines identify the blocker.
2. Play normally (including alt-tab, minimise, resolution or HD changes). If the Dark menu disappears again, note the time and keep the log: `HD_PANEL hidden reason=...`, `SCENE_STATE`, `HD_STATE` and `HD panel re-attached` lines show why. The menu must return by itself when the reason ends.

## r12 checks - test first (Fortress completes promptly)

1. Build a Fortress right next to your main base, where idle units gather. When the rise finishes, units standing on its footprint must walk off within a few seconds and the Fortress must become usable (recruitment panel) shortly after. Log: `SITE_EVICT ... unit=.. to=x,y`, then `BUILD_STATE ... native completion`.
2. Repeat with a Temple and a Hall near crowds; your units must walk aside, never get stuck or teleport. Enemy units on the site are not moved; the site waits for them.
3. Check that a selected unit evicted this way does not trigger aircraft boarding or flights.

## r10 checks - test first (Load nearby)

1. Land a Manacopter next to idle Warriors, Throwers, Gardeners or Shamans. Select it and press "Load nearby": the closest units within about 24 tiles walk over and board, up to 6. Units farther away and Mushroom Farmers stay. Log: `LOAD_NEARBY ... queued=N`.
2. With 2-3 landed aircraft selected, press it once: each fills from its own closest units without exceeding 6, and no unit boards twice.
3. A partly full aircraft only takes its free seats. A flying aircraft is skipped ("Land the aircraft first").

## r9 checks - test first (targeted unload)

1. Board units into a Manacopter. Select it and press Unload passengers. A green down-arrow badge must follow the mouse pointer, and the button reads "Click map: unload here".
2. Left-click open ground: the aircraft flies there, lands and the units get out. Log: `UNLOAD_TARGET ... spot=x,y`, then `TRANSPORT ... passengers=0`.
3. Click water, a building or a crowded spot: it must choose the nearest clear landing place instead and unload there.
4. Press Unload, then right-click: the badge disappears and nothing happens ("Unload cancelled").
5. With several selected loaded aircraft, press "Unload passengers" and click once: each lands on a separate nearby spot and unloads. Give a new flight order during the approach: it cancels that aircraft's unload.

## r8 checks

1. Select a Manacopter: a white corner bracket with a green bar must appear above it, like a selected unit's, both landed and flying. Select several (Shift-click or drag): each gets a bracket. Deselect: the brackets disappear.
2. Scroll and zoom the view: brackets follow the aircraft. Note any constant vertical/horizontal offset from the sprite (send a screenshot), and any lag beyond a fraction of a second.
3. Brackets must not block clicks and must hide when the game is minimised or loses focus.

## r7 checks

1. Left-click one Manacopter (single click, no drag), then right-click the map: it must fly immediately, every time. Repeat 5 times with different aircraft, also near the map edge and after scrolling the view.
2. Drag-select several aircraft and right-click: all fly. Select aircraft together with ground units: the aircraft fly and the units walk.
3. The aircraft panel must no longer show a Destination button or coordinate controls; Return to base fills that row.

## r5 checks

1. While a Farm, Temple, Fortress and Hall are each growing, click them: no crash. The native site window may show limited information. Also click its pause/stop-construction button if one is shown: no crash, and the building still completes and works normally afterwards (Farm produces, Temple recruits). Report a crash with the newest `Trace*.txt` and `evidence/v02-runtime.log` (`FIRST_CHANCE` line).
2. With nothing selected (or a unit or another building selected), left-click a Manacopter: it is selected. Check whether the native selection brackets appear above it. Report if they do not; the flag-based marker is unverified for aircraft.
3. Move 6 selected units onto a landed Manacopter: each unit must disappear from the map as it boards (`BOARD_CALL accepted=1`). Fly and unload: the same units reappear only at the landing site, with no copies left at the pickup place. Old saves with earlier "duplicate" passengers log `PASSENGER_NORMALISED` on unload; check no unit appears twice afterwards.
4. Use a NEW map. Fly a Manacopter over black land: ground and objects within about 25 tiles become visible with a soft edge, without black blocks inside. After it flies away, that area must fade back to normal fog, unless it is Dark land or your own units or buildings are nearby.

## In-place construction (0.2.14)

1. After **Dark Tribe 0.2.15 ready**, confirm the log line `Pinned native rise, reservation and completion verified; reserved cells checked for occupancy only; tick read cache r3; ordinary-site rise, aircraft group and sight r4; site panel, passengers and terrain sight r5; native temporary aircraft vision r6; aircraft right-click r7; aircraft selection marker r8; targeted unload r9; load nearby r10; radius 24 r11; site eviction r12; eviction and HD panel diagnostics r13; wild animals do not block r14; native-style Dark UI and Manacopter health r15; native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23` for this PID. If the line lacks `native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23` (or `terrain sight r5; native temporary aircraft vision r6; aircraft right-click r7; aircraft selection marker r8; targeted unload r9; load nearby r10; radius 24 r11; site eviction r12; eviction and HD panel diagnostics r13; wild animals do not block r14; native-style Dark UI and Manacopter health r15; native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23`, an older build is still running. Start a NEW First Dawn map. Old saves with paid 0.2.13 conversions refund those jobs once ("Old cancelled construction refunded") and never recreate them.
2. Earn mana and place Farm (25), Temple (100), Fortress (150) and Manacopter Hall (120) separately, including close to the main Temple where Mushroom Farmers walk. Each must charge once, keep its foundation (no disappearance/rubble), show "The building is growing from the ground", rise for about three game seconds and complete as the same building. No mana refund should occur.
3. Place Farms close to existing Farms where Mushroom Farmers plant; mushrooms appearing next to a growing site must not cancel it. Place a building where units are standing or walking. Expect units to leave the site; completion may wait for them (log `GROW_BLOCK ... unit clearing reserved site` every 100 ticks) but must not refund or force the building over them. Report any site that waits for more than one game minute with the unit ID from the log.
4. Select each completed building: Temple/Fortress recruitment, Farm automatic Mushroom Farmer and Hall aircraft orders must work. Check no leftover boards pile or construction stake remains beside Temple/Fortress, and that units cannot walk through the new building.
5. Save during growth and reload the matching pair: the job must continue without a second charge and complete once.
6. In `evidence/v02-runtime.log` expect `Pinned native rise, reservation and completion verified` at startup, then per job `BUILD_STATE ... growth=1 paid once; original site retained` and `BUILD_STATE ... native completion; same site and identity`. `Pinned in-place construction signatures mismatch` disables the plugin (fail-closed) - preserve the log. A crash during completion: keep the newest `Trace*.txt`, the runtime log and the save pair, and note which building type.

7. Fortress and Temple: every green marker must now build. Report any instant demolish with its `GROW_BLOCK` line. Expect "Waiting for units to leave the building site." when units stand on it.
8. Performance: play several minutes with a building tool selected and with units moving. The periodic short freezes should be gone. If they remain, note how often they occur and whether a building tool, a waiting construction or an aircraft was active.

9. Temple and Fortress: after payment the building must visibly rise from the ground (about 8 and 10 game seconds) and then complete. Farm and Hall rise too, in about 4 and 6 seconds.

## Manacopter group selection and sight (r4)

1. Left-click a Manacopter: it alone is selected (panel shows "Aircraft #id"). Left-click empty ground: the aircraft selection clears.
2. Shift- or Ctrl-click further Manacopters to add them, or click one again to remove it. Drag a box over several: all inside are selected and the panel shows "N aircraft". A box around both units and aircraft selects both.
3. Right-click the map: every selected aircraft flies there, landing on separate nearby tiles. Selected ground units walk normally. Return to base also moves the whole group.
4. With several aircraft carrying passengers selected, click "Unload passengers" and then a place on the map: each one lands at its own nearby clear spot and unloads.
5. Fly a Manacopter into unexplored or dark land: a circle of about 15 tiles around it must stay visible, so the aircraft does not vanish into fog.
6. Log lines: `AIRCRAFT_PICK box=.. additive=.. hits=.. group=..` per click/box, and one `FLIGHT ...` per aircraft. If clicks never select aircraft, report your resolution/HD scaling together with the `AIRCRAFT_PICK` lines; "projection not affine" means the pick was refused safely.

## HD menu regression

1. Close the game, run Launch Dark Tribe.cmd and wait for **Dark Tribe 0.2.15 ready**. Open First Dawn, or a backed-up matched 0.2.10 save pair. This display fix does not require a new map by itself. Use a client height of at least 600 pixels.
2. Open a construction category or click the small Fortress icon beside the new aircraft tab. Expect the Dark-only Farm/Temple/Fortress/Hall list and current mana over the ordinary grid. Unaffordable rows must remain disabled. Click the icon again to close the custom construction override.
3. Select a completed Farm, Temple, Fortress and Hall: verify automatic native Farmer information, manual recruitment/queue controls and aircraft orders. Select a Manacopter: verify passengers and Unload. Farm stock/courier counters must stay absent.
4. Verify clicks match their drawn targets with your HD/DPI setting and after resizing/moving the game window. Clicking a panel must retain game keyboard focus; the minimap and map outside the panel remain usable. Alt-tab/minimize and return: the panel should hide with the game and reappear without covering other applications. Check the panel after leaving the scenario and during video playback.
5. If display fails, keep the screenshot and PID-matched log. Expect `HD panel owned window initialized`, `Pinned HD renderer: independent cached panel enabled`, then `HD panel composed from cached snapshot; input scaled with panel` after the economy activates. A composition error differs from a native economy initialization failure. Do not replace patch files or change settings merely to bypass a guard.

## Settlers United compatibility

1. Keep the installed patch and settings intact. The launcher supports this exact LAA executable and installed S4ModApi 2.3.2.0; no API replacement is needed. File verification refusing a changed executable/API is expected protection. Preserve the error for investigation rather than replacing files blindly.
2. With your current HD/zoom/resolution, verify Dark panel labels, disabled unaffordable buttons, hit targets, aircraft sprite selection and right-click commands align. Check Farm/Temple/Fortress/Hall menus and passenger rows; no stock/courier controls should return.
3. Record optional UBO, speed and transport settings for the acceptance run, since they can affect the Viking opponent and observed timing. SU Lua/Forge features are research candidates and are not required by this addon. If readiness fails due to a native signature mismatch, preserve the log; no game compatibility has been asserted from file checks alone.

## Green placement circles - retained

1. After **Dark Tribe 0.2.15 ready**, use a NEW First Dawn map or a backed-up matched 0.2.12 save/sidecar pair. Earn enough mana for each building. Select Farm (25), Temple (100), Fortress (150) and Hall (120) separately. Keep the mouse still: green circles must populate valid visible Dark land without sweeping it. Typical views update on the next game tick; extreme zoom may take several bounded batches. Paused game ticks do not scan.
2. For each type, pan/zoom to another Dark area without moving the cursor over it, then reselect the same type. Markers must update for the new viewport. Invalid/non-Dark/blocked locations must not become valid. Cancel placement and confirm circles hide normally.
3. Construction itself is covered by the in-place section above; a marked site must still accept placement.
4. Let a unit enter/leave a candidate site and allow the automatic occupancy refresh (ten game ticks). Check markers update without hovering. Place repeatedly and move the view: no gradual slowdown or marker accumulation should occur.
5. Preserve `evidence/v02-runtime.log` on failure. `PLACEMENT_HINTS ... automatic viewport scan` confirms the new scan was requested; it does not prove display. `GROW_BLOCK` records the blocking tile and entity. No signature mismatch should be bypassed.

## Refusals and refunds

1. With a blocked paid site (another building or a tree/stone inside the shape), verify exactly one refund, the foundation removed by native cancellation, and no recharge or free completion, also after a matching save/reload.
2. An unpaid foundation (outline opened, mana spent elsewhere before placing) must be removed without later funding by income.
3. There must be no repeating charge/refund cycles for the same site.

## Native farm workers and mana

1. Starting capital is 80, but native worker recruitment may immediately spend some. Note the current mana bank and observe an initial Mushroom Farmer grow mushrooms, walk to a Temple/Fortress, deliver and return to work. No extra Shaman or courier assignment is required.
2. Open the Farm panel and all other panels: there must be no economic Stock/Cargo counters, Hire courier button or courier status. Clicking the old hire-button area must do nothing. The Farm panel should explain native workers; the global resource is current mana.
3. Observe several production/delivery cycles. Record delivery times and bank changes, accounting for native recruitment. Farm creation itself must give no mod mana, and a low bank must not receive synthetic AI catch-up grants. The mod adds no production timer or duplicate delivery credit. Actual native income timing/rate still requires this observation.
4. Build two new Farms separately on clear Dark land (25 each), saving after the first. Each must receive its original automatic Mushroom Farmer and produce/deliver without any additional Shaman or manual assignment. Repeated placement must not crash; the earlier second-Farm crash remains unconfirmed.
5. Manually recruit an ordinary Shaman at a base (10). It must respond to your normal commands and have no automatic farm-delivery assignment. Test Clear all orders on manual queues: automatic Mushroom Farmer provisioning must continue.

## Strict costs and Temple/Fortress completion

1. Check all rows below their cost: Farm 25, Temple 100, Fortress 150, Hall 120. An unaffordable outline must not open. If another purchase spends mana after opening an affordable outline, its newly placed unpaid foundation must be removed, with no delayed completion or refund credit.
2. Once affordable, place each building separately on open Dark ground, including the prepared clearing around (406,308). Keep the full footprint and margin away from units, objects and other buildings. Confirm exactly one charge, the foundation retained while growing for about three game seconds, and one completed building with the same identity.
3. Repeat Temple/Fortress placement elsewhere. Non-Dark ground must refuse; a static obstruction refunds once; units are waited for. It must never force a building into occupied tiles or refund twice.
4. Select a foundation and then a completed Temple/Fortress. Foundations show construction status without recruitment controls. Completed bases offer Warrior/Thrower (15), Gardener (5) and Shaman (10) queues. Unfunded recruitment waits; native mana is charged at birth. Test +/minus/Clear all orders without deleting born units or cancelling automatic Farmers.
5. Save/reload with paid construction and pending recruitment using matching pairs. Jobs/queues must persist without a second charge. Old 0.2.13 paid conversions refund once and are not recreated; NEW placements are required for acceptance.

## Manacopter controls and real passengers

1. Queue two aircraft at a Hall: 300 mana each, sequential 30-second assembly. Cancellation must refund only pending prepaid jobs. Record both aircraft IDs.
2. Left-click each visible aircraft sprite, then right-click empty ground. Only the selected aircraft must fly, including the older one. Test camera/zoom changes. The helicopter tab/Hall selector and Select next aircraft are alternative selectors. Destination opens coordinate controls/Fly; Passengers returns to the roster. Selecting another unit/building restores normal orders.
3. Land on open ground. Select two Warriors and one Thrower, leave another eligible unit unselected nearby, then right-click the aircraft or the ground right next to it (with HD this move order is the reliable way). The log should show BOARD_ORDER/BOARD_QUEUE, then BOARD_CALL accepted=1 per unit. BOARD_REFUSED gives the reason if the aircraft is not considered landed. Only selected units must approach and board when within six coordinates, up to six passengers/reservations. Repeat with Gardeners and ordinary Shamans, including a former delivery Shaman from an old save. Automatic Mushroom Farmers must remain outside. Airborne/full craft refuse; repeated orders must not duplicate passengers.
4. A new ordinary movement order cancels boarding for its selected units. Flight/unloading cancels the craft's approaching units. Unreachable approaches expire after 60 game seconds without teleportation or new units.
5. Check Passengers N/6, exact native passenger type/ID rows and Boarding N, also during flight. Unload passengers is enabled only when landed with passengers and all six native exit tiles clear. The same IDs must reappear and accept normal orders. Obstruct an exit tile: unloading must refuse without losing or duplicating passengers.
6. Return to base should find a clear entrance. Shaman enslavement/captives remain a separate native acceptance check; the mod creates no captives/free troops. Save/reload with passengers; IDs must persist. Reissue transient walking-to-board orders after loading. Recycled unit/craft pool IDs must never replace the original identity.

## Existing matched saves

1. Back up an older schema-2 `.sav` and its matching sidecar, then load the pair. Current native mana, native units, paid construction and Hall orders must remain. Old simulated stock/cargo and pending diverted harvest must not create a refund or duplicate bank credit. Previously hired delivery Shamans remain alive and accept normal commands; the mod sends no new farm orders (an existing native movement order may finish).
2. Save again and reload the new matching pair. There must be no recurring inventory conversion, bank credit, replacement Shaman or second job payment. Old source files remain untouched until your save action. Missing/mismatched sidecars disable the mod economy; preserve logs and close/relaunch.
3. Earlier inflated balances are deliberately retained. Use a NEW game to judge the mana fixes.

## Viking startup, visibility and victory

1. Vikings start outside Dark territory at (75,401), with four occupied towers and six economy buildings. Their entrances/ground must belong to player 2. Tick START_SECTOR log entries should show owner=2, nonzero sector/economy and economyPresent=1, especially barracks kind=24.
2. Observe actual workers, wood/boards/stone delivery, new construction and recruitment at 5/10/20 game minutes. Native AI attack delay is 20 minutes on easy, 15 otherwise; strength/pathfinding may postpone attacks. Starting buildings alone do not establish development.
3. No Viking troops/leader should remain inside the Dark start. Dark and newly darkened remote terrain should stay visible, while ordinary enemy land keeps normal fog; check the main view and minimap.
4. Victory must arm only after both genuine bases exist. One destroyed tower must not win while another enemy military building remains. Destroying all enemy military buildings should win; losing all completed Dark Temples/Fortresses should lose. Waiting for both starting bases indicates an initialization failure.
5. With the game closed, Restore original files.cmd should restore originals/remove only this scenario while retaining saves and sidecars.

For failures, retain exact clicks, coordinates, game minute, resource amounts/entity IDs, the newest `Trace*.txt` from game/User Documents logs, runtime/helper/status logs, and the matching save pair. Record whether construction failed at outline, placement, foundation selection, conversion or completed selection. Historic signatures include Fortress RVA 0xD428A/read 0x12E, startup CBarrackRole RVA 0x134BFE/read 0x345, and later startup RVA 0x1A36D2. A first-chance log entry alone is not proof of an unhandled crash.
