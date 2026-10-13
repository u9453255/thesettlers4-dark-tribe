**Integration update, 2026-10-12:** included in the primary combined 0.2.4 package with the other fixes. The isolated-candidate notes below record the earlier investigation. Rebuilt/staged artifacts and final user checks belong to the primary project; gameplay remains unverified.

# Fortress research and construction repair — 2026-10-12

Scope: isolated repair candidate based on 0.2.2, not a new activated release. The primary source matched this checkout at investigation start (`8afd2c2`). No game launch, capture, control, live-process read or game injection occurred. All runtime acceptance belongs to the user.

## Original purpose and interface

The original [English manual, chapter 18, printed pages 53–54](https://manualmachine.com/gamespc/thesettlersiv/1119087-user-manual/) describes the Dark Temple as the Dark Tribe's headquarters. Mushroom Farms provide mana for its army; the Temple produces Shamans who enslave enemy settlers for farm work. Its magical flame protection is sustained by farms. The [German original manual, chapter 18, printed pages 55–56](https://www.mogelpower.de/manuals/Die_Siedler_IV_Handbuch.pdf) corroborates these mechanics.

The original Dark Tribe was controlled by the computer, not offered as a playable race (English manual printed page 6; [Ubisoft's History Edition description](https://www.ubisoft.com/en-gb/games/the-settlers-iv-history-edition) lists the four ordinary playable races). No original human Fortress recruitment/production interface is established by these sources. The playable mod's recruitment panel is therefore a new control interface for original native functionality, not a reconstruction of a documented original human panel.

**The Fortress-specific role comes from local original game data**, not an assumption from its name. The verified original XML assigns both Fortress and Dark Temple `HOUSE_KIND_DARKTEMPLE`. Their shapes/hotspots differ. The pinned executable's native role recruitment table is `[0, 53, 52, 54, 29, 32]`: unused, Mushroom Farmer, Dark Gardener, Shaman, Warrior, Thrower. These are the existing six role queues. Automatic farm-worker recruitment remains native; the four manual unit queues retain their current +/−/clear controls and native mana charges. No evidence supports adding weapons, goods manufacture, aircraft assembly or ordinary tower garrison behavior to Fortress. Aircraft remain the separate Hall's responsibility in this mod.

Fortress's current delivery-base behavior belongs to the explicitly requested mod economy. The manuals describe native mana production, not this mod's courier inventory. It is retained without claiming it is original delivery behavior.

## Construction crash evidence

Read existing `Trace020.txt` from the user's `Documents\TheSettlers4\Log`: local time 2026-10-12 13:42:12, exception `0xC0000005`, runtime image base `0x00E80000`, fault `0x00F5428A` → RVA `0xD428A`, ECX zero, EAX `FFFFFFFF`, invalid read `0x12E`. Stack return RVA `0x143DE0` follows the construction-panel goods query. The separate user report identifies constructing Fortress as the trigger. This trace is an unhandled crash record, unlike the mod's general first-chance log.

Static analysis of the hash-pinned executable establishes this narrow path:

- XML tool parsing stores a signed-byte enum at building-definition offset `0x1DF`. Empty input returns −1 from the goods enum resolver (failure path RVA `0x228A23`). `GOOD_NO_GOOD` is the explicit enum zero.
- Fortress alone among the four original Dark definitions has an empty `tool` element. The other three explicitly specify `GOOD_NO_GOOD`. The installed compatibility data retained Fortress's empty value.
- The construction panel reads that field at RVA `0x143DC8`. Zero skips the goods query. Every nonzero value, including −1, enters it. The query's faulting operand at RVA `0xD428A` uses `ECX + EAX*2 + 0x130`; logged ECX=0/EAX=−1 yields exactly `0x12E`.

The source of the null economic-sector pointer itself is not established. Explicit no-tool removes this unnecessary invalid query for Fortress; it is a concrete correction matching the observed signature, not proof of complete gameplay crash resolution or a diagnosis of the earlier second-farm report.

## Implemented correction

`tools/build-compat.py` now writes only Fortress's empty tool as `GOOD_NO_GOOD`, while retaining original byte content outside this element and inserted compatibility definitions. It asserts all native Dark definitions against expected values; the only permitted Fortress difference is this field. Unexpected nonempty tools are refused. Generated reports identify the exception explicitly and reset historical `runtime_verified` claims. Known prior mod installation hashes are accepted when regenerating from the verified original backup; unrelated XML and damaged backups remain refused. Generation is idempotent.

The cached selection data distinguishes construction sites and verified native recruitment roles. A Fortress foundation displays construction status and has no recruitment or demolition click targets. A completed base with an unavailable/unexpected role shows unavailable status instead of fictitious zero queues with active controls. Native manual queue operations also require an owned completed building of the correct type/role. Lua and gameplay mutation remain on ticks; rendering/input continue using cached data and nonblocking state reads. Original Fortress role, native worker recruitment, masks, hash guards, entity identities, economy transactions and save schema remain intact.

## Offline verification and handoff

Passed `python tools/inspect-fortress.py`, `python tools/test-fortress.py`, `python tools/build-native.py`, `python tools/verify-offline.py`, `python tools/test-startup.py`, and `git diff --check`. These cover the pinned parser/panel/fault operand, role table, exact data exception, native geometry preservation, other-race bytes, idempotency, unexpected enum refusal, economy/queue/placement tests, Foundation/Fortress panel click guards, runtime hashes/dependencies, installer upgrade/unrelated-change refusal/original restoration, and the disposable non-game loader.

Reports: `fortress-native-checks.json`, `fortress-offline-checks.json`, `building-compatibility.json`, `ui-queue-checks.json`, `v02-offline-checks.json`, `startup-regression.json`. `art/panel-preview.png` and `art/fortress-preview.png` are offscreen illustrative previews, not game screenshots.

Ignored local binaries/generated XML and the local upgrade manifest are prepared in this worktree. The primary project and live installation were not changed. Integrate the source commit into the primary project, regenerate compatibility data, rebuild and stage with the current release owner before user activation. No independent version bump is made in this isolated candidate; all existing version/readiness markers remain 0.2.2 until coordinated release integration. Cherry-picking Git files alone does not bring the ignored XML/binaries/install manifest. Follow the dedicated Fortress checks in `../TESTING.md` after integration.
