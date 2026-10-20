# Experiment journal

## Round r44: economy telemetry (2026-10-20, still 0.2.15)

- User request: log what happens in the computer economies all the time so issues can be analysed later.
- Implementation (research/ECONOMY_TELEMETRY_0215.md): every `dtledger::EcoEvery` 500 ticks each active computer gets one row in `evidence/v02-economy.csv` (ms and pid as in the runtime log): population and settlers per type from the engine's Statistics counters, completed buildings per type from the per-player table and sites from the building lists, housing value (0xDF730), home-economy carriers/busy/reserve and the r43 keep, and the free stock of 22 goods. Pure formatting in `native/economy-telemetry.hpp` (tested in offence-tests `ledgerRows`), reads in the bridge `ledger()`, writer `ledgerWrite` in dark-tribe.cpp. `tools/economy-report.py` summarises the file per process, map and computer and flags drains, stalled construction, missing builders/smiths, starved weapon chains and flat population.
- Status: built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested. Startup marker: `...; scaling carrier reserve r43; economy telemetry r44`.

## Round r43: scaling carrier reserve (2026-10-20, still 0.2.15)

- User report (Iron Kingdom): the computer trained so many soldiers that no carriers were left to transport or build; the user wants a reserved population that grows with the kingdom, so that more soldiers need more houses.
- Cause (research/AI_OFFENCE_0215.md, Carrier drain analysis): the barracks drafts an idle carrier while carriers > busy + reserve, and the AI's own CPeopleManagerTask sets that reserve as clamp(min(7*pendingTasks + 10, carriers - 10), 5, 80) every pass, so the reserve follows every draft down to 5. The scenario SoldierLimit variables have no reader in this build.
- Fix (Round r43): the one call of the reserve setter (0x2567B8 -> CEcoManager +0x138) is redirected at startup to `reserveHook`, which raises the AI's value to `dtoffence::carrierKeep(completed buildings)` = 15 + one per 2 buildings, at most 120 (`ReserveBase`/`ReservePer`/`ReserveMax`, offence-model.hpp, tested), and calls the native setter. Pinned, fail-closed (code 17), idempotent; `inspect-construction.py` checks the site, the setter, the formula and the building table. Log: `CARRIER_RESERVE` on changes, `keep=` in `OFFENCE_SUPPLY`.
- Status: built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested. Startup marker: `...; map reveal toggle r42; scaling carrier reserve r43`.

## Round r42: map reveal toggle (2026-10-20, still 0.2.15)

- User request: Alt+9 toggles a view of the whole map.
- Implementation (research/DARK_SIGHT_FOG_0215.md Round r42): keys polled on the tick (foreground process and in game only, one edge per press), and while on every tick clears the pending fog bits of every cell so the native merge shows level 0 everywhere; base bits and counters untouched (saves honest, natural re-fogging when off). `dt::revealPending`/`dt::pressed` tested in aircraft-tests; `inspect-construction.py` asserts the contracts; `MAP_REVEAL` log lines.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `map reveal toggle r42`. Not verified in game. Startup marker: `...; no Manacopter healing r41; map reveal toggle r42`.

## Round r41: no Manacopter healing (2026-10-20, still 0.2.15)

- User report: a landed Manacopter heals to 100% almost instantly; enemies cannot kill it; it should not regenerate at all.
- Cause (research/HELICOPTER_023.md Round r41): the native CManakopter update stores DTManakopterHitpoints into the aircraft's hit points every 10 ticks while it stands on a Dark cell (flag 0x04) at its destination (0x16B58F..0x16B597).
- Fix: the one store (88 47 21 at 0x16B594) is NOPed at startup (`installNoHealing`, pinned, fail-closed, idempotent); the constructor and the damage path are untouched. `inspect-construction.py` checks the sequence and the other hit-point writers.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `no Manacopter healing r41`. Not verified in game. Startup marker: `...; Iron Kingdom toolsmith hammers r40; no Manacopter healing r41`.

## Round r40: Iron Kingdom toolsmith hammers (2026-10-20, still 0.2.15)

- User question after the Iron Kingdom game (pid 8520): why no shovels were made with 16 iron bars and 63 coal in stock. Answer (log): the toolsmith made about four shovels before tick 22,500, then lost its smith; the AI's building manager sat in "built toolsmith waiting for its smith" for the rest of the game (it clears the shovel order in that state: the plugin's restore counter rose from 190 at 22,510 to 17,519 at 132,000) and no smith ever came, because a smith needs a free carrier plus a hammer, the kingdom's 15 start hammers were gone, the toolsmith is the only hammer maker, and the carriers had become soldiers (40 at 24,000, 5 at 60,000). The iron and coal were the economy's stock, still delivered to the weaponsmith (15 swords), never usable by a toolsmith without a smith.
- Change (map only): `build-iron-kingdom.cjs` places HAMMER x2 beside the toolsmith entrance like First Dawn and The Alliance (`placeToolsmithPiles` default); `test-iron-kingdom.cjs` expects the pile (count, goods, totals, distance). The Gardener-supply proposal (emergency shovels, Gardener reserve, pull-back, soldier limits) stays unimplemented at the user's request.
- Status: Staged on 2026-10-20 (with r41): the full `python tools/workflow.py` passed, `test-iron-kingdom.cjs` checks the hammer pile and the staged DLL contains `Iron Kingdom toolsmith hammers r40`. Not verified in game. Startup marker: `...; Dark Gardeners leave the Manacopter r39; Iron Kingdom toolsmith hammers r40`.

## Round r39: Dark Gardeners leave the Manacopter (2026-10-20, still 0.2.15)

- User report: Dark Gardeners dropped from a Manacopter are "invincible", selectable but not properly, and enemies cannot attack them. Only this bug is in the round; the Gardener-supply proposal of the Iron Kingdom analysis stays unimplemented at the user's request.
- Cause (file-only disassembly, research/HELICOPTER_023.md Round r39): the native unload posts event 0x19 (leave at the exit tile) to each passenger; soldier and computer-Gardener roles place the unit, re-index it and clear the hidden flag, but the Dark Gardener role's event handler (0x174F10) knows only 0xD and 0x11 and treats 0x19 as a scheduler re-add. The dropped Gardener stayed hidden-flagged, without a ground cell and without a spatial index entry, while its 0x11 event still gave it work.
- Fix: after the native unload the plugin does the 0x19 part for Dark Gardeners (0x182A40 at the native exit tile, 0x1BB560, flags &= ~0x8000 | 0x2100); `PASSENGER_PLACED` logs it. Pins and vtable slots in `inspect-construction.py`; `dt::unloadExit` tested in aircraft-tests.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `Dark Gardeners leave the Manacopter r39`. Not verified in game. Startup marker: `...; walked progress and steady sieges r38; Dark Gardeners leave the Manacopter r39`.

