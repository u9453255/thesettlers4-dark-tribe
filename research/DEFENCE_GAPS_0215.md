# Computer defence against Dark land: findings (0.2.15, 2026-10-20)

Status: analysis only. No code, test or doc was changed for this note. Follow-up: the accepted items (A, B, D, E diagnostics, F, G diagnostics) and the counter-proposal to C (a gardener home guard) were implemented as round r50 on 2026-10-20 (`R50_PLAN_0215.md`, `AI_OFFENCE_0215.md` Round r50); not verified in game.

The question was why the computer barely reacts when the Dark Tribe darkens its territory.

Sources:
- `evidence/v02-runtime.log` and `evidence/v02-economy.csv`.
- `python tools/economy-report.py`.
- Two Iron Kingdom games; the computer is player 2 (Vikings).

| Game | Build (startup marker) | Length | Dark push |
|---|---|---|---|
| pid 31460 | r48 | 155,063 ticks | 19 Dark Gardeners recruited at tick 118,538; map Dark cells 47,687 -> 60,160 by tick 154,095 |
| pid 18584 | r49 (shovel priority drop) | 166,530 ticks | map Dark cells 47,478 (tick 45,280) -> 81,047 (tick 164,180) |

"Own" below means Dark cells the computer owns (`own=` in `OFFENCE_DEFEND`).

## Summary

- **r48 game: no gardeners.** The computer had 0-1 gardeners from about tick 15,000 to the end, so no defence could green anything.
- **r49 fixed the gardener supply.** In pid 18584 the computer kept gardeners all game. Its defences took 4-20 gardeners each and won land back while they ran.
- **The defence rules from r37 are unchanged, and they still lose the race.**
  - A land defence ends after 3,000 ticks.
  - It waits behind attacks.
  - Dark Gardeners never call back a running campaign.
  - It aims at units before land.
  - The fire keep-offs hide or block the threat near the Temple.
  - Own Dark still grew from 189 (tick 40,830) to 4,974 (tick 142,350) in the r49 game.
- **One shovel jam remains possible after r49.** It was not seen in pid 18584.

## Game 1: pid 31460 (r48), why there was no reaction

### 1. The toolsmith output was full, so no shovels and no gardeners

- The toolsmith output held 8 of 8 from about tick 9,500 to the end (288 of 310 telemetry samples).
- At tick 18,260 (`OFFENCE_TOOLSMITH`) it held 1 hammer, 2 pickaxes, 3 fishing rods and 2 scythes. No Viking settler takes those tools.
- Inputs were full (iron bars 8, coal 8), stock held 22 bars and 60 coal, and the smith was present. Only the output gate (sub_14CDC0) stopped production.
- r48 clears orders only, so it could not remove tools already lying there. Meanwhile the AI's axe order rose 2 -> 34 and its saw order 1 -> 14; neither was ever served.
- Gardeners went 14 (tick 3,260) -> 0 by tick 24,500, then stayed at 0-1 until the end. The 28 start shovels were used up by tick 15,000.

### 2. The plugin's shovel reserve kept the one shovel that was made

- Around tick 46,000 the toolsmith made one shovel. It lay in the output (`31:1/0`, sum 8/8) for the rest of the game.
- Shovel stock stayed at 2, which equals `ShovelReserve`. So `recruitOrder` answered `no spare shovels` 26 times and never turned that shovel into a gardener.
- The reserve is meant for diggers, but the AI had 10 diggers all game, and its steady state is 5.

### 3. Land loss waits behind attacks

Defence decisions (`OFFENCE_DEFEND` reasons):

| Reason | Count |
|---|---|
| `deferred: attacks first` | 18 |
| `start: serious intrusion` | 8 |
| `defence time` | 7 |
| `declined: no gardeners` | 7 |
| `recall: serious intrusion` | 1 |
| `no way outside the fire defence` | 1 |
| `no gardeners left` | 1 |

- It was deferred even at tick 130,510, with a rise of 396 cells in one scan pass.
- Between defences the army ran 9 hunts toward the far Dark base.

### 4. The biggest defence chased units instead of land

- At tick 131,430 it sent 111 soldiers and 1 gardener to 2 Dark Gardeners at (389,240), 118 tiles north.
- The army waited 570 ticks at the staging cell for its gardener, which was 219 tiles behind.
- During those 6,000 ticks own Dark rose 2,207 -> 3,961.

## Game 2: pid 18584 (r49), what changed and what did not

