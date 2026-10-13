---
kind: game
title: Playable Dark Tribe single-player scenario
game: The Settlers IV
games_also: []
game_version: 2.50.1516.0 History Edition
platform: windows
engine: native
route: data
tools: [Python 3.12, Node 24, Settlers.ts, universal-modder 0.2.0]
anti_cheat: Not encountered in the tested offline single-player route; online mode untested
status: working
agents: [Codex (GPT-6)]
humans: []
date: '2026-10-11'
links:
- https://github.com/tomsoftware/Settlers.ts
- https://github.com/nyfrk/S4ModApi
- https://www.drechselhuber.de/editor.htm
- https://docs.settlers-united.com/s4-lua-api-de/api-enums/buildings
tags: [dark-tribe, scenario, map-format, native-session]
---
# Playable Dark Tribe single-player scenario

A dedicated scenario puts the local human in control of native Dark Tribe troops, eight original farms and a temple. Base-game Lua adds timed recruitment, a once-only mushroom-farm expansion, command recruitment and victory conditions. Real-game selection, movement, construction, recruitment and save persistence passed; this is a scenario prototype, not general race selection.

## Setup

Ubisoft History Edition 2.50.1516.0 on Windows x64; native x86 executable SHA256 f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b. Python 3.12 handles installation and a narrow session assignment helper; Node 24 builds maps. MIT Settlers.ts commit 2cb73d13d363f0f40b5360b565f19f03775a3098 supplies the codec. S4ModApi commit bb5d40ead1bb2a111ec0f787a6da5c15e130684e supplies published layout/API references; its DLL was not installed. Preserve the existing rendering wrapper.

## Route and why

Local data conversion plus base-game scripting avoids requiring a loader. A narrowly guarded helper assigns an imported map's already-human actor to the offline local host; the executable file is untouched. Native race UI and full production remain future work.

## How the game works (what we had to learn)

Dark race is ID 3. Player records are 45 bytes, settlers six bytes, buildings 20 bytes. The eight-byte map header and encrypted 24-byte chunk headers use little-endian integers. Swap the campaign's two player records and owners; replace the Lua/briefing chunks and prepare valid recruitment land. Missing ordinary placement definitions crash Dark construction initialization; add missing Roman entries while preserving all original Dark entries.

Imported campaign maps can retain actor UID FFFFFFFF despite showing a human slot. This build's player initializer then leaves localPlayer zero. Assigning the first actor UID to host zero before loading resolves that imported-map failure. The helper checks executable hash, scenario layout, human/AI types and local host. Only one four-byte session value is written; no native instructions are patched.

For read-only evidence, entity pool index is the ushort at entity offset 8, not the DWORD at offset 4. The latter differs for later-created entities.

## Build steps

1. Back up local game data and profile. Locate the game through Ubisoft Connect after moving it to a writable folder.
2. Decode the user's campaign map and validate checksums/round trips.
3. Swap player/ownership records, prepare the clearing, embed original Lua and briefings.
4. Generate missing Dark placement definitions from original local XML. Keep generated game-derived assets private.
5. Install with a manifest of original/installed hashes. Start through the session-helper launcher and keep the expected human/AI settings.
6. Restore original files through the manifest after closing the game when removing the mod.

## Verification

The actual game, GPU window screenshots, native read-only observations and game logs are the oracle. Twelve original maps passed byte-identical reassembly and metadata recompression. Eight selected Dark units moved. Native farm count rose eight to nine through the command; a native worker and building appearance were observed. Native recruitment counts increased, with explicit credits. Loading a save preserved nine farms, the once-only expansion flag and a nine-warrior/two-thrower timed wave. Isolated test maps removed the enemy tower or own temple; the unchanged victory handler showed victory and defeat respectively. Restore/reinstall was verified. Full ordinary mission completion, balancing, online modes and other builds were not tested.

## Gotchas

1. Managed scan result: it identifies the configuration utility. Inspect the actual game binary; use the native route.
2. Copied executable relaunches the old installation: Ubisoft's registered path wins. Use Locate installed game and verify the actual process path.
3. Imported map crashes: unassigned human actor leaves localPlayer zero. Assign the expected already-human slot before loading; keep exact build/scenario guards.
4. Repeated-start guard rejects valid setup: localPlayer can remain one after leaving a previous game. Accept zero or one only with the full expected setup signature.
5. Ordinary building placement divides by zero: Dark lacks placement data. Preserve its four native definitions and add missing Roman definitions.
6. Units fail to spawn: candidate terrain is unsuitable. Prepare valid land and confirm actual native unit creation.
7. Scripted building placement on occupied land can crash. Check nearby units, goods and all building states, including foundations, before using the base-game creation call. Use a fixed expansion pad separated from recruitment.
8. Observation misses later troops: entity DWORD offset 4 is not the pool index. Read ushort offset 8.
9. F6 is not quicksave. Use the Save menu and verify the save file and actual reload.

## Assets

No new art/audio was needed. Native owned-game assets are used locally. Generated map/XML and native analysis are kept outside version control. MIT codec license is retained. Game-only audio accompanies a short local recording.

## Cost and time

One local development session on 2026-10-11. No paid asset generation was used.

## Open questions

Full Dark construction UI, additional production, map balance, ordinary race selection and other builds remain unverified. The note is saved locally; no public contribution has been sent.
