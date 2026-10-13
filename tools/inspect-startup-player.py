"""Replay Trace024's index arithmetic against pinned instructions, file-only."""
import hashlib,json,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from project_paths import game_exe
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile

def run():
    exe=game_exe()
    game_build=verify_executable(exe)
    pe=pefile.PE(str(exe));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def check(rva,size,*expected):
        rows=[(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,size),base+rva)]
        for row in expected:assert row in rows,(hex(rva),row)
    check(0x1a3663,6,('mov','edx, dword ptr [0x129482c]'))
    check(0x1a36b4,46,('sub','ecx, dword ptr [esi + 0x658]'),
          ('cmp','ecx, 1'),('dec','dl'),('movzx','eax, dl'),
          ('lea','eax, [eax + ecx*8]'),
          ('mov','esi, dword ptr [esi + eax*4 + 0x14]'),
          ('mov','dword ptr [esi + 0xc], 0'))
    check(0x11437a,5,('call','0x5a3630'))
    # Trace024: ECX=1, EDX=255, EAX=263, ESI=0x002f004d.
    def slot(player,tick_delta):return ((player-1)&255)+tick_delta*8
    assert slot(0,1)==0x107 and slot(1,1)==8 and slot(2,1)==9
    assert 0x002f004d+12==0x002f0059
    report=dict(version='0.2.15',game_sha256=game_build['sha256'],game_code_sha256=ORIGINAL_SHA256,crash_trace='Trace024.txt',
        crash_time='2026-10-12 15:40:14 Europe/Warsaw',crash_rva='0x1A36D2',
        caller_rva='0x11437A',local_player_byte_before_decrement=0,
        table_index_observed=263,valid_player1_index_for_same_tick=8,
        invalid_pointer='0x002F004D',write_address='0x002F0059',
        helper_started='15:39:35',helper_finished='15:39:45',trace_opened='15:40:03',
        map_change_causality_proven=False,gameplay_resolution_verified=False,
        game_started=False,live_process_read=False)
    (ROOT/'research/startup-player-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: pinned Trace024 fault/caller and player-zero index 263 replay; no live game access')

if __name__=='__main__':run()
