"""Prove both exact executable pins and refusal of unrelated changes; no game writes."""
import json
import struct
import tempfile
from pathlib import Path
from game_build import ORIGINAL_SHA256, LAA_SHA256, BUNDLED_API_SHA256, SU_API_SHA256, HD_FILES, verify_bytes, verify_executable, select_api, verify_hd
from project_paths import bundled_api, game_dir

ROOT = Path(__file__).resolve().parents[1]


def run():
    raw = (game_dir() / 'S4_Main.exe').read_bytes()
    observed = verify_bytes(raw)
    original = bytearray(raw)
    struct.pack_into('<H', original, 0x1a6, 0x102)
    laa = bytearray(original)
    struct.pack_into('<H', laa, 0x1a6, 0x122)
    assert verify_bytes(original)['sha256'] == ORIGINAL_SHA256
    assert verify_bytes(laa)['sha256'] == LAA_SHA256
    assert [i for i, (a, b) in enumerate(zip(original, laa)) if a != b] == [0x1a6]
    changed_code = bytearray(laa); changed_code[0x1000] ^= 1
    changed_header = bytearray(laa); changed_header[0x1a6] ^= 1
    for invalid in (changed_code, changed_header, laa + b'changed', laa[:100]):
        try:
            verify_bytes(invalid)
        except RuntimeError as error:
            assert str(error) == 'Unsupported executable build'
        else:
            raise AssertionError('Unpinned executable accepted')
    with tempfile.TemporaryDirectory(prefix='dt-build-pins-') as tmp:
        fixture = Path(tmp) / 'fixture.bin'
        for variant in (original, laa):
            fixture.write_bytes(variant)
            assert verify_executable(fixture) == verify_bytes(variant)
        bundled = Path(tmp) / 'runtime' / 'S4ModApi.dll'
        bundled.parent.mkdir();bundled.write_bytes(bundled_api().read_bytes())
        installed = Path(tmp) / 'S4ModApi.dll'
        target, api = select_api(fixture, bundled)
        assert target == bundled and api['sha256'] == BUNDLED_API_SHA256 and api['source'] == 'bundled'
        guard=Path(tmp)/'S4ModApiNoUpdate.cfg';guard.write_text('// fixture only\n')
        known=[bundled.read_bytes()]
        live_api=game_dir()/'S4ModApi.dll'
        if live_api.exists():known.append(live_api.read_bytes())
        for data in known:
            installed.write_bytes(data);target,api=select_api(fixture,bundled)
            assert target==installed and api['source']=='installed' and api['sha256'] in (BUNDLED_API_SHA256,SU_API_SHA256)
        installed.write_bytes(installed.read_bytes()+b'changed')
        try:select_api(fixture,bundled)
        except RuntimeError as error:assert str(error).startswith('Unsupported S4ModApi build:')
        else:raise AssertionError('Unknown installed API accepted or silently replaced')
        installed.write_bytes(bundled.read_bytes());guard.unlink()
        try:select_api(fixture,bundled)
        except RuntimeError as error:assert str(error)=='Installed API update guard missing'
        else:raise AssertionError('Unprotected installed API accepted')
        assert verify_hd(fixture)=={}
        plugins=Path(tmp)/'Plugins';plugins.mkdir()
        game_plugins=game_dir()/'Plugins'
        if (game_plugins/'S4HD-Patch.asi').exists():
            for name in HD_FILES:(plugins/name).write_bytes((game_plugins/name).read_bytes())
            assert verify_hd(fixture)==HD_FILES
            target=plugins/'S4HD-Manager.dll';target.write_bytes(target.read_bytes()+b'changed')
            try:verify_hd(fixture)
            except RuntimeError as error:assert str(error).startswith('Unsupported HD renderer build:')
            else:raise AssertionError('Unpinned HD manager accepted')
    report = dict(version='0.2.15', observed=observed, exact_hash_pins=[ORIGINAL_SHA256, LAA_SHA256],
                  only_difference='IMAGE_FILE_LARGE_ADDRESS_AWARE bit 0x20 at file offset 0x1a6',
                  both_variants_accepted=True, unrelated_code_header_append_truncation_refused=True,
                  single_installed_api_selected=True, bundled_api_fallback_without_installed_api=True,
                  unknown_installed_api_and_missing_update_guard_refused=True,
                  installed_hd_exact_pins_and_changed_manager_refusal=True,
                  game_started=False, live_process_read=False, game_files_written=False)
    (ROOT / 'research/game-build-variants.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS: original and exact Settlers United LAA executable pins; unrelated changes refused')


if __name__ == '__main__':
    run()