## Round r38: walked progress and steady sieges (2026-10-20, still 0.2.15)

- User report (First Dawn on r37): "stops attack at the middle of it, It moves a bit then going back".
- Field cause (pid 3832): the walkable way from the Viking base to the farms loops south-east around the lake, so the hex distance to the leg end grows for more than 1,500 ticks and r37's no-progress rule failed every march about 3,000 ticks in (`target not reached`, then home). Three large sieges of farm 1607 were given up as `lured toward the fire defence` after single soldiers on formation cells crossed the fire line three times.
- Fix: march progress and leg/target budgets by the walked steps (bridge `walkSteps`: a breadth-first field per leg end, the last 4 kept); a lure episode needs LureMin 3 soldiers (or a tenth of the army) at once; detourLimit 4 x hex + 100; `OFFENCE_TARGETS` lists every building when nothing can be attacked.
- Tests: offence-tests `marchBudget` (looping walk never late), `guarding` (stragglers do not count), `unreachable`, `gating`; `inspect-offence.py` updated.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `walked progress and steady sieges r38`. Not verified in game. Startup marker: `...; relentless and smart attacks r37; walked progress and steady sieges r38`.

## Round r37: relentless and smart attacks (2026-10-20, still 0.2.15)

- User report (after testing all three maps on r35): "they attack rarely, really rarely, and uneffective, they go for 1 farm and then different farm which is much further away ... rework it so computer players are much more offensive, and attack smartly".
- Field diagnosis (pid 17196, games 2..4; research/AI_OFFENCE_0215.md Round r37): First Dawn 14 campaigns in 167,000 ticks, 8 ended "target not reached" (MarchTimeout 4,000 on farms 290+ hex away, then avoided 6,000..24,000 ticks), five hunts in a row, about 4,500 ticks between campaigns; Iron Kingdom 8 campaigns, a chain that walked 318 hex past a farm 40 hex away (the home-based detour was added to the army-based distance), 46,000 ticks of "no gardeners" with 14 alive but stranded; The Alliance 3 attacks and 117 defence lines (thousands of own Dark cells from the start made every decision a 6,000-tick land defence).
- Targets: every launch and retarget runs one breadth-first search over the land region from the chooser's position (bridge `walkAll`, `ops.paths`) and picks by the walked path, with a cluster bonus for farms that have neighbours (ClusterRadius 40, 8 per neighbour, at most 24); a target without a walk from here is skipped.
- Cadence: first attack 2..4 minutes, checks every 128..255 ticks, no chance gate for an army of AssaultMin or more, rest 150..449 ticks, raids chain 5 buildings, hunts and sweeps weigh 1 while a building is in reach.
- Budgets: a leg has MarchTimeout 2,000 + 25 ticks per hex and fails only without progress for 1,500 ticks (`stalled()`), a target 6,000 + 25 per hex; a failed target is replaced from where the army stands instead of walked home from; LongAvoid 12,000.
- Defence: land alone never comes before an attack (`attackable()`): deferred until 12,000 ticks after the first attack time or the last land defence, then an escort (escortSize) for 3,000 ticks; only Dark fighters recall a running attack.
- Gardeners: a decision short of gardeners calls the 4 nearest far ones home (every 600 ticks, `gardeners called home`); recruitment counts only gardeners within LaunchReach of home.
- Tests: offence-tests `targetOrder`, `gating`, `styles`, `campaign`, `defending`, new `landFirst` and `marchBudget`; soaks unchanged and passing. `inspect-offence.py` asserts the new contracts. near/far are Windows macros (renamed kin/away/close).
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `relentless and smart attacks r37`. Not verified in game. Startup marker: `...; fire pull and route diagnostics r36; relentless and smart attacks r37`.

## Round r36: fire pull and route diagnostics (2026-10-20, still 0.2.15)

