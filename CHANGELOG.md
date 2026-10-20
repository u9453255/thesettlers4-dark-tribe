# Changelog

What changed for players, newest first. The detailed development journal is [MODLOG.md](MODLOG.md).

The version stays **0.2.15** while updates come as numbered rounds. The mod's startup line in `evidence\v02-runtime.log` (the line that starts with "Pinned native rise") ends with the latest round's name, so you can tell which update you are running (for example `... computer defence and reclaim priority r33`). Updates marked *not yet play-tested* passed every offline check but have not been tried in a real game yet.

## 0.2.15 updates (October 2026)

### r49: Shovels get priority when gardeners are missing (*not yet play-tested*)
- If a computer's toolsmith is full of tools nobody takes while it needs shovels for new gardeners, it now throws one of the surplus tools away and makes the shovel.

### r48: Computer mines get their pickaxes again (*not yet play-tested*)
- The r46 toolsmith fix cut the AI's pickaxe batch short, so its iron mines could stay without workers and the whole iron chain stopped. Tool orders now run until the toolsmith's output is nearly full.

### r47: Computers can rebuild their workforce (*not yet play-tested*)
- When a computer loses its builders, diggers or gardeners, its carrier reserve now makes room for new ones (and the barracks waits), so it no longer freezes with idle carriers and open building sites.

### r46: Computer toolsmiths keep working (*not yet play-tested*)
- A computer's toolsmith used to fill its output with pickaxes and hammers nobody needed and then stop forever, so no new shovels and no replacement gardeners came. Surplus tool orders are now cleared while a tool of that kind is already lying there.

### r45: Computers count their real town, and Iron Kingdom gets stone (*not yet play-tested*)
- The free-settler pool a computer keeps now depends on its real economy buildings, not on decorations and towers, so the Iron Kingdom Vikings no longer freeze with every carrier held back.
- Iron Kingdom: the Vikings start with a stone mine on their stone mountain and 16 stone. Until now they had no stone at all after the first two huts and could never build another house or tower. Start a new Iron Kingdom game to get the new map.

### r44: Economy log for computer players (*developer feature*)
- The computers' economies are recorded every few seconds to `evidence/v02-economy.csv`, so problems like the soldier drain can be found from a play session instead of guessed.

### r43: Computers keep a working population (*not yet play-tested*)
- A computer player no longer turns its last carriers into soldiers. It keeps a pool of free settlers that grows with its town (15 plus one per two buildings), so transport and construction go on, and new soldiers come from new houses.

### r42: Alt+9 shows the whole map (*not yet play-tested*)
- Press Alt+9 in a running map to see the whole map, enemy units and buildings included; press it again to go back to the normal fog. Saves are not affected.

### r41: Manacopters no longer heal themselves (*not yet play-tested*)
- A Manacopter standing on Dark land used to go back to full health every second, so enemies could never finish one off on the ground. Manacopters now keep their damage; only a new one starts at full health.

### r40: Iron Kingdom's toolsmith gets hammers (*not yet play-tested*)
- The Viking toolsmith on Iron Kingdom now starts with two hammers next to it, like the toolsmiths on the other maps. Without them the Vikings could not replace a lost smith, made no more shovels and therefore no more Gardeners, and stopped attacking for good. New Iron Kingdom games only.

### r39: Dark Gardeners really get out of the Manacopter (*not yet play-tested*)
- Dark Gardeners dropped from a Manacopter used to stay half-inside: they could not be clicked properly and nobody could attack them. They now step out like soldiers do, can be selected normally and can be fought.

### r38: armies no longer turn back in the middle of a march (*not yet play-tested*)
- On First Dawn the Viking army turned back after walking part of the way around the lake, because its straight-line distance to the goal grew while it walked. Progress is now measured along the real route.
- A big siege near your Temple or Fortress is no longer called off because one or two soldiers stepped into the fire line; they are just moved back out.

