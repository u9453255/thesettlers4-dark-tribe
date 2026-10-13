"""Strict file-only pins for the original executable and its verified LAA variant."""
import hashlib
import struct
from pathlib import Path

ORIGINAL_SHA256 = 'f6dbb89072f3f0fefa35de23810a22317b8621e7a8a0aec2503ef28aaa25ea0b'
LAA_SHA256 = '3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816'
SUPPORTED_GAME_HASHES = frozenset((ORIGINAL_SHA256, LAA_SHA256))
BUNDLED_API_SHA256 = '73482077644fd1fa8eadfaf9fa4d0ad2799f381713a3204988493e8261b26651'
SU_API_SHA256 = 'ed111491b1a6bd2822ccd21fec31adedfb35b6bf311cd568d9ff0e6e2193126f'
API_VERSIONS = {BUNDLED_API_SHA256: '2.3.0.0', SU_API_SHA256: '2.3.2.0'}
HD_FILES = {'S4HD-Patch.asi':'9dcc4a187da18702f17636daa61d8f7478f4454b8b875eac14886c4501cea5e8',
            'S4HD-Manager.dll':'9299b9f4ea77b6b34b43897706a78571518c7760e110772bd42d86d01bd5b0a7'}


def verify_bytes(raw):
    actual = hashlib.sha256(raw).hexdigest()
    if actual not in SUPPORTED_GAME_HASHES:
        raise RuntimeError('Unsupported executable build')
    pe = struct.unpack_from('<I', raw, 0x3c)[0]
    offset = pe + 22
    if offset != 0x1a6 or raw[pe:pe+4] != b'PE\0\0':
        raise RuntimeError('Unsupported executable header')
    flags = struct.unpack_from('<H', raw, offset)[0]
    if flags != (0x122 if actual == LAA_SHA256 else 0x102):
        raise RuntimeError('Unsupported executable characteristics')
    normalized = bytearray(raw)
    struct.pack_into('<H', normalized, offset, flags & ~0x20)
    if hashlib.sha256(normalized).hexdigest() != ORIGINAL_SHA256:
        raise RuntimeError('Executable code differs from inspected binary')
    return dict(sha256=actual, code_sha256=ORIGINAL_SHA256,
                large_address_aware=bool(flags & 0x20), characteristics_offset='0x1a6')


def verify_executable(path):
    return verify_bytes(Path(path).read_bytes())


def select_api(game_executable, bundled_api):
    """Use the game's already-shared API when present; refuse an unknown build."""
    installed = Path(game_executable).parent / 'S4ModApi.dll'
    target = installed if installed.exists() else Path(bundled_api)
    actual = hashlib.sha256(target.read_bytes()).hexdigest()
    if actual not in API_VERSIONS:
        raise RuntimeError('Unsupported S4ModApi build: ' + str(target))
    if installed.exists() and not any((installed.parent / name).is_file() for name in ('S4ModApiNoUpdate.cfg', 'NoUpdateForS4ModApi.txt')):
        raise RuntimeError('Installed API update guard missing')
    return target, dict(source='installed' if installed.exists() else 'bundled',
                        sha256=actual, version=API_VERSIONS[actual])


def verify_hd(game_executable):
    plugins=Path(game_executable).parent/'Plugins'
    if not (plugins/'S4HD-Patch.asi').exists():return {}
    for name,expected in HD_FILES.items():
        path=plugins/name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
            raise RuntimeError('Unsupported HD renderer build: '+str(path))
    return dict(HD_FILES)
