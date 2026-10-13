"""Repeat the failed 0.2.4 path from the pinned executable on disk only."""
import hashlib,json,struct,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from project_paths import game_exe
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import pefile,capstone

def run():
    exe=game_exe()
    game_build=verify_executable(exe)
    pe=pefile.PE(str(exe));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def check(rva,size,*expected):
        ins=[(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,size),base+rva)]
        for row in expected:assert row in ins,(hex(rva),row)
    check(0x20d71f,14,('movzx','ecx, byte ptr [edi + 1]'),('mov','word ptr [eax + edx*2], cx'))
    # Initial sector records 1 and 2 have owner nibble 1 and 2 respectively.
    check(0x1b1f10,65,('mov','dword ptr [0x12f1a34], 0x21'),('mov','dword ptr [0x12f1a48], 0x42'))
    check(0x1b3d40,65,('mov','ecx, dword ptr [eax*4 + 0x12f1a20]'),('and','ecx, 0xf'))
    check(0x1b50a0,104,('and','edx, 0xf'),('call','0x4dec50'),('call','0x5b5ff0'))
    check(0x134b30,80,('mov','eax, dword ptr [esi*4 + 0x146c8a4]'))
    check(0x134bfe,8,('cmp','byte ptr [eax + ecx + 0x33c], 0'))
    assert 9+0+0x33c==0x345
    col=struct.unpack('<I',pe.get_data(0xc58480,4))[0]
    typ=struct.unpack('<I',pe.get_data(col-base+12,4))[0]
    assert b'CBarrackRole' in pe.get_string_at_rva(typ-base+8)
    check(0x1c83a1,65,('call','eax'),('mov','eax, dword ptr [eax + 0x10]'))
    report=dict(version='0.2.15',game_sha256=game_build['sha256'],game_code_sha256=ORIGINAL_SHA256,game_started=False,live_process_read=False,
        crash_rva='0x134BFE',role='CBarrackRole',null_economy_pointer_read='0x345',
        map_chunk6_byte1='initial owner ID, converted by native sector initializer',
        native_default_end_check_also_consults_game_mode=True,
        gameplay_resolution_verified=False,actual_victory_winner_or_trigger_verified=False)
    (ROOT/'research/startup-sector-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: pinned map ownership loader, sector ownership/economy creation, barracks RTTI/fault operands and default end game-mode branch')

if __name__=='__main__':run()
