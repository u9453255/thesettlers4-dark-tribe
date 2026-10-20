# Iron Kingdom: second playable Dark Tribe scenario (0.2.15 round r24)

Date: 2026-10-14. Offline work only; no game was started, inspected or changed. Gameplay is unverified until the user tests it (TESTING.md, r24).

## Choice of source map

The user asked for a second Dark Tribe map with a serious challenge. A file-only survey of every map in the game folder (decoded with `tools/map-format.cjs`) found 47 maps with a Dark Tribe player. 31 ship with the game (Campaign/Singleplayer); the "SP-DV-DarkMirror" maps in the user's Singleplayer folder were added later (community campaign) and are not used.

Constraints that decided the choice (see the inventory in this note's last section):
- the plugin and tools assume a 576x576 map, two players, player 1 the only Dark Tribe player;
- the computer opponent needs a real settlement on its own land (First Dawn had to author one because dark03's Viking start was the campaign's human start pack);
- the opponent must reach the Dark base by land, or there is no pressure.

`Map/Campaign/AO_trojan10.map` (Trojan campaign, mission 10; SHA-256 `05f34cbb...b122`) fits all of them:
- 576x576, one land mass for every settlement;
- player 2: a complete computer Viking kingdom (82 buildings: 11 small and 6 large towers, coal/iron/gold mines, smelters, two weaponsmiths, toolsmith, farms, bakeries, mill, ranches, 25 decorations; 129 units), every building and entrance on Viking land (30,832 owned cells);
- players 3 and 4: two Dark Tribe camps (Temple + 5 farms; Temple + 6 farms + Manacopter Hall), 156 units in total (71 warriors, 56 throwers, 29 Shamans);
- player 1: the Trojans' campaign start (one small tower, 71 units, 24 goods piles, 3,500 owned cells) in the south-west.

Other candidates and why they were not first: `dark07` (1 vs 3 alliance, the hardest) needs map size 512, four players and Roman/Maya support; `MD_roman3` (siege of a full Roman empire) needs size 640 and Roman support; `dark05` has weak opponent economies; `dark12` is 896 with an unreachable island player. The island maps (`Flying Horror`, `AO_trojan09`, `Golden Land`) leave the computer unable to reach the player.

## Map format facts used (decoded and round-tripped offline)

- Chunk 1 (24 bytes): u32 at +4 = player count, u32 at +12 = map size. +20 is 0x40 in Trojan add-on maps and 0 in dark03; kept.
- Chunk 2: 45-byte player records (race u32, start x u32, start y u32, 33-byte name). Header +20 = record count.
- Chunk 3 (slot/team table): 33 zero bytes, then (slot, team) per player; length 33 + 2n, header +20 = n. dark03 (and therefore First Dawn) is `00 01 | 01 02`: player 1 slot 0 team 1, player 2 slot 1 team 2. AO_trojan10 is `00 01 | 00 02 | 01 02 | 01 02`.
- Chunk 7 units (6 bytes: x, y, type, owner), chunk 8 buildings (20 bytes: x, y, type, owner, garrison flag at 6, counts of soldier types 29..37 at 7..15), chunk 9 goods (8 bytes: x, y, good, amount, 2 padding bytes); header +20 = record count.
- Chunk 6 byte 1 = initial owner (seeds native sectors before buildings load). Chunk 10 (83 bytes) is identical in dark03 and AO_trojan10 and was kept.
- Chunk 16 = Lua script, 11/14 = briefing, 12/15 = tips (AO_trojan10 has empty ones; its campaign texts come from the string table).

## What `tools/build-iron-kingdom.cjs` does

1. Players: the Dark record of player 3 becomes player 1 (start 378,134, next to its Temple), the Viking record stays player 2 (475,353). Player count 2; slot/team table exactly as First Dawn's.
2. Owners: 3 and 4 become 1, 2 stays, 1 (Trojans) is removed with its units, tower, initial land (owner 1 -> 0) and its 24 goods piles. The 16 Viking goods stay.
3. Viking towers: garrisons as in First Dawn (small 2 swordsmen + 1 bowman, large 3 + 3), up from one swordsman each.
4. A Viking field army of 24 (12 swordsmen L1, 6 swordsmen L2, 6 bowmen L1) on free, owned grass next to the Viking start: no object, no blocking/keep-clear flag (mask 0x5B), not under any building footprint (complete Viking masks from the original XML), not on another unit, not Dark land.
   After the army, 8 authored Viking gardeners (settler type 50; the native AI never recruits gardeners, so the plugin's offence director needs them to take back Dark land) on cells found by the same search: (473..478,350), (472,351), (478,351). The Vikings now start with 161 units (`research/iron-kingdom-checks.json`).
   r28 (`tools/scenario-supply.cjs`, start content): a Viking BARRACKS at (441,358), entrance (439,360), so the existing weapon chain can turn into soldiers natively (the AI recruits at every ready barracks and orders weapons at its weaponsmiths); 32 starter weapons (SWORD x2 at (472,352), (478,353), BOW at (478,355), BATTLEAXE at (477,356)) and SHOVEL x2 at (471,349), (473,349), piles of 8. Nothing needed clearing at the barracks site. As every authored building of this map, the barracks stands on level ground: its whole footprint (112 cells) is set to the hotspot height 3 (102 heights changed), the slope shading (low 4 bits of landscape byte 2, 8 + (height above - height below)/2, the rule every source cell follows) is recomputed next to the changed heights, and the step to the land around stays within the map's own maximum (9 against 12). `test-iron-kingdom.cjs` checks every new record.
   r29 (start content for the plugin's shovel orders, `research/AI_OFFENCE_0215.md` section r29): the kingdom's own Viking TOOLSMITH (map type 21) at (523,463), entrance (524,465), stays unchanged and gets IRONBAR x16 at (529,465), (518,459) and COAL x16 at (530,467), (530,469), piles of 8, appended after every r28 record. r40: HAMMER x2 beside the entrance as well (r29 left them out because the kingdom has 15 hammers and 2 smiths; field pid 8520: the toolsmith lost its smith at about tick 22,500 and never got another, the AI's "waiting for its smith" task cleared the shovel order 17,519 times, and no shovel was made for 95,000 ticks although 16 iron bars and 63 coal were in stock; a smith needs a carrier and a hammer, and the toolsmith is the only hammer maker). No carriers (105 carriers, 2 smiths). Only chunk 9 (piles) changes; no ground was touched. `test-iron-kingdom.cjs` checks the toolsmith, the goods and totals and that each pile lies within 16 steps of the entrance (15 broken supplies rejected). The two weaponsmiths at (522,476) and (529,456) use the same iron bars and coal and may take part of this buffer.
5. One Viking priest of the campaign story stood between the Dark camps (310,210) on unowned land; it is moved into the kingdom.
6. Script `scenario/DarkTribe_IronKingdom.txt`, briefing and tips. Landscape (chunk 13) and every other chunk stay byte-identical (r28: except the heights and slope shading under the new barracks).

No terrain preparation was needed: with the native Dark rule (masked flags == Dark bit 4, permitted terrain, no object) applied to mask union plus halo, the existing Dark land already accepts 2,921 Farm, 247 Temple, 197 Fortress and 200 Hall anchors (First Dawn's source accepted Farms only). The large-building spots lie mostly on the open Dark land between the two northern camps (x 256..319, y 64..127) and a few near the southern Temple; the land right around the north-eastern Temple is wooded and fits Farms only.

## Plugin, helper and installer

- `native/scenario-model.hpp`: the accepted start layouts (First Dawn, its legacy save layout, Iron Kingdom). The scene fingerprint uses `dt::layoutMask` (bits 128/256 as before); everything else (576, two players, human Dark player 1, computer Viking player 2, one Dark player) is unchanged. `native/scenario-tests.cpp` tests it.
- `tools/scenarios.py`: the same layouts plus source pins and file names; `activate-human-slot.py`, `setup.py`, `install.py`, `test-scenario.py` and `verify-offline.py` use it. `verify-offline.py` checks that both lists agree.
- Installer: the target list comes from `install.targets()`; `install.upgrade()` adds entries for scenarios released after the first installation (an existing installation gets `Map/User/01_Dark_Tribe_Iron_Kingdom.map` added on `stage-release.py` / the next launch; restore removes it). `verify-offline.py` simulates exactly that upgrade on a temporary copy.
- Lua: same structure and victory rule as First Dawn (both bases seen once, then the last Dark Temple/Fortress or the last Viking tower/castle decides). Viking AI settings at the aggressive end of the shipped scripts: AttackMode 2/3/3, AttackDelay 30/15/10, SoldierLimitAbsolute 150/250/400, SoldierLimitRelative 100/200/400.

## Evidence boundary and open points

- Offline: the generated map decodes, round-trips and passes `tools/test-iron-kingdom.cjs`; the Lua runs under Lupa with mocked APIs; the native layout model is unit-tested; installer upgrade/restore is exercised on a temporary copy.
- r29 not verified: that the toolsmith gets a smith and makes the shovels the plugin orders. r40: in the field it made about four shovels before tick 22,500 and none after it lost its smith; whether the hammer pile lets the AI replace a lost smith is not verified.
- Not verified: that the game lists and loads the map, that the human slot is assigned, that the Viking AI attacks, how hard the scenario plays, and whether the removed Trojan player or the Trojan add-on map flags matter to the engine. Two Dark Temples count against the one-Temple limit (no further Temple can be built, both existing ones work); a Fortress can still be built.
- If the map fails to start, the launcher log (`evidence/session-helper.jsonl`) and `SCENE_STATE mask=` lines in `evidence/v02-runtime.log` show which fingerprint part refused it.

## r31: clean start (no farm in a fire field)

The builder removes the Dark Mushroom Farms closer than FarmLock (Euclid 33, `native/citadel-fire.hpp`) to a Dark Temple/Fortress fire centre (position + (1,2)) of the merged Dark player, with `removeFarmsInFire` from `tools/citadel-fire.cjs`. It runs after every other placement.
- Removed: 391,124 (Euclid 12.8) and 413,161 (25.5), both by the northern Temple at 387,133. Kept: 9 farms, 2 Temples, 1 Hall; 12 Dark buildings. The farm 167,110 stays although it is Euclid 25.5 from the Manacopter Hall at 137,89 (centre offset +(1,2); 26.7 from the plain position): the Hall has no fire, and the rule covers only Temple 50 and Fortress 51.
- Under each removed farm the flags the editor baked are cleared on its 52 footprint cells: 0x08 footprint, 0x01 core, 0x02 entrance. Objects, owner, resource and landscape bytes stay.
- The map has no Mushroom Farmers (type 53) and no mushroom objects tied to farms, so no unit or object is removed.
- `tools/test-iron-kingdom.cjs` checks the result. Its authored-ground reference is now the source buildings (the farms included), as the builder levels against them.
- The northern Temple has almost no buildable Dark land around it with or without the farms (3 strict / 48 body anchors outside the field), so early farms come from the 9 kept farms and the southern Temple's land. Not verified in game.
