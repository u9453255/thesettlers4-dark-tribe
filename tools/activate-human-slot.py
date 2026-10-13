"""Assign this offline scenario's already-human slot to the local host.

Changes one game-session field in memory, not executable code or game files.
Offsets are pinned to the locally inspected 2.50.1516.0 x86 build.
"""
import argparse,ctypes,hashlib,json,struct,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from ctypes import wintypes as W
from importlib.util import spec_from_file_location,module_from_spec
ROOT=Path(__file__).resolve().parents[1]
spec=spec_from_file_location('runtime',ROOT/'tools/inspect-runtime.py');runtime=module_from_spec(spec);spec.loader.exec_module(runtime)

def activate(pid):
    report=runtime.inspect(pid)
    pin=json.loads((ROOT/'research/native-build-pin.json').read_text())
    actual=verify_executable(report['executable'])
    if pin['sha256']!=ORIGINAL_SHA256 or actual['code_sha256']!=pin['sha256']:raise RuntimeError('Game build differs from inspected binary')
    a,b=report['players']
    if not matches_layout(report):raise RuntimeError('Wrong scenario layout')
    if a['0x90']!=0 or b['0x90']!=1 or report['skipFlags'][:2]!=[0,0]:raise RuntimeError('Unexpected player slot types')
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
    """Exact new-scenario layout or unchanged legacy layout for paired old saves."""
    if report.get('playerCount')!=2 or len(report.get('players',[]))!=2:return False
    a,b=report['players']
    return (a.get('0x18c')==3 and b.get('0x18c')==1
            and (a.get('0xfc'),a.get('0x120'))==(277,214)
            and (b.get('0xfc'),b.get('0x120')) in ((75,401),(261,279)))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('pid',type=int);a=p.parse_args();activate(a.pid);print('First Dawn human slot assigned to the offline local host')
