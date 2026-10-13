# 0.2.6: prepared terrain flags and explicit courier hiring

## User report and saved evidence

The user reports that 0.2.5 starts/runs, but Mushroom Farms alone can be built; Dark Temple, Fortress and Manacopter Hall cannot be placed even on apparently empty prepared ground. This establishes a user placement failure, not native AI development or delivery success.

Saved runtime log PID29212 identifies 0.2.5, initialization at tick 10, and a completed Farm at (341,345), tick 5135. The starting Viking barracks log has owner 2 and a nonzero economy pointer. These establish those individual logged outcomes only; a residence logged owner 2 but economy 0. Initial sectors, worker activity, attacks and overall victory remain separate user acceptance. No new live process inspection or game action was performed.

Before changes, source/logs/generated data/save pairs were SHA256 indexed under ignored `evidence/placement-025-20261012`. Original backups, saves and sidecars remain intact. Private masks, map contents, native dumps and captured source copies stay in ignored folders.

## Confirmed prepared-clearing regression

The 0.2.5 sector repair correctly retained initial ownership and resource bytes, but it also retained old chunk 6 **byte 2 blocking flags** after the generator removed objects and flattened terrain to Dark grass. Of 2401 tiles in the authored clearing x384..432/y284..332, 315 retain nonzero old flags (1, 3, 64 or 65). These flags do not disappear merely because the displayed terrain/object bytes have changed.

Pinned executable SHA256 `f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b`: the Dark race branch dispatches to RVA **0x13EBC0**. At **0x13ECBC..CC2**, every native body cell requires `(flags & 0x5F) == 4`. The loader obtains old chunk 6 byte 2 and ORs Dark bit 4. Thus flags 64/65/1/3 remain blockers on visually flattened Dark grass. This native check does not depend on the Viking owner remapping. The mod's later conversion guard has no additional owner/sector predicate, and its original footprint/entity/halo checks remain unchanged.

`tools/inspect-placement.py` verifies those instructions, dispatch, terrain rules and loader against the pinned file. `tools/test-placement.cjs` uses all four real local XML mask unions plus the mod's one-cell halo. For 391 alternative anchors fully inside the prepared patch:

| Building | Body cells | Reservation cells | 0.2.5 anchors accepted | 0.2.6 anchors accepted |
|---|---:|---:|---:|---:|
| Farm | 52 | 92 | 72 | 391 |
| Dark Temple | 327 | 421 | 0 | 391 |
| Fortress | 312 | 410 | 0 | 391 |
| Manacopter Hall | 343 | 437 | 0 | 391 |

The older trial without the halo also rejected all larger buildings. This reproduces the user's Farm-only symptom on the authored clearing; it does not prove the exact location or native state of every attempted placement elsewhere. Visual emptiness alone is insufficient evidence for other ground, objects, units or saved terrain.

## Limited map repair and retained safety

`scenario-terrain.cjs` clears only obsolete bits **0x43** while explicitly removing objects and flattening authored cells. It retains initial owner byte 1, resource byte 3, and all other flag bits. The helper is used only for the existing Dark clearing and already prepared new Viking footprints/halos. Outside these cells, original metadata remains. Tests verify retained resources, original owner remapping and no global flag erasure; a synthetic 0xFF flag retains 0xBC, including other collision/reservation bits.

All runtime collision/entity/identity/terrain/Dark/map-boundary/halo checks remain. Paid foundations must still disappear and wait 30 ticks; blocked completion refunds under the existing transaction checks. Actual construction masks, native roles, XML definitions, Fortress no-tool repair and automatic Mushroom Farmer queues remain. Four occupied Viking towers and all ten enemy building entrances/starting units/materials retain owner 2. No guessed memory addresses or validator patching.

A NEW 0.2.6 map is required. Old saves keep their old landscape and cannot verify this map repair. Paired schema 2 saves are still supported; terrain is not rewritten during load.

## Automatic Farm Shamans removed

The user explicitly requests that creating a Farm must not create a Shaman, including the analogous initial-Farm behavior. Current source establishes these units as the mod's automatic `spawnCourier` births during `discoverFarms`; they are not the native automatic Mushroom Farmers. Their reported immobility is a user observation, not independently reproduced movement evidence. The prior courier controller periodically issues delivery orders and was not intended to be a manually commanded ordinary Shaman.

Initial/new Farm registration now creates a stock record with no courier. It never calls the unit birth API. Stock remains conserved at that Farm and cannot load without a courier; it is never silently credited to spendable mana. The Farm panel explicitly says **No courier - stock waits** and **Farms never hire automatically**.

The existing **Hire courier - 5 mana** button is retained as an explicit manual choice. Only that action can call `spawnCourier`; it creates a Shaman-shaped courier and retains the walking delivery/arrival credit mechanism. This is not Farm-triggered creation. Regular manual Shaman recruitment and native automatic Mushroom Farmers remain. There is no free transport, timer income or teleportation. User testing must still establish that a manually hired courier actually walks and delivers; no movement fix is claimed from the automatic-birth removal.

No courier fields or sidecar schema were removed: that would discard conserved stock/cargo or orphan saved units. Restoring a matched older save retains its existing couriers and carried cargo without additional births. New Farms added to that save use explicit hiring. Offline models verify eight initially unassigned Farms, waiting production without credit/load, manual loading/arrival credit, waiting-stock save round trip, legacy courier/cargo round trip and existing loss/conservation/corruption checks.

## Build, package and test boundary

The first rebuild encountered a filesystem refusal to overwrite the loaded DLL. The user explicitly confirmed saving/closing the game; the next build succeeded. No process query, game termination or game injection was used to resolve that lock.

All current runtime/DLL/manifest/loader/installer/helper/readiness markers are **0.2.6**, with V026SessionHelper and startup-status-026.json. Relevant map/Lua/pinned-native/native-model/UI tests, disposable non-game startup/manifest-refusal checks, temporary installer upgrade/refusal/restore, staging and file-only loader verification passed. Shared panel previews are illustrative offscreen renders, not gameplay screenshots. Final package evidence is in combined-release-checks.json.

Activate only through the user launcher, wait for **Dark Tribe 0.2.6 ready**, then start a NEW map and follow TESTING.md. **Offline reproduction and package success do not establish native placement/completion, courier movement, AI development or complete playable-race readiness.**
