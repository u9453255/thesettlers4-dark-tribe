# Dark-land sight and the native fog merge (0.2.15 round r35)

Date: 2026-10-20. User report (r34 build), with a screenshot of a lit green patch inside fogged land: "the area which has been taken by dark tribe but then took back by computer player its still showing visible for me, even when its green again".

Offline static analysis of the pinned `S4_Main.exe` only (capstone, `work/native-analysis/ai/scripts/q.py`); no game was started, attached or captured. Gameplay is unverified until the user tests it (TESTING.md, r35). `tools/inspect-construction.py` re-checks every fact below that the code relies on.

## Data

- Land byte 3 (landscape 0x11630D4 / view copy 0xD69220, 4 bytes per cell): bits 3-5 the current fog level, bits 0-2 the previous level (the renderer fades between them), bits 6-7 another attribute. Level 0 is fully visible, 3 explored (dim), 7 never seen (black).
- Fog manager (CFogging, object at RVA 0x1159D30, vtable 0xC60A84): +8 the landscape and +0xC a byte map of N x N, both stored by slot 2 (0x1AE600). Fog byte bits 0-2 = the cell's base level, bits 3-5 = pending level of the current cycle, bits 6-7 = stable-cycle counter.
- Slot 5 (0x1AEAB0) is the cycle. Phase 1, 32 bands: the band pass 0x1AE210 lowers pending through 0x1AE100(x, y, r) around allied settlers and buildings (radius r at pending 0, then a 6-ring soft edge 8, 16, ... in pending units). Phase 2, 8 steps: the merge 0x1AE3F0 over N/8 rows each, then `SetFoggingRange` with the rows that changed.

## The merge (0x1AE3F0, MMX, 8 cells per iteration; step table 0xC1BD30 = 0x40, 0x18, 0x07, 0x38, 0xC0, 8 bytes each)

- Counter: E = (base << 3 == pending). The fog byte becomes base | base << 3 | counter, where counter = E ? min(old counter + 0x40, 0xC0) : 0 (`paddusb`, `and 0xC0`). Pending is reset to the base for the next band pass.
- Skip: when all 8 counters are 0xC0 (three stable cycles), the land write of these 8 cells is skipped (`cmp eax, -1` / `je 0x1AE538`).
- Otherwise the new land level is min(pending, max(old, 3)); old 7 uses step 7, so black jumps straight to pending. The old level moves to bits 0-2; changed rows widen the range passed to the renderer.
- Consequence: a visible cell no longer seen drops to 3 (explored) on the next written cycle, and cells nobody sees stop being written after three cycles.

## The defect

The plugin's Dark-land visibility clears land byte 3 bits 0-5 on every Dark cell every 5 ticks (level 0). A Dark cell nobody sees has pending == base each cycle, so its group is skipped once the counter saturates. When a computer greened the cell, the plugin stopped clearing it, but the skipped merge never rewrote it: the green land kept level 0 and stayed lit (the screenshot). Units near the land unblock their groups natively, which is why only unwatched patches stayed lit.

## The fix (r35)

- `dt::darkSight` (native/aircraft-model.hpp, tested in aircraft-tests.cpp): Dark cells are revealed as before and marked; a marked cell that is no longer Dark gets its fog counter cleared once (`fog[i] &= 0x3F`). The next merge processes its group and applies the native rule itself (explored, or visible if a unit sees it), including the renderer's row range. No fog level is written by the plugin for green land.
- The marks are transient (`sightShown`, cleared by mapInit). The first pass of a map treats every cell as marked, so every non-Dark cell is re-checked once; this heals cells an older round left lit in a save. Cost: one extra merge write per group, which the native cycle does for any changing group anyway.
- Guard: only when the fog manager's +8 equals the view landscape and the +0xC map is readable; otherwise the marks wait and `DARK_SIGHT fog map unavailable` is logged once.
- Startup pins: 0x1AE438..0x1AE49B (counter and skip, with the relocated step-table pointer) and the 40-byte step table at 0xC1BD30 (`Pinned fog merge signature mismatch; plugin disabled`, code 15).

## Round r42: whole-map reveal (Alt+9)

User request (2026-10-20): a toggle on Alt+9 that makes the whole map visible.

- Keys: S4ModApi has no keyboard callback, so the tick polls `GetAsyncKeyState` (VK_MENU and '9') and counts a press only while this process owns the foreground window and a map runs (`inGame()`); one edge per press (`dt::pressed`). `MAP_REVEAL on/off` is logged.
- Reveal: every tick while on, `dt::revealPending` clears the pending bits 3-5 of every fog byte (fog manager +0xC). The native band pass only lowers pending, so pending stays 0 at every merge step; the merge shows level min(pending, max(old, 3)) = 0 on every cell (terrain, objects and units of every player drawn), resets pending to the base, and the next tick clears it again. The counter (bits 6-7) never saturates while base != 0, so the land write is never skipped.
- What is not touched: the base bits 0-2 (the saved explored state: 7 never seen, 3 explored, 0 own) and the counters. A save made while the reveal is on therefore stays honest. Switching off stops the clearing; the next merge writes min(base, max(0, 3)) = 3 for base 3 or 7, so the map fogs back to the explored look within one cycle; cells never explored show explored (dim) instead of black until the game is reloaded (the displayed level, not the saved one).
- Cost: one pass over N x N bytes per tick (331 KB on a 576 map).
- Not verified in game; the HD renderer reads the same maps as the Dark-land sight does.

## Limits

- Not verified in game. Dark cells themselves still flicker to level 3 for up to 5 ticks when a native merge writes their group (unchanged since 0.2.x).
- The minimap's fog is not analysed.
