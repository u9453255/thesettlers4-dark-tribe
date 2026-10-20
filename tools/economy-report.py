#!/usr/bin/env python3
"""Summarise evidence/v02-economy.csv (Dark Tribe r44 computer economy telemetry) and flag likely issues.

usage: python tools/economy-report.py [csv] [--pid N] [--player P] [--last]

Rows are grouped by process (pid) and split into maps where the tick drops (a new map restarts at tick 10; a loaded
save jumps forward and stays in the same segment). Every computer of every map gets a summary and a list of flags.
The flags are heuristics over 500-tick samples; read evidence/v02-runtime.log (same ms and pid columns) for the cause.
"""
import csv
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SETTLERS = {1: 'carrier', 2: 'digger', 3: 'builder', 4: 'woodcutter', 5: 'stonecutter', 6: 'forester', 7: 'grainfarmer',
            8: 'animalfarmer', 9: 'fisher', 10: 'waterworker', 11: 'hunter', 12: 'sawmillworker', 13: 'smelter',
            14: 'mineworker', 15: 'smith', 16: 'miller', 17: 'baker', 18: 'butcher', 19: 'shipyardworker', 20: 'healer',
            21: 'charcoalmaker', 22: 'ammomaker', 23: 'vehiclemaker', 24: 'vintner', 25: 'beekeeper', 26: 'meadmaker',
            27: 'agavefarmer', 28: 'tequilamaker', 29: 'swordsman1', 30: 'swordsman2', 31: 'swordsman3', 32: 'bowman1',
            33: 'bowman2', 34: 'bowman3', 35: 'medic1', 36: 'medic2', 37: 'medic3', 38: 'axewarrior1', 39: 'axewarrior2',
            40: 'axewarrior3', 41: 'blowgun1', 42: 'blowgun2', 43: 'blowgun3', 44: 'squadleader', 45: 'priest',
            46: 'saboteur', 47: 'pioneer', 48: 'thief', 49: 'geologist', 50: 'gardener', 51: 'landscaper',
            52: 'darkgardener', 53: 'mushroomfarmer', 54: 'shaman', 55: 'slave', 56: 'servant', 57: 'angel1',
            58: 'angel2', 59: 'angel3', 60: 'donkey', 61: 'catapultist1', 62: 'catapultist2', 63: 'catapultist3',
            64: 'sunflowerfarmer', 65: 'oilmaker', 66: 'manacopterpilot'}
BUILDINGS = {1: 'woodcutter', 2: 'forester', 3: 'sawmill', 4: 'stonecutter', 5: 'waterwork', 6: 'fisher', 7: 'hunter',
             8: 'slaughterhouse', 9: 'mill', 10: 'bakery', 11: 'grainfarm', 12: 'animalranch', 13: 'donkeyranch',
             14: 'stonemine', 15: 'ironmine', 16: 'goldmine', 17: 'coalmine', 18: 'sulfurmine', 19: 'goldsmelter',
             20: 'ironsmelter', 21: 'toolsmith', 22: 'weaponsmith', 23: 'vehiclehall', 24: 'barracks', 25: 'charcoal',
             26: 'trainingcenter', 27: 'healer', 28: 'ammomaker', 29: 'gunpowder', 30: 'landscaper', 31: 'shipyard',
             32: 'port', 33: 'market', 34: 'storage', 35: 'vinyard', 36: 'agavefarm', 37: 'tequila', 38: 'beekeeper',
             39: 'meadmaker', 40: 'house_s', 41: 'house_m', 42: 'house_b', 43: 'smalltemple', 44: 'bigtemple',
             45: 'lookout', 46: 'tower_s', 47: 'tower_b', 48: 'castle', 49: 'mushroomfarm', 50: 'darktemple',
             51: 'fortress', 80: 'manacopterhall', 81: 'oilmaker', 82: 'sunflowerfarm'}
GOODS = ['log', 'board', 'stone', 'ironore', 'coal', 'ironbar', 'sword', 'bow', 'battleaxe', 'blowgun', 'catapult', 'shovel',
         'hammer', 'axe', 'pickaxe', 'saw', 'bread', 'fish', 'meat', 'grain', 'flour', 'water']
FIXED = ['ms', 'pid', 'tick', 'player', 'eco', 'population', 'carriers', 'busy', 'idle', 'reserve', 'keep', 'housing', 'houses_s',
         'houses_m', 'houses_b', 'buildings', 'sites', 'barracks', 'toolsmiths', 'weaponsmiths', 'ironmines', 'coalmines',
         'ironsmelters', 'soldiers', 'leaders', 'builders', 'diggers', 'gardeners'] + GOODS
OPTIONAL = ['tsout', 'tsin', 'tsworker']  # r46 toolsmith columns; older files lack them (-1)
SAMPLE = 500  # ticks between two rows of one computer (dtledger::EcoEvery)


def parse_list(text):
    return {int(k): int(v) for k, v in (pair.split(':') for pair in text.split() if ':' in pair)}


