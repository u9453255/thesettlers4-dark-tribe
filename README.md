# Dark Tribe for The Settlers IV

A single-player mod for **The Settlers IV: History Edition** that makes the Dark Tribe playable. Mushroom Farmers deliver mushrooms for mana, and you spend that mana on Dark buildings (Mushroom Farm, Dark Temple, Fortress, Manacopter Hall), troops and Manacopters that carry units. The computer opponents attack you with their own soldiers and Gardeners: the soldiers clear your defenders, the Gardeners turn the ground around your buildings green until they collapse, Mushroom Farms first and your Temple and Fortress only once no farm is left. They attack early and in six ways (raids, full assaults, pincers, sweeps of Gardeners under escort, hunts for your army, and joint attacks of allied computers). Every style grows with the size of their army, and soldiers and Gardeners march together so the Gardeners stay protected. Lost armies are replaced from their own barracks, their toolsmiths make the shovels for new Gardeners, and their Gardeners steadily take back Dark land, first inside their own territory. Three scenarios:

- **First Dawn** (`00_Dark_Tribe_First_Dawn`): your Dark base against a Viking settlement in the west. The introduction.
- **Iron Kingdom** (`01_Dark_Tribe_Iron_Kingdom`): two Dark camps against a fully built Viking kingdom with seventeen manned towers, mines and weaponsmiths, whose army grows the longer you wait.
- **The Alliance** (`02_Dark_Tribe_The_Alliance`): one lonely Temple and an army, no farms, against Romans, Vikings and Mayas allied on a compact map. The hardest.

