# Queue/UI and second-farm report, 0.2.2

User asked for readable queues, cancellation and a graphical interface, and reported a crash on the second mushroom farm. Existing v02 log only records initialization for PID3672; no fault address or placement phase was available. No game was launched, captured or controlled in this session.

Native Temple handler at RVA 0x145d60 adjusts counters at role + 0x180 and caps native finite orders. The existing private temple-event/temple-recruit observations show no additional wake flag in that handler, and show mana spending plus queue decrement immediately before birth. Original queue logic changes only the four manual counters, caps at99, clamps zero and preserves automatic farmer queue1. Action IDs carry selected identities to reject stale targets. Hall jobs use sequential ready times; cancellation refunds only matching pending jobs.

The API source S4::IsOccupied reads Blockmap2 and returns whether its sector value is zero; it is not a comprehensive entity occupancy oracle. LandscapeGetObject reads the entity map. The new placement guard combines both, direct nearby entity positions, Dark/terrain bounds and existing footprints. Locally generated footprints now union blockPosLines, buildingPosLines and digPosLines and add a one-tile safety reservation. Old sites must disappear before conversion. Only one conversion is attempted per update, after a three-second settling delay. Rendering/input skip rather than wait when the economy lock is held, preventing lock reentry stalls.

New construction/courier phase logs and first-chance exception address diagnostics aid a further report. Exceptions are never suppressed; ordinary handled exceptions may be recorded. Exact cause and crash resolution are not claimed established.

Panel renderer in native/panel.hpp draws its own vector icons, cards, badges, +/- controls, resource header and progress bar through GDI. Shared hit targets and graphics are used by the native mod and standalone preview. No game artwork was extracted and no art generation service was needed. Preview values are illustrative, not live game data.

Offline tests cover queue bounds and immediate add/cancel, protected auto-workers, aircraft generation guards/scheduling/refunds, occupied cells, adjacent overlap, map edges, safety margin and actual panel hit targets. Offscreen visual preview inspected; package/loader/inventory checks passed. User acceptance instructions are in TESTING.md. Existing state version2/schema is preserved.

