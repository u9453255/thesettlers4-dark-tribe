"""The playable Dark Tribe scenarios: source campaign map, generated/installed files and player layout.

The native plugin keeps the same layouts in native/scenario-model.hpp (verify-offline.py checks that
both lists agree). The plugin recognises a running scenario by its map size, player count, races and
start positions: player 1 is the human Dark Tribe, every other player a computer.
Races: 0 Roman, 1 Viking, 2 Maya, 3 Dark Tribe.
"""
SCENARIOS = [
    dict(key='first_dawn', title='First Dawn',
         source='Map/Campaign/dark03.map',
         source_sha256='237a1df871a31818e00496a8b1b802cb414902136657aeb5e10561c11ec854fb',
         builder='build-scenario.cjs', script='scenario/DarkTribe_FirstDawn.txt',
         generated='work/generated/Map/User/DarkTribe_FirstDawn.map',
         installed='Map/User/00_Dark_Tribe_First_Dawn.map',
         size=576, players=[(3, 277, 214), (1, 75, 401)],
         # The unchanged 0.1 layout (Viking start 261,279), kept for paired old saves.
         legacy=[[(3, 277, 214), (1, 261, 279)]]),
    dict(key='iron_kingdom', title='Iron Kingdom',
         source='Map/Campaign/AO_trojan10.map',
         source_sha256='05f34cbb22aad2e8e390cf9f3d38dcd13e3513d69ccfa6f54c67450fc0b7b122',
         builder='build-iron-kingdom.cjs', script='scenario/DarkTribe_IronKingdom.txt',
         generated='work/generated/Map/User/DarkTribe_IronKingdom.map',
         installed='Map/User/01_Dark_Tribe_Iron_Kingdom.map',
         size=576, players=[(3, 378, 134), (1, 475, 353)], legacy=[]),
    dict(key='alliance', title='The Alliance',
         source='Map/Campaign/dark07.map',
         source_sha256='7d95a9fa2b8ee7019144e980b240a43d032bd2cf7c724037bcfb5ba9f4dcb403',
         builder='build-alliance.cjs', script='scenario/DarkTribe_Alliance.txt',
         generated='work/generated/Map/User/DarkTribe_Alliance.map',
         installed='Map/User/02_Dark_Tribe_The_Alliance.map',
         size=512, players=[(3, 303, 423), (0, 98, 353), (1, 367, 116), (2, 260, 263)], legacy=[]),
]


def layouts():
    """Every accepted (map size, ((race, x, y) per player)) layout, in native/scenario-model.hpp order."""
    return [(s['size'], tuple(players)) for s in SCENARIOS for players in [s['players'], *s['legacy']]]
