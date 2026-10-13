# CLAUDE.md

@AGENTS.md

The rules in AGENTS.md are binding. This file adds the practical knowledge needed to continue the work.

## Orientation

- Read `HANDOFF.md`, then `MAIN.md`, before you change anything. The top section of each describes the current release. Older sections are history only.
- Current release: **0.2.15**. Readiness marker: `Dark Tribe 0.2.15 ready`. Save sidecar schema 2. `DT_Version=200`.
- Game: `S4_Main.exe` in the configured game folder (`DARK_TRIBE_GAME_DIR` or `local-config.json`; see `tools/project_paths.py`), x86, image base 0x400000. Every address in the code is an RVA in this exact pinned build (`tools/game_build.py`). Another build is refused; porting to it means re-deriving every pin by disassembly, never guessing.
- The user does all in-game testing. Never launch, attach to, capture or inject into the game. Only read the existing logs in `evidence/`.

## Layout

- `native/dark-tribe.cpp`: the plugin. It contains tick, frame and mouse callbacks, economy, construction ops, aircraft, save hooks and startup guards.
- `native/*-model.hpp`: pure logic, unit-tested by `native/*-tests.cpp`. `build-native.py` compiles and runs these tests.
- `native/*-native.hpp`: pinned engine bridges. These are `construction-native.hpp`, `placement-native.hpp` and `hd-overlay.hpp`.
- `tools/*_pins.py`: list the signature regions. `build-native.py` generates `work/native-generated/*-signatures.hpp` from them, and the DLL checks these at startup and fails closed on any mismatch.
- `tools/inspect-*.py`: file-only disassembly checks with capstone/pefile from `work/python`. Add one whenever new engine facts are relied on.
- `tools/project_paths.py` / `tools/project-paths.cjs`: the only place that knows the game folder and toolchain locations; `tools/setup.py` (one-time setup and pin report) and `tools/workflow.py` (offline workflow) use them. `Launch Dark Tribe.cmd` takes the game path from `runtime/manifest.json`, i.e. the game the build was verified against.
- `work/`, `backups/`, `evidence/`, `runtime/native/*.dll`, `save-states/` and `local-config.json` are ignored and private. Never publish them.

## Construction (0.2.14)

Successful construction completes the **same native site in place**. Nothing is deleted or recreated. The 0.2.13 approach (mode-1 cancel, then `BuildingsAdd`) failed because units walked into the released footprint.

On payment, the mod:
1. Freezes `CBuildingSiteRole` at phase 1.
2. Detaches the ordinary requests for Temple and Fortress (slot 11).
3. Applies the reservation (RVA 0x136470) once.

It then draws 30 ticks of progress and completes the site with the native sequence for its kind:
- Farm and Hall: the grow-path phase-8 steps.
- Temple and Fortress: RVA 0x1424C0 and 0x1429F0 when the site has an economy. Without one, the mod removes the material piles itself and then runs the phase-8 steps.

Wild animals (class 0x80, owner 0; Dark-land critters) never block completion except on the anchor tile (native checks occupancy only before claiming cells). The mod waits while units stand on the site and every 20 ticks orders its own idle units off the footprint (`evict`), because idle units never leave reserved cells by themselves. A static blocker gets one refund, then the site is cancelled with mode 1. Mod-issued moves set `internalSend` so the send callback ignores them.

Once a site is reserved, never re-check the placement flag bits on its own cells. When an object is created next to the site (for example a mushroom a Mushroom Farmer plants), RVA 0x163510 sets keep-clear bit 0x40 on the surrounding cells, including the site's own. Re-checking that bit refunded growing Farms. Check only occupancy, as native phase 7 does. Log a block reason before you refund or cancel, because cancelling changes what the log can see afterwards.

`research/CONSTRUCTION_INPLACE_0214.md` documents every native field and RVA, and `tools/inspect-construction.py` checks them. Before you change this code, re-verify each native behaviour you rely on by disassembly. Do not guess.

The AGENTS.md bullet about "Foundation conversion uses Lua cancellation mode 1 ... original-identity disappearance" describes the replaced design. Mode 1 is still used, but only for refusals and refunds.

## Performance and input

- Read memory through `dtmem` (`native/fast-memory.hpp`). Inside `tick()`, a `dtmem::Scope` caches queried memory regions. Call `dtmem::invalidate()` after any native or API call that can free memory.
- Never add a raw `VirtualQuery` per read: a single uncached scan of 65,536 entity slots costs about 40 ms.
- The HD renderer does not deliver the old draw (blt) callbacks, so sprite hit-testing doesn't work under HD. Resolve map-targeted actions through the `send` callback's map position instead (for example, boarding when units are moved onto an aircraft).
- Construction start must not repeat the placement-flag test. The game's site setup changes those flags on its own cells.

- Manacopters cannot be put in native selection: their selection group bits (low 4 bits of `+0x14`) are 0. The mod keeps its own `fleetPick` group, resolved on the tick through the projection (RVA 0x259910). Before adding a command number, check `panel.hpp` `hit()` for ones already in use.
- Construction drawing: native draws a site only from a map-layer anchor. Grow sites (Farm/Hall) are anchored in the object layer. Ordinary sites (Temple/Fortress) are anchored in the building layer at native phase 4, which the mod mirrors.

