# Manakopter research and implementation — 0.2.3

2026-10-12. Offline inspection only; no game process access or gameplay testing.

## Sources and evidence

- [Settlers United: DarkTribe.FlyTo](https://docs.settlers-united.com/s4-lua-api-de/library-functions/darktribe/darktribe.flyto) explicitly describes the aircraft just created by the script. There is no entity/player argument. This is community API documentation, corroborated by the pinned executable.
- [Settlers United: DarkTribe.AddManakopter](https://docs.settlers-united.com/s4-lua-api-de/library-functions/darktribe/darktribe.addmanakopter) documents creation at coordinates for a party and no return value. Its example creates, then immediately flies the aircraft.
- [S4ModApi scripting implementation](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CS4Scripting.cpp) wraps FlyTo as a Lua call success boolean; its AddManakopter wrapper tries a numeric Lua result despite the game's no-result contract. API success alone cannot establish movement or creation identity.
- [GameStar, March 2002 Trojan campaign guide](https://download.gamestar.de/public/31500/31506/03_2002_tipps_siedler4ta.pdf), page 2, tips 22–23, describes airborne Dark landing parties: Shamans land, convert and abduct settlers; troops protect them. This contemporaneous play report supports the transport/raid role, not a human aircraft menu, an original passenger capacity, or specific player click behavior.
- [S4ModApi send hook](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CSettlerSendHook.cpp) provides a packed map destination and cancellation of the normal command. [Mouse hook](https://github.com/nyfrk/S4ModApi/blob/master/S4ModApi/CMouseHook.cpp) supplies UI clicks. Neither callback should run Lua. The mod provides its own cached aircraft panel/roster because the existing human Dark controls are incomplete.

The separate promotional Manacopter minigame is not evidence for Settlers IV transport mechanics. No credible source inspected establishes a complete original human-operated Dark aircraft menu. The mod interface is explicitly a new interface to existing native behavior, rather than a claim to reproduce an undocumented original menu.

## Confirmed old defects

0.2.2 expected AddManakopter's numeric result to be an ID, allowing a real creation to be treated as failure/refunded. Flight ignored the requested entity and identity, always calling the last-created Lua route. Only building kinds were rendered in the mod panel. The send callback depended on native single selection and recorded no identity, and it announced success without checking the native destination. These are source/contract defects; the user's inability to move is an observed outcome, not a proven complete causal diagnosis.

## Pinned executable findings

Executable SHA256 remains `f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b`. Addresses below are RVAs and apply only to this build. Private disassembly remains under ignored `work/native-analysis`.

- Lua creation at RVA `0x1cb370` calls the native creator and pushes no Lua result. Creator stores the new pool ID at RVA `0x113fc98`.
- FlyTo at RVA `0x1cb4a0`, lookup at `0x1cb557`, uses that global, checks entity base type `0x40`, writes destination WORDs at entity `+0x48/+0x4a`, sets the flight flag at `+0x4c`, and schedules the native update. The null case is unsafe in the original routine; never call it without a validated live aircraft.
- Aircraft RTTI/vtable is CManakopter, RVA `0xc5a3b8`. The aircraft owner byte is encoded as player shifted four bits plus Dark tribe 3. Object kind is 1, which cannot distinguish it from ordinary objects; use base type plus RTTI and identity.
- Aircraft storage is `0x88` bytes. Passenger vector starts/ends at `+0x58/+0x5c`; entries are 12 bytes containing identity, pool ID and unit kind. Native destruction/unloading rechecks both identity and pool ID.
- Native boarding at RVA `0x16b890` retains the real unit in that vector, hides it and clears its ground occupancy. Native unloading at `0x16b990` restores passengers through unit events. Entry signatures are pinned at initialization and offline verification.
- Movement update at `0x16b4e0` takes off, moves and descends. Ground offsets `+0x78=-10`, `+0x7c=-50`, and cleared flight flag are used as the mod's idle/landed precondition. This interpretation requires gameplay confirmation.
- Unload tile offsets come from RVA `0xd69988`, first six records: `(2,0),(2,1),(2,2),(1,2),(0,2),(-1,1)`. The implementation checks every one, regardless of native rotating start index. These are not a guessed adjacent hexagon.

## Implementation and limits

Tick-only scoped rebinding of the last-created slot lets the existing Lua flight routine control a chosen identity, then restores the original slot. Flight verifies the resulting target before reporting acceptance. Creation validates a newly changed native ID/identity rather than the Lua result. A roster tab/Hall button and next-aircraft action avoid relying solely on the missing human Dark menu. Native map destination events are handled using cached selection; coordinate +/-10 and Fly provide an independent fallback. Return chooses an existing clear base entrance.

Boarding uses existing nearby Warriors, Throwers and non-courier Shamans, up to six passengers for mod-issued boarding. Six is a conservative mod policy, not a claim about original capacity. Couriers, farmers and Gardeners are protected. Unload requires idle/landed state, readable passenger vector and clear exact exit tiles. No new soldiers, resources or slave cargo are synthesized. The native passenger records and flight state are saved by the engine; sidecar schema remains unchanged. Captive acquisition/return and economic assignment depend on existing engine Shaman/AI behavior and remain unverified; this patch does not implement a new slave economy or promise automatic AI raids for the human player.

## Validation

`build-native.py`: x86 DLL and loader compile; economy, queue, placement, panel and aircraft model tests pass. Aircraft tests cover coordinate boundaries, recycled identity refusal, scoped routing/restoration and protected boarding types. Shared five-panel preview inspected offline. `verify-offline.py`: native instruction signatures/unload ring, executable/API/runtime hashes, imports/dependencies and disposable installer upgrade/refusal/restore pass. `test-startup.py`: disposable non-game loader fixture and independent host assignment pass.

No game started, controlled, captured, injected or inspected live. Actual map-click dispatch, flight/landing, native board/unload calls, passenger save/load and Shaman captives require the user checks in `TESTING.md`. The second-farm crash remains unresolved and separate.
