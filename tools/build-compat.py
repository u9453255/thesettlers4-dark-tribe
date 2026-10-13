"""Generate Dark Tribe placement compatibility using only the local game XML."""
import argparse, copy, hashlib, json, re
from pathlib import Path
import xml.etree.ElementTree as ET
from project_paths import game_dir

def build(game, output):
    source = game / 'GameData/buildingInfo.xml'
    raw = source.read_bytes()
    manifest_path=Path(__file__).resolve().parents[1]/'backups/install-manifest.json'
    if manifest_path.exists():
        manifest=json.loads(manifest_path.read_text())
        entry=next((e for e in manifest['entries'] if Path(e['destination']).resolve()==source.resolve()),None)
        if entry:
            current=hashlib.sha256(raw).hexdigest()
            if current in (entry['installed_sha256'], *entry.get('previous_sha256', [])):
                raw=Path(entry['backup']).read_bytes()
                if hashlib.sha256(raw).hexdigest()!=entry['original_sha256']:raise RuntimeError('Original XML backup hash mismatch')
            elif current!=entry['original_sha256']:raise RuntimeError('Game XML changed outside this mod')
    root = ET.fromstring(raw)
    roman = next(r for r in root if r.get('id') == 'RACE_ROMAN')
    dark = next(r for r in root if r.get('id') == 'RACE_DARK')
    old = {b.get('id'): ET.tostring(b) for b in dark}
    fortress = dark.find("building[@id='BUILDING_FORTRESS']")
    if fortress is None:
        fortress = next((b for b in dark if b.get('id') == 'BUILDING_FORTRESS'), None)
    if fortress is None or fortress.find('tool') is None:
        raise ValueError('Fortress tool definition missing')
    tool = fortress.find('tool')
    if (tool.text or '').strip() not in ('', 'GOOD_NO_GOOD'):
        raise ValueError('Unexpected Fortress tool; refusing to change it')
    # Empty enum becomes -1 in the native parser. The construction panel treats
    # any nonzero value as a goods index. Explicit zero skips that lookup.
    tool.text = 'GOOD_NO_GOOD'
    expected = dict(old)
    expected['BUILDING_FORTRESS'] = ET.tostring(fortress)
    ids = set(old)
    additions = [copy.deepcopy(b) for b in roman if b.get('id') not in ids]
    # Preserve original bytes except this one empty enum; insert missing entries.
    payload = b'\r\n'.join(ET.tostring(b, encoding='utf-8') for b in additions) + b'\r\n  ' if additions else b''
    match = re.search(rb'<race\s+id="RACE_DARK">.*?</race>', raw, re.S)
    if not match: raise ValueError('Dark race XML not found')
    dark_bytes = match.group()
    # Match the element name explicitly so nested closing tags cannot end it.
    block = re.search(rb'<(?P<tag>\w+)\s+id="BUILDING_FORTRESS">.*?</(?P=tag)>', dark_bytes, re.S)
    if not block: raise ValueError('Fortress XML not found')
    replacement, count = re.subn(rb'<tool>\s*</tool>|<tool\s*/>', b'<tool>GOOD_NO_GOOD</tool>', block.group())
    if not (tool.text == 'GOOD_NO_GOOD' and count in (0, 1)):
        raise ValueError('Ambiguous Fortress tool definition')
    start = match.start() + block.start()
    raw_patched = raw[:start] + replacement + raw[match.start() + block.end():]
    match = re.search(rb'<race\s+id="RACE_DARK">.*?</race>', raw_patched, re.S)
    at = match.end() - len(b'</race>')
    patched = raw_patched[:at] + payload + raw_patched[at:]
    checked = ET.fromstring(patched)
    newdark = next(r for r in checked if r.get('id') == 'RACE_DARK')
    for b in newdark:
        if b.get('id') in old:
            assert ET.tostring(b) == expected[b.get('id')], 'Unexpected native Dark building change'
    assert len({b.get('id') for b in newdark}) == len(newdark)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(patched)
    report = dict(source=str(source), source_sha256=hashlib.sha256(raw).hexdigest(), output_sha256=hashlib.sha256(patched).hexdigest(), added=[b.get('id') for b in additions], native_dark_preserved=[k for k in old if old[k] == expected[k]], native_dark_corrections={'BUILDING_FORTRESS.tool': 'GOOD_NO_GOOD'}, other_races_unchanged=True, runtime_verified=False)
    return report

if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('--game',type=Path,default=None);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'work/generated/GameData/buildingInfo.xml');a=p.parse_args();a.game=a.game or game_dir()
    report=build(a.game,a.output)
    target=Path(__file__).resolve().parents[1]/'research/building-compatibility.json';target.write_text(json.dumps(report,indent=2))
    print('Generated',len(report['added']),'placement definitions; preserved',len(report['native_dark_preserved']),'native Dark Tribe buildings')
