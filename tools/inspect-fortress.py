"""Repeat pinned Fortress crash analysis using only the executable on disk."""
import hashlib
import json
import struct
import sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from project_paths import game_exe

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'work/python'))
import capstone
import pefile



def run():
    executable = game_exe()
    game_build = verify_executable(executable)
    pe = pefile.PE(str(executable))
    base = pe.OPTIONAL_HEADER.ImageBase
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    def instructions(rva, count):
        return list(cs.disasm(pe.get_data(rva, count), base + rva))

    parser = instructions(0x139919, 62)
    assert any(i.mnemonic == 'push' and i.op_str == '0x1058854' for i in parser)
    assert pe.get_string_at_rva(0x1058854 - base) == b'tool'
    assert any(i.mnemonic == 'mov' and i.op_str == 'byte ptr [ebx + 0x1df], al' for i in parser)
    resolver = instructions(0x22899b, 14)
    assert resolver[0].mnemonic == 'test' and resolver[0].op_str == 'eax, eax'
    assert resolver[1].mnemonic == 'je' and resolver[1].op_str == '0x628a23'
    assert resolver[2].mnemonic == 'cmp' and resolver[2].op_str == 'byte ptr [eax], 0'
    assert resolver[3].mnemonic == 'je' and resolver[3].op_str == '0x628a23'
    failure = instructions(0x228a27, 3)[0]
    assert failure.mnemonic == 'or' and failure.op_str == 'eax, 0xffffffff'
    assert struct.unpack('<II', pe.get_data(0xc1a248, 8)) == (0, 0x103cfb0)
    assert pe.get_string_at_rva(0x103cfb0 - base) == b'GOOD_NO_GOOD'
    panel = instructions(0x143dc2, 30)
    assert any(i.mnemonic == 'movsx' and i.op_str == 'ebx, byte ptr [eax + 0x1df]' for i in panel)
    assert any(i.mnemonic == 'test' and i.op_str == 'ebx, ebx' for i in panel)
    assert any(i.mnemonic == 'je' and i.op_str == '0x543def' for i in panel)
    assert any(i.mnemonic == 'call' and i.op_str == '0x4d4280' for i in panel)
    fault = instructions(0xd428a, 8)[0]
    assert fault.mnemonic == 'movzx' and fault.op_str == 'esi, word ptr [ecx + eax*2 + 0x130]'
    assert 0 + (-1) * 2 + 0x130 == 0x12e
    units = struct.unpack('<6I', pe.get_data(0xc1a230, 24))
    assert units == (0, 53, 52, 54, 29, 32)
    report = {
        'game_sha256': game_build['sha256'], 'game_code_sha256': ORIGINAL_SHA256, 'game_started': False, 'live_process_read': False,
        'tool_parser_rva': '0x139919', 'tool_field_offset': '0x1DF',
        'empty_enum_result': -1, 'explicit_no_good': 0,
        'panel_read_rva': '0x143DC8', 'zero_skips_goods_lookup': True,
        'fault_rva': '0xD428A', 'logged_null_goods_sector': True,
        'computed_fault_read': '0x12E',
        'native_queue_units': list(units), 'gameplay_resolution_verified': False,
        'boundary': 'Static matching failure path; no proof that all construction crashes are resolved',
    }
    (ROOT / 'research/fortress-native-checks.json').write_text(json.dumps(report, indent=2))
    print('PASS: pinned parser, empty-enum failure, explicit zero, panel branch, crash operand and native units')


if __name__ == '__main__':
    run()
