# Human gameplay tests - Dark Tribe 0.2.15

All in-game testing belongs to you. Offline checks cover code, pinned files, disposable models, map/Lua fixtures and installer copies; they do not verify gameplay or crash resolution.

## Startup gate - verify before entering a map

1. Preserve old logs, `.sav` files and their exact matching `save-states` sidecars. Close The Settlers IV, then run `Launch Dark Tribe.cmd` in this repository.
2. Remain at the menu until **Dark Tribe 0.2.15 ready** appears (up to three minutes). If readiness fails, retain the launcher error, `evidence/session-helper.jsonl`, `evidence/v02-runtime.log` and `evidence/startup-status-0215.json`; close the game before retrying.
3. Confirm this launch logs `HD manager hash verified before save hooks`, `HD panel owned window initialized`, `Pinned HD renderer: independent cached panel enabled` and `Version 0.2.15 native callbacks installed` for the same PID. The previous 0.2.11 tick-0/address-zero crash was corrected in code; actual resolution requires this run. Failed 0.2.11 evidence remains archived.
4. Start a NEW `00_Dark_Tribe_First_Dawn` custom map, player 1 human Dark and player 2 computer Viking. Keep races unchanged. Use a client height of at least 600 pixels.
5. Observe at least two minutes before other tests: no crash or early victory/defeat. If an ending occurs, record its actual text/winner; sound alone is insufficient. Existing saves retain their old terrain, AI and bank and cannot validate the new scenario economy.

## r51 checks - test first (Ubisoft services job guard)

The game no longer dies when the built-in Ubisoft online-services component hits a broken background job. That crash came about 35 seconds after the game started, in about one session in twenty (3 of the 61 logged sessions that ran that long); it was not caused by Dark Tribe.

**built and staged with the full offline workflow on 2026-10-21; not verified in game.** Until the startup line ends with `Ubisoft services job guard r51`, you are still running r50 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `gardener supply and land defence r50; Ubisoft services job guard r51`, followed by `Ubisoft services job guard installed (RVA 0x46854C: ...)`.
2. Play as usual, including several fresh starts of the game (the crash came about 35 s after a start).
3. If `UBISERVICES_JOB_PARKED count=... class=...` appears in the log, the guard caught the crash: the game should simply go on. Send that line; it names the Ubisoft job behind it.
4. A crash with `deadbaad` in the log or in `Documents\TheSettlers4\Log\TraceNNN.txt` should not happen any more; if it does, send the trace and the log.

## r50 checks - test first (gardener supply and land defence)

Computers replace lost gardeners faster, their toolsmith drop never empties a pile or jams on a lying shovel, and they defend their own land better: a fast Dark spread is answered at once, a winning land defence keeps going, a defence goes to the land being lost rather than to a few far units, and attacks leave four gardeners at home while that land is under pressure. Attacks themselves are unchanged.

**built, model-tested and staged with the full offline workflow on 2026-10-20; not yet play-tested.** Until the startup line ends with `gardener supply and land defence r50`, you are still running r49 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `shovel priority drop r49; gardener supply and land defence r50`. The first telemetry row renames the old `evidence/v02-economy.csv` (it has one column less) to `v02-economy-<number>.csv`; that is expected.
2. Play Iron Kingdom (a new game) for a long session and push Dark land into the Viking territory, fast at least once (several Dark Gardeners or a new Dark building next to their land), and also kill some of their gardeners.
3. Gardeners: after losses, `OFFENCE_RECRUIT player=2 gardeners=... wanted=...` should climb back to `wanted` within about 5,000 ticks; `OFFENCE_RECRUIT_WAIT ... reason=no spare shovels` should be rare.
4. Toolsmith drops (only if the output ever fills): every `OFFENCE_TOOL_DROP ... lying=a->b` has `b` of 1 or more, and a full output with one shovel in it clears within about 500 ticks.
5. Land defence in the log:
   - `OFFENCE_DEFEND ... reason=start: serious rise` appears when you darken their land fast, even while they are attacking regularly.
   - `OFFENCE_DEFEND ... reason=extended: land won back` (or `units near the area`, `still on the way`) shows a defence going past 3,000 ticks; its final `defence time` line should show a lower `own=` than its start line.
   - `OFFENCE_DEFEND ... area=x,y landAt=x,y`: when area equals landAt the defence went to the land, not to your units.
   - `OFFENCE_LAUNCH ... reason=... with a home guard` while their land is under pressure, and `OFFENCE_RECLAIM ... reason=own territory` lines during those attacks.
