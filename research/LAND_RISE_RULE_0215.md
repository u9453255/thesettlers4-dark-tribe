# The land rise rule: what it is and why it never fires (0.2.15, after r51)

Date: 2026-10-21. Status: analysis and proposal only; nothing in this note is built. Sources: `evidence/v02-runtime.log` (eight sessions from build r35 to r51: pids 17196, 10832, 22292, 6384, 31460, 18584, 7008 and 5768) and `native/offence-model.hpp` / `native/offence-native.hpp` as of r51. Every figure below can be recomputed from the `OFFENCE_SUPPLY` and `OFFENCE_DEFEND` lines of that log.

## In short

- A computer measures how fast it is losing land to the Dark Tribe with one number, the **rise**: how many more Dark cells its territory holds than one map scan earlier. One scan of the whole map takes 32 to 36 game ticks.
- r50 added a rule: a rise of **40 cells or more in one scan** lets a computer defend its land at once, instead of waiting behind its attacks for up to 12,000 ticks (`start: serious rise`).
- It never fired in your r51 games, and it cannot fire while Dark land spreads normally. The fastest land loss in all eight sessions, 2,865 cells in 3,000 ticks, averages 34 cells per scan, and no defence decision of the r51 games saw more than 19. Only a sudden jump passes 40, and the logs show four such jumps in eight sessions.
- Proposal: measure the gain over a window (3,000 ticks suggested) instead of one scan, and defend at once when it passes a threshold (250 cells suggested). In these sessions that would have started a defence in 8 of the 48 measurable deferrals, each time while land was being lost fast. Old Dark land that is not growing would still wait behind the attacks.

## How the computer measures land loss today

**Own Dark cells.** Every tick the plugin scans 16 rows of the map (`scanTerritory`) and counts, for each computer, the walkable Dark cells inside its territory. A full pass takes 36 ticks on a 576 map and 32 on a 512 map. `own` is the count after the last full pass.

**Rise.** `rise` = `own` after the last full pass minus `own` after the pass before. Gardeners on both sides change the land all the time, so the value moves a few cells up and down, and it is 0 whenever nothing changed during the pass.

`own` grows in two ways: Dark Gardeners turn the computer's land Dark one cell at a time (Dark land spreads into computer territory, 0x175199), or the computer's border moves over land that is already Dark (territory claims never skip Dark cells, 0x20CD30), for example when a new tower is finished. Only the second can add hundreds of cells in one pass.

**Where the rise is used** (`offence-model.hpp`):

| Rule | Condition | Effect |
|---|---|---|
| Serious threat (r33) | reclaimable own Dark land and (`own` ≥ 150 or `rise` ≥ 40); or a Dark fighter within 40 of home; or 3 or more Dark Gardeners near the territory | the threat is answered sooner (no attack needed in between, a shorter pause after a defence) and, against Dark units, with half the army instead of a quarter |
| Fast rise (r50) | a threat of land only (no Dark units) that is serious with `rise` ≥ 40 | the defence starts now instead of waiting behind the attacks (`start: serious rise`) |
| Land aim (r50) | `rise` > 0 or `own` ≥ 150, no Dark fighter within 40 of home, and the group of Dark units the defence would go to has fewer than 3 and stands more than 60 tiles from the land | the defence goes to the land being lost instead of to those few far units |
| Home guard (r50) | `rise` > 0 or `own` ≥ 150, and own Dark land reclaimable within 130 of home | an attack takes 4 gardeners fewer than its style wants (at least 2) and leaves 4 free ones at home when it can |

**The wait that the fast-rise rule skips.** While the computer could attack, a threat of land only is deferred until 12,000 ticks (`LandDefendGap`) after its first attack time or after the start of its last land defence (`deferred: attacks first`). The wait exists because the computers of The Alliance own Dark land from the map start (player 2 about 1,850 cells): before r37 every decision began a land defence, and only 3 attacks came in 112,000 ticks. You asked for relentless attacks (r37), so old Dark land alone must not keep the armies at home.

## What the logs show

Per session and computer (Iron Kingdom and The Alliance): own Dark at its peak, the biggest one-scan rise in the logged defence decisions, the biggest gain over 3,000 ticks (`OFFENCE_SUPPLY` samples own Dark every 3,000 ticks), and how many 3,000-tick windows gained 200 cells or more.

| Session (pid, build, map) | Computer | Own Dark at the peak | Biggest one-scan rise logged | Biggest gain in 3,000 ticks | Windows with a gain ≥ 200 |
|---|---|---|---|---|---|
| 7008, r51, Iron Kingdom | 2 | 8,008 | 19 | 2,865 | 19 of 93 |
| 5768, r51, The Alliance | 2 | 1,587 | 0 | 845 | 3 of 27 |
| 5768, r51, The Alliance | 3 | 7,675 | 0 | 1,771 | 9 of 27 |
| 5768, r51, The Alliance | 4 | 5,438 | 16 | 964 | 9 of 27 |
| 18584, r49, Iron Kingdom | 2 | 5,562 | 11 | 1,126 | 15 of 54 |
| 31460, r48, Iron Kingdom | 2 | 3,952 | 396 | 996 | 8 of 50 |
| 22292, r42, The Alliance | 3 | 3,380 | 13 | 1,315 | 8 of 33 |
| 17196, r35, The Alliance | 3 | 4,007 | 1,326 | 1,326 | 9 of 36 |

