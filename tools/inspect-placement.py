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
    report=dict(version='0.2.15',game_sha256=game_build['sha256'],game_code_sha256=ORIGINAL_SHA256,game_started=False,live_process_read=False,
        dark_validator_rva='0x13EBC0',native_flags_mask='0x5F',required_masked_flags=4,
        discarded_map_flags_on_explicitly_prepared_terrain='0x43',
        retained_owner_and_resources=True,gameplay_placement_verified=False)
    (ROOT/'research/placement-native-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: pinned Dark race dispatch, native flag-4-only placement guard, terrain rules and map flag loader')
if __name__=='__main__':run()