6. Diagnostics to send back with the log (they decide the next round, no behaviour depends on them): `OFFENCE_DEFEND_FIRE` (your units the computer ignores because they stand near your Temple's fire), `OFFENCE_DEFEND ... beyondFire=1`, `OFFENCE_GARDENER_FAR` (their gardeners far from home) and `far=` in the recruit lines.
7. After the session `python tools/economy-report.py --last`: the summary prints `room ...`; no "reserve below keep" flag while builders, diggers or gardeners are missing; gardeners above 0 late in the game. Attacks should still come as often as on r49 (about 16 per 166,000 ticks on Iron Kingdom).
8. The Alliance (optional): its computers own much Dark land from the start, so most of their attacks launch `with a home guard` (four gardeners fewer); they must still attack regularly and not sit in defences.

## r49 checks - test first (shovel priority drop)

When gardeners are missing and the computer's toolsmith output is full of other tools, one of the most numerous ones is thrown away so a shovel can be made.

**built, model-tested and staged with the full offline workflow on 2026-10-20; the user reports it working in game (2026-10-20).** Until the startup line ends with `shovel priority drop r49`, you are still running r48 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `tool orders until the output fills r48; shovel priority drop r49`.
2. Play as before. Gardeners must keep being replaced after losses; if the output ever fills, `OFFENCE_TOOL_DROP player=2 good=... lying=a->b ... drops=N` lines appear (one per 250 ticks at most) and shovels follow. No line is also fine: it means the output never filled.
3. After the session `python tools/economy-report.py --last`: no "toolsmith output full" flag, gardeners above 0 late in the game, mines staffed (no "mines without a worker" flag).

## r48 checks - test first (tool orders until the output fills)

The toolsmith watch no longer cuts a batch order short: the AI's pickaxes, hammers and axes get made as long as the output has room, and only a nearly full output with two or more of one tool lying there clears that tool's order.

**built, model-tested and staged with the full offline workflow on 2026-10-20; the user reports it working in game (2026-10-20).** Until the startup line ends with `tool orders until the output fills r48`, you are still running r47 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `specialist room r47; tool orders until the output fills r48`.
2. Play Iron Kingdom: the Viking iron mines must get workers early (iron ore and iron bars above 0 within the first few thousand ticks, swords appearing, soldiers growing again), and the toolsmith must still make shovels for replacement gardeners.
3. After the session `python tools/economy-report.py --last`: no "mines without a worker" or "iron chain dead" flag, no "toolsmith output full" flag, gardeners replaced after losses.

## r47 checks - test first (specialist room)

A computer that loses its builders, diggers or gardeners can recruit them again even when its carriers are down at the reserve: the reserve leaves room for the missing specialists and the barracks waits for them.

**built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested.** Until the startup line ends with `specialist room r47`, you are still running r46 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `toolsmith output watch r46; specialist room r47`.
2. Play Iron Kingdom and hurt the Viking town once (kill workers or burn buildings): afterwards builders and diggers must come back (`CARRIER_RESERVE player=2 ... room=N builders=... diggers=...` lines with room above 0, then room 0 again), open sites must get finished and houses must appear again.
3. After the session `python tools/economy-report.py --last`: no "sites without a builder" flag for long stretches, builders above 0 at the end, gardeners replaced after losses, carriers still mostly above keep (no drain to 5).

## r46 checks - test first (toolsmith output watch)

The computer's toolsmith no longer clogs its output with tools nobody takes, so it can make the shovels the plugin orders for new gardeners.

**built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested.** Until the startup line ends with `toolsmith output watch r46`, you are still running r45 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `economy-only keep and Iron Kingdom stone r45; toolsmith output watch r46`.
2. Play Iron Kingdom (a new game) long enough for the Viking gardeners to die a few times (several attacks). Replacement gardeners must keep coming: `OFFENCE_RECRUIT player=2 gardeners=...` lines continue late in the game and `OFFENCE_RECRUIT_WAIT ... no spare shovels` must not last for more than a few thousand ticks at a time.
3. In the log, `OFFENCE_TOOL_SURPLUS player=2 good=... lying=... order=N->0` shows the watch clearing the AI's surplus tool orders, and `OFFENCE_TOOLSMITH ... sum=X/8` must stay below 8 (`clogged=0`).
4. After the session `python tools/economy-report.py --last`: the toolsmith line must show output below 8 and worker 1, and no "toolsmith output full" flag. If the output is still full or the worker is 0, send the log: the diagnostics then say which gate blocks.

## r45 checks - test first (economy-only keep, Iron Kingdom stone)

The computer's carrier pool now counts only real economy buildings (Iron Kingdom: keep 34 instead of 58), and the Iron Kingdom Vikings start with a stone mine on their stone mountain plus 16 stone, so they can keep building houses.

**built, map-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested. Start a new Iron Kingdom game for the stone mine.** Until the startup line ends with `Iron Kingdom stone r45`, you are still running r44 or older.

1. Start with `Launch Dark Tribe.cmd`. The startup line in `evidence/v02-runtime.log` ends with `economy telemetry r44; economy-only keep and Iron Kingdom stone r45`. The installer upgrade replaces the Iron Kingdom map; start a **new** Iron Kingdom game (a save keeps the old map).
2. With Alt+9 look at the Viking town: a stone mine stands on the mountain west of the coal mines (around 448,428) and two stone piles lie by the castle area. During the game the mine should get a worker and stone should flow to building sites; new houses should appear over time.
3. After the session run `python tools/economy-report.py --last`: keep should read about 34 and rise slowly with the kingdom, carriers should stay above keep, the stone column above 0, and the population should grow instead of falling. Gardeners and attacks must come back after losses.
4. Report if the Vikings still build no houses (then the AI does not staff the mine or stone is not the limit; send the log) or if attacks stop again.

## r44 checks - test first (economy telemetry)

Every computer writes one row per 500 ticks to `evidence/v02-economy.csv` (population, carriers busy/idle/reserve/keep, housing, buildings and sites by type, soldiers, goods). `python tools/economy-report.py` summarises it and flags drains, stalled construction, starved smiths and flat population.

**built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested.** Until the startup line ends with `economy telemetry r44`, you are still running r43 or older.

1. Start with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `scaling carrier reserve r43; economy telemetry r44`, and a line `Computer economy telemetry every 500 ticks to evidence/v02-economy.csv` follows.
2. Play any scenario for a while. `evidence/v02-economy.csv` must exist and grow (one header line, then rows), and the game must not stutter every 500 ticks.
3. After the session run `python tools/economy-report.py` (or tell me to read it): it lists every computer of every map with population, carriers, soldiers, buildings and the flagged issues. The r43 check is in the same output: `reserve` must not be below `keep`, and carriers must not drain to 5.

## r43 checks - test first (scaling carrier reserve)

A computer keeps a pool of free carriers that grows with its town (15 plus one per two completed buildings, at most 120); its barracks turn only carriers above that pool into soldiers, so new soldiers need new houses.

**built, model-tested and staged with the full offline workflow on 2026-10-20 (all steps passed); not yet play-tested.** Until the startup line ends with `scaling carrier reserve r43`, you are still running r42 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `map reveal toggle r42; scaling carrier reserve r43`, and a line `Computer carrier reserve scaled with the kingdom (RVA 0x2567B8 -> 0x51C10)` follows. If the log says `Pinned carrier reserve signature mismatch; plugin disabled`, stop and send the log.
2. Play Iron Kingdom (where the drain showed) for a long round. With Alt+9, look at the computer towns now and then: carriers must keep carrying goods and building sites must still get finished late in the game; the armies should still grow, but only as houses add people.
3. In the log, `OFFENCE_SUPPLY player=N ... carriers= ... reserve= keep=` every 3,000 ticks: `reserve` must never be below `keep`, and `carriers` should stay near or above `reserve` instead of falling to 5. `CARRIER_RESERVE player=N native= keep= buildings= set= held=1` lines show when the floor holds the AI's own value up.
4. Tell me if the computers now attack too rarely (the pool is too big: ReservePer goes up) or still run dry (the pool is ignored: send the log).

## r42 checks - test first (map reveal toggle)

Alt+9 switches a view of the whole map on and off: every cell, building and unit is shown while it is on.

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `map reveal toggle r42`. Not verified in game.** Until the startup line ends with `map reveal toggle r42`, you are still running r41 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `no Manacopter healing r41; map reveal toggle r42`.
2. In a running map press Alt+9: within a second the whole map should be visible, including enemy units and buildings; the log shows `MAP_REVEAL on (Alt+9)`. Press Alt+9 again: `MAP_REVEAL off`, and the fog returns within a few seconds to what your units see (never-explored areas look explored rather than black until you reload).
3. The key must do nothing in the menus or while another window is in front. Report any flicker of fog while the reveal is on, and whether the minimap shows the revealed map too.
4. Save while the reveal is on, switch it off, reload: the save must show the normal fog, not the revealed map.

## r41 checks - test first (no Manacopter healing)

A landed Manacopter no longer heals itself: its health only changes through damage (and a new aircraft starts full).

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `no Manacopter healing r41`. Not verified in game.** Until the startup line ends with `no Manacopter healing r41`, you are still running r40 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `Iron Kingdom toolsmith hammers r40; no Manacopter healing r41`, and a line `Manacopter healing on Dark land disabled (RVA 0x16B594)` is there. If the log says `Pinned Manacopter healing signature mismatch; plugin disabled`, stop and send the log.
2. Land a damaged Manacopter on your Dark land and watch its health bar in the aircraft panel for a minute: it must stay where it is.
3. Let enemy bowmen shoot a landed Manacopter: its health must go down and stay down, and it must be destroyable.
4. Flying, landing, loading and unloading are otherwise unchanged.

## r40 checks - test first (Iron Kingdom toolsmith hammers)

Map change only: Iron Kingdom's Viking toolsmith now has two hammers beside its entrance (like the other maps), so the computer can replace a lost smith and keep making shovels for its Gardeners.

**Staged on 2026-10-20 (with r41): the full `python tools/workflow.py` passed, `test-iron-kingdom.cjs` checks the hammer pile and the staged DLL contains `Iron Kingdom toolsmith hammers r40`. Not verified in game.** The installer upgrade replaces the installed Iron Kingdom map; a NEW Iron Kingdom game is needed, old saves keep the old start content.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `Dark Gardeners leave the Manacopter r39; Iron Kingdom toolsmith hammers r40`.
2. Start a NEW Iron Kingdom game. Next to the Viking toolsmith at the south-east (523,463) there is now a small hammer pile with the iron bar and coal piles.
3. Play long enough for the Vikings to lose Gardeners. `OFFENCE_SUPPLY ... shovels=` should rise above 2 again now and then and `OFFENCE_RECRUIT ... ordered=1` should follow; report a game in which `OFFENCE_TOOLS ... restores=` keeps climbing by hundreds per 1,000 ticks for a long time (the toolsmith has no smith) or `no spare shovels` lasts tens of thousands of ticks again.

## r39 checks - test first (Dark Gardeners leave the Manacopter)

Dark Gardeners dropped from a Manacopter now really step out: they get a ground cell like a soldier does, can be clicked normally, and enemies can attack them.

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `Dark Gardeners leave the Manacopter r39`. Not verified in game.** Until the startup line ends with `Dark Gardeners leave the Manacopter r39`, you are still running r38.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `walked progress and steady sieges r38; Dark Gardeners leave the Manacopter r39`.
2. Load Dark Gardeners into a Manacopter (click them onto it or use Load nearby), fly, land and Unload. The Gardeners should appear next to the Manacopter, be selectable with a single click like any unit, show their bracket, and start darkening land around the drop point.
3. Drop them near enemy soldiers: the soldiers must attack them, and the computer's defence should react (`OFFENCE_DEFEND ... units=`). Report Gardeners that still cannot be clicked or hit.
4. Unload Warriors, Throwers and a Shaman as well: unchanged behaviour. The log shows `PASSENGER_PLACED craft=... gardeners=N unplaced=0` for every unload with Gardeners; send the log if `unplaced` is ever above 0.

## r38 checks - test first (walked progress and steady sieges)

Fixes the r37 behaviour you saw on First Dawn: armies turned back in the middle of a march because their straight-line distance to the goal grew while they walked around the lake. Progress is now measured along the real walking route, and a siege is no longer given up because one or two soldiers stepped over the fire line.

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `walked progress and steady sieges r38`. Not verified in game.** Until the startup line ends with `walked progress and steady sieges r38`, you are still running r37.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `relentless and smart attacks r37; walked progress and steady sieges r38`.
2. First Dawn, NEW game: the Viking army should walk the whole way around the lake to your farms without turning back. `OFFENCE_AVOID ... reason=target not reached` should no longer appear on a march that is still moving.
3. At a farm near your Temple or Fortress, a big army should keep its siege while single soldiers get pushed back out of the fire line (`OFFENCE_FIRE ... units=1`); only a real pull of several soldiers at once still counts (`lured toward the fire defence`).
4. When the computer attacks nothing for a long time, the log now has `OFFENCE_TARGETS` lines listing every Dark building with `reach=`, `path=`, `locked=` and `region=`; send them if that happens.

## r37 checks - test first (relentless and smart attacks)

The computers now attack far more often and pick their targets by the real walking distance from where their army stands, chaining through groups of farms. Taking back their own land no longer stops them attacking: a small escort does it between attacks. Soldiers no longer give up a far target after a fixed time and walk home; they take the next farm from where they are.

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `relentless and smart attacks r37`. Not verified in game.** Until the startup line ends with `relentless and smart attacks r37`, you are still running r36 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `fire pull and route diagnostics r36; relentless and smart attacks r37`.
2. First Dawn, NEW game: the first attack should come within 2-4 minutes of game time and the next ones a few hundred ticks after each army is home (`OFFENCE_LAUNCH` lines in the log). Report roughly how often you are attacked compared with before.
3. Watch which farm falls next: after a farm collapses the army should go for the nearest farm it can walk to, through groups of farms, not across the map (`OFFENCE_STAGE`/`OFFENCE_RETARGET` lines show the door positions).
4. Iron Kingdom, NEW game: the same checks; also whether attacks keep coming once the computer's Gardeners have been far away for a while (`OFFENCE_RECLAIM ... reason=gardeners called home` when they are called back).
5. The Alliance, NEW game: the three computers should attack you from early on instead of only reclaiming their own Dark land. Reclaiming still happens between attacks with a few soldiers (`OFFENCE_DEFEND ... reason=start: serious intrusion` with a small `army=`; `deferred: attacks first` while an attack is possible).
6. Report armies that stand around doing nothing for minutes, attacks that repeatedly fail (`OFFENCE_AVOID ... reason=target not reached` or `orders not executed` on the same farm), and any computer whose Gardeners never come back.

## r36 checks - test first (fire pull and route diagnostics)

No gameplay change: this round only adds log lines to find out why computer soldiers and Gardeners keep turning back at the edge of the Temple/Fortress fire and why Gardeners get lost there.

**Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `fire pull and route diagnostics r36`. Not verified in game.** Until the startup line ends with `fire pull and route diagnostics r36`, you are still running r35.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `reclaimed land fog and fast army marches r35; fire pull and route diagnostics r36`.
2. Start a NEW First Dawn game (or another scenario) and keep at least one Mushroom Farm, so the Temple/Fortress fire stays on. Let the computers attack farms near your Temple or Fortress and walk home past it; a Dark Gardener in their land makes them defend and walk past it too.
3. When you see soldiers or Gardeners turning back at the fire, or Gardeners leaving their army, note the rough game time. Play on for a while afterwards so the log has the whole episode.
4. Send `evidence/v02-runtime.log`. It now has `OFFENCE_FIRE_PULL` lines (each unit turned back, where it stood and which leg it was walking) and `OFFENCE_ROUTE` lines (each planned route and each replan that came out the same).

## r35 checks - test first (reclaimed land fog and fast army marches)

Land you darkened and a computer turned green again now goes back to the normal fog. Computer armies march at full walking speed; their Gardeners follow behind instead of holding the army back. This replaces the r32/r33 convoy behaviour described in the r33 checks below (the soldiers no longer wait on the way or walk back for slow Gardeners).

**Staged.** Staged on 2026-10-20: the full `python tools/workflow.py` passed and the staged DLL contains `reclaimed land fog and fast army marches r35`. Until the startup line ends with `reclaimed land fog and fast army marches r35`, you are still running r34 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `site selection crash fix r34; reclaimed land fog and fast army marches r35`. If the log says `Pinned fog merge signature mismatch; plugin disabled`, stop and send the log.
2. Fog: start a NEW First Dawn game. Darken land near or inside the Vikings' territory and let their Gardeners turn it green again (or watch a defence reclaim it). Once a patch is green and none of your units stands near it, it should dim to the normal explored fog within a few seconds. Your own Dark land must stay fully visible, and green land next to your units stays visible as usual.
3. Fog in a save: load a save where reclaimed green land was still lit (from r34 or older). A few seconds after the load the lit green areas should dim. The log shows `DARK_SIGHT fog re-checked map=... dark=... handedBack=...` once per map and `DARK_SIGHT greened cells handed back to native fog=...` when land was reclaimed. Report any `DARK_SIGHT fog map unavailable` line.
4. Army speed: watch computer attacks and defences on the march. The soldiers should walk to their target in one go at normal walking speed, without stop-and-go, with the Gardeners following behind them.
5. Near the target the army may wait for its Gardeners for up to about 300 ticks (about 20 seconds at normal speed): `OFFENCE_CONVOY ... reason=waiting for gardeners`, then `assault before the gardeners` if they are still late. There should be no long series of `waiting for gardeners` lines any more, and no `army walks back to the gardeners` or `gardeners do not follow`.
6. Late Gardeners should still reach the farm, wait behind the soldiers and green it; farms should fall as before. Report Gardeners that never arrive, or that Dark fighters often kill on the way.
7. Big armies (100+ soldiers, late in the game): report if one still hardly moves, with `OFFENCE_ORDER_IGNORED ... reason=order not executed` repeating for the same targets.

## r34 checks - test first (site selection crash fix)

Clicking a Dark building site right after placing it crashed the game (r33, 26 s into First Dawn). The native site panel now gets a safe view of any site that has no ordinary economy at its entrance.

**Staged.** Staged on 2026-10-19: the full `python tools/workflow.py` passed and the staged DLL contains `site selection crash fix r34`. Until the startup line ends with `site selection crash fix r34`, you are still running r33 or older.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `computer defence and reclaim priority r33; site selection crash fix r34`. If the log says `Pinned site panel entry mismatch; plugin disabled`, stop and send the log.
2. Start a NEW First Dawn game. Place a Mushroom Farm and click the new site immediately, as fast as you can. Repeat 5-10 times, also with several farms placed quickly in a row and each clicked at once. No crash.
3. Click sites that are refused: place a Farm with too little mana or inside the citadel fire field, or a second Temple/Fortress, and click the outline right away. No crash; the site disappears as before.
4. Place a Temple, a Fortress and a Manacopter Hall and click each site immediately and again while it grows. No crash; each site still grows and completes.
5. Let a Farm complete and check that its Mushroom Farmer still arrives and the farm produces as before.
6. Save while a site grows, load, and click the site at once after the load. No crash.
7. `SITE_PANEL_GUARD views=... skips=...` lines may appear when such a site panel was shown; send them with any crash lines (`FIRST_CHANCE`).

## r33 checks - test first (computer defence and reclaim priority)

The computers now defend their land. When your Dark Gardeners or fighters spread Dark land into a computer's territory, it sends soldiers together with its own Gardeners to fight your units there and turn the land green again, before it goes on attacking. Gardeners that travel with soldiers are now parked next to them and no longer wander off to green land on their own.

**Staged.** Staged on 2026-10-19: the full `python tools/workflow.py` passed and the staged DLL contains `computer defence and reclaim priority r33`. Until the startup line ends with `computer defence and reclaim priority r33`, you are still running r32.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `scaled styles and protected convoy r32; computer defence and reclaim priority r33`. If it says `Pinned computer offence signatures mismatch; plugin disabled`, stop and send the log.
2. Start a NEW First Dawn game. Send Dark Gardeners (and some Warriors/Throwers) into the Vikings' territory and darken land there. Within a few hundred ticks soldiers should march there with Gardeners (`OFFENCE_DEFEND ... reason=start: intrusion` or `start: serious intrusion`), stand next to your units and attack them, while their Gardeners green the land nearby. Report whether the soldiers really fight your Dark Gardeners, and whether they now stand idle anywhere near your Dark land.
3. In the log, `OFFENCE_DEFEND` shows `units=` (your units in or near their territory), `fighters=`, `own=` (Dark cells they own) and `rise=` (growth per scan). `phase=3` with `reason=land clean`, `defence time` or `threat too strong` is the end of a defence. `declined: ...` says why a defence did not start.
4. `OFFENCE_RECLAIM` lines should now appear in every game where a computer owns Dark land: `own territory` / `border` when Gardeners are sent, `none sent: ...` when none could go (sent at most every 3,000 ticks), and `parked gardener sent home`. Report if a computer owns many Dark cells (`OFFENCE_SUPPLY ownedDarkCells=`) for a long time without any defence or reclaim line.
5. Attacks still happen. Leave the computers' land alone for a while: raids, assaults and the other styles should still come (`OFFENCE_LAUNCH`). While you keep darkening their land, they defend first and attack less; report whether that feels too passive or too strong.
6. Watch a convoy (an attack or a defence on the march). Gardeners should stay right behind the soldiers, stand still when the soldiers stop and never walk off to green land on the way. `OFFENCE_CONVOY reason=army walks back to the gardeners` means the soldiers turned back for slow Gardeners; `gardeners do not follow` means the march was given up. No Gardener should be sent home alone.
7. At a farm under attack, the waiting Gardeners stand behind the soldiers and only start greening when the soldiers hold the position. None should stray far from the soldiers.
8. A farm the computer cannot reach (First Dawn: the Vikings behind the lake and the river) should not be attacked at all, or be dropped after one or two tries: `OFFENCE_AVOID ... reason=target not reached` or `orders not executed`, then attacks on other targets or a long pause. Report if the same farm is attacked again and again.
9. The r30 fire rules still apply: while you have farms, no soldier or Gardener of a defence or attack comes within about 27 tiles of your Temple/Fortress.
10. Save during a defence and load: no crash. The squad is adopted (`OFFENCE_ADOPT`) as an ordinary raid or assault (the defence itself is not saved); if your units are still in their land, a new `OFFENCE_DEFEND ... recall` or `start` line should follow.
11. Send all `OFFENCE_` lines of the session, including `OFFENCE_DEFEND`, `OFFENCE_RECLAIM`, `OFFENCE_CONVOY` and `OFFENCE_TOOLS`.

## r32 checks (scaled styles and protected convoy)

Every attack style now grows with the computer's army, and the soldiers march together with their Gardeners instead of running ahead. At the target the soldiers stand between your fighters and the Gardeners. The Gardeners wait behind them and start greening only once the soldiers hold the position.

**Staged.** Staged on 2026-10-19 together with r31: the full `python tools/workflow.py` passed and the staged DLL contains `clean start maps r31` and `scaled styles and protected convoy r32`. Not verified in game.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `clean start maps r31; scaled styles and protected convoy r32`. If it ends with `... r31` or earlier, r32 is not staged. If it says `Pinned computer offence signatures mismatch; plugin disabled`, stop and send the log.
2. Start a NEW game (The Alliance shows the most attacks) and watch the first armies march. Soldiers and Gardeners should travel as one group. No Gardener should trail far behind alone, and the soldiers should never stand at a farm while the Gardeners are still on the way. If the Gardeners fall behind, the soldiers should stop and wait for them.
3. In the log, `OFFENCE_CONVOY ... reason=waiting for gardeners` means the army is holding for its Gardeners. `gap=` is how far the farthest one is, and `limit=` is the distance at which the army holds. `reason=gardeners left behind` means some never caught up within 600 ticks and were sent home (`left=` says how many). `reason=rally to the gardeners` means an army walked back to its Gardeners after a hunt. Send these lines, and report whether armies now feel too slow.
4. At a farm, the soldiers should stand on the side your Warriors/Throwers come from, and the Gardeners a few tiles behind them. Attack the Gardeners with your fighters: the soldiers should be in the way. Report whether Gardeners still die unprotected, and where.
5. Kill some Gardeners during a siege. Replacements should only be Gardeners that were already close to the army (`OFFENCE_GARDENERS`). None should walk alone across the map to it.
6. Let a computer build up a big army (Iron Kingdom, or a long Alliance game) and compare its attacks with the early ones. Raids, hunts and sweeps should get bigger: with about 60 soldiers, a raid of about 20, a hunt of about 30, a sweep escort of about 15. Each brings a few more Gardeners. `OFFENCE_LAUNCH ... army= gardeners=` shows the sizes. `OFFENCE_RECRUIT ... wanted=` should grow above 12 (up to 20) once the computer has more than 50 soldiers. Report which styles you saw and roughly how big they were.
7. The r30 fire rules still apply. While you have farms, no soldier or Gardener of a convoy should come within about 27 tiles of your Temple/Fortress, also while waiting for its Gardeners.
8. Save during a march and load: the army continues with its Gardeners (`OFFENCE_ADOPT`). A big raid stays a raid. No crash.
9. Send all `OFFENCE_` lines of the session, including the `OFFENCE_CONVOY` lines.

## r31 checks (clean start maps)

The maps themselves no longer contain the start farms next to your Temple. In r30 they collapsed at the start (`FARM_FIRE_REMOVED`); now they are never there.

**Staged.** Staged on 2026-10-19 together with r31: the full `python tools/workflow.py` passed and the staged DLL contains `clean start maps r31` and `scaled styles and protected convoy r32`. Not verified in game.

1. Close the game and start it with `Launch Dark Tribe.cmd` (new DLL and maps). After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `citadel fire keep-off and farm blockade r30; clean start maps r31`. If it ends with `... r30`, r31 is not staged and the old maps are still installed.
2. Start a NEW First Dawn game. No farm stands just north of your Temple, and nothing collapses at the start. The other 7 start farms are there. The log for this game has no `FARM_FIRE_REMOVED` line.
3. Start a NEW Iron Kingdom game. No farm stands by the northern Temple, and nothing collapses at the start. The other 9 start farms are there, including the one south-east of your Manacopter Hall (about 26 tiles away): the Hall has no fire, so it was left on purpose. No `FARM_FIRE_REMOVED` line.
4. Start a NEW The Alliance game: unchanged, no start farms, no `FARM_FIRE_REMOVED` line.
5. Where the removed farms stood is plain Dark land: units walk across it, with no invisible obstacle. A Mushroom Farm there is still refused (it is inside the fire field).
6. Old saves keep their old map content. Load sweeps as in r30 step 10 (`reason=load`) still apply.
7. The r30 checks stay valid, except steps 2 and 3: on new games there is nothing to collapse any more.

## r30 checks (citadel fire keep-off and farm blockade)

Your Temple and Fortress burn enemy soldiers and Gardeners that stand on Dark land near them while you have Mushroom Farms. The kill zone reaches about 24 tiles (with 4 or more farms), farther than the flames you see, which always sit at about 16 tiles. In r27/r28 the computers lost Gardeners there (First Dawn: all 6 at your start farm next to the Temple; Iron Kingdom and The Alliance: whole sweeps at the edge). Now no computer unit comes within about 27 tiles of a Temple/Fortress while any farm stands, and the citadel is attacked only after the last farm has fallen. You can no longer build a Mushroom Farm inside that field, and start-map farms inside it are removed at the start.

**Staged.** Staged on 2026-10-19: the full `python tools/workflow.py` passed and the staged DLL contains `citadel fire keep-off and farm blockade r30`. Not verified in game.

1. Close the game and start it with `Launch Dark Tribe.cmd`. After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `citadel fire keep-off and farm blockade r30`. If it still ends with `toolsmith shovel orders r29`, r30 is not staged. If it says `Pinned computer offence signatures mismatch; plugin disabled` or `Pinned in-place construction signatures mismatch; plugin disabled`, stop and send the log.
2. Start a NEW First Dawn game. The start farm right next to your Temple (just north of it) collapses at the start; the other 7 start farms stay. The log shows one `FARM_FIRE_REMOVED 337,282 reason=start ... gone=1`.
3. Start a NEW Iron Kingdom game. The two start farms by the northern Temple collapse at the start; the other 9 stay. The log shows two lines: `FARM_FIRE_REMOVED 391,124 reason=start ...` and `FARM_FIRE_REMOVED 413,161 reason=start ...`. The Alliance has no start farms, so it shows none.
4. Try to place a Mushroom Farm close to your Temple or Fortress (within about 30 tiles). No green marker appears there. If you place one anyway, it is removed at once with *"Too close to the fire of your Temple or Fortress."*, and no mana is taken. A farm farther out (about 40 tiles or more) builds normally.
5. The other way round: try to place a Temple or Fortress close to your farms. It is refused with *"Too close to your Mushroom Farms. The Temple's fire needs room."* (or *"The Fortress's"*), and no mana is taken. A Manacopter Hall may stand anywhere, as before.
6. Play until attacks come, with farms both near the edge of the zone and far away. Watch the area around your Temple/Fortress: no computer soldier or Gardener should enter it while you have farms, not even when marching past, sweeping, hunting your fighters or walking home. Computer Gardeners should no longer die near your citadel. Report any computer unit that dies near your Temple/Fortress and roughly where.
7. In the log, `OFFENCE_FIRE ... reason=soldiers out of the fire defence` or `gardener out of the fire defence` means the plugin pulled units out (it should be rare). `OFFENCE_FIRE_STATE ... on=1 centres=...` shows when the plugin sees the fire on. `OFFENCE_FIRE_NATIVE` means the computer's own (non-attacking) soldiers stood in the zone; the plugin does not move those. Send these lines.
8. Lure test: stand some of your Warriors in the fire zone while an army is near. The army may chase them for a moment, but it should pull back (`OFFENCE_FIRE`) and, after three tries, leave that target (`OFFENCE_AVOID ... reason=lured toward the fire defence`).
9. Let the computers destroy all your farms (or demolish them). Only then should armies march on the Temple and the Fortress. If you build a new farm meanwhile, they should turn away from the citadel again (`fire defence back on`).
10. Load a save made before r30 that still has a farm close to the Temple: that farm collapses once after loading (`FARM_FIRE_REMOVED ... reason=load`). Load an r30 save: no such line.
11. The r29, r28 and r27 checks below stay valid. Where they say Gardeners keep "about 25 tiles" from your Temple, r30 makes it at least 28, and a computer with only farms inside the zone now declines with `farms only inside the fire defence`.
12. Send all `OFFENCE_` and `FARM_FIRE_REMOVED` lines of the session.

## r29 checks (computers make shovels for their Gardeners)

In r28 the computers ran out of shovels. Once the start shovels were used up, no new Gardener came and no Dark building fell (Iron Kingdom, pid 10104: `OFFENCE_RECRUIT_WAIT ... reason=no spare shovels` for about 70,000 ticks). Now, while Gardeners are missing and shovels are short, the plugin orders shovels at the computer's own toolsmith, as the toolsmith menu does. First Dawn and each Alliance people get a toolsmith next to their barracks, with iron bars, coal and two hammers beside it. Iron Kingdom's own toolsmith gets iron bars and coal.

**Staged.** Staged on 2026-10-19 after the game was closed: the full `python tools/workflow.py` passed (all offline steps), and the staged `runtime/native/DarkTribe-0.2.15.dll` contains `toolsmith shovel orders r29`. Not verified in game.

1. Close the game and start it with `Launch Dark Tribe.cmd` (new DLL and maps). After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` contains `toolsmith shovel orders r29` (since r30 it ends with the r30 marker); if it still ends with `attack styles, re-attacks and territory greening r28`, r29 is not staged. If it says `Pinned computer offence signatures mismatch; plugin disabled`, stop and send the log.
2. Start NEW games; old saves keep the old maps without the new toolsmiths and piles. In First Dawn and The Alliance, check that each computer has a toolsmith near its barracks with piles of iron bars, coal and hammers beside it, and that a smith walks in.
3. Let Gardeners die (kill them, or let the sieges use them up). When the shovels run short, the log shows `OFFENCE_RECRUIT_WAIT ... reason=no spare shovels` and then `OFFENCE_TOOLS player=.. ... order=0->4 ... reason=ordered`. Shovels should appear at the toolsmith, and new Gardeners should follow (`OFFENCE_RECRUIT ... ordered=1`, `gardeners=` climbing again).
4. `OFFENCE_SUPPLY` (every 3,000 ticks) now also shows `toolsmiths=`, `shovelOrder=`, `ironbars=` and `coal=`. Report whether iron bars or coal run out, and whether `shovelOrder=` stays above 0 for a long time while no shovel is made.
5. `OFFENCE_TOOLS ... reason=restored after a native clear` means the computer's own AI had cleared the shovel order and the plugin put it back; report the `restores=` count. `reason=no toolsmith` means the computer has no ready toolsmith in its home economy.
6. Over a long game (an hour or more), report whether attacks keep coming with Gardeners and whether Dark buildings still collapse late in the game.
7. Send all `OFFENCE_` lines of the session.

## r28 checks (aggressive computers, new armies, land taken back)

The computers now attack early and keep coming. They use six kinds of attack, replace lost armies from their own barracks, bring many more Gardeners, and take Dark land back, first inside their own territory. Once your farms are gone, they go for your Temple and your Fortress.

1. Close the game and start it with `Launch Dark Tribe.cmd` (new DLL and maps). After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `attack styles, re-attacks and territory greening r28`. If it says `Pinned computer offence signatures mismatch; plugin disabled`, stop and send the log.
2. Start NEW games; old saves keep the old maps without the new barracks, weapons and shovels. Test The Alliance and First Dawn first, then Iron Kingdom. At the start each computer has extra shovels and weapon piles near its barracks. The Alliance peoples and Iron Kingdom's Vikings have a new barracks next to their base.
3. Within the first minutes, computer Gardeners should start turning Dark ground green inside the computers' own territory and at the edge of your Dark land (`OFFENCE_RECLAIM ... reason=own territory` or `reason=border`). In The Alliance the computers own Dark ground from the start. Report whether your Dark area shrinks over time and roughly how fast.
4. The first attack should come about 5 game minutes in (The Alliance about 7). Report the game time and the `style=` of the first `OFFENCE_LAUNCH`/`OFFENCE_JOIN` lines.
5. Over a longer game (30+ minutes), report which attack styles you saw. The log names each one:
   - `raid`: about 10 soldiers and 4 Gardeners (since r32 more with a big army) go for a weakly defended building, take at most 3 and go home;
   - `assault`: most of the army and 8 Gardeners (since r32 up to 16 with a big army), farm after farm, then the Temple/Fortress;
   - `pincer`: the army splits and comes from two sides (`OFFENCE_STAGE` then `OFFENCE_ASSAULT`), then continues as an assault;
   - `sweep`: about 8 soldiers escort up to 10 Gardeners (since r32 both grow with a big army) that green a band of your Dark land and move forward step by step (`OFFENCE_FRONT`); a building near that front gets besieged;
   - `hunt`: up to 16 soldiers (since r32 half of a big army) go for a group of your Warriors/Throwers/Shamans and follows it (`OFFENCE_HUNT` when it ends), then attacks a building;
   - `allied` (The Alliance only): when one people starts an assault (`OFFENCE_CALL`), the others join the same target (`OFFENCE_JOIN`).
6. Destroy a whole attacking army. A new army should come again after a few minutes when the computer has soldiers (`OFFENCE_SUPPLY ... soldiers=` should grow while its barracks has weapons: `barracks=1` or more, `swords=`/`bows=` falling as soldiers are trained). Report how many attacks each computer made and whether they stopped at some point (send the last `OFFENCE_SUPPLY` and `OFFENCE_DECLINE` lines of that player).
7. Kill Gardeners: new ones should appear at the computer's base while it has shovels (`OFFENCE_RECRUIT ... ordered=1`). If none come, `OFFENCE_RECRUIT_WAIT ... reason=` says why (`no spare shovels`, `no spare carriers`).
8. Let them destroy all your farms (or demolish them yourself). The next armies must go for your Temple and your Fortress until both have collapsed; you lose when neither stands. Report whether that happens and how long the citadel holds with and without defenders.
9. While the computer besieges your Temple or Fortress, keep recruiting there. The last ground next to it may wait up to about a minute (`OFFENCE_DEFER ... reason=settlers inside`), then it falls anyway. Report whether any of your units vanish when it collapses.
10. Check that soldiers never stand still forever next to a target, Gardeners do not walk into your Temple's fire while you still have farms (they should keep about 25 tiles away; since r30 at least 28), and that the game stays smooth with three computers attacking (The Alliance).
11. Save during an attack (also during a sweep or a pincer), load: the attack continues (`OFFENCE_ADOPT`), no crash.
12. Send all `OFFENCE_` lines of the session from `evidence/v02-runtime.log`, the game times you noted, and whether the game felt too easy or too hard.

## r27 checks (computer attacks on the Dark Tribe)

The computer players now attack you with soldiers and Gardeners. The soldiers clear your defenders; the Gardeners turn the ground around a Dark building green, and the building collapses when its ring of ground is green. They go for your Mushroom Farms first, and only a full attack goes for your Temple or Fortress once no farm is left. While you have farms, your Temple/Fortress fire burns attackers that come close. Idle computer Gardeners also take back Dark land near their own base. All of this is offline work; nothing below has been seen in a game yet.

1. Close the game and start it with `Launch Dark Tribe.cmd` (new DLL and maps). After **Dark Tribe 0.2.15 ready**, the startup line in `evidence/v02-runtime.log` ends with `computer offence against the Dark Tribe r27`. If it says `Pinned computer offence signatures mismatch; plugin disabled`, stop and send the log.
2. Start a NEW game (old saves keep the old maps without Gardeners): Iron Kingdom has the strongest computer army; First Dawn and The Alliance also work. Each computer player starts with a few Gardeners next to its base (First Dawn 6, Iron Kingdom 8, The Alliance 4 per people).
3. After the attack delay (step 5), if your Dark land reaches within about 40 tiles of a computer base, its idle Gardeners should walk over and turn the nearest Dark land green (`OFFENCE_RECLAIM` in the log). They keep at least 35 tiles from your Temple/Fortress and from buildings you are constructing, but can collapse a Mushroom Farm or Hall in reach. Report whether that happens.
4. Build several Mushroom Farms, some near your Temple and some farther away. Keep part of your army away from the farms so the computer has a target it dares to attack.
5. Wait for the first attack. It cannot come before the scenario's attack delay (Normal: about 15 game minutes in First Dawn and Iron Kingdom, 20 in The Alliance), and usually comes a few minutes after that. Speeding the game up is fine. Report the game time of the first attack and who attacked.
6. During an attack, check and report:
   - soldiers and a few Gardeners march together towards a farm, mostly avoiding the area right around your Temple/Fortress while you have farms;
   - the soldiers fight your Warriors, Throwers and Shamans near the farm; the Gardeners wait until no Dark fighter stands within about 10 tiles;
   - the soldiers stand a few tiles away from the farm (not on it); the Gardeners then work around the farm, the ground turns green, and the farm collapses when its ring is green (it can take a minute or two);
   - after a farm falls they go for the next farm, and finally walk home.
7. Let the computer destroy all your farms (or demolish the rest yourself), keep few defenders at the Temple, and wait for a full attack: its Gardeners should green the ground around the Temple/Fortress until it collapses, and you lose once no Temple and no Fortress is left. A small raid (about 12 soldiers) must never go for your Temple or Fortress.
8. Kill the Gardeners of an attack: the army should take other Gardeners if the computer has some left, otherwise walk home. When a computer player has lost Gardeners and still has shovels, new Gardeners appear at its base over time (`OFFENCE_RECRUIT ... ordered=1`).
9. Defend once: kill most of an attacking army. The rest should walk home after it has lost more than 60 % of its soldiers.
10. Save during an attack, load that save: the attack continues (the log shows `OFFENCE_ADOPT`). Loading the save also must not crash.
11. If a Manacopter Hall with queued Manacopters is destroyed, the "Manacopter Hall lost. Mana refunded." notice appears (300 mana per queued Manacopter). A building you are still building that gets caught in green ground is lost and its mana refunded.
12. Send all `OFFENCE_` lines of the session from `evidence/v02-runtime.log`. If no attack ever comes, the `OFFENCE_DECLINE ... reason=` lines say why (for example `fewer than 15 fighters`, `no gardeners`, `raid too weak`, `no farm or Hall for a raid`, `raid into the fire defence`; since r30 `farms only inside the fire defence`). `OFFENCE_SIEGE`/`OFFENCE_GREENING ... ring=green/total` show the greening progress; `OFFENCE_RAZE` means a building with an all-green ring was destroyed by the fallback. `OFFENCE_ORDER_IGNORED`, `OFFENCE_ORDER_REFUSED`, `OFFENCE_RAISE_FAILED` or a repeated `target not reached` point to a problem.
13. Known limit: the computer players have few soldiers and (in these maps) little or no way to train more, so after one or two lost attacks they may stop attacking. Report how many attacks each computer player made.

## r26 checks (crash and freeze diagnostics)

1. Close the game and start it with `Launch Dark Tribe.cmd` (new DLL). After **Dark Tribe 0.2.15 ready**, `evidence/v02-runtime.log` for the new PID contains `Crash and freeze diagnostics ready: fatal exceptions with stacks, freeze watchdog, freeze dumps`, and the startup line ends with `crash and freeze diagnostics r26`. Report it if the line says `unavailable`.
2. Start any map: a Dark Tribe scenario or an ordinary map such as `3_1_0_1`. Within a few seconds the log shows `FREEZE_WATCH ready main thread <id> window found`. Report it if it says `window not found`.
3. Normal play must not produce `FREEZE` lines. Try these: pause for 30 s, keep the in-game menu open for 30 s, save, switch to another window for a minute, minimise and restore. If a `FREEZE` line appears and a `FREEZE_END` follows shortly after (for example while a map loads or when leaving a map), send both lines and say what you were doing.
4. Quit to Windows normally. The log for that PID ends with `PROCESS_EXIT process ending`.
5. Known crash: play `3_1_0_1.map` (started with `Launch Dark Tribe.cmd`) for at least 15 minutes, then quit to the main menu or restart the map. If the game crashes, the log should show:
   - `FATAL_EXCEPTION code=c0000374 (heap corruption)`;
   - `HEAP_FAILURE type=8 (double free (block not busy))`;
   - a `STACK source=heap-failure` line containing `S4_Main.exe+0x27fde8` and `S4_Main.exe+0x1bd257`.

   Send these lines.
6. Whenever the game crashes or freezes, note the time and what you did just before it. If it freezes, wait at least 15 seconds before closing it, so the watchdog can log the stack and write `evidence/freeze-<pid>-<n>.dmp`. Then send that PID's `FATAL_EXCEPTION`, `HEAP_FAILURE`, `FIRST_CHANCE`, `STACK`, `FREEZE*` and `PROCESS_EXIT` lines. Keep any `evidence/freeze-*.dmp` and `%LOCALAPPDATA%\CrashDumps\S4_Main.exe.*.dmp` files. They stay on your PC; `python tools/read-crash-dump.py --latest` reads them.
7. Games started directly from Ubisoft Connect or the game executable do not load the mod and get none of these lines.

## r25 checks - test first (The Alliance)

1. Close the game and start it again with `Launch Dark Tribe.cmd` (the launcher helper changed). After **Dark Tribe 0.2.15 ready**, Custom Maps lists `02_Dark_Tribe_The_Alliance`. Start a NEW game: player 1 human (Dark Tribe), players 2-4 computer (Romans, Vikings, Mayas); keep races unchanged.
2. The map opens at your Temple (between the Mayas in the centre and the Vikings in the north-east), the three intro lines appear, and you have 150 mana. Your army stands in the south-east. Dark menus, mana section, construction markers and Manacopter controls work as on the other maps (this is the first 512x512 map: report anything that behaves differently, e.g. markers in the wrong place, units refusing to move, construction never finishing).
3. Build Mushroom Farms on Dark land; a Fortress and Halls fit on the open Dark land in the south-east near your army (markers show where).
4. The Romans (south-west), Vikings (north-east) and Mayas (centre) should develop, never fight each other, and attack you. Report roughly when the first attack comes, from whom, and whether any of the three stays passive.
5. Victory needs every tower of all three peoples destroyed; losing every Temple and Fortress loses. Report any early or missing ending with its text.
6. If the map does not start or the Dark menus stay missing, send `evidence/session-helper.jsonl` and the `SCENE_STATE mask=` lines from `evidence/v02-runtime.log`.
7. First Dawn and Iron Kingdom still start and play as before (they are 576 maps; this round changed how the plugin reads the map size).

## r24 checks (Iron Kingdom)

1. After **Dark Tribe 0.2.15 ready**, open Single Player, Custom Maps: `01_Dark_Tribe_Iron_Kingdom` is listed next to `00_Dark_Tribe_First_Dawn`. Start a NEW game with player 1 human (Dark Tribe) and player 2 computer (Vikings); keep races unchanged.
2. The map opens at your north-eastern Temple, the three intro lines appear, and the Dark menus, mana section and construction work as in First Dawn. You control both Dark camps (north-east and north-west, plus a Temple in the south near the Vikings) and the Manacopter Hall in the far north-west. No Trojans exist anywhere.
3. Construction: Mushroom Farms fit on most Dark land; Temple, Fortress and Hall fit on the open Dark land between the two northern camps. The Temple tile is greyed ("Built") because you already have Temples; one Fortress can be built.
4. Recruit at either Temple; build a Manacopter at the Hall.
5. The Vikings: towers manned, soldiers near their castle area; within the first game hour they should attack (report roughly when and where). Report whether the difficulty feels right (too easy / fair / too hard) and anything the Vikings never do (build, recruit, attack).
6. Victory/defeat: destroying every Viking tower wins; losing every Temple and Fortress loses. Report any early or missing ending with its text.
7. If the map does not start or the Dark menus stay missing, send `evidence/session-helper.jsonl` and the `SCENE_STATE mask=` lines from `evidence/v02-runtime.log`.
8. First Dawn still starts and plays as before.

## r23 checks (markers on new Dark land)

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

1. After **Dark Tribe 0.2.15 ready**, confirm the log line `Pinned native rise, reservation and completion verified; reserved cells checked for occupancy only; tick read cache r3; ordinary-site rise, aircraft group and sight r4; site panel, passengers and terrain sight r5; native temporary aircraft vision r6; aircraft right-click r7; aircraft selection marker r8; targeted unload r9; load nearby r10; radius 24 r11; site eviction r12; eviction and HD panel diagnostics r13; wild animals do not block r14; native-style Dark UI and Manacopter health r15; native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23; Iron Kingdom scenario r24; The Alliance scenario and 512 maps r25` for this PID. If the line lacks `native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23; Iron Kingdom scenario r24; The Alliance scenario and 512 maps r25` (or `terrain sight r5; native temporary aircraft vision r6; aircraft right-click r7; aircraft selection marker r8; targeted unload r9; load nearby r10; radius 24 r11; site eviction r12; eviction and HD panel diagnostics r13; wild animals do not block r14; native-style Dark UI and Manacopter health r15; native game art menus r16; engine unit portraits r17; right-drag pan, text bounds and native recruitment cluster r18; player-facing texts only r19; mana section, aircraft panel and base limit r20; Hall panel like the aircraft panel r21; hidden outside a running map r22; placement scan diagnostics r23; Iron Kingdom scenario r24; The Alliance scenario and 512 maps r25`, an older build is still running. Start a NEW First Dawn, Iron Kingdom or The Alliance map. Old saves with paid 0.2.13 conversions refund those jobs once ("Old cancelled construction refunded") and never recreate them.
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
