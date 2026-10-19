"""Read original, local Viking/Dark masks for offline scenario placement only."""
import json
import re
from pathlib import Path
import xml.etree.ElementTree as ET
from project_paths import ROOT, original_building_xml
source=original_building_xml()  # pinned original: install backup or the untouched game file
ids={'BUILDING_WOODCUTTERHUT':1,'BUILDING_FORESTERHUT':2,'BUILDING_SAWMILL':3,
     'BUILDING_STONECUTTERHUT':4,'BUILDING_TOOLSMITH':21,'BUILDING_RESIDENCESMALL':40,'BUILDING_BARRACKS':24,
     'BUILDING_GUARDTOWERSMALL':46,'BUILDING_GUARDTOWERBIG':47,'BUILDING_MUSHROOMFARM':49,'BUILDING_DARKTEMPLE':50,
     'BUILDING_FORTRESS':51,'BUILDING_MANACOPTERHALL':80}
# Every building id as numbered by the engine (the S4ModApi enum mirrors the map's building type byte).
header=re.sub(r'//[^\n]*','',(ROOT/'third_party/S4ModApi/S4ModApi.h').read_text(encoding='latin1'))
enum={};value=-1
for item in re.search(r'enum\s+S4_BUILDING_ENUM\s*:\s*\w+\s*\{(.*?)\}',header,re.S).group(1).split(','):
    item=item.strip()
    if not item:continue
    name,_,number=item.partition('=');value=int(number,0) if number else value+1
    enum[name.strip().replace('S4_BUILDING_','BUILDING_')]=value
assert all(enum[k]==v for k,v in ids.items())
def mask(b,tags=('blockPosLines','buildingPosLines','digPosLines')):
    hx=int(b.findtext('iHotSpotX'));hy=int(b.findtext('iHotSpotY'))
    return sorted({(31-bit-hx,row-hy) for tag in tags
                   for m in [b.find(tag)] if m is not None for row,v in enumerate(m)
                   for bit in range(32) if int(v.text)&(1<<bit)})
result={};radii={};doors={};viking={};vikingDoors={};vikingRadii={}
RACES={'RACE_ROMAN':'0','RACE_VIKING':'1','RACE_MAYA':'2','RACE_DARK':'3'}  # map race numbers
races={r:{} for r in RACES.values()};raceDoors={r:{} for r in RACES.values()};raceBlocks={r:{} for r in RACES.values()}
for race in ET.parse(source).getroot():
    if race.get('id') in RACES:
        # Complete per-race tables (footprints and doors) for authored settlements of any computer player.
        for b in race:
            if b.get('id') not in enum:continue
            races[RACES[race.get('id')]][str(enum[b.get('id')])]=mask(b)
            # Blocked core alone (blockPosLines, inside the footprint above): the cells authored maps keep level.
            raceBlocks[RACES[race.get('id')]][str(enum[b.get('id')])]=mask(b,('blockPosLines',))
            door=b.find('door')
            if door is not None:raceDoors[RACES[race.get('id')]][str(enum[b.get('id')])]=[int(door.findtext('xOffset')),int(door.findtext('yOffset'))]
    if race.get('id') not in ('RACE_VIKING','RACE_DARK'):continue
    for b in race:
        if race.get('id')=='RACE_VIKING' and b.get('id') in enum:
            # Complete Viking table: footprints, doors and influence for a whole authored settlement.
            key=str(enum[b.get('id')]);viking[key]=mask(b)
            door=b.find('door')
            if door is not None:vikingDoors[key]=[int(door.findtext('xOffset')),int(door.findtext('yOffset'))]
            if b.findtext('influenceRadius'):vikingRadii[key]=int(b.findtext('influenceRadius'))
        if b.get('id') not in ids:continue
        if ids[b.get('id')] in (46,47):
            assert int(b.findtext('influenceRadius'))=={46:20,47:30}[ids[b.get('id')]]
            radii[str(ids[b.get('id')])]=int(b.findtext('influenceRadius'))
        door=b.find('door')
        if door is not None:doors[str(ids[b.get('id')])]=[int(door.findtext('xOffset')),int(door.findtext('yOffset'))]
        cells=mask(b)
        assert cells
        result[str(ids[b.get('id')])]=cells
assert set(result)==set(map(str,ids.values()))
assert all(viking[str(t)]==result[str(t)] for t in (1,2,3,4,21,24,40,46,47))
result['_radii']=radii;result['_doors']=doors
result['_viking']=viking;result['_vikingDoors']=vikingDoors;result['_vikingRadii']=vikingRadii
assert races['1']==viking and all(races[r] for r in races)
assert all(set(map(tuple,raceBlocks[r][t]))<=set(map(tuple,races[r][t])) for r in races for t in races[r])
result['_races']=races;result['_raceDoors']=raceDoors;result['_raceBlocks']=raceBlocks
print(json.dumps(result))
