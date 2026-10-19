"""Read-only inspection of this installed build's player setup."""
import argparse,ctypes,json,struct
from pathlib import Path
from ctypes import wintypes as W
from functools import lru_cache
ROOT=Path(__file__).resolve().parents[1]

@lru_cache(maxsize=1)
def expected_executable():
    """The game executable the native build was verified against (runtime/manifest.json)."""
    return Path(json.loads((ROOT/'runtime/manifest.json').read_text())['game_executable']).resolve()

class Module(ctypes.Structure):
    _fields_=[('dwSize',W.DWORD),('th32ModuleID',W.DWORD),('th32ProcessID',W.DWORD),('GlblcntUsage',W.DWORD),('ProccntUsage',W.DWORD),('modBaseAddr',ctypes.POINTER(ctypes.c_byte)),('modBaseSize',W.DWORD),('hModule',W.HMODULE),('szModule',W.WCHAR*256),('szExePath',W.WCHAR*260)]

def inspect(pid):
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
    k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
    k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];k.CreateToolhelp32Snapshot.restype=W.HANDLE
    k.Module32FirstW.argtypes=[W.HANDLE,ctypes.POINTER(Module)];k.Module32FirstW.restype=W.BOOL
    k.CloseHandle.argtypes=[W.HANDLE]
    snapshot=k.CreateToolhelp32Snapshot(0x18,pid);module=Module();module.dwSize=ctypes.sizeof(module)
    if not k.Module32FirstW(snapshot,ctypes.byref(module)):raise ctypes.WinError(ctypes.get_last_error())
    k.CloseHandle(snapshot)
    if Path(module.szExePath).resolve()!=expected_executable():raise RuntimeError('Wrong game executable')
    base=ctypes.cast(module.modBaseAddr,ctypes.c_void_p).value
    h=k.OpenProcess(0x10,False,pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    def read(address,n):
        buf=ctypes.create_string_buffer(n);got=ctypes.c_size_t()
        if not k.ReadProcessMemory(h,ctypes.c_void_p(address),buf,n,ctypes.byref(got)) or got.value!=n:raise ctypes.WinError(ctypes.get_last_error())
        return buf.raw
    def u32(address):return struct.unpack('<I',read(address,4))[0]
    pointer=u32(base+0xe968a0)
    report=dict(pid=pid,base=hex(base),executable=module.szExePath,localPlayer=u32(base+0xe9482c),setupPointer=hex(pointer))
    if pointer:
        data=read(pointer,0x340)
        # Nine-entry per-player arrays; the scenarios use two or four players.
        count=struct.unpack_from('<I',data,0x338)[0]
        report['players']=[{hex(off):struct.unpack_from('<I',data,off+4*p)[0] for off in [0x90,0xb4,0xd8,0xfc,0x120,0x144,0x168,0x18c]} for p in range(count if 2<=count<=9 else 2)]
        report['skipFlags']=list(data[0x1b0:0x1b8]);report['playerCount']=count
    report['mapSize']=u32(base+0xd6921c)  # diagnostics only: stale or 0 until a map loads
    context=u32(base+0x1054100)
    connection=u32(context+0xa30) if context else 0
    report['hostId']=u32(connection+0x20) if connection else 0
    k.CloseHandle(h)
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('pid',type=int);args=parser.parse_args();print(json.dumps(inspect(args.pid),indent=2))
