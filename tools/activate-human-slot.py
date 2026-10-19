"""Assign this offline scenario's already-human slot to the local host.

Changes one game-session field in memory, not executable code or game files.
Offsets are pinned to the locally inspected 2.50.1516.0 x86 build.
"""
import argparse,ctypes,hashlib,json,struct,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from scenarios import layouts
from ctypes import wintypes as W
from importlib.util import spec_from_file_location,module_from_spec
ROOT=Path(__file__).resolve().parents[1]
spec=spec_from_file_location('runtime',ROOT/'tools/inspect-runtime.py');runtime=module_from_spec(spec);spec.loader.exec_module(runtime)

def activate(pid):
    report=runtime.inspect(pid)
    pin=json.loads((ROOT/'research/native-build-pin.json').read_text())
    actual=verify_executable(report['executable'])
    if pin['sha256']!=ORIGINAL_SHA256 or actual['code_sha256']!=pin['sha256']:raise RuntimeError('Game build differs from inspected binary')
    if not matches_layout(report):
        # Name what the game reported, so a refused layout can be told apart in session-helper.jsonl.
        seen=[(q.get('0x18c'),q.get('0xfc'),q.get('0x120')) for q in report.get('players',[])]
        raise RuntimeError(f"Wrong scenario layout: players={report.get('playerCount')} {seen} mapSize={report.get('mapSize')}")
    players=report['players'];a=players[0]
    if a['0x90']!=0 or any(q['0x90']!=1 for q in players[1:]) or any(report['skipFlags'][:len(players)]):raise RuntimeError('Unexpected player slot types')
    if report['hostId']!=0:raise RuntimeError('Only the offline local host is supported')
    if report['localPlayer'] not in (0,1):raise RuntimeError('Unexpected local player')
    if a['0xd8']==0:return report
    if a['0xd8']!=0xffffffff:raise RuntimeError('Slot already assigned to another actor')
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
    k.WriteProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)];k.WriteProcessMemory.restype=W.BOOL
    k.CloseHandle.argtypes=[W.HANDLE]
    h=k.OpenProcess(0x28,False,pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    value=ctypes.c_uint32(0);written=ctypes.c_size_t()
    target=int(report['setupPointer'],16)+0xd8
    try:
        if not k.WriteProcessMemory(h,ctypes.c_void_p(target),ctypes.byref(value),4,ctypes.byref(written)) or written.value!=4:raise ctypes.WinError(ctypes.get_last_error())
    finally:k.CloseHandle(h)
    checked=runtime.inspect(pid)
    if checked['players'][0]['0xd8']!=0:raise RuntimeError('Assignment did not persist')
    (ROOT/'evidence/human-slot-activation.json').write_text(json.dumps(dict(before=report,after=checked),indent=2))
    return checked

def matches_layout(report):
    """Exact layout of one of this mod's scenarios (tools/scenarios.py), including the legacy First Dawn save layout."""
    players=report.get('players',[])
    if report.get('playerCount')!=len(players):return False
    actual=tuple((q.get('0x18c'),q.get('0xfc'),q.get('0x120')) for q in players)
    # Players, races and starts only: the slot must be assigned in the lobby, before the map loads, and the
    # map size (view size RVA 0xD6921C) is only written while the map loads. The plugin checks the size in game.
    return any(actual==starts for size,starts in layouts())

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('pid',type=int);a=p.parse_args();activate(a.pid);print('Dark Tribe human slot assigned to the offline local host')
