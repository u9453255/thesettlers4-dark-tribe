"""Offline Fortress data regressions; reads files, never opens the game process."""
import copy
import hashlib
import importlib.util
import json
import re
import tempfile
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
from project_paths import original_building_xml
spec = importlib.util.spec_from_file_location('compat', ROOT / 'tools/build-compat.py')
compat = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compat)


def run():
    original = original_building_xml().read_bytes()
    parsed = ET.fromstring(original)
    dark = parsed.find("race[@id='RACE_DARK']")
    fortress = next(b for b in dark if b.get('id') == 'BUILDING_FORTRESS')
    temple = next(b for b in dark if b.get('id') == 'BUILDING_DARKTEMPLE')
    assert not (fortress.findtext('tool') or '').strip()
    assert fortress.findtext('kind') == temple.findtext('kind') == 'HOUSE_KIND_DARKTEMPLE'
    with tempfile.TemporaryDirectory(prefix='dt-fortress-offline-') as temp:
        game = Path(temp) / 'game'
        source = game / 'GameData/buildingInfo.xml'
        source.parent.mkdir(parents=True)
        source.write_bytes(original)
        output = Path(temp) / 'patched.xml'
        report = compat.build(game, output)
        patched = ET.fromstring(output.read_bytes())
        newdark = patched.find("race[@id='RACE_DARK']")
        newfortress = next(b for b in newdark if b.get('id') == 'BUILDING_FORTRESS')
        assert newfortress.findtext('tool') == 'GOOD_NO_GOOD'
        restored = copy.deepcopy(newfortress)
        restored.find('tool').text = fortress.find('tool').text
        assert ET.tostring(restored) == ET.tostring(fortress), 'Fortress role/geometry changed'
        for race in parsed.findall("race"):
            if race.get('id') != 'RACE_DARK':
                assert ET.tostring(race) == ET.tostring(patched.find(f"race[@id='{race.get('id')}']"))
        for building in dark:
            if building.get('id') != 'BUILDING_FORTRESS':
                counterpart = next(b for b in newdark if b.get('id') == building.get('id'))
                assert ET.tostring(building) == ET.tostring(counterpart)
        assert hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(original).digest()
        # Already corrected data is accepted without duplicating placement entries.
        source.write_bytes(output.read_bytes())
        compat.build(game, Path(temp) / 'again.xml')
        assert (Path(temp) / 'again.xml').read_bytes() == output.read_bytes()
        # Unexpected goods must not be silently changed to no-tool.
        bad = output.read_bytes().replace(b'<tool>GOOD_NO_GOOD</tool>', b'<tool>GOOD_AXE</tool>')
        source.write_bytes(bad)
        try:
            compat.build(game, Path(temp) / 'bad.xml')
        except ValueError:
            pass
        else:
            raise AssertionError('Unexpected Fortress tool was overwritten')
        # Explicitly protect other race bytes, beyond semantic equality.
        for race_bytes in re.findall(rb'<race\b[^>]*>.*?</race>', original, re.S):
            if b'id="RACE_DARK"' not in race_bytes:
                assert race_bytes in output.read_bytes()
        # After staging an upgrade, the installed predecessor must still rebuild
        # from the original backup rather than silently treating mod XML as stock.
        project = Path(temp) / 'project'
        (project / 'backups').mkdir(parents=True)
        backup = project / 'backups/original.xml'
        backup.write_bytes(original)
        legacy = output.read_bytes().replace(b'<tool>GOOD_NO_GOOD</tool>', b'<tool></tool>')
        source.write_bytes(legacy)
        entry = {'destination': str(source.resolve()), 'backup': str(backup),
                 'original_sha256': hashlib.sha256(original).hexdigest(),
                 'installed_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                 'previous_sha256': [hashlib.sha256(legacy).hexdigest()]}
        (project / 'backups/install-manifest.json').write_text(json.dumps({'entries': [entry]}))
        module_path = compat.__file__
        compat.__file__ = str(project / 'tools/build-compat.py')
        try:
            compat.build(game, Path(temp) / 'upgrade.xml')
            assert (Path(temp) / 'upgrade.xml').read_bytes() == output.read_bytes()
            source.write_bytes(legacy + b'\n<!-- unrelated change -->')
            try:
                compat.build(game, Path(temp) / 'unrelated.xml')
            except RuntimeError:
                pass
            else:
                raise AssertionError('Unrelated installed XML accepted')
            source.write_bytes(legacy)
            backup.write_bytes(original + b'\n<!-- corrupt backup -->')
            try:
                compat.build(game, Path(temp) / 'corrupt.xml')
            except RuntimeError:
                pass
            else:
                raise AssertionError('Corrupt original backup accepted')
        finally:
            compat.__file__ = module_path
    checks = ['Fortress explicit no-tool enum', 'Native Fortress/Temple role equivalence',
              'Fortress geometry and all other native data preserved', 'Original backup unchanged',
              'Idempotent generation', 'Unexpected tool refused', 'Other race bytes preserved',
              'Accepted predecessor regeneration', 'Unrelated XML refused', 'Corrupt backup refused']
    (ROOT / 'research/fortress-offline-checks.json').write_text(json.dumps({
        'checks': checks, 'game_started': False, 'gameplay_verified': False,
        'version': '0.2.15', 'status': 'integrated; gameplay unverified'}, indent=2))
    for check in checks:
        print('PASS:', check)


if __name__ == '__main__':
    run()