def load(path):
    rows = []
    with open(path, newline='', encoding='utf-8', errors='replace') as handle:
        for raw in csv.DictReader(handle):
            try:
                row = {key: int(raw[key]) for key in FIXED}
            except (KeyError, TypeError, ValueError):
                continue  # a torn line from a crashed process
            for key in OPTIONAL:
                try:
                    row[key] = int(raw.get(key) or -1)
                except ValueError:
                    row[key] = -1
            row['settlers_by_type'] = parse_list(raw.get('settlers') or '')
            row['ready_by_type'] = parse_list(raw.get('ready') or '')
            row['sites_by_type'] = parse_list(raw.get('sites_by_type') or '')
            rows.append(row)
    return rows


def segments(rows):
    """[(pid, map index, player, samples)] in file order; a tick drop inside a pid starts a new map."""
    by_pid = {}
    for row in rows:
        by_pid.setdefault(row['pid'], []).append(row)
    out = []
    for pid, prows in by_pid.items():
        maps, current, last = [], [], None
        for row in prows:
            if last is not None and row['tick'] < last - 100:
                maps.append(current)
                current = []
            current.append(row)
            last = row['tick']
        maps.append(current)
        for index, mrows in enumerate(maps, 1):
            players = {}
            for row in mrows:
                players.setdefault(row['player'], []).append(row)
            for player, samples in sorted(players.items()):
                out.append((pid, index, player, sorted(samples, key=lambda r: r['tick'])))
    return out


def runs(samples, predicate):
    """Consecutive samples where predicate holds: [(first tick, last tick, count)]."""
    found, start, last, count = [], None, None, 0
    for sample in samples:
        if predicate(sample):
            start = sample['tick'] if start is None else start
            last, count = sample['tick'], count + 1
        elif start is not None:
            found.append((start, last, count))
            start, count = None, 0
    if start is not None:
        found.append((start, last, count))
    return found


def longest(found):
    return max(found, key=lambda r: r[2]) if found else None


def analyse(samples):
    first, last = samples[0], samples[-1]
    flags = []
    span = last['tick'] - first['tick']
    missing = sum(1 for s in samples if not s['eco'])
    if missing:
        flags.append(f'home economy not found in {missing} of {len(samples)} samples (goods and carriers are 0 there)')
    eco = [s for s in samples if s['eco']] or samples
    drained = longest(runs(eco, lambda s: s['idle'] < 5))
    if drained and drained[2] >= 3:
        flags.append(f'carriers drained (idle < 5) for {drained[1] - drained[0] + SAMPLE} ticks from tick {drained[0]} to {drained[1]}')
    below = sum(1 for s in eco if s['keep'] > 0 and s['reserve'] < min(s['keep'], s['carriers']))
    if below:
        flags.append(f'reserve below keep in {below} samples (the r43 floor did not hold)')
    capped = sum(1 for s in eco if s['keep'] > 0 and s['carriers'] <= s['keep'])
    if capped * 2 >= len(eco):
        flags.append(f'no carrier above the keep in {capped} of {len(eco)} samples: nothing can be drafted or recruited (the setter caps the reserve at the carrier total)')
    drafted = 0
    for prev, cur in zip(eco, eco[1:]):
        if cur['soldiers'] > prev['soldiers'] and prev['keep'] > 0 and prev['carriers'] < prev['keep']:
            drafted += cur['soldiers'] - prev['soldiers']
    if drafted:
        flags.append(f'{drafted} soldiers appeared while carriers were below keep')
    last_grow, stalled = first['tick'], None
    for prev, cur in zip(samples, samples[1:]):
        if cur['buildings'] > prev['buildings']:
            last_grow = cur['tick']
        elif cur['sites'] > 0 and cur['tick'] - last_grow >= 20000:
            stalled = (last_grow, cur['tick']) if stalled is None or stalled[0] == last_grow else stalled
    if stalled:
        flags.append(f'construction stalled: sites open but no building completed from tick {stalled[0]} to {stalled[1]}')
    no_builders = longest(runs(samples, lambda s: s['sites'] > 0 and s['builders'] == 0))
    if no_builders and no_builders[2] >= 3:
        flags.append(f'sites without a builder for {no_builders[2]} samples from tick {no_builders[0]}')
    smiths = [s for s in samples if s['toolsmiths'] + s['weaponsmiths'] > 0]
    no_smith = longest(runs(smiths, lambda s: s['settlers_by_type'].get(15, 0) == 0))
    if no_smith and no_smith[2] >= 3:
        flags.append(f'smithies without a smith for {no_smith[2]} samples from tick {no_smith[0]} (hammers?)')
    weapons = [s for s in eco if s['weaponsmiths'] > 0]
    starved = sum(1 for s in weapons if s['ironbar'] == 0 and s['coal'] == 0)
    if weapons and starved * 2 >= len(weapons):
        flags.append(f'weapon chain starved (no iron bar and no coal) in {starved} of {len(weapons)} samples')
    tools = [s for s in eco if s['toolsmiths'] > 0]
    dry = sum(1 for s in tools if s['shovel'] == 0 and s['ironbar'] == 0)
    if tools and dry * 2 >= len(tools):
        flags.append(f'toolsmith starved (no shovel and no iron bar) in {dry} of {len(tools)} samples')
    smith = [s for s in samples if s['toolsmiths'] > 0 and s['tsout'] >= 0]
    clog = sum(1 for s in smith if s['tsout'] >= 8)
    if smith and clog * 4 >= len(smith):
        flags.append(f'toolsmith output full (8 or more tools lying there) in {clog} of {len(smith)} samples: nothing more is made')
    unmanned = longest(runs(smith, lambda s: s['tsworker'] == 0))
    if unmanned and unmanned[2] >= 3:
        flags.append(f'toolsmith without a worker for {unmanned[2]} samples from tick {unmanned[0]}')
    dry_in = longest(runs(smith, lambda s: s['tsin'] == 0 and s['ironbar'] + s['coal'] > 4))
    if dry_in and dry_in[2] >= 6:
        flags.append(f'toolsmith input empty while iron or coal is in stock for {dry_in[2]} samples from tick {dry_in[0]}')
    window = [s for s in samples if s['tick'] >= last['tick'] - 30000]
    if len(window) >= 20 and span >= 30000:
        growth = window[-1]['population'] - window[0]['population']
        houses = any(s['sites_by_type'].get(t, 0) for s in window for t in (40, 41, 42))
        if growth < 5 and not houses:
            flags.append(f'population flat ({growth:+d}) over the last 30,000 ticks with no residence site')
    if last['population'] and last['soldiers'] * 10 >= last['population'] * 6:
        flags.append(f'soldiers are {100 * last["soldiers"] // last["population"]}% of the population at the end')
    zero = [name for name in GOODS if eco and sum(1 for s in eco if s[name] == 0) * 5 >= len(eco) * 4]
    return flags, zero


