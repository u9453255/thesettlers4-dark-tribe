"""Regression tests using a disposable fixture, never the game process."""
import importlib.util,json,subprocess,tempfile,shutil
from pathlib import Path
from project_paths import COMPILER
ROOT=Path(__file__).resolve().parents[1]
def run():
    spec=importlib.util.spec_from_file_location('loader',ROOT/'tools/load-native.py');loader=importlib.util.module_from_spec(spec);spec.loader.exec_module(loader)
    assert loader.verify()['version']=='0.2.15'
    with tempfile.TemporaryDirectory(prefix='dt-manifest-') as tmp:
        fixture=Path(tmp);shutil.copytree(ROOT/'runtime',fixture/'runtime')
        loader.ROOT=fixture;manifest=fixture/'runtime/manifest.json'
        original=manifest.read_bytes();m=json.loads(original);m['version']='0.2.6';manifest.write_text(json.dumps(m))
        try:loader.verify();raise AssertionError('Old manifest accepted')
        except RuntimeError as e:assert str(e)=='Unsupported native manifest'
        manifest.write_bytes(original);assert loader.verify()['version']=='0.2.15'
        for field,value in [('game_sha256','unknown'),('game_code_sha256','unknown')]:
            m=json.loads(original);m[field]=value;manifest.write_text(json.dumps(m))
            try:loader.verify();raise AssertionError('Unpinned game manifest accepted')
            except RuntimeError as e:assert str(e)=='Unsupported native manifest'
        m=json.loads(original);m['loaded_api']['sha256']='unknown';manifest.write_text(json.dumps(m))
        try:loader.verify();raise AssertionError('Changed shared API accepted')
        except RuntimeError as e:assert str(e)=='Selected API changed; loader disabled'
        manifest.write_bytes(original)
        m=json.loads(original);m['hd_renderer']={'unknown':'changed'};manifest.write_text(json.dumps(m))
        try:loader.verify();raise AssertionError('Changed HD renderer manifest accepted')
        except RuntimeError as e:assert str(e)=='HD renderer changed; loader disabled'
        manifest.write_bytes(original)
        dll=fixture/'runtime/native/DarkTribe-0.2.15.dll';dll.write_bytes(dll.read_bytes()+b'changed')
        try:loader.verify();raise AssertionError('Changed DLL accepted')
        except RuntimeError as e:assert str(e)=='Native file verification failed: DarkTribe-0.2.15.dll'
    loader.ROOT=ROOT
    spec=importlib.util.spec_from_file_location('policy',ROOT/'tools/session-policy.py');policy=importlib.util.module_from_spec(spec);spec.loader.exec_module(policy)
    lifetime=policy.SessionLifetime(0)
    assert not lifetime.expired(0,[100],set())
    # The old helper ended at 10s, before the observed new process at 28s.
    assert not lifetime.expired(10.1,[],set())
    assert not lifetime.expired(28,[200],set())
    assert not lifetime.expired(30,[200],{200})
    assert not lifetime.expired(41,[],{200})
    assert not lifetime.expired(59,[300],{200})
    assert not lifetime.expired(118.9,[],{200})
    assert lifetime.expired(119,[],{200})
    idle=policy.SessionLifetime(0)
    assert not idle.expired(179.9,[],set()) and idle.expired(180,[],set())
    calls=[]
    state={'pid':123,'players':[{'0x18c':3,'0xd8':0xffffffff}]}
    def activate(pid):calls.append(('activate',pid))
    def failed_loader(state):calls.append(('loader-failed',state['pid']));raise RuntimeError('simulated native failure')
    try:policy.process_state(state,activate,failed_loader,lambda **kw:calls.append(('record',kw['status'])))
    except RuntimeError:pass
    assert calls[0]==('activate',123) and calls[-1]==('loader-failed',123)
    # Successful and already-assigned starts do not change the scheduling order.
    calls.clear();policy.process_state(state,activate,lambda s:calls.append(('load',s['pid'])),lambda **kw:None)
    assert calls==[('activate',123),('load',123)]
    calls.clear();state['players'][0]['0xd8']=0
    policy.process_state(state,activate,lambda s:calls.append(('load',s['pid'])),lambda **kw:None)
    assert calls==[('load',123)]
    compiler=COMPILER
    out=ROOT/'work/native-generated';out.mkdir(exist_ok=True)
    flags=[str(compiler),'-O2','-static']
    src=str(ROOT/'native/loader-fixture.cpp');host=out/'loader-fixture.exe';dll=out/'loader-fixture.dll'
    subprocess.run(flags+['-municode',src,'-o',str(host)],check=True)
    subprocess.run(flags+['-shared','-DBUILD_LOADER_FIXTURE_DLL',src,'-o',str(dll)],check=True)
    injector=ROOT/'runtime/native/inject.exe'
    noargs=subprocess.run([str(injector)],capture_output=True,text=True,timeout=10)
    assert noargs.returncode==2 and 'usage:' in noargs.stderr
    process=subprocess.Popen([str(host)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    try:
        assert process.stdout.readline().strip()=='fixture-ready'
        result=subprocess.run([str(injector),str(process.pid),str(dll.resolve())],capture_output=True,text=True,timeout=15)
        assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
        stdout,stderr=process.communicate(timeout=12)
        assert process.returncode==0,(process.returncode,stdout,stderr)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
    report={'version':'0.2.15','matching_manifest_accepted':True,'old_manifest_refused':True,'changed_dll_refused':True,'unpinned_game_or_code_manifest_refused':True,'changed_selected_api_refused':True,'changed_hd_manifest_refused':True,'slot_assignment_before_native_loading':True,'slot_assignment_with_failed_loader':True,'x86_loader_fixture':True,'injector_usage_check':True,'transient_startup_pid_survives_10_second_gap':True,'new_pid_at_28_seconds_survives':True,'post_ready_restart_grace_60_seconds':True,'startup_grace_180_seconds':True,'game_started':False}
    (ROOT/'research/startup-regression.json').write_text(json.dumps(report,indent=2))
    print('PASS: delayed/restarted helper lifetime, slot assignment despite loader failure and disposable non-game x86 loader')
if __name__=='__main__':run()
