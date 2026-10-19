# In-place Dark construction - 0.2.14

2026-10-12. No agent gameplay test, process inspection or injection. Analysis is
file-only disassembly of the pinned `S4_Main.exe` (image base 0x400000; RVAs below).

## Evidence

The user reports that Dark buildings still cannot be built in 0.2.13. In the PID 23844 runtime log, every
Temple (50), Fortress (51) and Manacopter Hall (80) job follows this order: paid, `BUILD_CANCEL mode=1`, then about
300 ticks later `BUILD_BLOCK reason=ground entity` and a refund. The blocking entities are moving units. Entity
1206 blocks three different sites at different ticks. Farms complete because the shorter footprint is free more often.
Confirmed defect: the cancel-then-`BuildingsAdd` lifecycle releases the reservation, so units can walk into the footprint.

## Native construction site (CBuildingSiteRole, vtable 0xC591BC)

| Field | Meaning |
|---|---|
| +0x178 | building info (`0x109C600 + (race*83+kind)*0x32C`) |
| +0x180 | levelling slot (-1 none) |
| +0x18C | phase |
| +0x19C / +0x1A0 / +0x1AC | progress / total / clipping |
| +0x1B0/2/4 | material pile object IDs |

Building info vectors: +0x2CC buildingPosLines, +0x2D8 digPosLines, +0x2E4 blockPosLines and +0x2F0 repealingPosLines.

- Init slot 6 (0x141A50) sets entity flags `|0x1001000` and calls 0x143680. Farm 0x31 and Hall 0x50 take the
  **builder-free grow path**: they claim the building cells and set phase 6. Other kinds create material piles and start at
  levelling phase 2 (or 4).
- Tick slot 3 (0x141CC0) requires flag 0x1000. It applies an ordinary sector-economy check except for Farm and Hall, and
  dispatches only phases 2..8 through the jump table at 0x14236C. The mod freezes the site at phase 1.
- Phase 6 sets the solid flags on the block cells. Phase 7 waits until those cells are free (0x20D870), reserves through
  0x136470 and sets phase 8. Phase 8 (0x142312) writes the entity-map anchor and clears the work slot. It then sets the
  flags with `&~0x1000000|0x2000000` and calls 0x1362F0, which updates counts and installs the final role.
- Ordinary phase 5 completes through 0x1424C0 and then 0x1429F0. The second call needs an economy at the entrance
  sector. It removes the piles through CBuilding slot 16 (0x137100), `dec [0xDCDB4C]` and 0x16D030 mode 0. It also
  performs flag-guarded, idempotent economy bookkeeping, sets the same flags and calls 0x1362F0.
- Slot 11 (0x142D50) detaches levelling and economy requests without deleting anything. With levelling slot -1 at phase 2
  it would index another player's slot, so the mod first moves the site to phase 1.

## Implementation

The code is in `native/construction-native.hpp` and `native/construction-model.hpp`. On payment, the mod freezes the
site, detaches requests for Temple and Fortress, and reserves once unless the native phases already did. Growth is
drawn over 30 ticks. Completion follows the native sequence for the kind as described above. If Temple or Fortress has
no economy, the mod uses the pile-removal steps plus phase 8. Mobile units are waited for without a timeout refund.
Static blockers, lost sites and refused starts refund once and cancel with mode 1. Paid conversions from 0.2.13 are
refunded once and never recreated. Startup fails closed if any of the 36 pinned regions differ.
`tools/inspect-construction.py` checks every native fact listed here against the file.

## Field result 1 (PID 31592)

In this run, Farms were paid and started growing, then were refunded at completion as "static obstruction". Three findings from the code explain this:

- The completion check re-tested the placement flag mask `0x57` (and `0x55` once claimed) on the site's own cells.
- Object creation (RVA 0x163510 to 0x1635DA, with callers including building and settler roles) sets bits 2 and 0x40 on the object cell, and bit 0x40 on a ring of cells around it.
- The reservation itself only adds bits 2, 8 and 0x80.

A planted mushroom therefore explains the refund. It is the only setter of a masked bit found in the executable.

Fix: reserved cells are checked for occupancy only, as native phase 7 does. Non-entity map values wait instead of refunding. The block reason is now captured at the decision. The exact bit seen in game was not logged.

## Unverified

- That all four building kinds complete in game.
- That units leave reserved cells. Native phase 7 relies on the same behaviour, but a unit that never leaves would stall
  the job.
- Whether Dark land has an ordinary sector economy, which decides the Temple/Fortress path.
- That no pile or stake object is left behind.
- That saving during growth keeps the frozen phase.

## Round r30 (0.2.15): citadel fire blockade and start-map farm removal

User decision (2026-10-19): no Mushroom Farm may stand inside the fire field of a Dark Temple or Fortress, and start-map farms inside it are destroyed at once. The fire facts and the offence side are in `AI_OFFENCE_0215.md` (Round r30). File-only; not verified in game.

**Rule (`native/blockade-model.hpp`, geometry `native/citadel-fire.hpp`).** A Farm position (49) is blocked when it lies strictly closer than `dtfire::FarmLock` 33 (Euclidean, `fire2 = dx² − dx·dy + dy²`) to the fire centre (building position + (1,2)) of any own Temple (50) or Fortress (51), completed or site. A Temple/Fortress position is blocked when any own Farm, completed or site, would then lie that close to its centre. The Manacopter Hall (80) has no fire and is never blocked. The offence director uses the same `FarmLock` for its farm lock, so no farm can stand where the computers may not besiege it.