- Computers lost hundreds or thousands of cells within 3,000 ticks again and again, yet the one-scan rise stayed small. To reach 40 in a scan by steady spread, own Dark would have to grow by about 3,300 cells in 3,000 ticks on a 576 map (3,750 on a 512 map), more than in any window of these sessions.
- In the two r51 sessions no defence decision saw a rise of 40 or more (62 decisions in Iron Kingdom, 101 in The Alliance), so `start: serious rise` never appeared. Most saw 0 or less: the rise was above 0 in only 6 of the 62 and 13 of the 101.
- In all eight sessions only four logged defence lines showed 40 or more: 176, 194, 396 and 1,326. Each was a jump within one pass (own Dark 30 → 206, 6 → 200, about 1,830 → 2,225, and 1 → 1,327). The log does not say what caused them; a border moving over land that was already Dark is the likely cause, since gardeners cannot turn hundreds of cells in 36 ticks. One of them came at a deferral (pid 31460, tick 130,510, rise 396): the only logged deferral at which the r50 rule would have fired.
- `rise` > 0 in the land aim and the home guard is therefore rarely true, and those two rules run on `own` ≥ 150 in practice. The home guard was on for 18 of 23 attacks in Iron Kingdom and 13 of 14 in The Alliance (r51).

**The deferrals.** The logs hold 63 `deferred: attacks first` decisions. 48 of them come late enough to measure the land gain in the 3,000 ticks before them (own Dark at the decision minus own Dark 3,000 ticks earlier, read between the `OFFENCE_SUPPLY` samples):

| Land gain in the 3,000 ticks before the deferral | Deferrals | Gains |
|---|---|---|
| 0 or less (flat, or land won back) | 29 | |
| 1 to 99 | 4 | |
| 100 to 199 | 4 | 156, 163, 182, 195 |
| 200 to 299 | 3 | 241, 242, 246 |
| 300 to 399 | 2 | 358, 363 |
| 400 or more | 6 | 477, 508, 613, 669, 739, 986 |

So about one deferral in six (8 of 48 with a gain of 250 or more) came while the computer was losing land fast. Those are the cases the r50 rule was meant to answer and did not. The other 15 deferrals came before tick 6,000, most at tick 840 on maps that start with Dark land; nothing had risen there yet.

## Proposal (not built)

1. **Measure the gain over a window.** The bridge already has `own` after every full pass. Keep `own` of the last passes in a small ring (about 90 passes, 3,000 ticks; transient, nothing saved, no change to the save format) and compute `gain` = `own` now minus the oldest `own` in the ring. The ring starts empty on a new map or a load, and the gain is measured from the end of the last land defence when that is later, so neither start land nor a gain that a defence already answered counts again.
2. **Fast rise on the gain.** A threat of land only skips the wait when `gain` ≥ 250 cells (suggested). In these sessions a threshold of 200 would have started 11 of the 48 measurable deferrals, 250 or 300 eight, 400 six. Logged as `start: serious rise` with the gain.
3. **Use the gain for "rising" too.** The land aim and the home guard test `gain` > 0 (or ≥ 100) instead of `rise` > 0, so "rising" means land was really lost recently.
4. **Keep the r37 and r50 safeguards.** A land-only defence stays escort-sized (a quarter of the army), lasts 3,000 ticks, is extended by 1,500 at a time only while it wins land back, Dark units stand near it or it is still on its way (6,000 at most), and never recalls a running attack. Old Dark land that is not growing (The Alliance's start) still waits behind the attacks.

**What it would change in play.** When you push Dark land into a computer's territory fast, the computer sends an escort and its gardeners as soon as it has lost 250 cells within 3,000 ticks, instead of up to 12,000 ticks later. While you keep taking its land, it attacks a little less often, because the escort's soldiers are defending. When you are not taking land, nothing changes.

**Decisions for you.** The window (3,000 ticks suggested) and the threshold (250 cells suggested). A lower threshold makes the computers defend more and attack less; a higher one keeps them attacking while you take their land.

## Evidence boundary

All figures come from the runtime log. Own Dark is sampled every 3,000 ticks (`OFFENCE_SUPPLY`) and at each logged defence decision, so the window gains are interpolated between samples. The one-scan rise is known only at logged decisions: a decision is logged when its reason changes, then at most every 3,000 ticks, while the computer looks every 40 ticks. The cause of the four jumps is not logged. Nothing in this note is built or tested.
