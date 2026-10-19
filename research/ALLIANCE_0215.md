# The Alliance: third playable Dark Tribe scenario, 512 maps and four players (0.2.15 round r25)

Date: 2026-10-14. Offline work only; no game was started, inspected or changed. Gameplay is unverified until the user tests it (TESTING.md, r25). Map choice survey: `research/IRON_KINGDOM_0215.md`.

## Source map

`Map/Campaign/dark07.map` ("Helping Hands", Dark campaign mission 7; SHA-256 `7d95a9fa...b403`), 512x512, one land mass, a ring of Dark land (28 % of the map) around a central settlement:
- player 1 Mayas (the campaign's human start, centre): 2 large and 2 small towers, 4 lookout towers, a stonecutter, 63 units, 21 goods piles;
- player 2 Romans (south-west) and player 3 Vikings (north-east): computer settlements with towers, woodcutters, sawmill, forester, stonecutter and waterworks (13 and 14 buildings);
- player 4 Dark Tribe: one Temple (332,210, between the Mayas and the Vikings) and 109 units (68 warriors, 17 throwers, 16 gardeners, 8 Shamans), most of them in the south-east; no farms.

All computer units and entrances stand on their own initial land; the Dark Tribe owns none. The campaign script (connect territory to the allies) is replaced.

## What `tools/build-alliance.cjs` does

1. Players reordered to Dark Tribe (1), Romans (2), Vikings (3), Mayas (4); the player count stays 4. Slot/team table: human Dark player 1 in team 1, the three computer peoples in team 2 (the First Dawn pattern extended).
2. Owners of units, buildings and initial land follow the players (1 <-> 4).
3. Towers topped up to at least 2 swordsmen + 1 bowman (small) or 3 + 3 (large); authored level 2 soldiers stay (19 towers reinforced; lookout towers unchanged).
4. 12 soldiers per computer people (6 swordsmen L1, 3 L2, 3 bowmen L1) on free, owned grass next to its start (same rules as Iron Kingdom: no object, no 0x5B flag, not under any building footprint of that race, not on a unit, not Dark land). `scenario-footprints.py` now emits footprints and doors of every race (`_races`, `_raceDoors`). After the armies, 4 authored gardeners (settler type 50) per computer people on cells found by the same search, because the native AI never recruits gardeners and the plugin's offence director needs them to take back Dark land: Romans (100,351) (96,352) (100,352) (96,353), Vikings (369,114) (365,115) (369,115) (365,116), Mayas (264,273) (267,273) (269,273) (270,273).
   r28 (`tools/scenario-supply.cjs`, start content), per people: a barracks of its race (Romans (122,364), entrance (126,371), 10 objects cleared; Vikings (367,103), entrance (365,105), 1 object; Mayas (299,272), entrance (298,274), 3 objects; as the map's own buildings, the blocked core of each new barracks is set to its hotspot height (Romans 5, Vikings 3, Mayas 2; 55/55/32 heights changed, slope shading recomputed, the step to the land around 10/4/3 against the map's own maximum 11), 32 weapons next to it as piles of 8 (Romans SWORD x2 + BOW x2; Vikings SWORD x2 + BOW + BATTLEAXE; Mayas SWORD x2 + BOW + BLOWGUN), SHOVEL x2, and 40 carriers after the gardeners (the peoples have no residence). Chunk 9 now changes (source records kept as the prefix); `test-alliance.cjs` checks every new record.
   r29 (start content for the plugin's shovel orders, `research/AI_OFFENCE_0215.md` section r29): no people had a toolsmith, so each gets a TOOLSMITH (map type 21) of its race beside its barracks, placed and levelled exactly like the r28 barracks, appended after every unit and r28 pile (their positions stay): Romans (137,383), entrance (139,385), core at height 11, step 7, 1 object cleared; Vikings (382,96), entrance (383,98), height 7, step 6, 42 objects cleared; Mayas (298,285), entrance (301,287), height 2, step 6, 6 objects cleared (map maximum 11; the cleared objects are probably trees). Beside each entrance: IRONBAR x16 and COAL x16 in piles of 8 and HAMMER x2 (Romans (144,385), (139,379), (145,387), (132,378), (132,380); Vikings (388,99), (377,92), (388,97), (389,101), (389,103); Mayas (304,288), (305,287), (305,290), (301,282), (306,289)). No carriers added (70..72 each, 2..3 smiths). `test-alliance.cjs` checks owner, level ground, clearing, goods, totals and pile distance (17 broken supplies rejected).
5. Script `scenario/DarkTribe_Alliance.txt` (150 starting mana because no farm exists; the three AIs at the aggressive end of the shipped values; victory when no tower or castle of players 2..4 remains), briefing and tips. Landscape and every other chunk byte-identical (r28: except the heights and slope shading under the three new barracks; r29: also under the three new toolsmiths).

No terrain preparation: with the stricter offline rule the Dark land accepts 6,827 Farm, 25 Temple, 14 Fortress and 29 Hall anchors, the large ones in the south-east near the Dark army (x 320..383, y 320..447). The native halo rule (flags & 0x1F == 4) accepts more. A First-Dawn-style clearing was considered and not made: the candidate sites would need height steps of 23+ (First Dawn's were up to 12).

## Plugin: map size and player count

- Map size: the plugin used 576 as the map stride everywhere. Engine facts (pinned in `tools/inspect-placement.py`): RVA 0x1054138 is the row stride the Dark validator (0x13EC48) and its neighbourhood pass (0x13DCAB) use for the landscape (0x11630D4) and flag (0x11630D8) arrays; the view setup 0x114B90 calls 0x2589D0(stride, landscape), which stores the stride at 0xD6921C and the landscape at 0xD69220. `native/map-size.hpp` (`dtmap::size`) returns the stride only when both agree and it is a supported size (512 or 576); every former 576 in `dark-tribe.cpp`, `construction-native.hpp` and `placement-native.hpp` now uses it (entity map, flags, landscape, fog, hint cache grid, view clipping, aircraft bounds and group targets). Saved coordinates are bounded by 576 (`MaxMapSize`).
- Fingerprint: `native/scenario-model.hpp` lists every scenario as map size, player count and race/start per player; `dt::sceneMask` compares them with the session setup (player count +0x338 and the nine-entry arrays of slot type +0x90, race +0x18C, start +0xFC/+0x120) and the view size. Player 1 must be the human (slot 0), every other player a computer (slot 1). Same bits as before (4 size, 16 count, 32 slots, 64 races, 128/256 starts). `native/scenario-tests.cpp` tests each part.
- Launcher helper: `inspect-runtime.py` reports all players and the map size; `activate-human-slot.py` matches the whole layout from `tools/scenarios.py` and requires slot 0 for player 1 and 1 for the others.

## First test: crash while loading (fixed)

PID 8116: the helper logged `human-slot-assigned` at 02:33:45.67, after the HD patch's `New map started` (02:33:44.78); the HD log ended with `Current Tribe: 255` and the game trace with an unhandled CLR exception (0xE0434352) from S4HD-Patch.asi's tick path at the first GameLogicTick. The helper had refused the layout in the lobby (`Wrong scenario layout` at 02:33:36) because r25 also compared the view map size (RVA 0xD6921C), which the engine writes only while the map loads (0x2589D0). Without a human actor the map started with no local tribe. The helper now ignores the map size and names the refused layout in its log.

## Evidence boundary and open points

- Offline: map decode/round trip and `tools/test-alliance.cjs`; Lua under Lupa (victory needs every tower of all three peoples gone); native fingerprint and aircraft/economy model tests at both sizes; inspection of the stride facts; installer upgrade on a temporary copy.
- Confirmed by the user after the r25 helper fix (2026-10-14): the map loads with four players and the human slot ("Nice, now it worked."); the user's r27 and r28 sessions (pids 5672, 15576) also show all three computer players active.
- r29 not verified: that the new toolsmiths become ready buildings with a smith and make the shovels the plugin orders; iron bars and coal are finite (16 each per people) unless the AIs mine and smelt. Staged 2026-10-19.
- Not verified: that the runtime slot types for players 2..4 read 1 (if not, `SCENE_STATE mask=020` and the launcher log show it); that the three AIs build, ally and attack; that everything that worked on 576 maps (placement markers, construction, Manacopters, fog) also works on 512. The `S4ModApi` calls the plugin uses take map coordinates and are size-independent.
- The game's Lua is a 3.x dialect without `for` loops (no shipped map script uses them); the scenario scripts avoid them and `verify-offline.py` refuses them.

## r31: clean start

The source has no Dark farm, so `tools/build-alliance.cjs` removes nothing. It still runs the shared check (`tools/citadel-fire.cjs`), and `tools/test-alliance.cjs` rejects a farm added beside the Temple (Euclid 20 or 32) and accepts one at Euclid 33.