**Discovery (`construction-model.hpp`).** `discoverConstruction(..., FireBlockade* fire)` checks Duplicate first, then `ConstructionEvent::FireBlocked`. A blocked site is cancelled with mode 1 through the existing `refuseConstruction`, before payment, so it is never charged. A kept site joins the blockade at once, so of two sites on the same tick the first (a Temple/Fortress is decided before the others) wins. The blockade is rebuilt every tick inside the existing discovery entity scan (own completed citadels and farms plus the growing jobs); it is transient and never saved (no `dt::State` field). The player text is *"Too close to the fire of your Temple or Fortress."* for a Farm and *"Too close to your Mushroom Farms. The Temple's fire needs room."* (or *"The Fortress's"*) for a citadel.

**Placement hints (`placement-native.hpp`).** A valid anchor that the blockade blocks gets result 0 (no green marker) and is counted in `Diag::fire`; `PLACEMENT_SCAN` logs `fireBlockade=`. The hints are reset whenever the blockade snapshot changes (sorted with `normalize()` before comparing). The native cursor outline (validators 0x13E4E0/0x13EBC0) is unchanged, so a click on a blocked spot can still create a site; discovery then refuses it with the notice.

**Start-map removal.** `initialize()` sets `fireStartPending = !restored`, so only the `New Dark economy initialized` path arms it. On that construction pass every own completed Farm inside a fire field is removed once with S4ModApi `BuildingsCrush(id)`, the player's own demolish path (command 5): Lua CrushBuilding entry 0x1C4D4C → mode-1 removal 0x136960 (race-3 economy skip at 0x136A26) → Kill `0x16D030(id, 0)` → CBuilding slot 4 0x1361D0 (call at 0x16D1C7) → 0x13C680 (call at 0x1362A9), which decrements the ready counter `[0x10EF624 + idx*8]` at once (0x13C719). Attacker 0 skips every statistic, and the Farm's slot 11 (0x145050) credits `MushroomFarmsDestroyed` only for a non-zero building byte +0x25, so no computer is credited. 0x136A80 is not used (it credits an attacker and posts a "building lost" notice). On this tick a citadel wins over a Farm; a start-map Farm site inside a field is cancelled silently (`FARM_FIRE_REMOVED ... reason=start site`).

**Load sweep (review fix).** `fireLoadPending` is set on a restored save; the first construction pass runs the same sweep once, logged `reason=load`. A save from before r30 may hold a farm inside a field, which the computers never take while it keeps the fire on. An r30 save holds none, so the pass is idempotent without a schema change. A Farm still growing inside a field in a pre-r30 save is not swept.

**Pins (`tools/construction_pins.py`, checked at startup by `nativeConstruction.initialize`, and by `tools/inspect-construction.py`).** (0x1C4D4C, 0x23) CrushBuilding entry, (0x136A26, 0x58) race-3 skip and Kill(id, 0), (0x16D1C7, 7) Kill's slot-4 call, (0xC585E4+0x10, 4) CBuilding slot 4 = 0x1361D0, (0x1362A9, 10) the call to 0x13C680, (0x13C719, 0x5D) the ready/site counter decrement, (0xC58AE8+0x2C, 4) and (0x145050, 0x23) the Farm's slot 11 and its +0x25 credit test. 47 generated regions in total.

**Logs.** `FARM_FIRE_REMOVED x,y reason=start|load farm inside the citadel fire field farm= citadel=x,y crushAccepted= gone=`; `FARM_FIRE_REMOVED x,y reason=start site inside the citadel fire field site= kind=`; a refusal is a `BUILD_STATE site= ... inside the citadel fire field (Farm vs Temple/Fortress); refused before payment` line (`start-map site inside the citadel fire field; cancelled before payment` on the new-map tick).

**Expected start removals (survey of the generated maps).** First Dawn: one farm at 337,282 (Euclid 18.3 from the Temple's centre). Iron Kingdom: two farms, 391,124 (12.8) and 413,161 (25.5), both by the northern Temple at 387,133. The Alliance: none (no start farms). Every map keeps room for farms outside the fields.

**Checks.** `construction-tests` `fireBlockade()`: the strict Euclid-33 edge on and between the axes (hex 38 is still inside), an exact count of 3,943 blocked cells against `farmTooClose`, both directions, the Fortress like the Temple, the Hall never blocked, refusal before payment, kept records and later charging, Duplicate first, same-tick order, snapshot order and digit-free texts. `inspect-construction.py` checks the new pins' disassembly and the source contracts (BuildingsCrush only in the sweep, `fireStartPending=!restored`, the load sweep, `discoverConstruction(..., &fire)`, `blockWith`, `fireNotice`, the hint filter, the shared geometry).

**Unverified (r30).** That the placement anchor equals the entity position +0x18 (it follows from the footprint and reservation code). Whether the native crush consumes the logic LCG (it is a native action, not a plugin roll). That the collapse effect at map start looks acceptable. That a stale marker clears within one rescan.
