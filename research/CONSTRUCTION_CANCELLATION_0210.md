# All-building foundation cancellation repair - 0.2.10

2026-10-12. Pinned x86 S4_Main.exe 2.50.1516.0 in the game folder, SHA256 f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b. Primary source: this repository. Code/file/model work only: no agent game launch, control, capture, live process inspection, game injection or live installation.

The user installed Settlers United during this work. Its exact LAA full-file variant is additionally supported; normalization restores the original hash with no code/data differences. The installed API is shared under its own exact pin. See SETTLERS_UNITED_0210.md; this does not change the construction evidence below.

## Observed failure and corrected interpretation

The user reports no Dark buildings complete. Existing 0.2.9 log session PID 34416 records a charge at tick 1005, a blocked refund at 1336, then another charge at 1337. The same 331-tick timeout/next-tick recharge pattern repeats, without any CREATE attempt. Earlier PID 32444 (0.2.8) has the same pattern. These are preexisting user-session logs, not agent gameplay tests. An immutable local snapshot and its SHA256 are retained under ignored work/native-analysis/construction030; no existing log/save was overwritten.

My 0.2.8 interpretation that Buildings.Delete mode 0 immediately deletes the foundation was wrong. S4ModApi's BOOL for this void Lua wrapper reports that CallLuaFunc succeeded, not that the native entity disappeared. The plugin marked jobs removed even though mode 0 only performed cleanup. Its later identity gate could never pass, and discarding a timed-out job let discovery charge the same retained foundation again.

## Pinned file evidence

- Lua dispatch RVA 0x1C5EDF routes mode 0 through CBuilding vtable +0x10, mode 1 to RVA 0x136960, and mode 2 to RVA 0x136A80. CBuilding vtable RVA 0xC585E4 resolves +0x10 to RVA 0x1361D0.
- The 273-byte cleanup body at RVA 0x1361D0 returns without the native removal scheduling call at RVA 0x16D030. It unlinks building bookkeeping and notifies listeners, while the pool entity remains.
- Mode 1 clears native masks through RVA 0x136D00, performs role-specific cleanup, and calls RVA 0x16D030 with the entity ID. That marks UID bit 0x20000000 and queues deletion through RVA 0x16DB20. Later pool release at RVA 0x16D200 zeros the slot and marks/frees the native object. The 0x40000000 UID flag also marks a released object.
- Mode 2 additionally emits destruction graphics 0x3D/0x4D (other branches use 0x3F). Mode 1 has native cancellation visuals, so no claim is made that cancellation creates no graphics at all. Actual cleanup timing/placement remains user acceptance.

inspect-interactions.py reproduces these checks against the whole-executable SHA256. A read-only 32-byte dispatch guard also runs before runtime callbacks are installed. No guessed native address or direct destructor call is added; tick-time construction still uses the pinned API/Lua path.

## Shared controller and safeguards

native/construction-model.hpp drives production tick processing and the disposable construction tests. All four types use cancellation mode 1. Current native mana is checked at discovery and before payment. Unfunded sites are refused before future income can fund them. A paid job records one charge, requests cancellation once, waits for the original identity to disappear/change and at least 30 ticks, and checks the original full masks/halo, terrain, Dark land, objects and all nearby entity/building positions before Buildings.AddBuilding. The scan skips UIDs marked for deletion/freeing so a retained dying pool slot cannot be rediscovered as new work. At most one completion/retirement mutates the queue per update, as before.

Blocked work and failed cancellation/creation refund exactly once through verified native bank transactions. If the original foundation remains after refund or refusal, its job stays registered until that identity disappears. It cannot charge again or later complete without payment. Completed/departed jobs retire normally; actual failed transactions disable the plugin. Native removal-call acceptance and job transitions log BUILD_CANCEL/BUILD_STATE with ID, original/current UID, type, coordinates, mode, deadline and refund status.

A matched old paid mode-0 job whose exact owned foundation still exists requests corrected cancellation once, resets its settling deadline and never takes another payment. A recycled pool ID cannot trigger cancellation of another entity. Unpaired saves are still refused. Already-gone old foundations use their existing footprint/refund checks.

## Save compatibility and retained behavior

DTSAVE2/schema 2 and the entire 15920-byte State layout remain. Construction stays 24 bytes; its old zeroed reserved bytes at offsets 21/22 now hold refunded and cancelRequested flags. New validation rejects invalid flags/combinations. These flags persist to prevent refund/recharge loops across matching reloads. Old zeroed records enter recovery correctly. New sidecars with these semantics must be used with 0.2.10 or later, not downgraded to old plugins.

Native bank, units, saves/backups and automatic Mushroom Farmers remain. Legacy courier inventory retirement, absence of courier/stock/cargo UI, ordinary Shaman recruitment, Manacopter selection/flight/selected boarding/real passengers, Viking startup, visibility and all hash/scenario/identity/collision/transaction guards are retained. Old engine bookkeeping affected by repeated mode-0 cleanup cannot be safely reconstructed from the available evidence; begin a NEW map for construction acceptance.

## Offline validation and remaining acceptance

Seven native executables include all-four-type construction completion/costs, deferred pool/footprint cleanup, legacy paid recovery, successful Lua calls with retained entities, blocked/failed refunds, persisted refund markers, rejection before later income, recycled IDs and fatal transactions. Exact old State/Construction layouts and invalid-state refusal are checked. Existing placement, queues, panel, aircraft and interaction models remain.

The required HANDOFF.md workflow rebuilds native/map/data, validates pinned files, exercises actual scenario Lua with mocked APIs, performs twelve map round trips, verifies release hashes/dependencies/readiness, tests startup in a disposable non-game x86 process, and checks installer upgrade/refusal/restore using temporary game copies. Shared offscreen previews show the corrected construction status text and are not game screenshots. Current reports record the final offline results; no gameplay/crash resolution is claimed.

Release markers: DarkTribe-0.2.10.dll, startup-status-0210.json, V0210SessionHelper, Dark Tribe 0.2.10 ready; scenario marker 200, opponent revision 2, sidecar schema 2. Installation/activation and every in-game construction/recruitment/transport test remain the user steps in TESTING.md.