- User report (r35 build): near a Dark Temple or Fortress the gardeners "get lost from army and seems like strategy stops working properly ... minions are just moving back when they are too close but still want to go same direction ... without properly changing pathfinding". First a check and a proposed fix, then: "First add the diagnostics only (log where each pulled unit stood and which leg it was walking), so the cause is confirmed in my next game."
- Field evidence (pid 17196, r35, First Dawn, Temple fire centre 341,302): 284 soldier pulls and 276 gardener recalls; two loops on the way home past the Temple's north-west side (ticks 59,790-63,070 and 147,250-148,370, 1-8 soldiers turned back every 10 ticks, exit cells 306-337/260-275 between the ring and a rock band); a defence lost all 12 gardeners to benching within 1,800 ticks.
- Likely causes (unconfirmed, research/AI_OFFENCE_0215.md Round r36): geometric routes that the native pathfinder bends around rocks and buildings; stragglers ordered to the next leg end from the previous leg (corner cutting through the fire); a guard that pushes units radially out and replans the same route; gardeners benched by walking dips and by their native greening search. Proposed fix there (real walked paths with the fire blocked, per-unit legs at waypoint changes, a guard that changes the route, no benching of escorted gardeners).
- Diagnostics (log only, no behaviour change): `OFFENCE_FIRE_PULL` for every unit the guard turns back (soldiers, held gardeners with bench flag, reclaim's free gardeners) with the ordered leg (from, to, previous leg, clearance, cells off land or built on, age, army body) and per unit its fire clearance and distance to the current and previous leg; `OFFENCE_ROUTE` for every plan of a marching or homeward route (waypoints with clearance, legs with clearance/off-land/built cells) and for every replan or held way home that gave the same route (`same=`). Model hook `ops.pulled()`; the bridge formats it (`offenceSegFire`, `offenceLegLand`, `offenceRouteNote`).
- Tests: offence-tests check the hook (pulled soldier id, cell and exit; a free gardener's first recall); `inspect-offence.py` asserts the hook call sites (soldiers before replanAt), the log formats and the reset.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `fire pull and route diagnostics r36`. Not verified in game. Startup marker: `...; reclaimed land fog and fast army marches r35; fire pull and route diagnostics r36`.

## Round r35: reclaimed land fog and fast army marches (2026-10-20, still 0.2.15)

- User reports (r34 build): with a screenshot of green land still lit next to the fogged land around it: "the area which has been taken by dark tribe but then took back by computer player its still showing visible for me, even when its green again". And: "Due to computer army always try to stay near gardeners while executing strategy, the whole movement of army is ullllltra sloooooow ... It should not babysit gardeners that hard, movement of army should be fast, not like barely moving becouse babysitting all time."
- Fog cause (file-only disassembly): every 5 ticks the plugin clears land byte 3 bits 0-5 on Dark cells (the Dark-land visibility). Land byte 3 holds the fog level of the cell (bits 3-5 current, bits 0-2 previous for the fade; 0 visible, 3 explored, 7 black). The native fog merge (CFogging slot 5 0x1AEAB0 -> 0x1AE3F0, MMX, 8 cells per step) writes level = min(pending, max(old, 3)) (7 jumps straight to pending), but it counts stable cycles in the fog byte's bits 6-7 (`paddusb` 0x40, `and` 0xC0) and skips the land write for a group of 8 cells that are all at 0xC0. A cell nobody sees is stable, so once the land was green nothing overwrote the plugin's level 0 and it stayed lit.
- Fog fix: `dt::darkSight` (aircraft-model.hpp) marks every cell it reveals; when a marked cell is no longer Dark it clears that cell's counter once (fog byte map at fog manager +0xC, `&=0x3F`). The next merge then applies the native rule (back to explored, or visible if a unit sees it) and marks the rows for the renderer itself. The marks are transient: mapInit empties them, and the first pass of a map re-checks every non-Dark cell once (this also heals cells an older round left lit in a save). Guard: the fog manager's landscape (+8) must be the view landscape and its byte map readable, else `DARK_SIGHT fog map unavailable` and the marks wait. Logs: `DARK_SIGHT fog re-checked map= dark= handedBack=` once per map, `DARK_SIGHT greened cells handed back to native fog=` at most every 600 ticks.
- Fog pins: the merge's counter and skip code 0x1AE438..0x1AE49B (with the relocated step-table pointer) and the step table 0xC1BD30 (0x40, 0x18, 0x07, 0x38, 0xC0, 8 bytes each) are checked at startup (`Pinned fog merge signature mismatch; plugin disabled`, code 15). `inspect-construction.py` checks the merge rows, slot 2 (0x1AE600 stores the landscape at +8 and the byte map at +0xC), the vtable slots and slot 5's call.
- Army cause (pid 22316, r34, First Dawn, Vikings): campaigns held `waiting for gardeners` for 2,000-5,200 ticks (ticks 40,150-45,360 with the gardeners 234 tiles behind, 58,410-60,590, 67,760 on), three marches ended `target not reached`, and at ticks 141,000-152,000 a 221-soldier army logged `order not executed` three times per target while alternating between farms 635 and 637 without moving. The r32/r33 convoy ordered the army only ConvoyLead (10) tiles beyond the slower of army and gardeners every 30 ticks (each order re-forms the whole army) and held it whenever a gardener was beyond convoyLimit (about 14).
- Army fix (offence-model.hpp `convoy`/`follow`/`gathered`): one order per leg to its end (waypoint, staging cell, assault cell, home), re-sent only when the leg end moves or after 600 ticks (30 near the fire). No hold, no walk-back, no rally. The gardeners follow to their own cells around the same leg end (WATCH, one id per order, re-ordered only when the cell changes); one whose straight way there passes the fire takes the latest point of the stored route it reaches clear. Waypoints do not wait. The staging cell waits for the gardeners at most StageWait (300 ticks): `OFFENCE_CONVOY reason=waiting for gardeners`, then `assault before the gardeners`; late gardeners join at the rear point behind the screen. A direct assault (no staging) now happens within AssaultDistance 40 (was 2 x ConvoyLead) when the gardeners are together. The assault leg is one order (the r33 assault-leg convoy is gone). The way home waits only at home (HomeTimeout).
- Removed: ConvoyStall, RallyLimit, ConvoyHysteresis, ConvoyBehind, the Campaign fields rally/rallies/bestGap/bestAt/assaultX/assaultY/assaultLeg and the reasons `army walks back to the gardeners`, `gardeners do not follow`, `rally to the gardeners`.
- Tests: aircraft-tests `darkSight`; offence-tests `convoyRules`, `convoyField` and the r33 review case rewritten for the new contract; `convoySoak` (walking soak) now asserts the army never stands still for more than 3 looks on a march away from its order cell (measured 0 in all 24 runs), a staging wait of at most StageWait + 150 (measured at most 160) and that a gardener that joined the army at the target stays within EscortRadius + 4; `defenceSoak` keeps its in-place bounds for joined gardeners. `inspect-offence.py` asserts the new convoy text and the absence of the removed parts.
- Limits: not verified in game. Lagging gardeners now walk behind the army without escort, so Dark fighters can catch them on the way (soak gaps up to 76 tiles). Whether one order per leg ends the 221-soldier `order not executed` loop is expected but untested; the native group order still takes at most 100 soldiers per order. Remote fog effects (minimap) are not checked offline.
- Status: Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `reclaimed land fog and fast army marches r35`. Startup marker: `...; site selection crash fix r34; reclaimed land fog and fast army marches r35`.

## Round r34: site selection crash fix (2026-10-19, still 0.2.15)

- User report: "game crashed after I runned it, few second after round started, apply fixes" (r33 build).
- Field evidence (pid 8768, First Dawn, tick 7014, about 26 s into the map): `FIRST_CHANCE c0000005 at S4_Main.exe+0x143E07` read 0x8E inside CBuildingSiteRole slot 22 (the native site panel, RVA 0x143D10), reached from a click (`SelectSingleEntity` 0xEB3B0 -> 0xEE730). 0x8E - 0x24 = 0x6A, so the inhabitant byte +0x1DE was 53 (Mushroom Farmer): the role still pointed at the native Farm info, and the economy at the site entrance was null (Dark land). The two farm sites in the log (1229, 1377) were paid and already panel-safe; the player was in farm placement (`PLACEMENT_SCAN kind=49` at 7008) and no line mentions a third site.
- Cause (file-only disassembly): `sub_E9AC0` runs the plugin tick hook (0xE9B18) once and then the whole event ring: the build net event (CNetInputProcessor, BuildBuilding at 0xF6714) and later click events (0x231100 -> selection). A site created in that batch can be clicked before any plugin tick sees it. Nothing native keeps a class-8 site of the player out of `SelectOneAtPoint` (0xED630), so no tick-side change can close the window. Unpaid Temple/Fortress sites also carry their material piles from creation (also null-economy reads), and the post-load role slot 7 (0x1419C0) writes the native info back.
- Fix: the CBuildingSiteRole slot-22 entry (RVA 0xC59214, pinned) is replaced once at startup by `dtconstruction::PanelGuard::panel`. It repeats the panel's own economy lookup (0x143D43..0x143D7B). With an economy, or when nothing the panel reads needs one, it calls the native panel unchanged. Without an economy it calls the native panel with a transient view (a copy of the site's own info with inhabitant/tool cleared, piles 0) and restores the role's fields right after the call. Unreadable native reads (the native panel would fault) or a nested call skip the refresh. This covers every caller and every site (any race, kind, owner, paid or not, refused, after a load, plugin disabled). Completion is unchanged: each completed role writes the native info itself (Mushroom Farmer provisioning reads it).
- The wrapper runs inside the native input loop: heap-free, lock-free, no Lua or log; the tick logs `SITE_PANEL_GUARD views= skips= site= kind=` when its counters change.
- Pins: (0xC59214,4) slot entry, (0x143D3B,0x48) economy lookup, (0x143DC2,0x55) tool/inhabitant reads, the three pile reads and economy calls, (0x1440EB,3) `ret 8`. `inspect-construction.py` checks them, that the slot entry is the panel's only reference in the image, and source contracts (no heap, log or Lua in the guard). Pure logic in `native/site-panel-model.hpp`, tested by `construction-tests` `panelGuard()`.
- Checks (offline): construction-tests pass, `-fsyntax-only` and a full scratch compile of the DLL (the wrapper is `@16` fastcall with `ret 8`), `inspect-construction.py`, `inspect-interactions.py`, `inspect-offence.py` pass. Not built or staged by this change.
- Limits: not verified in game. Site pause (slot 8) still uses the economy while flag 0x1000 is set; it is a net order processed after the plugin's next tick, and the Dark menus cover the native site panel.
- Status: Staged on 2026-10-19: the full `python tools/workflow.py` passed and the staged DLL contains `site selection crash fix r34`. Startup marker: `...; computer defence and reclaim priority r33; site selection crash fix r34`.

## Round r33: computer defence and reclaim priority (2026-10-19, still 0.2.15)

- User requests:
  - With a screenshot of computer soldiers idle while the Dark land spread into their territory: "Add computer defensive strategy: when I'm taking and making their area dark they should be defending it, but now they do nothing. Also they should prioritize reclaiming it."
  - "Gardeners still moving away from main group of soldiers, so you need to fix it."
- Field diagnosis (pid 13612, r32, First Dawn, Vikings):
  - No `OFFENCE_RECLAIM` line in about 95,000 ticks, although the Vikings' owned Dark cells rose to 5,805 and 8-13 gardeners lived. Reclaim used only idle free gardeners (campaigns held most of them for about 55,000 ticks), searched only 120 tiles around them (the Dark land lay 194 hex from home, behind a lake and a river), stayed 10 tiles away from Dark fighters and logged nothing when it failed. No code sent soldiers to owned Dark land.
  - Native cause of the idle soldiers: CAIAgentIntruderCombat treats only types 46, 48, 49, 47, 50, 45 as intruders (.rdata table 0xBF0438), so Dark units never are; idle soldiers only engage within about 13 tiles.
  - Farm 638 was attacked five times and never reached: a 695-step walk for 227 hex, straight convoy legs over water (15 `OFFENCE_ORDER_IGNORED`), three march timeouts, and a 1,000-tick avoidance that never outlasted the next launch.
  - Gardeners walked away because every convoy move was a FORWARD work order: each cell became the gardener's work centre and it greened its way off on arrival. A held army sent no orders (its gardener cells were the soldiers' formation cells), re-orders ignored where the gardener stood, and stragglers were sent home alone after 600 ticks (seven at tick 15,560; gardeners 13 → 5 by tick 16,760). Waiting gardeners at the target got FORWARD orders into Dark land too.
  - Iron bars 8 → 0 by tick 18,000 (the weaponsmith used them), so the r29 shovel order (4) was restored 1,443 times for nothing.