- Native UI and order code often assumes an ordinary settler economy at a building's entrance, and Dark land has none. Before letting players interact with a native object, look for `[edi*4+0x146c8a4]` (economy table) reads. Examples are the site panel (slot 22) and pause (slot 8).
- Passengers must go through the native arrival sequence: mode +0x42 = 0xA, then scheduler removal at RVA 0x16D720, then board at RVA 0x16B890. Calling RVA 0x16B890 alone leaves the unit walking on the map.
- Fog: the fog manager (RVA 0x1159D30) keeps a byte map at +0xC. Bits 3-5 are pending visibility; bits 0-2 are the current level, which is merged into land byte 3. For temporary unit vision, call RVA 0x1AE100(x, y, radius) every tick, as the native band pass does.
- Never use slot 7 (sets the current level permanently) or slot 15 (appends persistent saved entries). Never clear land byte 3 bits around units.

- The game sends no move order (the `send` callback) when its own selection is empty. Orders for mod-only selections, such as Manacopters, must come from the mouse callback and be resolved on the tick.
- The game must be closed to rebuild: a running game locks `runtime/native/DarkTribe-*.dll`. Never stop the user's game; ask them to close it.

- Selection brackets come from each entity's draw method (CSettler slot 5 fills the draw descriptor at RVA 0x129ADFC when flag 0x200 is set). Manacopters have no such path, so the mod draws `dtui::markerPixels` itself (`dthd::Markers` under HD). Never set 0x400 on aircraft: it triggers a virtual call (+0x94) that does not exist in the Manacopter vtable.

- The HD host thread must survive the game window being replaced: `Host::run` re-attaches, and it logs `HD_PANEL` show/hide reasons. Keep both when changing the overlay. Manacopters always carry flag 0x8000, so "hidden" means class 1 with 0x8000 only.

- UI: the Dark menus use the game's own sprites, loaded read-only from the game's `Gfx` folder by `native/gfx-assets.hpp` with per-range SHA-256 pins from `tools/gfx_pins.py` (add a sprite there, then to `dtgfx::Art`). Layouts copy the native menus in `Menu/GUISetRomans.DAT` (`python tools/guiset.py Romans <menu>` lists them; format notes in that file). Draw through `panel.hpp` (`background`, `title`, `separator`, `iconButton`, `action`, `portrait`, `picture`, `vbar`, `label` with fonts F1..F14) so every element has a drawn fallback when the art is unavailable. Resolve settler sprites by animation name through the engine table (RVA 0xC00974, job = id - 1), never by eye. Never commit game-derived images: previews with real art go to `work/previews/`. Never draw pure black (the HD colour key). The Dark unit panel lives in `native/unit-panel.hpp`; its commands are 40..49, handled in `unitCommand` on the tick.
- Mouse: the S4ModApi mouse callback never sees WM_MOUSEMOVE. Always pass the game a right release whose press it received (its pan state is set on press and cleared only on release); decide click vs drag with `dt::rightClick` (4 px, the game's own pan threshold).
- Text: `panel-tests` checks every label per glyph against the measured frame bounds (`InnerX0..InnerY1`, title box) in both art modes; keep it at zero failures when changing layouts. Temple/Fortress recruitment commands are 21 (select, UI only) and 22 (order with amount; 100 = the game's own continuous recruitment).
- Layout: Dark menus cover y 210..599 (panel #589) and the mana section covers the native resource panel y 600..767 (#587) whenever the plugin is active; the HD canvas is 200x768. Construction allows one Dark Temple and one Dark Fortress (`buildLimit`, enforced on command 1 and on new sites). Aircraft panel command 37 keeps one aircraft.
- Ticks stop in the menus: never rely on tick-cached scene state alone to show UI. Gate every display/input path with `inGame()` (frame callback screen check, tick fallback).
- Player texts: `say()` only for failures, in player language, never versions, log paths, coordinates or internal states; notices vanish after `NoticeMs`. Keep the scenario script silent on load.
- Never put a `//` comment inside the long single-line statements in `dark-tribe.cpp`; it silently comments out the rest of the line. Use `/* */`.

## Build and verify (offline only)

Run the full list in `HANDOFF.md` ("Primary offline workflow") with one command:

```bash
python tools/setup.py --check   # toolchain, game pins and config; without --check it downloads/installs what is missing
python tools/workflow.py        # every offline build/check step in order; stops at the first failure
```

Node.js comes from `DARK_TRIBE_NODE`, `"node"` in `local-config.json` or PATH (record one with `python tools/setup.py --node <node.exe>`). When running single steps by hand, call the `.cjs` steps with that node; `verify-offline.py` finds it the same way.

- The compiler is `work/native-sdk/compiler/llvm-mingw-*/bin/i686-w64-mingw32-clang++.exe` (`project_paths.COMPILER`), and `build-native.py` invokes it. `tools/setup.py` downloads it with the hash in `research/compiler-download.json`.
- Pins must end on instruction boundaries and must not cut a relocation. The pin generator raises an error on a truncated relocation.
- When you run disassembly helper scripts, do not name them after stdlib modules. A script named `dis.py` breaks capstone's import.

## Releasing a version

Bump every version and readiness marker together. These appear in the native sources, the tools, the scenario, the HD panel class name, `startup-status-0XYZ.json`, `V0XYZSessionHelper`, and README/MAIN/TESTING/HANDOFF. `verify-offline.py` enforces them.

Each release also needs:
- a new MODLOG entry;
- a research note;
- a TESTING.md section with the user's test steps;
- staging with `tools/stage-release.py`. This never touches the live game; the user activates the release with `Launch Dark Tribe.cmd`.

Do not commit, publish or message anyone unless the user asks.
