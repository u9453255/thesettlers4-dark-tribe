"""Read original, local Viking/Dark masks for offline scenario placement only."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from project_paths import original_building_xml
source=original_building_xml()  # pinned original: install backup or the untouched game file
ids={'BUILDING_WOODCUTTERHUT':1,'BUILDING_FORESTERHUT':2,'BUILDING_SAWMILL':3,
     'BUILDING_STONECUTTERHUT':4,'BUILDING_RESIDENCESMALL':40,'BUILDING_BARRACKS':24,
     'BUILDING_GUARDTOWERSMALL':46,'BUILDING_GUARDTOWERBIG':47,'BUILDING_MUSHROOMFARM':49,'BUILDING_DARKTEMPLE':50,
     'BUILDING_FORTRESS':51,'BUILDING_MANACOPTERHALL':80}
result={};radii={};doors={}
for race in ET.parse(source).getroot():
    if race.get('id') not in ('RACE_VIKING','RACE_DARK'):continue
    for b in race:
        if b.get('id') not in ids:continue
        hx=int(b.findtext('iHotSpotX'));hy=int(b.findtext('iHotSpotY'))
        if ids[b.get('id')] in (46,47):
            assert int(b.findtext('influenceRadius'))=={46:20,47:30}[ids[b.get('id')]]
            radii[str(ids[b.get('id')])]=int(b.findtext('influenceRadius'))
        door=b.find('door')
        if door is not None:doors[str(ids[b.get('id')])]=[int(door.findtext('xOffset')),int(door.findtext('yOffset'))]
        cells=sorted({(31-bit-hx,row-hy) for tag in ('blockPosLines','buildingPosLines','digPosLines')
                      for mask in [b.find(tag)] if mask is not None for row,v in enumerate(mask)
                      for bit in range(32) if int(v.text)&(1<<bit)})
        assert cells
        result[str(ids[b.get('id')])]=cells
assert set(result)==set(map(str,ids.values()))
result['_radii']=radii;result['_doors']=doors
print(json.dumps(result))
