"""Launcher-only helper: load verified native modules, then assign the First Dawn human slot."""
import ctypes,json,time,subprocess,threading
from pathlib import Path
from ctypes import wintypes as W
from importlib.util import spec_from_file_location,module_from_spec
ROOT=Path(__file__).resolve().parents[1]
spec=spec_from_file_location('activation',ROOT/'tools/activate-human-slot.py');activation=module_from_spec(spec);spec.loader.exec_module(activation)
loader_spec=spec_from_file_location('native_loader',ROOT/'tools/load-native.py');loader=module_from_spec(loader_spec);loader_spec.loader.exec_module(loader)
loader.verify()
policy_spec=spec_from_file_location('session_policy',ROOT/'tools/session-policy.py');policy=module_from_spec(policy_spec);policy_spec.loader.exec_module(policy)
class ProcessEntry(ctypes.Structure):
    _fields_=[('dwSize',W.DWORD),('cntUsage',W.DWORD),('th32ProcessID',W.DWORD),('th32DefaultHeapID',ctypes.c_size_t),('th32ModuleID',W.DWORD),('cntThreads',W.DWORD),('th32ParentProcessID',W.DWORD),('pcPriClassBase',W.LONG),('dwFlags',W.DWORD),('szExeFile',W.WCHAR*260)]
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];k.CreateToolhelp32Snapshot.restype=W.HANDLE
k.Process32FirstW.argtypes=[W.HANDLE,ctypes.POINTER(ProcessEntry)];k.Process32FirstW.restype=W.BOOL
k.Process32NextW.argtypes=[W.HANDLE,ctypes.POINTER(ProcessEntry)];k.Process32NextW.restype=W.BOOL
k.CloseHandle.argtypes=[W.HANDLE]
k.CreateMutexW.argtypes=[ctypes.c_void_p,W.BOOL,W.LPCWSTR];k.CreateMutexW.restype=W.HANDLE
mutex=k.CreateMutexW(None,True,'Local\\Settlers4DarkTribeV0215SessionHelper')
if not mutex:raise ctypes.WinError(ctypes.get_last_error())
if ctypes.get_last_error()==183:
    k.CloseHandle(mutex)
    raise SystemExit(0)
def pids():
    snapshot=k.CreateToolhelp32Snapshot(2,0);entry=ProcessEntry();entry.dwSize=ctypes.sizeof(entry);result=[]
    try:
        ok=k.Process32FirstW(snapshot,ctypes.byref(entry))
        while ok:
            if entry.szExeFile.lower()=='s4_main.exe':result.append(entry.th32ProcessID)
            ok=k.Process32NextW(snapshot,ctypes.byref(entry))
    finally:k.CloseHandle(snapshot)
    return result
(ROOT/'evidence').mkdir(exist_ok=True)
(ROOT/'save-states').mkdir(exist_ok=True)
log=ROOT/'evidence/session-helper.jsonl'
def record(**kwargs):
    with log.open('a',encoding='utf-8') as f:f.write(json.dumps(dict(time=time.time(),**kwargs))+'\n')
started=time.monotonic();lifetime=policy.SessionLifetime(started);loaded=set();failures=set();inspection_errors={}
u=ctypes.WinDLL('user32',use_last_error=True)
EnumProc=ctypes.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.EnumWindows.argtypes=[EnumProc,W.LPARAM];u.EnumWindows.restype=W.BOOL
u.IsWindowVisible.argtypes=[W.HWND];u.IsWindowVisible.restype=W.BOOL
u.GetWindowThreadProcessId.argtypes=[W.HWND,ctypes.POINTER(W.DWORD)];u.GetWindowThreadProcessId.restype=W.DWORD
u.GetClientRect.argtypes=[W.HWND,ctypes.POINTER(W.RECT)];u.GetClientRect.restype=W.BOOL
window_seen={};scheduled=set();log_lock=threading.Lock()
original_record=record
def record(**kwargs):
    with log_lock:original_record(**kwargs)
def window_ready(pid):
    found=False
    @EnumProc
    def visit(hwnd,_):
        nonlocal found
        owner=W.DWORD();rect=W.RECT()
        u.GetWindowThreadProcessId(hwnd,ctypes.byref(owner))
        if owner.value==pid and u.IsWindowVisible(hwnd) and u.GetClientRect(hwnd,ctypes.byref(rect)) and rect.right>=400 and rect.bottom>=300:found=True
        return True
    u.EnumWindows(visit,0)
    if not found:window_seen.pop(pid,None);return False
    window_seen.setdefault(pid,time.monotonic())
    return time.monotonic()-window_seen[pid]>=2
def native_worker(state):
    pid=state['pid']
    for attempt in range(1,4):
        if not (ROOT/'runtime/active.json').exists():return
        try:
            record(status='native-load-started',pid=pid,attempt=attempt)
            current=activation.runtime.inspect(pid)
            result=loader.load(pid,current['executable']);record(**result);loaded.add(pid)
            status=ROOT/'evidence/startup-status-0215.json';temp=status.with_suffix('.tmp');temp.write_text(json.dumps(dict(status='ready',pid=pid,version='0.2.15')));temp.replace(status)
            return
        except (OSError,RuntimeError,subprocess.SubprocessError) as error:
            record(status='native-load-failed',pid=pid,attempt=attempt,error=str(error))
            if attempt<3:time.sleep(0.5)
    failures.add(pid)
    status=ROOT/'evidence/startup-status-0215.json';temp=status.with_suffix('.tmp');temp.write_text(json.dumps(dict(status='failed',pid=pid,version='0.2.15',message='See session-helper.jsonl for native loader errors')));temp.replace(status)
def queue_native(state):
    pid=state['pid']
    if pid not in scheduled and window_ready(pid):
        scheduled.add(pid)
        threading.Thread(target=native_worker,args=(state,),daemon=True).start()
record(status='started',version='0.2.15',startup_grace_seconds=180,restart_grace_seconds=60)
while time.monotonic()-started<12*3600:
    if not (ROOT/'runtime/active.json').exists():break
    if json.loads((ROOT/'runtime/active.json').read_text()).get('version')!='0.2.15':break
    candidates=pids()
    if lifetime.expired(time.monotonic(),candidates,loaded):break
    for pid in candidates:
        try:
            state=activation.runtime.inspect(pid)
            policy.process_state(state,activation.activate,queue_native,record)
        except (OSError,RuntimeError,KeyError,ValueError) as error:
            detail=str(error);last=inspection_errors.get(pid)
            if last is None or last[0]!=detail or time.monotonic()-last[1]>=10:
                record(status='inspection-failed',pid=pid,error=detail)
                inspection_errors[pid]=(detail,time.monotonic())
    time.sleep(0.1)
record(status='finished')
k.CloseHandle(mutex)
