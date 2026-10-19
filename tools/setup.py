"""One-time build setup on this machine. Never starts, inspects or changes the game.

  python tools/setup.py --game "D:/Games/thesettlers4" [--node "C:/Program Files/nodejs/node.exe"]
  python tools/setup.py --check     # report only: no downloads, installs or config changes

- Records the game folder (and optionally node/python) in the git-ignored local-config.json.
- Downloads the pinned LLVM-MinGW compiler and S4ModApi 2.3 SDK into work/native-sdk and
  checks their SHA-256 before use (an existing, verified copy is reused).
- Installs missing Python helpers from tools/requirements.txt into work/python.
- Reports whether the game files match the exact build this mod is pinned to.
"""
import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

import project_paths as paths
from game_build import BUNDLED_API_SHA256, select_api, verify_executable, verify_hd
from scenarios import SCENARIOS

ROOT = paths.ROOT
SDK_ROOT = ROOT / 'work/native-sdk'
COMPILER_PIN = json.loads((ROOT / 'research/compiler-download.json').read_text(encoding='utf-8'))
API_PIN = dict(url='https://github.com/nyfrk/S4ModApi/releases/download/v2.3/S4ModApi_v2_3.zip',
               sha256='11dd2c51aeccc23aab5e4bb597762a6cc010111bbf310c80debf6dfaf1821254')
API_FILES = ('S4ModApi.dll', 'S4ModApi.lib', 'S4ModApi.h', 'COPYING.txt', 'COPYING.LESSER.txt', 'README.md')
MODULES = {'pefile': 'pefile', 'capstone': 'capstone', 'dnfile': 'dnfile', 'dncil': 'dncil', 'Pillow': 'PIL', 'lupa': 'lupa'}
problems = []


def report(ok, label, detail=''):
    print(('OK       ' if ok else 'PROBLEM  ') + label + (': ' + detail if detail else ''))
    if not ok:
        problems.append(label)


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def download(url, target, expected):
    if target.is_file() and sha256(target) == expected:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + '.part')
    print('Downloading', url)
    with urllib.request.urlopen(url, timeout=60) as response, open(part, 'wb') as out:
        while block := response.read(1 << 20):
            out.write(block)
    actual = sha256(part)
    if actual != expected:
        part.unlink()
        raise RuntimeError(f'Hash mismatch for {url}: {actual}')
    part.replace(target)
    return target


def extract(archive, destination, names=None):
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as z:
        for member in z.namelist():
            if names is not None and member not in names:
                continue
            if not (destination / member).resolve().is_relative_to(destination):
                raise RuntimeError('Unsafe path in archive: ' + member)
            z.extract(member, destination)


def compiler(check):
    if paths.COMPILER.is_file():
        return report(True, 'Compiler', str(paths.COMPILER.relative_to(ROOT)))
    if check:
        return report(False, 'Compiler', 'missing (run without --check to download LLVM-MinGW 20261006, about 190 MB)')
    archive = download(COMPILER_PIN['browser_download_url'], SDK_ROOT / 'llvm-mingw.zip', COMPILER_PIN['digest'].split(':', 1)[1])
    print('Extracting the compiler ...')
    extract(archive, SDK_ROOT / 'compiler')
    report(paths.COMPILER.is_file(), 'Compiler', 'installed under work/native-sdk/compiler')


def modapi(check):
    have = all((paths.SDK / name).is_file() for name in API_FILES[:3])
    if have and sha256(paths.SDK / 'S4ModApi.dll') == BUNDLED_API_SHA256:
        return report(True, 'S4ModApi 2.3 SDK', str(paths.SDK.relative_to(ROOT)))
    if check:
        return report(False, 'S4ModApi 2.3 SDK', 'missing or changed (run without --check to download it)')
    archive = download(API_PIN['url'], SDK_ROOT / 'S4ModApi_v2_3.zip', API_PIN['sha256'])
    extract(archive, paths.SDK, set(API_FILES))
    report(sha256(paths.SDK / 'S4ModApi.dll') == BUNDLED_API_SHA256, 'S4ModApi 2.3 SDK', 'installed under work/native-sdk/S4ModApi')


