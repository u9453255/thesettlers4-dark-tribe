# Native farm economy correction - 0.2.9

2026-10-12. Primary project: this repository. Executable: S4_Main.exe in the game folder, x86 2.50.1516.0, SHA256 f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b. No game launch/control/capture/live-process inspection or game injection; activation remains the user's work.

## User correction and implementation

The user states the native Mushroom Farmer already walks to a base carrying mushrooms. The extra Shaman-shaped courier was redundant and its simulated Stock/Cargo UI obsolete. This supersedes custom economy descriptions in PLACEMENT_COURIERS_026.md, CONSTRUCTION_MANA_TRANSPORT_028.md and earlier findings.

Removed the direct farm-income detour, pending-harvest queue, Farm discovery/attribution, simulated loading/delivery, courier creation and recurring movement, and Farm hire controls. No farm births or deliveries call mod mana transactions. The direct native credit at RVA 0x389C0 remains byte-for-byte native in the running package design, with only a read-only startup signature check. No new resource timer or XML farm/mana-rate adjustment is introduced. Native worker queue 1 and manual unit queues 2-5 retain their protections. Ordinary Shamans are no longer treated as protected couriers and may board aircraft; native Farmer kind 53 remains excluded.

The RVA 0x36380 hook is retained solely to suppress positive AI catch-up income for active human Dark player 1, unless a checked transaction bypass is active. Other players, inactive scenarios, negative changes and checked cost/refund bypass call the original function. Native bank checks/underflow-overflow preflight remain. Construction's mode-0 deletion, actual disappearance, settling, original masks/halo, entity identity and single refund remain unchanged.

All economic Stock/Cargo fields and render text are removed from the cached panel data. Farms explain their native worker; no clickable hire area remains. Manacopter passenger lists/unloading stay because they describe actual native passengers. The label is Passengers throughout; selected-unit approach orders remain tick-only and transient.

## File evidence and boundary

inspect-interactions.py checks the pinned 69-byte direct credit at RVA 0x389C0: CAIMain vtable entry RVA 0xC2A8D4, player stats at RVA 0xE92648 + player*0x17C, exact native amount at stats+0x2C, encoded bank at RVA 0x109B7FC + player*0x8C. There is no call to RVA 0x36380 inside this routine. The latter is reached from the AI catch-up branch at RVA 0x3B33E. These are file observations; actual delivery/rate/pathfinding is not established by disassembly or models. The farmer's delivery behavior is a user observation, not agent gameplay verification.

## Older sidecars

DTSAVE2/schema 2 binary layouts remain exact, with the original engine-save SHA256 pairing and state validation. Legacy Farm fields exist only for reading/validation and retirement. Before clearing records, held stock/cargo is accounted as lost legacy bookkeeping; no native bank credit/refund is generated. Historical harvested/delivered totals remain. Pending legacy diverted harvest is discarded, never replayed. Paid construction/Hall records are unchanged. Native units/balances live in the engine save and are never deleted, respawned or reconstructed by migration; old delivery Shamans receive no new mod movement but may finish their existing native order. Corrupt state is refused before mutation; retirement is idempotent.

No existing user save/sidecar is rewritten by this development session. New saves write empty Farm records/zero pending harvest. Old inflated native balances cannot be safely subtracted. A NEW map is required for mana acceptance; matched old saves may validate migration separately.

## Offline validation

The required primary build/check workflow is recorded in HANDOFF.md. Native model checks cover exact old layout, old inventory retirement, paid job preservation, idempotence, invalid-state refusal, affordability, protected workers, transport identities/proximity, nonblocking callbacks and UI hits. Pinned source checks assert that custom courier/income/movement/UI paths are absent and the direct native credit remains unintercepted. Map/Lua fixtures, 12 byte-identical round trips, runtime/dependency/hash/readiness checks, disposable non-game x86 startup and temporary installer upgrade/refusal/restore provide offline evidence only.

All required primary workflow checks passed for 0.2.9, including six native model executables, pinned file analysis, map/Lua checks and 12 round trips, runtime verification, disposable startup and temporary installer tests. Offscreen panel previews were inspected after the UI changes.

Current detailed results: interaction-native-checks.json, ui-queue-checks.json, v02-offline-checks.json, and startup-regression.json. Shared panel previews are generated offscreen from code and are not game screenshots.

Release: DarkTribe-0.2.9.dll, startup-status-029.json, V029SessionHelper, Dark Tribe 0.2.9 ready; scenario contract 200, opponent revision 2, sidecar schema 2. No live installation is claimed. TESTING.md assigns native farmer delivery/mana, repeated construction, transport and old-save acceptance to the user; earlier crash resolution remains unverified.
