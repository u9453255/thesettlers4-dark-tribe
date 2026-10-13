"""Checks, file-only, the native recruitment facts the Temple/Fortress panel relies on.

- The barracks recruitment menu (Menu/GUISetRomans.DAT menu 46) has the control
  cluster: elements 1766..1771 with sprites 124 (infinite), 123 (zero), 125/117
  (up/down 5) and 122/121 (up/down 1), 26x28, and pressed type tabs whose gold
  corners are the selection bracket (#487 = #486 plus corners).
- Its click dispatch (RVA 0xAF67F) posts message 0x1CE with the selected type and
  the amounts +100, 0, +5, -5, +1, -1 for those elements.
- The Dark Temple role's order handler (RVA 0x145D60) clamps finite orders to
  0..99, sets 100 for amounts >= 100 and clears for amounts <= -100.
- Dark Temple recruitment (RVA 0x145760) decrements a queue only below 100, so
  100 recruits continuously; queue index -> settler type is (0,53,52,54,29,32)
  and the role loads/saves the six queue DWORDs at +0x180 itself.
No game process is started or read.
"""
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
import guiset


def run():
    executable = game_exe()
    game_build = verify_executable(executable)
    pe = pefile.PE(str(executable), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    def code(rva, size):
        return [(i.mnemonic, i.op_str) for i in cs.disasm(pe.get_data(rva, size), base + rva)]

    # GUI definition: menu 46 control cluster and the pressed tabs.
    menu = guiset.load('Romans')[46]
    controls = {e[5]: (e[0], e[1], e[2], e[3], e[4], e[7]) for e in menu['els']}
    assert controls[1766] == (12, 280, 26, 28, 124, 116) and controls[1767] == (12, 309, 26, 28, 123, 115)
    assert controls[1768] == (65, 280, 26, 28, 125, 118) and controls[1769] == (65, 309, 26, 28, 117, 127)
    assert controls[1770] == (119, 280, 26, 28, 122, 120) and controls[1771] == (119, 309, 26, 28, 121, 119)
    assert controls[1776] == (12, 104, 32, 32, 486, 487)

    # Barracks control dispatch: element id - 1766 through a byte table, message 0x1CE with the amount.
    dispatch = code(0xAF67F, 0x1C)
    assert dispatch[0] == ('add', 'eax, 0xfffff91a') and dispatch[1] == ('cmp', 'eax, 0x16')
    assert dispatch[3] == ('movzx', 'eax, byte ptr [eax + 0x4afa80]') and dispatch[4] == ('jmp', 'dword ptr [eax*4 + 0x4afa4c]')
    cases = pe.get_data(0xAFA80, 6)
    targets = struct.unpack('<13I', pe.get_data(0xAFA4C, 52))
    amounts = []
    for element in range(6):
        handler = code(targets[cases[element]] - base, 0x20)
        assert handler[:3] == [('push', '0')] * 3 and handler[3][0] == 'push' and handler[4] == ('push', 'dword ptr [0x15aad24]')
        assert handler[5][0] == 'lea' and handler[6] == ('push', '0x1ce') and handler[7] == ('call', '0x44ccc0')
        amounts.append(int(handler[3][1], 0))
    assert amounts == [100, 0, 5, -5, 1, -1], amounts

    # Dark Temple role order handler.
    order = code(0x145D60, 0x8A)
    expected = [('cmp', 'eax, 0x2710'), ('cmp', 'eax, -0x64'), ('cmp', 'eax, 0x64'), ('cmp', 'ecx, 0x64'),
                ('mov', 'dword ptr [esi + edx*4 + 0x180], 0x63'), ('mov', 'dword ptr [esi + edx*4 + 0x180], 0x64')]
    for item in expected:
        assert item in order, item
    assert ('movzx', 'edx, byte ptr [edx + 0x15ab040]') in order

    # Recruitment: a queue below 100 is decremented before the birth; 100 is never decremented.
    recruit = code(0x14583B, 0x14)
    assert recruit[:5] == [('mov', 'eax, dword ptr [edi + ebx*4 + 0x180]'), ('cmp', 'eax, 0x64'), ('jge', '0x54584f'),
                           ('dec', 'eax'), ('mov', 'dword ptr [edi + ebx*4 + 0x180], eax')]
    skip = code(0x1457D0, 0x20)
    assert skip[1] == ('je', '0x5457e3') and skip[2] == ('cmp', 'edx, 1') and skip[4] == ('cmp', 'dword ptr [edi + edx*4 + 0x180], 0')
    units = struct.unpack('<6i', pe.get_data(0xC1A230, 24))
    assert units == (0, 53, 52, 54, 29, 32)

    # The role loads the six queue DWORDs itself (saved games keep 100).
    load = code(0x14537D, 0x20)
    assert load[0] == ('lea', 'esi, [ebx + 0x180]') and load[1] == ('mov', 'ebx, 6') and load[4] == ('call', '0x4be8e0')

    report = {
        'game_sha256': game_build['sha256'], 'game_code_sha256': ORIGINAL_SHA256, 'game_started': False, 'live_process_read': False,
        'barracks_menu': 46, 'cluster_elements': [1766, 1767, 1768, 1769, 1770, 1771], 'cluster_amounts': amounts,
        'dispatch_rva': '0xAF67F', 'temple_order_rva': '0x145D60', 'temple_recruit_rva': '0x145760', 'infinite_count': 100, 'finite_max': 99,
        'queue_units': list(units), 'queue_saved_by_role': True, 'bracket_sprite': '9.gfx #487 (pressed #486)',
        'gameplay_verified': False,
    }
    (ROOT / 'research/recruitment-native-checks.json').write_text(json.dumps(report, indent=2))
    print('PASS: native barracks cluster amounts, Dark Temple order clamps, continuous queue (100) and saved queue layout')


if __name__ == '__main__':
    run()
