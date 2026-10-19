# Changelog

What changed for players, newest first. The detailed development journal is [MODLOG.md](MODLOG.md).

The version stays **0.2.15** while updates come as numbered rounds. The mod's startup line in `evidence\v02-runtime.log` (the line that starts with "Pinned native rise") ends with the latest round's name, so you can tell which update you are running (for example `... scaled styles and protected convoy r32`). Updates marked *not yet play-tested* passed every offline check but have not been tried in a real game yet.

## 0.2.15 updates (October 2026)

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
