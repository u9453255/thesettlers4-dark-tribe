# Computer economy telemetry (0.2.15 round r44)

Date: 2026-10-20. User request after the r43 carrier-reserve work: "see what is happening in the computer economy, logging it all the time, so later on we can analyse it for issues".

Offline static analysis of the pinned `S4_Main.exe` (capstone, `work/native-analysis/ai/scripts/q.py`); no game started or attached. The engine facts below are pinned by `tools/inspect-offence.py`; gameplay is unverified until the user plays with the r44 build.

## What is written

Every `dtledger::EcoEvery` (500) ticks the plugin tick appends one row per active computer (a player with an AI that is an enemy of the Dark Tribe, as the offence tick sees it) to `evidence/v02-economy.csv`. The first line of a new file is the header. All processes append to the same file, so the `ms` (GetTickCount) and `pid` columns match the runtime log; a new map restarts the tick at 10, a load jumps forward.

Columns (`native/economy-telemetry.hpp`, `csvHeader`/`csvRow`; the `ledgerRows` test keeps them in step):

| Column | Source |
|---|---|
| `ms,pid,tick,player,eco` | GetTickCount, process id, AI tick, player 2..8, 1 when the home economy was found |
| `population` | Statistics 0xDABE30 + p*0x1128 + 0x110: living settlers of the player (0xF7D40 increments on creation, 0xF7DB0 decrements on removal) |
| `carriers,busy,idle,reserve` | home economy words +0x26 (total carriers), +0x22 (in flight), total − busy, +0x20 (the reserve the barracks respects) |
| `keep` | the r43 floor `carrierKeep(buildings)` last applied by the hook (`reserveSeen[p].keep`) |
| `housing` | 0xDF730(p): completed residences times their settlers (building info +0x30) plus the start settlers `[0x107C8A8 + 4p]` |
| `houses_s,houses_m,houses_b,buildings,barracks,toolsmiths,weaponsmiths,ironmines,coalmines,ironsmelters` | completed buildings per type from the table 0x10EF624 + (p*0x53 + type)*8 (incremented by 0x1362F0 when a building completes), types 40/41/42, the sum over all 83 types, 24, 21, 22, 15, 17, 20 |
| `sites` | entities of the per-player building lists [0x10EEA74 + (p*0x53 + type)*4] (next id +0x2A) that lack the ready flag 0x2000000, summed |
| `soldiers,leaders,builders,diggers,gardeners` | settler counts per type from Statistics + 4 + 4*type: 29..44 and 61..63, 44, 3, 2, 50 |
| 22 goods `log..water` | free stock of the home economy, 0xD4280(eco, good) for the S4ModApi ids in `dtledger::Goods` (LOG 22, BOARD 7, STONE 32, IRONORE 21, COAL 10, IRONBAR 20, SWORD 34, BOW 8, BATTLEAXE 5, BLOWGUN 6, BACKPACKCATAPULT 38, SHOVEL 31, HAMMER 18, AXE 4, PICKAXE 25, SAW 28, BREAD 9, FISH 11, MEAT 24, GRAIN 16, FLOUR 12, WATER 36) |
| `settlers,ready,sites_by_type` | `type:count` pairs separated by spaces for every type with a count above 0 (settlers 0..66, completed buildings 0..82, sites 0..82) |

Cost: per row about 83 short list walks (guard 512 each), 22 stock calls and a few hundred reads, once per 500 ticks per computer; the OFFENCE_SUPPLY census does more every 3,000 ticks. The file grows by about 1 KB per computer per 500 ticks (roughly 1 MB per hour of play at normal speed).

## Reading it

`python tools/economy-report.py [csv] [--pid N] [--player P] [--last]` groups the rows by process and map, prints per computer the population, carriers (min/median/last, idle), reserve and keep, soldiers, buildings and sites, the end stock, the buildings and settlers by name, and flags:

- carriers drained (idle < 5 for three samples or more), reserve below keep, soldiers appearing while carriers were below keep (the r43 floor not holding);
- construction stalled (sites open, no completion for 20,000 ticks), sites without a builder, smithies without a smith (hammers), weapon chain and toolsmith starved of iron bars and coal;
- population flat over the last 30,000 ticks with no residence site, soldiers above 60% of the population, goods at 0 in 80% of the samples.

The flags are heuristics; the runtime log (same ms and pid) holds the offence and recruitment lines that explain them.

## Limits

- Not verified in game. The sites count is read from the building lists while completed counts come from the table, so a site that has just completed can appear in both for one sample.
- `housing` relies on building info +0x30 being the settlers a residence provides (the AI's own planner uses it that way; the data is not in the exe file).
- Only computers the offence tick treats as active are logged; a human player or an allied computer is not.
