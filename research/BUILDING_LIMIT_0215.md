# One Dark Temple and one Dark Fortress - 0.2.15

2026-10-13. User request: "Limit to build 1 dark fortress and 1 dark temple also, does not allow to build more than 1 of
those building." No agent game launch, process inspection or injection. Engine facts come from file-only disassembly
of the pinned `S4_Main.exe` (RVAs below). `tools/inspect-interactions.py` re-checks them.

## Counting rule

A Temple (50) or Fortress (51) of player 1 (owner byte `+0x20` = 0x13) exists in either of two cases:

- **Built.** A completed building exists: an entity of class 8 that is not a construction site, whose UID has neither
  0x20000000 nor 0x40000000 set, and whose flags `+0x14` lack 0x4000000.
- **Building.** A construction job of that kind will become one. The job may be unpaid or growing, but not refunded or
  refused. Every native site of player 1 is either such a job or is discovered in the same tick.

Refunded or refused jobs do not count, and neither do sites or buildings marked for deletion. After a loss, the player
can therefore build again on the next tick. Counts are rebuilt on every tick (`dt::BuildingLimit`,
`native/construction-model.hpp`).

## Engine evidence

- RVA 0x16D030 is the universal entity removal scheduler. Before it queues the deletion (RVA 0x16DB20), it sets UID bit
  0x20000000 at `+4` and the dying flag 0x4000000 at `+0x14`. The pool release (RVA 0x16D200) later zeroes the slot.
  A sweep of `.text` found two other writes of 0x4000000 to a `+0x14` field, at RVA 0x9B4204 and 0xA07E6A. By
  inspection they are a flag-translation routine and a static initialiser, not entity code. The UID bit is the primary
  test in any case.
- Lua `Buildings.CrushBuilding` (registration entry RVA 0xD65CB8 to RVA 0x1C4CF0) is what S4ModApi `BuildingsCrush`
  calls, and it is how the mod demolishes. It requires class 8 and flag 0x4000000 clear, then jumps to the mode-1
  removal at RVA 0x136960, which calls RVA 0x16D030.
- Mode-2 destruction (RVA 0x136A80) also calls RVA 0x16D030. The native callers of the two removal routines are RVA
  0xE53E0, 0xF6356, 0x137229 and 0x13D03B, plus Lua Delete at RVA 0x1C5EDF.
- For reference only (not used by the DLL): Lua `Buildings.Amount` (RVA 0x1C4FA0 to RVA 0x13D1C0) reads per-player
  counters at RVA 0x10EF620 + (player*0x53+type)*8. The first DWORD is UNDERCONSTRUCTION (0) and the second is
  READY (1); ALL (2) is their sum. The Lua constants table is at RVA 0xD65F18.

## Where the limit is enforced (all on the tick)

1. **Counting.** `construction()` counts inside its existing discovery scan of the entity table, so there is no
   additional scan. For each completed Dark building of player 1 it adds one kind read and one flag read (from the
   tick read cache), and it adds one pass over at most 128 jobs.
2. **New sites.** New native sites are collected during the scan and decided after it, in scan order
   (`dt::discoverConstruction`). A site of a kind that already exists is cancelled with native mode 1 before payment
   (`dt::refuseConstruction`, event `Duplicate`, logs `BUILD_CANCEL`/`BUILD_STATE`). The player sees "You already have a
   Dark Temple." or "You already have a Dark Fortress.", and no mana is charged. The refused record is kept, schema-2
   valid, until the site's identity disappears, exactly like an unfunded refusal. If two outlines are placed in one
   tick, the first is kept and the second is refused.
3. **Command 1.** Command 1 (the construction tile) is refused with the same text when the kind exists.
4. **Construction tiles.** `Data::buildLimit[4]` disables the tile: the picture is greyed, and "Built" or "Building"
   replaces the cost. `hit()` returns no action anywhere on the tile.

## Native bypass paths found

All of these paths create a native construction site, which point 2 cancels before payment:

- **Settlers United Advanced Hotkeys** (`Plugins/HotkeysConfig/default.json` in the user's install):
  - `"_repeatedBuildingPlacement": "VK_CONTROL"`: holding Ctrl keeps placement active, so several outlines can be placed.
  - Alt+Space `onStartBuildingPlacementOfLastBuildingPlacementKey` starts placement of the last building type
    without the mod's menu.
  - Number keys run `onStartBuildingPlacementOfCurrentBuildingMenuBuildingN`.
  - Q/W/E/R/T open the native construction menus.
- **The native construction menu.** It is under the Dark panel, or visible when the panel is hidden.

Lua `Buildings.AddBuilding`/`AddBuildingEx` creates a finished building without a site. Only scripts, map editing or
cheat tools can reach it, and the scenario does not call it. Such a building counts as built but is not removed.

## Compatibility

Sidecar schema 2 and `DT_Version` 200 are unchanged, and no field was added to the saved state. A refused duplicate is
an existing refunded-job record (removed, refunded and cancelRequested set, pad 0), which `dt::valid` already accepts.
Saves that already hold two Temples, two Fortresses or two growing jobs of one kind keep them. Nothing is removed, and
only new sites are refused.

## Offline checks

- `construction-tests`: none, completed, unpaid job, growing job, refunded job, lost site, two or three outlines in one
  tick, and the next tick. It also covers the case where funds are missing (refused as unfunded, not counted) and
  sites off Dark land.
- `panel-tests`: tile hit targets (the whole tile is inert while limited, other tiles stay available) and the greyed
  picture identical to the unaffordable one. It also covers opaque coverage and the per-glyph layout check with both
  states (0 failures).
- `inspect-interactions.py`: the RVAs above and the source contracts.

## Unverified (user tests)

- The tile states with a real starting base.
- Cancellation of a second outline placed with Ctrl held or with Alt+Space.
- Rebuilding after the base is demolished or destroyed.

Since r19 the construction page has no notice area, so the refusal text shows only in panels that have one. On the
construction page, the vanished outline and the "Building"/"Built" tile are the visible result.