def python_modules(check):
    if sys.version_info < (3, 10):
        report(False, 'Python', 'version 3.10 or newer is required (3.12 is tested)')
    sys.path.insert(0, str(paths.PYTHON_LIBS))
    missing = [dist for dist, module in MODULES.items() if importlib.util.find_spec(module) is None]
    if missing and not check:
        pins = [line.strip() for line in (ROOT / 'tools/requirements.txt').read_text().splitlines()
                if line.strip() and not line.startswith('#') and line.split('==')[0] in missing]
        subprocess.run([sys.executable, '-m', 'pip', 'install', '--target', str(paths.PYTHON_LIBS), *pins], check=True)
        importlib.invalidate_caches()
        missing = [dist for dist, module in MODULES.items() if importlib.util.find_spec(module) is None]
    report(not missing, 'Python helpers', 'missing ' + ', '.join(missing) if missing else 'all present')


def node(choice):
    found = choice or paths.tool('node')
    if not found:
        return report(False, 'Node.js', 'not found: install Node.js (LTS), put it on PATH or pass --node <node.exe>')
    version = subprocess.run([found, '--version'], capture_output=True, text=True).stdout.strip()
    report(bool(version), 'Node.js', f'{found} {version}')


def game(folder):
    if folder is None:
        try:
            folder = paths.game_dir()
        except RuntimeError as error:
            return report(False, 'Game folder', str(error))
    exe = folder / 'S4_Main.exe'
    if not exe.is_file():
        return report(False, 'Game folder', f'{folder} has no S4_Main.exe')
    report(True, 'Game folder', str(folder))
    try:
        build = verify_executable(exe)
        report(True, 'S4_Main.exe', 'pinned 2.50.1516.0' + (' (Settlers United LAA variant)' if build['large_address_aware'] else ''))
    except RuntimeError:
        report(False, 'S4_Main.exe', f'not the pinned build (SHA-256 {sha256(exe)})')
    try:
        source = paths.original_building_xml(folder)
        report(True, 'GameData/buildingInfo.xml', 'pinned original' + (' (install backup)' if source.is_relative_to(ROOT) else ''))
    except (RuntimeError, OSError) as error:
        report(False, 'GameData/buildingInfo.xml', str(error))
    for scenario in SCENARIOS:  # each playable scenario is generated from one original campaign map
        campaign = folder / scenario['source']
        ok = campaign.is_file() and sha256(campaign) == scenario['source_sha256']
        report(ok, scenario['source'], ('pinned original' if ok else 'missing or different') + ' (source of ' + scenario['title'] + ')')
    installed = folder / 'S4ModApi.dll'
    if installed.is_file():
        try:
            _, api = select_api(exe, installed)
            report(True, 'S4ModApi.dll in the game folder', 'pinned ' + api['version'] + ', shared with this mod')
        except RuntimeError as error:
            report(False, 'S4ModApi.dll in the game folder', str(error))
    else:
        report(True, 'S4ModApi.dll in the game folder', 'none; the bundled S4ModApi 2.3.0 is used')
    try:
        report(True, 'HD patch', 'pinned Settlers United HD files' if verify_hd(exe) else 'not installed (classic renderer)')
    except RuntimeError as error:
        report(False, 'HD patch', str(error))
    return folder


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--game', type=Path, help='game folder containing S4_Main.exe (must be writable)')
    parser.add_argument('--node', help='node.exe to use when Node.js is not on PATH')
    parser.add_argument('--python', help='python.exe for the launcher, when python is not on PATH')
    parser.add_argument('--check', action='store_true', help='only report; download, install and write nothing')
    args = parser.parse_args()
    folder = game(args.game.resolve() if args.game else None)
    node(args.node)
    python_modules(args.check)
    compiler(args.check)
    modapi(args.check)
    if not args.check and (args.game or args.node or args.python):
        config = paths.config()
        if args.game and folder:
            config['game_dir'] = str(args.game.resolve())
        if args.node:
            config['node'] = str(Path(args.node).resolve())
        if args.python:
            config['python'] = str(Path(args.python).resolve())
        paths.CONFIG.write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
        print('Saved', paths.CONFIG.name)
    if problems:
        print('\n' + str(len(problems)) + ' problem(s). Game-file problems mean this game is not the build the mod is pinned to;'
              ' see "Not working on your setup?" in README.md.')
        sys.exit(1)
    print('\nReady. Next: python tools/workflow.py, then Launch Dark Tribe.cmd.')


if __name__ == '__main__':
    main()