def describe(pid, index, player, samples):
    first, last = samples[0], samples[-1]
    carriers = [s['carriers'] for s in samples if s['eco']] or [0]
    idle = [s['idle'] for s in samples if s['eco']] or [0]
    sites = [s['sites'] for s in samples]
    flags, zero = analyse(samples)
    lines = [f'pid {pid} map {index} player {player}: ticks {first["tick"]}..{last["tick"]} ({len(samples)} samples)',
             f'  population {first["population"]} -> {last["population"]}, housing {last["housing"]}, houses s/m/b {last["houses_s"]}/{last["houses_m"]}/{last["houses_b"]}',
             f'  carriers min {min(carriers)} median {int(statistics.median(carriers))} last {last["carriers"]}; idle min {min(idle)} last {last["idle"]}; reserve {last["reserve"]} keep {last["keep"]}',
             f'  soldiers {first["soldiers"]} -> {last["soldiers"]} (leaders {last["leaders"]}), gardeners {last["gardeners"]}, builders {last["builders"]}, diggers {last["diggers"]}',
             f'  buildings {first["buildings"]} -> {last["buildings"]}, sites median {int(statistics.median(sites))} last {last["sites"]}; barracks {last["barracks"]} toolsmiths {last["toolsmiths"]} weaponsmiths {last["weaponsmiths"]} ironmines {last["ironmines"]} coalmines {last["coalmines"]} smelters {last["ironsmelters"]}',
             '  stock at the end: ' + ' '.join(f'{name}={last[name]}' for name in GOODS),
             f'  toolsmith at the end: output {last["tsout"]} of 8, lowest input {last["tsin"]}, worker {last["tsworker"]} (-1 = no toolsmith or old file)']
    if zero:
        lines.append('  goods at 0 in 80% of the samples: ' + ', '.join(zero))
    ready = ', '.join(f'{BUILDINGS.get(t, t)}x{n}' for t, n in sorted(last['ready_by_type'].items()))
    lines.append('  buildings at the end: ' + (ready or 'none'))
    jobs = ', '.join(f'{SETTLERS.get(t, t)}x{n}' for t, n in sorted(last['settlers_by_type'].items()))
    lines.append('  settlers at the end: ' + (jobs or 'none'))
    lines.extend('  FLAG ' + flag for flag in flags)
    return lines


def main(argv):
    want_pid = want_player = None
    values = set()
    for i, a in enumerate(argv):
        if a == '--pid':
            want_pid = int(argv[i + 1])
            values.add(i + 1)
        if a == '--player':
            want_player = int(argv[i + 1])
            values.add(i + 1)
    args = [a for i, a in enumerate(argv) if not a.startswith('--') and i not in values]
    path = Path(args[0]) if args else ROOT / 'evidence/v02-economy.csv'
    if not path.exists():
        print(f'no telemetry at {path}; play with the r44 build first')
        return 1
    rows = load(path)
    if not rows:
        print(f'{path}: no complete rows')
        return 1
    found = segments(rows)
    if '--last' in argv:
        last_pid = rows[-1]['pid']
        found = [f for f in found if f[0] == last_pid]
        found = [f for f in found if f[1] == max(g[1] for g in found)]
    for pid, index, player, samples in found:
        if want_pid is not None and pid != want_pid:
            continue
        if want_player is not None and player != want_player:
            continue
        print('\n'.join(describe(pid, index, player, samples)))
        print()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
