"""Verify the pinned Dark placement rejection branch from the executable file."""
import hashlib,json,struct,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from project_paths import game_exe
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile
def run():
    exe=game_exe();game_build=verify_executable(exe)
    pe=pefile.PE(str(exe));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def check(rva,n,*rows):
        ins=[(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,n),base+rva)]
        for row in rows:assert row in ins,(hex(rva),row)
    assert struct.unpack('<I',pe.get_data(0x13eb58+8,4))[0]==base+0x13e8e8
    check(0x13e8e8,30,('call','0x53ebc0'))
    check(0x13ecb3,76,('and','cl, 0x5f'),('cmp','cl, 4'),('jne','0x53edde'),
          ('cmp','ecx, 0x10'),('cmp','ecx, 0x40'),('cmp','ecx, 0x90'),('cmp','edi, 0x30'))
    check(0x20d72b,26,('mov','bl, byte ptr [edi + 2]'),('or','bl, 4'))
    # Map size (native/map-size.hpp): RVA 0x1054138 is the row stride of the landscape (0x11630D4)
    # and flag (0x11630D8) arrays in the Dark validator and its neighbourhood pass; the view setup
    # (0x114B90 -> 0x2589D0) stores that same stride at 0xD6921C next to the landscape copy 0xD69220.
    stride,view=hex(base+0x1054138),hex(base+0xd6921c)
    check(0x13ec48,18,('mov',f'edi, dword ptr [{stride}]'),('mov',f'ebx, dword ptr [{hex(base+0x11630d8)}]'),('imul','edi, ecx'))
    check(0x13dcab,8,('mov',f'eax, dword ptr [{stride}]'),('imul','edi, eax'))
    check(0x114b90,17,('push',f'dword ptr [{hex(base+0x11630d4)}]'),('push',f'dword ptr [{stride}]'),('call',hex(base+0x2589d0)))
    check(0x2589d5,0x31,('mov','edi, dword ptr [ebp + 8]'),('mov','esi, dword ptr [ebp + 0xc]'),('mov',f'dword ptr [{view}], edi'),('mov',f'dword ptr [{hex(base+0xd69220)}], esi'))
    report=dict(version='0.2.15',game_sha256=game_build['sha256'],game_code_sha256=ORIGINAL_SHA256,game_started=False,live_process_read=False,
        dark_validator_rva='0x13EBC0',native_flags_mask='0x5F',required_masked_flags=4,
        discarded_map_flags_on_explicitly_prepared_terrain='0x43',map_stride_rva='0x1054138',view_map_size_rva='0xD6921C (copied from the stride at 0x2589D0)',
        retained_owner_and_resources=True,gameplay_placement_verified=False)
    (ROOT/'research/placement-native-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: pinned Dark race dispatch, native flag-4-only placement guard, terrain rules, map flag loader and map stride/view size')
if __name__=='__main__':run()