- Defence (`assess`/`defend`/`answer`/`hold`):
  - The bridge finds Dark Gardeners, Warriors, Throwers and Shamans standing in or within 4 of a computer's territory (the native 0x1B17E0 owner rule, re-implemented) plus the growth of its owned Dark cells per scan pass.
  - Threat: a Dark unit, or 24+ owned Dark cells. Serious: 150+ cells or a rise of 40+ (only with reclaimable land), a Dark fighter within 40 of home, or 3+ Dark Gardeners. Units within 42 of a live fire centre are ignored.
  - An idle sector defends before any attack decision; minor threats alternate with attacks (6,000 ticks between minor defences). A serious threat recalls a running attack (at most every 3,000 ticks, not when a siege ring is two-thirds green).
  - Size: at least 6 soldiers or a quarter of the army (half if serious), more until twice the Dark fighters' weight; declined when even the whole army is lighter. Gardeners 6 + soldiers/15 + own cells/100, from near home.
  - The soldiers stand FORWARD next to the Dark units (native combat engages within about 13), the gardeners green own Dark land within 16 of them; the defence moves area by area and ends with "land clean", "defence time" (6,000 ticks) or "threat too strong".
- Reclaim priority: own Dark land is searched on the whole map, but a free gardener goes alone only within 50 of itself or, from home, within 130 of home; farther land is the defence's. 8 gardeners per pass while defending. A pass that sends nobody says why every 3,000 ticks. Parked or stranded free gardeners walk home when no Dark fighter is near.
- Unreachable targets: a cached walking search per target from home (at most one per tick, 6,000-tick refresh) marks a target unreachable beyond 2.5 × hex + 20 steps and adds the detour to its score. A failed target (march timeout far from the stage, target timeout, 3 ignored orders, gardeners that do not follow) is avoided 6,000 ticks, from the second failure 24,000. Convoy points on water move to the nearest standable cell.
- Convoy fix:
  - Escorted gardeners move with WATCH (job 0xB0 `JOB_GARDENER_FORCE_WALK`: walk, then wait; the work centre stays). FORWARD only for a ring cell, a sweep or defence cell, reclaim and the release at home.
  - Gardeners get their own cells off the soldiers' formation grid and are re-ordered from their real job and destination, at most every 30 ticks.
  - The gap is measured to the nearest soldier of the army body (median-based). A held army really stops, and after 600 ticks without progress walks back to its farthest gardener (at most 4 times per leg) instead of sending stragglers home; then the target fails ("gardeners do not follow").
  - Gardeners are mustered only within 130 of home (60 of the army for hand-overs). The long assault leg is a convoy too. At the target, gardeners without a ring cell are parked behind the screen again, working or not; at a citadel that parks settlers, the surplus is parked instead of sent home.