### Fixed

- 7 `OFFENCE_TOOL_DROP` between ticks 15,020 and 28,770. After that the output stayed at 5-7 of 8.
- Shovel stock stayed at 3-4.
- Gardeners: 3-16 until tick 96,000, then 19-40.
- Defences: 11 started and 2 attacks were called back, each with 4-20 gardeners.

Defences now win land back while they run:

| Defence start | Gardeners | Own Dark, start -> end |
|---|---|---|
| 53,220 | 4 | 1,557 -> 1,361 |
| 67,060 | 12 | 2,027 -> 1,439 |
| 89,390 | 4 | 2,154 -> 1,967 |
| 105,570 | 16 | 3,567 -> 3,299 |

### Still wrong

The defence code in `native/offence-model.hpp` is untouched by r48/r49.

**A. A land-only defence ends after 3,000 ticks.**
- Code: `LandDefendTime` (`offence-model.hpp:891`), applied at `:1486`.
- 6 land-only defences ended on exactly that limit, including the 16-gardener one at 105,570, which was winning.
- After each one the sector went on an attack. Own Dark then grew again: 3,299 at 108,580 -> 4,636 at 121,950.

**B. "Attacks first" deferral.**
- Code: `LandDefendGap` 12,000 (`:891`, `:939-940`).
- It deferred 3 times in pid 18584 and 18 times in pid 31460.
- A fast rise counts as a serious threat but does not skip the gap.

**C. Dark Gardeners never call back a running campaign.**
- Code: `answer()` returns unless Dark fighters are seen (`:964`, r37).
- Campaigns are long, and the territory has no defence while one runs:

| Campaign | Ticks | Own Dark during it |
|---|---|---|
| raid | 93,940 -> 105,560 (11,620) | 1,967 -> 3,567 |
| sweep | 109,920 -> 121,940 (12,000, `campaign time`) | 3,253 -> 4,636 |

**D. The defence aims at units before land.**
- Code: `assess()` puts the defence area on the unit cluster whenever units are seen (`:903`). Reclaimable own land is used only when no units are seen.
- pid 31460: tick 131,430 (see Game 1, point 4).
- pid 18584: the recall at 58,350 went to (420,221), and own Dark rose 1,412 -> 2,135 during it. The defence at 71,420 moved to (366,367), and own Dark rose 1,396 -> 1,933.
- The land being lost elsewhere is not compared against the units.

**E. The fire keep-offs hide or block the threat near the Temple.**
- Partly hypothesis; the Temple's fire centre is at (368,423).
- `assess()` drops Dark units within `HuntKeep` 42 (Euclid) of a live fire centre (`:899-900`). Land within `FrontKeep` 37 is not reclaimable.
- In pid 31460 the land threat at tick 130,510 sat at (381,465), 37.2 from the fire centre. Dark Gardeners working there were most likely dropped. That made the threat land-only, so it was deferred or declined.
  - Not proven: the log records only the units left after this filter.
- Defences also stop when the next area lies past the fire with no leg around it (`no way outside the fire defence`):
  - pid 31460, tick 45,440: next area (377,384).
  - pid 18584, tick 123,860: 20 gardeners, area (368,477) -> (352,365) on the far side of the Temple; the defence stopped after 1,900 ticks.

**F. The shovel jam is still possible (not seen in pid 18584).**
- `dropCandidate` (`:1556-1558`) drops nothing while an unreserved shovel lies in the output.
- `recruitOrder` (`:1521`) uses no shovel while stock <= `ShovelReserve` 2 (`:138`), and that stock includes the shovel in the output.
- So this state blocks forever:
  - the output is full, with 7 other tools and 1 shovel;
  - at most 1 more shovel lies anywhere else;
  - no worker takes the other tools.
- This is pid 31460's state from tick 46,000 on. r49 stopped the junk from piling up earlier in pid 18584, so the state never arose there.

**G. Observation, not checked: gardeners far from home.**
- The economy CSV counts 40 gardeners at the end of pid 18584.
- The plugin's `OFFENCE_RECRUIT` counts 18-19 (`wanted=20`), because recruitment counts only gardeners within `LaunchReach` 130 of home.
- So about 20 gardeners were somewhere far away, and the shovels and carriers they cost were spent again at home.
- Before calling this a defect, check where they stood and why (dissolved sweeps ended at (316,385) and (261,350)).

## Suggested fixes (not implemented)