### r37: computers attack far more often and pick their targets sensibly (*not yet play-tested*)
- The first attack comes within 2-4 minutes, a big enough army never waits for luck, and the next attack follows a few hundred ticks after the army is home. An army that cannot reach a farm in time takes the next one from where it stands instead of walking home.
- Targets are chosen by the real walking distance from where the army is, and groups of farms are taken one after the other; no more marching across the map past nearer farms.
- Taking back their own Dark land no longer stops the computers attacking: while an attack is possible, a small escort reclaims land between attacks, and only your fighters in their territory call an attack back home.
- Computer Gardeners left far away are called home and replaced, so attacks keep coming in long games.

### r36: extra log lines for the fire field (*not yet play-tested*)
- No gameplay change. The log now records every computer soldier and Gardener that turns back at a Temple's or Fortress's fire field, and the routes the computers plan around it, to find out why their Gardeners get lost there.

### r35: reclaimed land goes dark again, faster computer armies (*not yet play-tested*)
- Land you had turned Dark and a computer turned green again no longer stays lit for good. It goes back to the normal fog unless your units can see it. Saves where this happened heal a few seconds after loading.
- Computer armies now march at full walking speed instead of stopping every few steps for their Gardeners. The Gardeners follow behind; near the target the army waits for them only briefly (about 20 seconds at normal speed) and then attacks anyway, and late Gardeners join behind the soldiers.

### r34: fixed a crash when clicking a new Dark building site (*not yet play-tested*)
- Clicking a Dark building site right after placing it could crash the game a few seconds into a map. Sites of every kind can now be clicked at any time.

### r33: computers defend their land, Gardeners stay with the soldiers (*not yet play-tested*)
- The computers now defend their territory. When your Dark Gardeners, Warriors, Throwers or Shamans turn their land Dark, they send soldiers together with their own Gardeners: the soldiers go for your units and the Gardeners turn the land green again, area by area. Taking back their land now comes before attacking you; if you keep darkening it, they defend more and attack less, and a big intrusion calls a running attack back home.
- Gardeners travelling with soldiers no longer wander off. They now walk to their place next to the soldiers and wait there instead of greening Dark land along the way. If they fall behind, the soldiers wait or walk back for them; no Gardener is sent home alone any more. At a farm, the Gardeners wait behind the soldiers until it is safe to start.
- Computers give up targets they cannot reach (for example a farm behind a river) instead of marching at them again and again, and they choose targets by the real walking distance.

### r32: bigger attacks in every style, protected Gardeners (*not yet play-tested*)
- Every attack style now grows with the computer's army: raids take a third of it (at least 10), hunts half (at least 16), sweep escorts a quarter (at least 8); assaults, pincers and allied joins take the whole army. Each style also brings one more Gardener per 15 soldiers, and big computers keep up to 20 Gardeners alive instead of 12.
- Soldiers and Gardeners move as one group. The soldiers stay at most about 10 tiles ahead and wait for Gardeners who fall behind. At the target the soldiers take position first, between the Gardeners and your army, and only then do the Gardeners start turning your land green. Replacement Gardeners no longer walk across the map alone, and the army goes home together.

### r31: clean start maps (*not yet play-tested*)
- First Dawn and Iron Kingdom no longer start with Mushroom Farms inside the fire field of a Temple or Fortress (First Dawn lost one farm, Iron Kingdom two). The Alliance was already clean.

### r30: the citadel's fire field (*not yet play-tested*)
- Your Temple and Fortress burn enemy soldiers and Gardeners on Dark land around them while you own at least one Mushroom Farm. The kill radius is 18 to 24 tiles depending on your number of farms, wider than the visible flames. The Manacopter Hall has no such field.
- New building rule: a Mushroom Farm can't be built within 33 tiles of your Temple or Fortress, and a Temple or Fortress can't be built where its field would reach a farm. Blocked spots show no green marker, and you get a short message if you try. Farms that would start inside a field are removed when a map starts or an older save loads.
- The computers no longer walk into the field. While you own farms, their soldiers and Gardeners stay out in every attack style, route around it, and are pulled back if they drift in. They attack the Temple and Fortress only after your last farm has fallen.