- Toolsmith: no shovel-order restore while the home economy has no iron bar. The scenario start goods are unchanged.
- Native facts (file-only, pinned and inspected): order mode copy 0x16A9E3, gardener WATCH handler 0x179B90/0x179BAB, job id entry 0xC049D0 (0xB0), intruder owner test 0x1B1808, intruder type table 0xBF0438; Dark unit groups Gardener 7, Warrior 2, Thrower 3, Shaman 4, Mushroom Farmer 1.
- Logs: new `OFFENCE_DEFEND` (start, recall, declined, end with phase=3). `OFFENCE_RECLAIM` adds `none sent: ...` and `parked gardener sent home`. `OFFENCE_CONVOY` adds `army walks back to the gardeners` and `gardeners do not follow` (`gardeners left behind` is gone). `OFFENCE_AVOID` adds `orders not executed`, `target not reached`. `OFFENCE_TOOLS` adds `no restore while no iron bar is in stock`. Startup marker: `; computer defence and reclaim priority r33`.
- Review: seven confirmed findings fixed (free gardeners sent alone across the map; defences launched with no gardeners and looping; the defence counting fighters inside the fire field; an expired path cache making an unreachable target reachable; unreclaimable cells near the Dark base keeping every threat serious; surplus citadel gardeners sent home alone; a stop order to an unchecked median cell). Each fix's test fails when the fix is reverted.
- Checks (offline): offence-tests pass, including `convoyField` (the field gaps; fails against r32), `defending`, `reclaimRegression`, `unreachable`, `reviewR33`, `defenceSoak` (469 own Dark cells → 0, no deaths, never over the convoy limit) and `convoySoak` (no deaths, gap at most 1 over the limit). All other model tests, `-fsyntax-only` of the DLL and `tools/inspect-offence.py` (318 instruction checks, 96 regions) pass.
- Status: Staged on 2026-10-19: the full `python tools/workflow.py` passed and the staged DLL contains `computer defence and reclaim priority r33`. Not verified in game. Limits are in `research/AI_OFFENCE_0215.md` (Round r33): WATCH parking, soldier combat range and the in-game walking search are unverified, and defences may hold back attacks too strongly while much Dark land stays in a computer's territory.

## Round r32: scaled styles and protected convoy (2026-10-19, still 0.2.15)

- User requests:
  - "Make all styles grow so it will vary across all styles."
  - From a screenshot of the user's game: the soldiers had already reached a Mushroom Farm while their Gardeners trailed behind unprotected. "Make it so they move all together, protecting them."
- Cause, read from the r28-r31 code:
  - Gardeners set off from wherever they were working (63 tiles from the army in the soak).
  - A march leg ended when 70 % of the soldiers had arrived.
  - For a target within 40 tiles, the gardeners got no order until the soldiers held it.
  - Replacements and a hunt's gardeners came straight from home.
  - The army was released at home without its gardeners.
- Scaling (`native/offence-model.hpp`):
  - Raid: a third of the army, at least 10.
  - Hunt: half the army, at least 16.
  - Sweep escort: a quarter of the army, at least 8.
  - None takes more than the army. Assault, pincer and allied still take the whole army.
  - Gardeners: the r28 number per style plus one per 15 soldiers the style takes.
  - Cap: 12 gardeners plus one per 10 soldiers above 40, at most 20 (was 12). Recruitment and the toolsmith shovel orders use the same number.
  - With 120 soldiers: raid 40, hunt 60, sweep escort 30, assault gardeners 16.
  - Style weights are unchanged.
- Convoy:
  - March: every 30 ticks the army goes at most 10 tiles ahead of the convoy's rear (the army or its gardeners). Each gardener gets its own order to a cell 3 tiles behind that point.
  - Waiting: the army holds while a gardener is farther than 8 + the formation spread from it. After 600 ticks, stragglers are dropped and sent home. A march left without gardeners goes home.
  - Legs (waypoint, staging cell, home) end only with the convoy together. Near the fire the army never holds; it leaves directly.
  - At the target, the soldiers screen within 6 tiles of the door, on the side of the nearest Dark fighter. The gardeners wait 4 tiles behind, away from that fighter. They get ring cells when the screen holds, or after 300 ticks with the army within 16 tiles of the door, and never while defenders are at the door.
  - Top-ups come only from free gardeners within 20 tiles of the army.
  - The sweep front moves only while the escort holds it, and its gardeners stay within 16 tiles of the escort.
  - After a hunt, the army walks back to its gardeners when they are more than 30 tiles away (`rally to the gardeners`).
  - Allied joins, the way home and squads adopted after a load use the same rules.
  - Scaled raids stay raids after a load.
  - Every new gardener order goes through the r30 fire keep-off.
- Logs:
  - New: `OFFENCE_CONVOY` (waiting for gardeners, gardeners left behind, rally to the gardeners; at most every 300 ticks).
  - Campaign lines gain `gap= wait=`. `OFFENCE_RECRUIT`/`OFFENCE_RECRUIT_WAIT` gain `wanted=`.
  - Startup marker: `; scaled styles and protected convoy r32`.
- Review: five confirmed findings fixed. They were the rear point facing the threat, gardeners left behind sent home, the army going home without them, convoy steps only along clear legs, and scaled raids adopted as assaults after a load. Each fix's test fails when the fix is reverted. A broader "go home without gardeners" rule was tried and reverted, because it aborted campaigns whose gardeners the fire guard had benched.
- Checks (offline): offence-tests passed, including:
  - `scaling`;
  - `convoyRules` with the review cases;
  - `convoySoak`, a walking simulation of all six styles with armies of 34 and 117 and gardeners at the soldiers' speed or 50 % slower. No deaths, convoy gap at most 21, no gardener more than 22 tiles from the nearest soldier, every assault with the convoy together.

  All other model tests, `-fsyntax-only` of the DLL and `tools/inspect-offence.py` also passed.