These keep the r37 reasons. The Alliance starts with thousands of computer-owned Dark cells and must not stop attacking. The user wants relentless attacks. So static old Dark land keeps the current rules, and only land being taken now is answered faster.

1. **Fast rise skips the gap.**
   - If `rise >= SeriousRise` in a scan pass, or Dark Gardeners are seen in the territory, skip `LandDefendGap`.
   - Old Dark land with no rise keeps the gap.
2. **Defence keeps going while it wins.**
   - A land-only defence that is still reducing own Dark, or still sees units, extends past `LandDefendTime` in steps.
   - It still ends at `DefendTime`, or when the land is clean.
3. **Call campaigns back for land.**
   - `SeriousGardeners` (3) Dark Gardeners in the territory, or a serious rise, call back raids, hunts and sweeps.
   - Sieges keep their current rule: two-thirds green goes first.
4. **Aim at the loss.**
   - When own Dark is rising and the unit cluster is far from the reclaimable land, defend the land.
   - Or split the defence: soldiers go to the units, gardeners go to the land.
5. **Shovel reserve.**
   - Apply `ShovelReserve` only while the AI has fewer than 5 diggers.
   - Or let `dropCandidate` ignore a shovel the recruitment refuses to use.
6. **Fire.**
   - Log the units dropped by `HuntKeep`, with count and nearest distance, to confirm E before changing the keeps.
   - For an area on the far side of the fire, try `route` around it before giving up.
7. **Gardeners far from home.**
   - Log the positions and jobs of gardeners outside `LaunchReach` before changing recruitment.

Each change needs coverage in `offence-tests.cpp`: the land-first test, a defence-length test and a recall test.

## Evidence boundary

- All figures are from the two games' logs and telemetry.
- Point E (units hidden near the Temple) is an inference; the log does not record the dropped units.
- Point F is a code-path argument. pid 31460 shows the state; it has not been reproduced on r49.
- None of the suggested fixes has been built or tested.

## Challenge (second agent, 2026-10-20)

Checked against `evidence/v02-runtime.log`, `evidence/v02-economy.csv` and `native/offence-model.hpp`; figures re-derived, not copied.

- Game 1 and Game 2 facts hold: pid 31460 output 8 of 8 in 288 of 310 samples (from tick 11,000), the lying shovel `31:1/0` with `no spare shovels` 24 times, 18 deferrals, 9 hunts, the 111-soldier defence against 2 units at (389,240); pid 18584: 11 defence starts, 3 deferrals, 2 recalls, the own-Dark figures of every defence, the `no way outside the fire defence` stop at 123,860 with 20 gardeners, own Dark 189 -> 5,002, map Dark 47,478 -> 81,047.
- **F is a real hole in r49**, not only in r48: `dropCandidate` refuses to drop while a free shovel lies in the output, but `recruitOrder` refuses that shovel while free shovels are at or below `ShovelReserve`, so one lying shovel in a full output jams both. pid 31460 ends in exactly that state and r49 would not have cleared it. Accepted.
- **A, B, D accepted** with the r37 guard kept (static old Dark land never blocks or recalls attacks; only land being taken now counts): extend a winning land-only defence in steps up to `DefendTime`; let a rise of `SeriousRise` or more skip `LandDefendGap`; aim a defence at the reclaimable land when the seen units are few and far from it.
- **C challenged.** Recalling raids, hunts and sweeps for Dark Gardeners or a rise re-creates the r38 complaint ("stops attack in the middle"): the Dark player's gardeners sit near the computer's land most of the time, so attacks would be recalled constantly. The land is lost during campaigns because the campaign holds every gardener (12 of 12 held while own Dark rose 1,967 -> 3,567). Counter-proposal: keep a home guard of gardeners out of campaigns while own Dark is rising (`gardenersFor` minus a guard of 4 when `rise > 0` or own above `SeriousCells`), so `reclaim` works during the campaign; no recall.
- **E partly accepted**: log the units dropped by `HuntKeep` (count, nearest distance) and the far-side areas; no keep change and no routing change until that log says the threat was really hidden.
- **G accepted as a diagnostic**: log gardeners outside `LaunchReach` (position, job, distance) every 3,000 ticks; 40 counted by the ledger against 18-19 by recruitment is a real gap to explain before changing recruitment.
- Added to the r50 list together with: shovel reserve 1 and batch 6, drops only from piles with two or more, and the reserve room in the telemetry.

