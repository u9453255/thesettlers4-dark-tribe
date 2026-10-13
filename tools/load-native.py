"""Load only the pinned mod binaries into the pinned, relocated single-player game.

Called by the launcher session helper. Importing or checking this module does not
read another process or start the game. It is never called during offline builds.
"""
import hashlib,json,subprocess,time
from pathlib import Path
from game_build import ORIGINAL_SHA256, BUNDLED_API_SHA256, SUPPORTED_GAME_HASHES, verify_executable, select_api, verify_hd
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify():
    m=json.loads((ROOT/'runtime/manifest.json').read_text())
    if m['version']!='0.2.15' or m['game_sha256'] not in SUPPORTED_GAME_HASHES or m.get('game_code_sha256')!=ORIGINAL_SHA256:raise RuntimeError('Unsupported native manifest')
    game_build=verify_executable(m['game_executable'])
    if game_build['sha256']!=m['game_sha256'] or game_build['large_address_aware']!=m.get('game_large_address_aware'):raise RuntimeError('Game executable changed; loader disabled')
    if set(m['files'])!={'DarkTribe-0.2.15.dll','S4ModApi.dll','inject.exe'}:raise RuntimeError('Unexpected runtime file list')
    if not (ROOT/'runtime/native/NoUpdateForS4ModApi.txt').is_file():raise RuntimeError('Pinned API update guard missing')
    for name,sha in m['files'].items():
        if digest(ROOT/'runtime/native'/name)!=sha:raise RuntimeError('Native file verification failed: '+name)
    if m['files']['S4ModApi.dll']!=BUNDLED_API_SHA256:raise RuntimeError('Bundled API pin changed')
    _,loaded_api=select_api(m['game_executable'],ROOT/'runtime/native/S4ModApi.dll')
    if loaded_api!=m.get('loaded_api'):raise RuntimeError('Selected API changed; loader disabled')
    if verify_hd(m['game_executable'])!=m.get('hd_renderer'):raise RuntimeError('HD renderer changed; loader disabled')
    return m
def load(pid,observed_executable):
    m=verify()
    if Path(observed_executable).resolve()!=Path(m['game_executable']).resolve():raise RuntimeError('Wrong process path; no injection')
    injector=ROOT/'runtime/native/inject.exe'
    runtime_log=ROOT/'evidence/v02-runtime.log'
    initial_size=runtime_log.stat().st_size if runtime_log.exists() else 0
    api_path,_=select_api(m['game_executable'],ROOT/'runtime/native/S4ModApi.dll')
    for module_path in (api_path,ROOT/'runtime/native/DarkTribe-0.2.15.dll'):
        name=module_path.name
        result=subprocess.run([str(injector),str(int(pid)),str(module_path.resolve())],capture_output=True,text=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        if result.returncode:raise RuntimeError(name+' load failed: exit='+str(result.returncode)+' status='+hex(result.returncode&0xffffffff)+' stdout='+repr(result.stdout.strip())+' stderr='+repr(result.stderr.strip()))
    # LoadLibrary success precedes the mod's asynchronous callback initialization.
    deadline=time.monotonic()+8
    tail=''
    while time.monotonic()<deadline:
        if runtime_log.exists():
            with runtime_log.open('rb') as stream:stream.seek(initial_size);tail=stream.read().decode('utf-8',errors='replace')
            own=[line for line in tail.splitlines() if 'pid='+str(int(pid))+' ' in line]
            if any('Version 0.2.15 native callbacks installed' in line for line in own):break
        time.sleep(0.1)
    else:raise RuntimeError('DLL loaded but native callbacks did not become ready: '+tail[-1200:])
    return dict(status='native-modules-loaded',pid=pid,version=m['version'])
if __name__=='__main__':
    verify()
    print('Native build and game hashes verified. No game launched or modified.')