- Status: Staged on 2026-10-19 together with r31: the full `python tools/workflow.py` passed and the staged DLL contains `clean start maps r31` and `scaled styles and protected convoy r32`. Not verified in game. Limits are in `research/AI_OFFENCE_0215.md` (Round r32): walking speed per type is assumed equal, armies may march more slowly, and the march timeout can still start an assault without the gardeners.

## Round r31: clean start maps (2026-10-19, still 0.2.15)

- User request: remove the start farms near the Temple/Fortress from the Dark Tribe maps themselves. The r30 crush at the start works, but the maps should be clean. Future agents must clear such farms when making new Dark Tribe maps.
- Maps: the builders remove every Dark Mushroom Farm closer than FarmLock (Euclid 33) to a Dark Temple/Fortress fire centre (position + (1,2)), the same rule as the r30 blockade. They also clear the flags the map editor baked under it: 0x08 on the 52 footprint cells, 0x01 on the blocked core, 0x02 on the entrance cells.
  - First Dawn: 337,282 (Euclid 18.3 from the Temple at 340,300); 7 start farms remain.
  - Iron Kingdom: 391,124 (12.8) and 413,161 (25.5), both by the northern Temple at 387,133; 9 remain.
  - The Alliance: nothing removed (it has no start farms).
  - Scope: Temple (50) and Fortress (51) only, the two buildings with a fire and the ones the r30 crush covers. The user's r30 test (pid 15144, `FARM_FIRE_REMOVED 337,282 ... citadel=340,300`) is the start removal the request calls working; 340,300 is the First Dawn Temple. The Manacopter Hall (80) has no fire, so the Iron Kingdom farm 167,110, Euclid 25.5 from the Hall at 137,89, stays. If the user also wants farms cleared around the Hall, that needs a separate Hall radius in the builder rule, the tests and the CLAUDE.md/AGENTS.md rules.
  - The maps store no Mushroom Farmer and no mushroom bound to a farm (no type 53 units; the mushroom-like objects are not clustered at farms), so nothing else is removed. Objects, owners, resources, landscape, units and goods are unchanged. The removal runs after every other placement, so the Viking barracks, toolsmiths, piles and units stay where r30 put them.
  - The build reports (`research/*-build.json`) list the removed farms under `fireCleanStart`.
- Shared geometry: `tools/citadel-fire.cjs`. `checkNative()` parses `native/citadel-fire.hpp` and `native/blockade-model.hpp` (constants, centre offset, fire2, the strict farm test, kinds 49/50/51) and repeats the construction-tests vectors (3,943 cells), so JS and C++ cannot drift.
- Tests: the three map tests assert that no farm lies in a field, that exactly the listed farms are gone with their baked flags, and that every other record and flag is unchanged. Broken starts are rejected: a farm moved in to Euclid 32, a removed farm put back, a farm added beside the Alliance Temple. Euclid 33 is accepted. `verify-maps.cjs` checks every `SCENARIOS` entry, including future ones.
- Rules for future agents: CLAUDE.md ("A new scenario needs") and AGENTS.md.
- Survey (r30 script on the new maps): the Farm anchors outside the fields are unchanged. First Dawn has 1,423 (2,382 with the body rule), Iron Kingdom 282 (692), The Alliance 575 (1,331). About 36/10/21 separate farms fit outside within hex 70 of the citadels. The northern Iron Kingdom Temple still has only 3 (48) anchors outside; that is the map's own terrain, as in r30.
- Native: no logic change. Startup marker `; clean start maps r31`. The runtime crush (`FARM_FIRE_REMOVED`) stays as a safety net for old saves and any future map.
- Checks (offline): the three builders, test-scenario.cjs, test-iron-kingdom.cjs, test-alliance.cjs, verify-maps.cjs, test-placement.cjs, test-scenario.py, inspect-offence.py, inspect-construction.py, inspect-placement.py and inspect-startup-sectors.py passed. Not verified in game.

## Round r30: citadel fire keep-off and farm blockade (2026-10-19, still 0.2.15)

- User decision: computers must not come or dig close to the Temple/Fortress before every Mushroom Farm is destroyed, and soldiers and Gardeners must avoid the fire field in every style and on every path; no farm may stand inside the field (a blockade), and start-map farms inside it are destroyed at once.
- Field evidence (r27/r28 logs): First Dawn farm 634, the start farm at 337,282 (18.3 from the fire centre), cost all 6 gardeners in 1,260 ticks (pid 5672). Sweeps whose fronts parked 32-36 from a fire centre lost 10, 9 and 5-7 gardeners (pids 10104 and 15576): the gardeners greened their way in to about 22-24. Iron Kingdom farm 778 (44 from the centre) was lost to Dark fighters, not fire.
- Reverse engineering (file-only, independently re-checked): only `CDarkTempleRole` (Temple and Fortress; the Manacopter Hall has none) runs the fire 0x1458C0, one pass per 31 ticks per building. Its zone is a **Euclidean** disc around position + (1,2), radius 18/20/22/24 for 1/2/3/4+ ready farms (`[0x10EF7AC + p*0x298]`), reaching 27 hex steps between the axes; the old model measured hex steps. Hits kill (255 damage, 6..21 ticks later) units with a selection group on walkable Dark cells of the door's land sector. The visible flames are drawn only at about 16 tiles, while kills reach 24. Green cells are safe, but a gardener stands on the Dark cell it greens, and its native search reaches about 28 tiles around itself.
- Offence (`native/citadel-fire.hpp` shared geometry, `offence-model.hpp`): every keep is Euclidean from each fire centre at the native maximum, whatever the farm count. `FireLine` 27: no computer unit stands within it. Gardener cells keep 28; soldier destinations keep 27 + formation spread; straight legs keep 3 more. Farms within 33 and Halls within 44 are never targeted, and the Temple/Fortress only once the fire is off. Hunts ignore fighters within 42, and sweep fronts keep 37. The fire counts as on while the native counter, a listed ready farm or a Farm site says so. Exact hex-parallelogram leg checks, a waypoint route search (up to 12 stops), exit points, turned staging lines and a way home that holds outside the fire instead of crossing it. A guard every 10 ticks orders each hot soldier out (per-soldier gate), recalls and benches gardeners whose cell or aimed cell (+0x40/+0x44, jobs 0xAE/0xAF) is within 27, counts lures, and re-plans marches. Free gardeners are checked every step. `FireMargin`, `FireRaidFarms` and the hex `zones`/`exposure`/`detour` are gone. Native AI soldiers in the fire are only logged.
- Blockade (`native/blockade-model.hpp`): a Farm closer than Euclid 33 to an own Temple/Fortress fire centre (completed or site), or a Temple/Fortress whose field would hold an own Farm, is refused with mode 1 before payment, with a player notice. The placement hints never mark a blocked anchor. On a new map, start farms inside a field are crushed once through the player's own demolish path (Kill with attacker 0, no computer credited); a restored save runs the same sweep once (`reason=load`). Expected: First Dawn 337,282; Iron Kingdom 391,124 and 413,161; The Alliance none. Every map keeps room for farms.
- Pins: 10 new offence regions (91 in total; 301 instruction checks) and 8 new construction regions (crush path, ready-counter decrement, Farm credit byte; 47 in total).
- Logs: `OFFENCE_FIRE`, `OFFENCE_FIRE_STATE`, `OFFENCE_FIRE_NATIVE`, `FARM_FIRE_REMOVED`, new avoid/return/decline reasons (`... the fire defence`), `PLACEMENT_SCAN ... fireBlockade=`. Startup marker `; citadel fire keep-off and farm blockade r30`.
- Review fixes (7 confirmed findings): per-soldier resend gate; exact leg checks with hex legs and a route search instead of sampled straight legs (a probe went from 19 deaths to 0); no way home straight through the fire; a two-citadel soak; sweep escort lures counted; the identifier `far` renamed; the load sweep for pre-r30 saves.
- Checks (offline): offence-tests including field-like soaks over all six styles and a Temple-plus-Fortress soak (0 deaths, no soldier within 24, outer farms fall; the soaks fail against the old model or with the guard disabled), construction-tests `fireBlockade`, all model tests, `-fsyntax-only` of the DLL, every file-only inspector and `test-scenario.py`. Staged 2026-10-19 after the full workflow passed. Not verified in game.

