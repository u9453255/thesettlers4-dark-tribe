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
