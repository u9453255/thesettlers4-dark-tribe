# Settlers United and the Dark Tribe addon - 2026-10-12

The user installed Settlers United during the 0.2.10 construction repair and requested an assessment of its benefits. Inspection is limited to installed files, public documentation/source and offline fixtures. No game/process/module was started, inspected or injected, and no installed file, patch setting, save or backup was changed.

0.2.12 corrects a confirmed null-call startup defect in the in-progress 0.2.11 hashing path; the installed patch was not modified. See HD_STARTUP_HASH_0212.md.

0.2.11 follow-up: the user reports missing Dark menus under HD despite successful 0.2.10 economy initialization. The original file/API checks did not verify display compatibility. The owned panel repair and acceptance boundary are documented in HD_MENU_DISPLAY_0211.md.

## What is installed and what changed

The local plugin reports SettlersUnited 1.4.56.0, HD Patch 0.16.1.0, WarriorsLib 1.4.0.0, ExtraZoom 2.3.0.0 and Logger 1.4.1.0. These are file versions, not proof of the launcher's version or enabled options. `settlers-united-files.json` records their hashes and shared API imports. HD assets are present locally. Forge is a separate development framework; its installation is not established here.

S4_Main.exe remains x86 2.50.1516.0. Its original full SHA256 is `f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b`; the installed variant is `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`. The entire file differs by exactly IMAGE_FILE_LARGE_ADDRESS_AWARE bit 0x20 at offset 0x1A6 (0x102 -> 0x122). Clearing that bit in a temporary in-memory copy restores the exact original full-file hash. All instructions, data, RVAs and construction evidence therefore remain identical on disk. This memory flag does not fix the addon's incorrect mode-0 cancellation. SU's runtime plugins can still change code after launch; startup signature checks remain mandatory.

`game_build.py` admits only these two complete hashes, then requires the original normalized file hash. It does not accept arbitrary header or code patches. The build manifest records the actual hash, normalized hash and LAA flag. Tests reject a code change, unrelated header change, appended bytes and truncation. Nothing rewrites the game executable.

## Immediate integration: one shared API

SU installed S4ModApi 2.3.2.0, SHA256 `ed111491b1a6bd2822ccd21fec31adedfb35b6bf311cd568d9ff0e6e2193126f`. Our retained bundled 2.3.0.0 hash is `73482077644fd1fa8eadfaf9fa4d0ad2799f381713a3204988493e8261b26651`. Both expose the required API GUID and Lua exports and are x86. Comparing the public SU fork's header with our current SDK verifies the entire header is identical after line-ending normalization, including the same ordered 170 interface method declarations. Source comparison supports interface compatibility; it does not prove gameplay behavior of either runtime.

The loader now selects the exact known API installed next to the game when present. SU and Dark Tribe use that path rather than requesting the older bundled API alongside it. A missing installed API uses the unchanged bundled fallback. Unknown installed APIs, missing update protection and any change from the manifest-selected API are refused. The game's S4ModApiNoUpdate.cfg is retained. No SU API/plugin is overwritten or downgraded. Offline tests cover installed selection, absent-installed fallback and refusals. This is the concrete compatibility change in 0.2.10; the SDK/import library and mod ABI remain unchanged.

## Useful additions and future work

The [official SU overview](https://settlers-united.com/en/) describes S4 terrain/building bug fixes, input customization, zoom/hotkeys, optional speed/transport/settler modes, balancing and multiplayer protection. The [HD Patch project](https://settlers4-hd.com/) describes HD textures, tribe-specific UI themes, fonts, cursors and local team colors. They can improve presentation and controls; they do not establish native Dark Tribe construction/recruitment/transport support. Optional balancing changes ordinary tribes and should be recorded when evaluating the Viking opponent.

The [SU changelog](https://settlers-united.com/en/changelog/) documents Lua selection, building placement state, entity queries and experimental game-data access in library 0.7.0. Some changes require UBO-TEST/Tournament modes. Availability in this dedicated single-player map has not been observed. These are useful candidates for reducing our private selection/placement/data lookups after version/feature checks. The current addon already gets selection through S4ModApi; no claim is made that SU fixes Manacopters or supplies passenger/unload APIs. The [StartPlacement reference](https://docs.settlers-united.com/s4-lua-api-de/su-library-functions/su.buildings/su.buildings.startplacement) also converts some race-specific mana buildings, so Dark type behavior needs explicit validation before adoption.

The related [S4Forge API](https://github.com/Settlers4-Reforged/S4Forge) offers managed game access and a UX engine for UI/sound. It is a future alternative for richer panels, not a dependency added by this repair. Moving the existing native addon to it would require a separate implementation and user gameplay validation. No package was downloaded or installed for that purpose.

Keep the present corrected native construction controller, native Farmer delivery/mana, aircraft identity/passenger checks and save validation. Highest-value next work is to validate HD panel positioning/input with the user's chosen resolution and check optional SU APIs on ticks before replacing any native lookup. Existing native signature mismatches must disable startup, not trigger blind hook replacement. All gameplay checks are in TESTING.md.

## Research evidence and limitations

Queries: `Settlers United Settlers IV HD patch features github mod API`; `S4ModApi 2.3.2`; `site:docs.settlers-united.com SU Mana`; `site:github.com/Settlers4-Reforged S4ModApi`. Primary sources above were read on 2026-10-12. No dead-source archive was needed. The docs index request returned 403; the official changelog and accessible function reference were used instead. No screenshots or live feature tests were taken.

Private PE/export/source comparisons are under ignored work/native-analysis/settlers-united. Public SU header capture SHA256 and method comparison are recorded in settlers-united-api-source.json. Permanent file-only checks are tools/test-game-build.py and tools/inspect-united.py. Reports explicitly distinguish present files, documented capabilities and unverified runtime availability/compatibility.