## Round r29: computers make shovels for their Gardeners (2026-10-19, still 0.2.15)

- User request: the computers' economies should make shovels when Gardeners are missing. Field log pid 10104 (r28, Iron Kingdom twice): shovels fell from 15 to 2 by tick ~21,000, then `OFFENCE_RECRUIT_WAIT reason=no spare shovels` for about 70,000 ticks; gardeners fell to 0 and no Dark building collapsed, although Iron Kingdom has a toolsmith and an iron/coal chain.
- Reverse engineering (file-only): a toolsmith makes nothing unless a tool is ordered (round robin 0x14D008 over the economy's order bytes eco+0x348, decremented unless 100). The order is net event 0x13A9 -> 0xD6A90(eco, good, amount): 0 clears, 100 is continuous, other amounts are added and clamped to 0..100. The byte vectors are saved with the economy (0xD3F02). The AI's building manager sub_249400 orders shovels only for its diggers (0x249835, always positive). While it holds a built toolsmith task, it also sends `10-2*diggers` (0x249770): 0 at its 5-digger steady state, which clears the order, and negative above, which lowers it.
- Plugin (`offenceTools`, `offenceShovelGuard`; model `shovelOrder`/`shovelTarget`/`ShovelWatch`): every 250 ticks, while gardeners are missing and free shovels are at the reserve, the plugin tops the home economy's shovel order up to the shovels still needed (at most 4). It orders only with a ready toolsmith in that economy and never on a continuous order. Every tick a transient watch puts back the plugin's part after a drop of two or more; a drop of one is production. The order byte is the only persistent state; there is no `dt::State` field.
- Maps (start content): First Dawn and each Alliance people get a TOOLSMITH beside their barracks with IRONBAR x16, COAL x16 and HAMMER x2. Iron Kingdom's own toolsmith gets IRONBAR x16 and COAL x16.
- Logs: `OFFENCE_TOOLS` (`ordered`, `order refused`, `no toolsmith`, `already ordered`, `restored after a native clear`, `restores=`) and `OFFENCE_SUPPLY ... toolsmiths= shovelOrder= ironbars= coal=`.
- Review fixes: the AI's 0x249770 clear (both review lenses, verified by disassembly) is pinned (0x24940E, 0x249720, 0x2497A3), checked in inspect-offence and handled by the watch. The fake toolsmith has an AI pass sending 0 or -4: 126 ticks to refill with the watch, 1,050 without it. The r29 round documents were added.
- Checks: offence-tests, `-fsyntax-only` of the DLL, inspect-offence (268 instruction checks, 81 pins), map tests. Staged on 2026-10-19 after the game was closed: the full `python tools/workflow.py` passed (all offline steps), and the staged `runtime/native/DarkTribe-0.2.15.dll` contains `toolsmith shovel orders r29`. Not verified in game.

## Round r28: aggressive computers, six attack styles, new armies, land taken back (2026-10-15, still 0.2.15)

- User test of r27 (log pid 5672): attacks worked and farms collapsed natively, but First Dawn's first attack came after about 67 game minutes, its gardeners all died and none was ever recruited (silent refusal); in The Alliance every army stayed a 12-soldier raid and raids never went for the Temple; reclaiming was mostly gardeners being called back. The user asked for earlier, stronger and varied attacks (at least 4-5 kinds), new armies after a lost one, many more gardeners, much more land taken back (also Dark ground inside the computers' own territory), and the Temple and Fortress as the goal once the farms are gone.
- Director (`native/offence-model.hpp`): first attack after AttackDelay/3 within 3..8 minutes, 50 % +25 % per check, no 15-fighter rule, a short rest between campaigns; six styles (raid, assault, pincer, sweep, hunt, allied) by weighted choice; every style may take the Temple/Fortress once no ready farm stands (before Halls); assaults fight down to 30 %; raids take at most 3 buildings.
- Reclaim in every phase: Dark ground of the computer's own territory first (rotating band scan of the flag map with the native owner reader 0x58A40 layout), then the Dark border from the first minute; up to 4 gardeners per 150 ticks, no 35-tile keep-off any more (fire and parked citadels only). Gardener recruitment up to 12 every 250 ticks, with `OFFENCE_RECRUIT_WAIT` reasons.
- Reverse engineering (file-only): the native AI re-arms by itself where a barracks, weapons and carriers exist (CAIAgentRecruitment 0x32CD0 -> barracks orders; CBarrackRole 0x134AC0 converts carrier + weapon; weaponsmith orders via event 0x1E). Flag 0x80 is military influence, so the r27 `free()` test refused every cell in computer territory: fixed.
- Maps (`tools/scenario-supply.cjs`, start content only): First Dawn gets 48 weapons at its barracks, 16 shovels and 30 carriers; Iron Kingdom a barracks for its weapon chain, 32 starter weapons and 16 shovels; each Alliance people a barracks of its race, 32 weapons, 16 shovels and 40 carriers. The new barracks stand on level ground like each map's own buildings (heights and slope shading of their levelled cells only). Map tests check every new record and refuse moved, missing or unlevelled ones.
- Four adversarial review rounds (each finding re-checked by an independent verifier): wider keep circles around citadel rings; while a Temple/Fortress parks settlers at most dark-2 gardeners of one campaign work its ring and the rest walk home, for at most 900 ticks per target; a target is given up for 6,000 ticks once as many gardeners died there as the style takes (or all did); fire-locked farms no longer block the Halls; rotating reclaim start; no target inside the fire of 3+ farms; Dark fighters as keep circles for reclaiming; stable pincer halves; allied escorts go home when the siege stalls; hunts take gardeners only at the hand-over; staggered, bounded reclaim passes; checked flank cells.
- New logs `OFFENCE_SUPPLY`, `OFFENCE_JOIN`, `OFFENCE_CALL`, `OFFENCE_FRONT`, `OFFENCE_HUNT`, `style=` on every campaign line. Inspect checks and nine new pins for the territory facts. Full offline workflow passed. Not verified in game; see TESTING.md r28.

