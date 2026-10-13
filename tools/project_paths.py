"""Where the repository, the game folder and the local build tools are.

The game folder is resolved in this order (first match wins):
1. the environment variable DARK_TRIBE_GAME_DIR;
2. "game_dir" in local-config.json at the repository root (git-ignored,
   written by tools/setup.py);
3. C:/Projects/thesettlers4, the original author's layout, if S4_Main.exe is there.

Node.js and Python for the offline workflow are resolved the same way
(DARK_TRIBE_NODE / DARK_TRIBE_PYTHON, then "node" / "python" in
local-config.json, then PATH). Nothing here reads or writes game files.
"""
import hashlib
import json
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'local-config.json'
LEGACY_GAME_DIR = Path('C:/Projects/thesettlers4')
SDK = ROOT / 'work/native-sdk/S4ModApi'
COMPILER = ROOT / 'work/native-sdk/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/i686-w64-mingw32-clang++.exe'
PYTHON_LIBS = ROOT / 'work/python'
# GameData/buildingInfo.xml of the pinned History Edition, before this mod changes it.
ORIGINAL_BUILDING_XML_SHA256 = 'b1828250fb9a8794e98305458716ad3299d8a5d61583268d343da786abc6c0cb'
NOT_CONFIGURED = ('The Settlers IV game folder is not configured. Run '
                  'python tools/setup.py --game "<folder containing S4_Main.exe>" '
                  'or set the environment variable DARK_TRIBE_GAME_DIR.')


def config():
    """Contents of the optional local-config.json ({} when absent)."""
    try:
        return json.loads(CONFIG.read_text(encoding='utf-8'))
    except FileNotFoundError:
        return {}


def game_dir():
    """The configured game folder; it must contain S4_Main.exe."""
    for source, value in (('DARK_TRIBE_GAME_DIR', os.environ.get('DARK_TRIBE_GAME_DIR')),
                          ('local-config.json', config().get('game_dir'))):
        if value:
            folder = Path(value)
            if not (folder / 'S4_Main.exe').is_file():
                raise RuntimeError(f'{source} points to {folder}, which has no S4_Main.exe')
            return folder
    if (LEGACY_GAME_DIR / 'S4_Main.exe').is_file():
        return LEGACY_GAME_DIR
    raise RuntimeError(NOT_CONFIGURED)


def game_exe():
    return game_dir() / 'S4_Main.exe'


def original_building_xml(folder=None):
    """The unmodified GameData/buildingInfo.xml: the install backup once the
    launcher has installed the mod, otherwise the game's own file. Both must
    match the pinned original hash."""
    backup = ROOT / 'backups/original-0-buildingInfo.xml'
    source = backup if backup.is_file() else (folder or game_dir()) / 'GameData/buildingInfo.xml'
    if hashlib.sha256(source.read_bytes()).hexdigest() != ORIGINAL_BUILDING_XML_SHA256:
        raise RuntimeError('Unexpected buildingInfo.xml (not the pinned original): ' + str(source))
    return source


def bundled_api():
    """The bundled S4ModApi 2.3.0: the staged runtime copy once built, else the SDK copy (same pinned file)."""
    staged = ROOT / 'runtime/native/S4ModApi.dll'
    return staged if staged.is_file() else SDK / 'S4ModApi.dll'


def tool(name):
    """Path of an external program (node, python) or None when it is not found."""
    value = os.environ.get('DARK_TRIBE_' + name.upper()) or config().get(name)
    if value:
        return value if Path(value).is_file() else shutil.which(value)
    return shutil.which(name)