[![Dark Tribe: First Dawn gameplay video](https://img.youtube.com/vi/gOwhw-3DTcg/maxresdefault.jpg)](https://youtu.be/gOwhw-3DTcg)

▶ **[Watch the gameplay video on YouTube](https://youtu.be/gOwhw-3DTcg)**

The mod is a native plugin DLL (`native/`), a Lua scenario (`scenario/`) and Python/Node build tools (`tools/`). The repository contains no game files or binaries. You build the DLL and the scenario maps from your own copy of the game: they are generated from three of its campaign maps.

## Requirements

- Windows 10 or 11.
- **The Settlers IV: History Edition from Ubisoft** (Ubisoft Connect), your own copy, updated to the newest version Ubisoft Connect installs (`S4_Main.exe` 2.50.1516.0 when this was written).
- **[Settlers United](https://settlers-united.com/en/)**, installed on top of the game, with its HD patch. This is the setup the mod is developed and tested with.
- The mod checks these files by hash and refuses anything else (`tools/game_build.py`). If Ubisoft or Settlers United ship an update that changes them, the mod stops working until its pins are updated (see [Not working on your setup?](#not-working-on-your-setup)):
  - `S4_Main.exe` 2.50.1516.0 (x86) as patched by Settlers United (only the Large Address Aware flag set), SHA-256 `3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816`, or the unpatched original `f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b`;
  - Settlers United's `S4ModApi.dll` 2.3.2 and HD patch files (`Plugins/S4HD-Patch.asi`, `Plugins/S4HD-Manager.dll`) in exactly the pinned versions;
  - the original `GameData/buildingInfo.xml`, `Gfx` files and the campaign maps `Map/Campaign/dark03.map` (First Dawn), `Map/Campaign/AO_trojan10.map` (Iron Kingdom; part of the Trojans add-on included in the History Edition) and `Map/Campaign/dark07.map` (The Alliance), with no other mods changing them.
- The game folder must be writable by your user, because the mod adds its maps and a script there and swaps `GameData/buildingInfo.xml` (the original is backed up). The default Ubisoft location under Program Files is usually not writable. The original author copied the game to `C:\Projects\thesettlers4` and pointed Ubisoft Connect at the copy with "Locate installed game".
- To build: Python 3.10 or newer (3.12 tested), Node.js (24 tested), Git, and an internet connection for the one-time setup (about 200 MB download, 1 GB on disk).

## Build

```bat
git clone https://github.com/u9453255/thesettlers4-dark-tribe.git
cd thesettlers4-dark-tribe
python tools\setup.py --game "D:\Games\thesettlers4"
python tools\workflow.py
```

- `setup.py` saves your game folder in `local-config.json` (git-ignored). It downloads the pinned LLVM-MinGW compiler and S4ModApi 2.3 into `work\`, checks their SHA-256, installs the Python helpers from `tools\requirements.txt` into `work\python`, and reports whether your game files match the pinned build. Add `--node "C:\path\to\node.exe"` if Node.js is not on PATH. `--check` only reports. You can also set the game folder with the environment variable `DARK_TRIBE_GAME_DIR`.
- `workflow.py` builds the compatibility XML, the scenario maps and the DLL, and runs all offline checks (about 2 minutes). It reads the game folder but never changes it, and never starts the game.

## Play

1. Close the game. Run **`Launch Dark Tribe.cmd`**. It verifies all hashes, installs the scenario files, starts a small helper and then starts the game. The helper loads the plugin into the game (`runtime\native\inject.exe`); some antivirus tools flag this kind of DLL injection.
2. Stay in the main menu until the launcher window prints **Dark Tribe 0.2.15 ready**.
3. Choose Single Player, click the map picture, then Custom Maps. Start a new **`00_Dark_Tribe_First_Dawn`**, **`01_Dark_Tribe_Iron_Kingdom`** or **`02_Dark_Tribe_The_Alliance`** game. Keep player 1 human (Dark Tribe), all other players computer, and the races unchanged.

**Your citadel's fire field.** While you own at least one Mushroom Farm, your Dark Temple and Fortress burn enemy soldiers and Gardeners on Dark land within 18 to 24 tiles (more farms, wider field; the visible flames show less). The computers stay out of it and attack your farms first. Their turn for the Temple and Fortress comes when your last farm falls. A Mushroom Farm can't be built within 33 tiles of your Temple or Fortress: a farm there could never be attacked, and the field would never drop.

Always start the mod through `Launch Dark Tribe.cmd`. A normal game start does not load it. A save consists of the game's `.sav` (in `Documents\TheSettlers4\Save`) and a matching sidecar in `save-states\`. Keep them together.

## Uninstall

Close the game and run **`Restore original files.cmd`**. It restores the original `buildingInfo.xml` and removes the scenario maps and script. Saves and sidecars are kept. The executable is never modified.

## Not working on your setup?

The mod deliberately refuses any game build, file or plugin version it was not pinned to. Its engine addresses are only valid for that exact `S4_Main.exe`. If `setup.py` reports a problem, or the build, launcher or game fails:

Open this repository in [Claude Code](https://claude.com/claude-code) with **Claude Opus** and ask it to help you integrate the mod with your game. Include the output of `python tools\setup.py --check` and any errors. `AGENTS.md` and `CLAUDE.md` give it the project's rules and engine knowledge. Claude does the offline work (building, file-only disassembly, re-deriving pins). It never starts, inspects or injects into your game, so you run the in-game tests yourself (see `TESTING.md`). Porting to a different game build means re-checking every pinned address, which takes real work.

## More

- [TESTING.md](TESTING.md): in-game test steps for the current release
- [MAIN.md](MAIN.md): behaviour, installation details and limits
- [CHANGELOG.md](CHANGELOG.md): what changed for players, update by update
- [MODLOG.md](MODLOG.md): detailed development journal of every release and round
- [HANDOFF.md](HANDOFF.md), [AGENTS.md](AGENTS.md), [CLAUDE.md](CLAUDE.md): notes for continuing development
- Third-party code: [S4ModApi](https://github.com/nyfrk/S4ModApi) (LGPL, `runtime/native/COPYING*`, header in `third_party/`), the Settlers.ts map codec (MIT, `tools/vendor/`)