### r29: computers make shovels for their Gardeners (*not yet play-tested*)
- When a computer runs short of Gardeners and shovels, it now orders shovels at its own toolsmith, so it can keep attacking in long games. Every computer starts with a toolsmith plus iron bars and coal (and hammers on First Dawn and The Alliance).

### r28: aggressive computers with six attack styles
- The first attack comes after 3 to 8 minutes, and attacks keep coming after a short rest.
- Six ways to attack: raids on weak targets, full assaults, pincers from two sides, sweeps of Gardeners under escort that push into your land, hunts for your army, and joint attacks where allied computers join each other.
- Lost armies are replaced from the computers' own barracks: every map now gives them barracks, weapons, shovels and carriers.
- Computer Gardeners take back Dark land, first inside their own territory so they can build there, then along the border.
- Once no Mushroom Farm is left, the computers go for your Temple and Fortress.

### r27: the computers attack the Dark Tribe
- The game's own computer AI cannot attack Dark buildings. The mod now leads their attacks: soldiers clear your defenders while Gardeners turn the ground around your buildings green until they collapse, Mushroom Farms first.

### r26: crash and freeze diagnostics
- The mod records crashes and freezes (with a snapshot after 10 seconds without response) in its log folder, to help find their cause.

### r25: The Alliance
- New scenario `02_Dark_Tribe_The_Alliance`: one Temple and an army, no farms, against allied Romans, Vikings and Mayas on a 512x512 map. The mod now supports 512 maps and four players.

### r24: Iron Kingdom
- New scenario `01_Dark_Tribe_Iron_Kingdom`: two Dark camps against a fully built Viking kingdom with manned towers, mines and weaponsmiths.

### Repository for other players (r23 and earlier)
- The game folder is configurable, and two commands set up and build everything (`tools\setup.py`, `tools\workflow.py`).

## 0.2.15 (October 13, 2026)

Dark menus that look and work like the game's own:
- The Dark menus use the game's own art, fonts and layouts. A Dark mana section replaces the normal goods panel. A Dark unit panel replaces the Roman unit menus, and Manacopters have their own panel.
- Temple and Fortress recruitment works like the native barracks: pick a unit, then queue endlessly, add or remove, or clear.
- One Temple and one Fortress per player.
- Right-drag still scrolls the map with only Manacopters selected. The Dark menus disappear outside a running map.
- Buildings no longer wait forever when units or wild animals stand on the site.

## 0.2.14 (October 12, 2026)

- Dark buildings rise in place on the native site, with their growth visible. Units no longer block or cancel finished buildings.
- Manacopters: click, Shift/Ctrl-click or drag-box selection; right-click flight for every selected aircraft; selection brackets; sight around each aircraft; "Load nearby" boards close units (24 tiles); "Unload" lets you pick the landing spot.
- Fixed a crash when clicking a growing Dark building.

## 0.2.8 to 0.2.13 (October 12, 2026)

- Construction costs mana once and refunds correctly when a site is refused. Placement markers show where Dark buildings fit.
- Mushroom Farmers deliver mushrooms the way the original game does. Earlier invented couriers and stock were removed.
- Works with the Settlers United HD patch: the Dark menus appear under HD.
- Fixed a crash at the main menu (0.2.12) and with the Fortress panel.

## 0.2 to 0.2.7 (October 12, 2026)

- Full Dark economy: mana from Mushroom Farms, all four Dark buildings (Mushroom Farm, Dark Temple, Fortress, Manacopter Hall), troop queues and Manacopters that carry units. Saves keep the mod's state in a matching file in `save-states\`.
- The Viking opponent in First Dawn starts with a real settlement and economy.
- Several startup and map-loading crashes fixed.

## 0.1 (October 11, 2026)

- First playable First Dawn scenario: the Dark Tribe as the human player against the Vikings, made from the original campaign map `dark03`.