## Round r27: computer offence against the Dark Tribe (2026-10-14, still 0.2.15)

- The user reported that the computer opponents never attack the Dark Tribe and asked to reverse-engineer the computer AI so it attacks properly: Mushroom Farms first, then the citadel, then win.
- Reverse engineering (file-only, with the game's own matching `S4_MainR.pdb` names): the native AI collects only towers and castles (types 46..48) as attack goals and needs the military group nibble 0xC on every target, so Dark buildings are never targets ("No attack"). No native unit can damage a Dark building. The native "farms first" rule is the Temple/Fortress fire defence, which weakens with every destroyed farm and is off without farms. Notes: `research/AI_OFFENCE_0215.md`.
- New offence director per computer sector AI (`native/offence-model.hpp`, `native/offence-native.hpp`, `OffenceOps`/`offenceTick` in `dark-tribe.cpp`): native attack timing and strength rules from the AI's own config, the AI's own free soldiers in a native state-6 squad, marches around fire zones, farms first, citadels only for total armies, and the native dissolve to give soldiers back. Saved mod squads are adopted again after a load; the plugin hands them back if it stops.
- Follow-up from the user: Dark buildings must fall to Gardeners greening the land around them. The army now escorts the AI's own Gardeners (type 50), which green the building's ring so it collapses natively (credited); the plugin's destroy call is only a fallback for an all-green ring. Idle Gardeners reclaim Dark land near their base. The maps give each computer player authored Gardeners (6/8/4 per people), and the AI recruits more natively (carrier + shovel, one at a time). A fourth reverse-engineering round (`work/native-analysis/ai/r4-*.md`) established the native recruitment path, the collapse rule (ring sizes 24/64/66/66) and that the AI never touches Gardeners.
- Hardening: destroyed buildings no longer count as standing for "Return to base", Temple queues and the panel.
- New `native/offence-tests.cpp`, `tools/inspect-offence.py` and `tools/offence_pins.py` (startup return code 13). Full offline workflow passed. Not verified in game; see TESTING.md r27.

## Round r26: crash and freeze diagnostics (2026-10-14, still 0.2.15)

- The user asked why the game crashed (`Trace078.txt`), then asked for better crash logging. The plugin was not loaded in any of the three crashed sessions (ordinary map `3_1_0_1.map`). The WER dumps from 00:40 and 01:26 show `c0000374`, ntdll heap failure type 8 (double free), while the game closes the map's Lua state in `CGameRun::Exit()` (`lua_close`, RVA 0x2797D0). The 01:55 stop left no crash record: its trace ends in the autosave's Lua save step, most likely a hang (unconfirmed). See `research/CRASH_DIAGNOSTICS_0215.md`.
- New heap-free diagnostics in the plugin (`native/diagnostics.hpp`):
  - fatal exceptions and ntdll heap failure records, each with a stack;
  - stacks for first-chance faults;
  - a 10 s main-thread freeze watchdog with snapshot dumps in `evidence/`;
  - a normal-exit line.
- New workflow step `tools/test-diagnostics.py` (disposable non-game fixture) and the offline dump reader `tools/read-crash-dump.py`.
- Unit tests, the fixture run and a compile check passed offline. Not verified in game; see TESTING.md r26.

## Round r25: The Alliance scenario, 512 maps, four players (2026-10-14, still 0.2.15)

- The user asked to make `dark07` playable. `tools/build-alliance.cjs` turns it into a four-player scenario: the Dark Tribe (one Temple, 109 units, no farms) as human player 1 against allied computer Romans, Vikings and Mayas with reinforced towers and field armies.
- The plugin no longer assumes a 576 map: `native/map-size.hpp` reads the engine's map stride (pinned) for every map array; the scene fingerprint covers any player count (`native/scenario-model.hpp`, mirrored by `tools/scenarios.py`).
- Offline workflow passed (24 steps). Not verified in game; see TESTING.md r25.
- First test: The Alliance crashed while loading because the launcher helper waited for the map size (only known once the map loads) and assigned the human slot too late. The helper no longer uses the map size.

## Round r24: Iron Kingdom scenario (2026-10-14, still 0.2.15)

- The user asked for a second playable Dark Tribe map with a serious challenge. A survey of all game maps chose the Trojan campaign map `AO_trojan10`: a complete computer Viking kingdom against two Dark Tribe camps on one 576x576 land mass.
- `tools/build-iron-kingdom.cjs` makes it a two-player scenario (merged Dark camps as human player 1, Vikings player 2 with manned towers and a field army, Trojans removed); `tools/test-iron-kingdom.cjs` checks it against the source. Lua `scenario/DarkTribe_IronKingdom.txt`.
- The plugin fingerprint, launcher helper, setup, installer (upgrade adds the new map to existing installations) and checks take the scenario list from `native/scenario-model.hpp` / `tools/scenarios.py`.
- Offline workflow passed (22 steps). Not verified in game; see TESTING.md r24.

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
