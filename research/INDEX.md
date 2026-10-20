# Research and evidence index

Updated 2026-10-20. Start with `../HANDOFF.md` for current decisions and `../MAIN.md` for current functionality.

| File | Evidence and scope |
|---|---|
| `DARK_SIGHT_FOG_0215.md`, `construction-inplace-checks.json` | r35: why reclaimed green land stayed lit (the native fog merge 0x1AE3F0 skips 8-cell groups with three stable cycles), the fog byte/land byte 3 layout, the counter hand-back fix and its pins; gameplay unverified |
| `R50_PLAN_0215.md`, `DEFENCE_GAPS_0215.md` | r50 plan (implemented in round r50, see `AI_OFFENCE_0215.md` Round r50; gameplay unverified): faster gardener replacement, the drop rule, the reserve room column, a winning land defence that keeps going, a fast rise that skips the attacks-first gap, a defence aimed at the land being lost, a gardener home guard, fire and far-gardener diagnostics; the other agent's defence review with its challenge |
| `ECONOMY_TELEMETRY_0215.md` | r44: the computer economy ledger (`evidence/v02-economy.csv` every 500 ticks: settlers and buildings per type, carriers/reserve/keep, housing, goods), its engine sources and `tools/economy-report.py`; gameplay unverified |
| `AI_OFFENCE_0215.md`, `offence-native-checks.json` | Why the computer AI never attacks the Dark Tribe (target filters, no damage path, fire defence), the mod-owned native squad, mirrored attack timing/strength, farms-first targeting, siege and credited destroy; r28: six attack styles, pressure, native soldier supply (barracks/weapons/carriers), territory owner and own-territory greening, flag 0x80; r29: toolsmith shovel orders; r30: the citadel fire re-derived (Euclidean disc, Temple/Fortress only, flames at 16 vs kills to 24), keep-off rules, fire routing and guard, field evidence of fire losses; r32: style sizes and gardener counts scaled with the army, the convoy that keeps gardeners with the soldiers (checked by `tools/inspect-offence.py`); gameplay unverified |
| `CRASH_DIAGNOSTICS_0215.md`, `diagnostics-checks.json` | r26: the vanilla `lua_close` double free on map exit (not Dark Tribe), heap-free crash/freeze diagnostics, the disposable fixture and the offline dump reader |
| `ALLIANCE_0215.md`, `alliance-build.json`, `alliance-checks.json` | r25 The Alliance (dark07, 512x512, four players), 512-map support, r27 gardeners and r28 start supply (barracks, weapons, shovels, carriers, levelled ground); r31 clean-start check |
| `IRON_KINGDOM_0215.md`, `iron-kingdom-build.json`, `iron-kingdom-checks.json` | r24 Iron Kingdom (AO_trojan10), r27 gardeners and r28 barracks, weapons and shovels; r31 clean start (two farms in the northern Temple's fire field removed) |
| `CONSTRUCTION_INPLACE_0214.md`, `construction-inplace-checks.json` | Current in-place construction: every native field/RVA of the reserved site, rise and completion sequences; r30: citadel fire blockade (no Farm within Euclid 33 of a Temple/Fortress fire centre) and start-map/load farm removal through the crush path (checked by `tools/inspect-construction.py`); gameplay unverified |
| `recruitment-native-checks.json` | Native barracks cluster amounts, Dark Temple order clamps and saved queue layout (`tools/inspect-recruitment.py`) |
| `BUILDING_LIMIT_0215.md`, `interaction-native-checks.json` | One Dark Temple and one Dark Fortress: counting rule, removal marking (RVA 0x16D030) and crush path, tick enforcement, Settlers United placement hotkeys that only create sites; gameplay unverified |
| `CONSTRUCTION_HINTS_0213.md`, `placement-hint-checks.json` | Current actual construction flags/terrain and automatic tick-only Dark viewport hints; all-mask/no-hover/cache-reuse models and native signature/ABI checks; gameplay remains user acceptance |
| `HD_STARTUP_HASH_0212.md` | User tick-0/null-call crash, confirmed pre-hook hash defect, corrected independent Windows SHA256 and ninth offline fixture; actual startup resolution unverified |
| `HD_MENU_DISPLAY_0211.md`, `hd-rendering-checks.json` | 0.2.11 startup failed; retained 0.2.12 menu implementation, static DX11/status-export inspection and owned cached-panel fixture; game visibility/input unverified |
| `CONSTRUCTION_CANCELLATION_0210.md`, `interaction-native-checks.json` | Current native cancellation/lifecycle repair, user failure logs and offline shared-controller regressions; gameplay unverified |
| `SETTLERS_UNITED_0210.md`, `settlers-united-files.json`, `game-build-variants.json`, `settlers-united-api-source.json` | Installed SU/HD/API file evidence, exact LAA and shared API integration, primary-source addon opportunities; runtime compatibility unverified |
| `NATIVE_FARM_ECONOMY_029.md` | Current native worker economy, removal of redundant couriers/UI, legacy sidecar retirement and preserved transport; gameplay unverified |
| `CONSTRUCTION_MANA_TRANSPORT_028.md` | Historical construction/mana/transport repair; native interception/custom delivery superseded by 0.2.9; mode-0 deletion claim corrected by 0.2.10 |
| `HELICOPTER_023.md` | Historical aircraft contracts and native transport findings; controls superseded by 0.2.9; gameplay remains unverified |
| `V02_NATIVE_FINDINGS.md` | Pinned layout, hooks, save imports and native observations; construction timing/masks describe initial 0.2 and are superseded by 0.2.2 |
| `QUEUE_UI_022.md` | Historical panel, retained queue/construction safeguards and diagnostic limits; Farm stock/cargo UI superseded by 0.2.9 |
| `FORTRESS_REPAIR.md` | Original manual/native role research, matching Fortress construction-panel crash, isolated correction and gameplay boundary |
| `fortress-native-checks.json`, `fortress-offline-checks.json` | Pinned parser/panel/crash path and generated data regressions; no game execution |

| `OPPONENT_START.md`, `opponent-offline-checks.json` | Isolated separated Viking/occupied tower candidate, native AI startup and guarded victory checks; no game verification |
| `native-build-pin.json` | Executable version/hash and key startup RVAs |
| `compiler-download.json` | Portable compiler provenance; `tools/setup.py` downloads and checks it |
| `v02-offline-checks.json` | Latest recorded package/native/legacy save compatibility and temporary installer checks |
| `startup-regression.json` | Helper ordering and disposable non-game injector checks |
| `ui-queue-checks.json` | Queue/placement/hit checks and offscreen preview; game_started false |
| `map-validation.json` | Byte-identical codec round trips, not gameplay proof |
| `scenario-build.json`, `building-compatibility.json` | Generated map/data reports |
| `FIELD_NOTE.md` | Historical 0.1 prototype field note (map format, imported-map human slot, Ubisoft Connect relocation); read with chronological MODLOG |
| `sources/` | Ignored local source captures/provenance |

Private working evidence lives in `../work/native-analysis` and `../evidence`; generated footprints are in `../work/native-generated`. These paths are retained locally and intentionally omitted from Git. A missing private artifact in a fresh checkout is not evidence that the corresponding runtime behavior was tested.

Interpretation rules: separate static source/disassembly, offline fixture/model tests, historical agent gameplay and user reports. Match evidence to executable hash, release, PID and session. An offline preview is not a screenshot. A first-chance exception is not necessarily a crash. Absence of a Windows Application Error does not establish a clean exit. Use explicit hexadecimal notation when referring to native RVAs/byte offsets; earlier compact notes sometimes omitted the `0x` prefix.

- [0.2.4 startup failure and 0.2.5 sector repair](STARTUP_SECTORS_025.md): new CBarrackRole fault, initial ownership, preserved metadata and ending guard; user acceptance pending.

- [0.2.6 prepared terrain and explicit couriers](PLACEMENT_COURIERS_026.md): Farm-only placement reproduction, limited map flag repair, no automatic Farm Shamans and conserved manual delivery.

- [0.2.7 startup lifetime](STARTUP_LIFETIME_027.md): newest fault/index replay, helper-session chronology and delayed/restarted startup regression; gameplay unverified.
