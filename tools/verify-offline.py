"""Check the staged native/map/installer package. No game execution or process access."""
import hashlib,importlib.util,json,struct,sys,tempfile,shutil,subprocess
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable, select_api
from project_paths import original_building_xml, tool
from scenarios import SCENARIOS, layouts
import re
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import pefile
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run():
    checks=[]
    native_source=(ROOT/'native/dark-tribe.cpp').read_text(encoding='utf-8')
    for obsolete in ['spawnCourier', 'discoverFarms', 'routeHarvest', 'pendingHarvest', 'installFarmIncomeHook', 'static void deliveries']:
        assert obsolete not in native_source,obsolete
    assert 'dt::retireLegacyFarmState(state)' in native_source
    assert 'dt::clearManualQueues(templeQueues(c.entity))' in native_source
    checks.append('Native farm worker delivery/income retained; custom courier creation, inventory and movement removed; manual Shaman/native Farmer recruitment retained')
    node=tool('node')
    if not node:raise RuntimeError('Node.js not found: put node on PATH, set DARK_TRIBE_NODE or run python tools/setup.py --node <node.exe>')
    subprocess.run([node,str(ROOT/'tools/test-placement.cjs')],check=True)
    subprocess.run([node,str(ROOT/'tools/test-iron-kingdom.cjs')],check=True)
    checks.append('Iron Kingdom: two players, merged Dark camps, removed Trojans, owned Viking entrances and placeable Dark land')
    subprocess.run([node,str(ROOT/'tools/test-alliance.cjs')],check=True)
    checks.append('The Alliance: four players on a 512 map, three allied computer peoples on their own land, placeable Dark land')
    header=(ROOT/'native/scenario-model.hpp').read_text(encoding='utf-8')
    native_layouts=[]
    for size,count,starts in re.findall(r'^ \{(\d+),(\d+),\{(.*?)\}\},',header,re.M):
        players=tuple(tuple(map(int,s)) for s in re.findall(r'\{(\d+),(\d+),(\d+)\}',starts))
        assert len(players)==int(count);native_layouts.append((int(size),players))
    assert native_layouts==layouts(),(native_layouts,layouts())
    checks.append('Native scene fingerprint and launcher helper accept exactly the scenario layouts of tools/scenarios.py')
    checks.append('All four original Dark masks reproduce stale clearing flags and pass repaired map without relaxing collision guards')
    for script in ['test-game-build.py','inspect-united.py','inspect-hd-rendering.py','inspect-interactions.py','inspect-fortress.py','test-fortress.py','inspect-startup-sectors.py','inspect-placement.py','inspect-placement-hints.py','inspect-construction.py','inspect-offence.py','inspect-startup-player.py','inspect-recruitment.py']:
        subprocess.run([sys.executable,str(ROOT/'tools'/script)],check=True)
    checks.append('Actual native Dark collision flags/terrain and all masks; automatic tick-only viewport hints using the native sparse cache; relocation and ABI guards')
    checks.append('Pinned Fortress crash path, explicit no-tool enum and preserved native role/geometry/data')
    checks.append('Pinned initial owner/sector/economy creation, CBarrackRole null read and native default ending branch')
    m=json.loads((ROOT/'runtime/manifest.json').read_text())
    assert m['version']=='0.2.15'
    required_markers={
        'tools/load-native.py':["m['version']!='0.2.15'",'Version 0.2.15 native callbacks installed'],
        'tools/launch.ps1':['startup-status-0215.json','Dark Tribe 0.2.15 ready'],
        'tools/watch-human-slot.py':['V0215SessionHelper','startup-status-0215.json',"version='0.2.15'"],
        'tools/install.py':["version='0.2.15'"],
        'tools/stage-release.py':["m['version']='0.2.15'"],
    }
    for name,markers in required_markers.items():
        source=(ROOT/name).read_text(encoding='utf-8')
        for marker in markers:assert marker in source,(name,marker)
    assert b'Version 0.2.15 native callbacks installed' in (ROOT/'runtime/native/DarkTribe-0.2.15.dll').read_bytes()
    for name in ['README.md','MAIN.md','TESTING.md','HANDOFF.md']:
        assert 'Dark Tribe 0.2.15 ready' in (ROOT/name).read_text(encoding='utf-8'),name
    checks.append('Combined release 0.2.15 runtime, loader, installer, helper mutex/status and user readiness labels')
    dll=(ROOT/'runtime/native/DarkTribe-0.2.15.dll').read_bytes()
    for marker in [b'crash and freeze diagnostics r26',b'FATAL_EXCEPTION code=',b'HEAP_FAILURE type=',b'FREEZE_WATCH ready main thread ',b'FREEZE_DUMP written ',b'PROCESS_EXIT ']:assert marker in dll,marker
    assert 'dtdiag::exception(p)' in native_source and 'dtdiag::beat(true,true)' in native_source and 'dtdiag::processExit(' in native_source
    diagnostics=json.loads((ROOT/'research/diagnostics-checks.json').read_text(encoding='utf-8'))
    assert diagnostics['game_started'] is False and diagnostics['heap_double_free_failure_type']==8 and diagnostics['freeze_dump_valid_with_main_thread']
    checks.append('Crash and freeze diagnostics r26 in the DLL; fixture run (fault, heap double free, stack overflow, freeze with dump) recorded without starting the game')
    exe=pefile.PE(m['game_executable'])
    game_build=verify_executable(m['game_executable'])
    assert game_build['sha256']==m['game_sha256'] and m['game_code_sha256']==ORIGINAL_SHA256
    assert exe.FILE_HEADER.Machine==0x14c
    expected=b'\x8d\x41\xff\x3b\x05'+struct.pack('<I',exe.OPTIONAL_HEADER.ImageBase+0xe94828)
    assert exe.get_data(0x36380,9)==expected,'Income hook signature changed'
    imports={i.address-exe.OPTIONAL_HEADER.ImageBase:i.name.decode() for d in exe.DIRECTORY_ENTRY_IMPORT for i in d.imports if i.name}
    for rva,name in [(0xb15318,'CreateFileA'),(0xb15320,'CreateFileW'),(0xb152d4,'CloseHandle'),(0xb1528c,'WriteFile')]:assert imports[rva]==name
    assert exe.get_data(0x1cb557,12)==b'\xa1'+struct.pack('<I',exe.OPTIONAL_HEADER.ImageBase+0x113fc98)+bytes.fromhex('f20f2cc88b3485')
    assert exe.get_data(0x16b890,16)==bytes.fromhex('558bec83ec0c538b5d0856578bf98b34')
    assert exe.get_data(0x16b990,16)==bytes.fromhex('558bec6aff68')+struct.pack('<I',exe.OPTIONAL_HEADER.ImageBase+0xab9760)+bytes.fromhex('64a100000000')
    assert [struct.unpack('<bb',exe.get_data(0xd69988+i*4,2)) for i in range(6)]==[(2,0),(2,1),(2,2),(1,2),(0,2),(-1,1)]
    checks.append('Pinned aircraft last-created lookup, native board/unload entrypoints and actual unload ring')
    checks.append('Pinned x86 executable, whole-instruction income gateway and save imports')
    api=pefile.PE(str(ROOT/'runtime/native/S4ModApi.dll'))
    exports={s.name.decode() for s in api.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
    for name in ['lua_beginblock','lua_endblock','lua_getglobal','lua_getnumber','S4CreateInterface']:assert name in exports,name
    selected_path,selected_api=select_api(m['game_executable'],ROOT/'runtime/native/S4ModApi.dll')
    assert selected_api==m['loaded_api']
    selected=pefile.PE(str(selected_path));assert selected.FILE_HEADER.Machine==0x14c
    selected_exports={s.name.decode() for s in selected.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
    assert {'lua_beginblock','lua_endblock','lua_getglobal','lua_getnumber','S4CreateInterface'}<=selected_exports
    assert bytes.fromhex('9f4b1005d35204498d6fa7c3012eabdd') in selected_path.read_bytes()
    checks.append('Exact original/SU executable pins and one shared, hash-pinned installed API; required GUID and Lua exports retained')
    allowed={'kernel32.dll','user32.dll','gdi32.dll','bcrypt.dll','s4modapi.dll','advapi32.dll','msvcrt.dll','ucrtbase.dll','ntdll.dll','ole32.dll','shell32.dll','shlwapi.dll','version.dll','winmm.dll','imm32.dll','comctl32.dll','ws2_32.dll','msimg32.dll','wininet.dll'}
    deps={}
    for name,sha in m['files'].items():
        p=ROOT/'runtime/native'/name;assert digest(p)==sha
        pe=pefile.PE(str(p));assert pe.FILE_HEADER.Machine==0x14c
        libraries=[d.dll.decode().lower() for d in pe.DIRECTORY_ENTRY_IMPORT]
        assert all(d in allowed or d.startswith('api-ms-win-') for d in libraries),libraries
        deps[name]=libraries
    assert (ROOT/'runtime/native/NoUpdateForS4ModApi.txt').is_file()
    checks.append('Runtime hashes, API exports, x86 architecture and bundled/Windows dependencies')
    for scenario in SCENARIOS:
        lua=(ROOT/scenario['script']).read_text()
        assert 'DT_Version = 200' in lua and 'DT_IsNew = 1' in lua and 'Game.EnemyPlayersLost(1)' in lua,scenario['script']
        for obsolete in ['DT_Economy','DT_Commands','DT_PadClear','Settlers.AddSettlers','Vars.Save3']:assert obsolete not in lua
        assert lua.count('Magic.IncreaseMana')==1
        # The game's Lua predates for loops (no shipped map script uses them).
        assert not re.search(r'\bfor\b.*\bdo\b',lua),scenario['script']
    checks.append('Version-scoped scenario without waves, timed credits or one-use farm logic')
    spec=importlib.util.spec_from_file_location('installer',ROOT/'tools/install.py');installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
    original_root=installer.ROOT
    # Absent until the user launcher installs the mod for the first time (fresh checkout).
    manifest_path=ROOT/'backups/install-manifest.json'
    real_manifest=json.loads(manifest_path.read_text()) if manifest_path.exists() else None
    if real_manifest:
        assert real_manifest['version']==m['version']
        for entry in real_manifest['entries']:
            assert Path(entry['source']).resolve().is_relative_to(ROOT.resolve()),'Source points outside primary package'
    import xml.etree.ElementTree as ET
    generated=ET.parse(ROOT/'work/generated/GameData/buildingInfo.xml').getroot()
    dark=generated.find("race[@id='RACE_DARK']")
    fortress=next(b for b in dark if b.get('id')=='BUILDING_FORTRESS')
    assert fortress.findtext('tool')=='GOOD_NO_GOOD'
    assert fortress.findtext('kind')=='HOUSE_KIND_DARKTEMPLE'
    checks.append('Actual primary generated Fortress data and staged sources match the combined package' if real_manifest else 'Actual primary generated Fortress data; nothing installed or staged yet')
    # Exercise the real installer against a temporary copy, including v0.1 upgrade.
    with tempfile.TemporaryDirectory(prefix='dt-offline-') as directory:
        lab=Path(directory);game=lab/'game';project=lab/'project';project.mkdir();game.mkdir()
        for sub in ['backups','research','work/generated/GameData']:(project/sub).mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/'research/building-compatibility.json',project/'research/building-compatibility.json')
        shutil.copy2(ROOT/'work/generated/GameData/buildingInfo.xml',project/'work/generated/GameData/buildingInfo.xml')
        for scenario in SCENARIOS:
            for relative in [scenario['generated'],scenario['script']]:
                (project/relative).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/relative,project/relative)
        entries=[];upgrade=bool(real_manifest)
        if real_manifest:
            # An installation made before the newest scenario lacks its entry: the installer must add it.
            newest=Path(SCENARIOS[-1]['installed']).name
            for index,entry in enumerate(e for e in real_manifest['entries'] if Path(e['destination']).name!=newest):
                e=dict(entry);dest=game/Path(entry['destination']).relative_to(Path(real_manifest['game']));dest.parent.mkdir(parents=True,exist_ok=True)
                backup=project/'backups'/Path(e['backup']).name
                if e['existed']:shutil.copy2(e['backup'],backup)
                e['backup']=str(backup);e['destination']=str(dest)
                previous=ROOT/'backups/v01'/Path(e['source']).name
                if index==0:shutil.copy2(e['source'],dest)
                elif previous.exists():shutil.copy2(previous,dest)
                else:
                    # Scenarios newer than 0.1 never had a v0.1 file; for First Dawn its absence means
                    # no v0.1 files on this machine: start from the original state instead.
                    if index<3:upgrade=False
                    if e['existed']:shutil.copy2(backup,dest)
                entries.append(e)
            temp_manifest={'game':str(game.resolve()),'entries':entries}
            (project/'backups/install-manifest.json').write_text(json.dumps(temp_manifest))
            installer.ROOT=project
            installer.install(game,None)
            entries=json.loads((project/'backups/install-manifest.json').read_text())['entries']
            assert any(Path(e['destination']).name==newest for e in entries),'Upgrade did not add the new scenario'
        else:
            # First install from the untouched original game XML; the installer writes its own manifest.
            (game/'GameData').mkdir(parents=True);shutil.copy2(original_building_xml(),game/'GameData/buildingInfo.xml')
            installer.ROOT=project
            installer.install(game,None)
            entries=json.loads((project/'backups/install-manifest.json').read_text())['entries']
        for e in entries:assert digest(Path(e['destination']))==e['installed_sha256']
        map_file=Path(entries[1]['destination']);map_file.write_bytes(b'unrelated content')
        script_before=digest(Path(entries[2]['destination']))
        try:installer.install(game,None)
        except RuntimeError:pass
        else:raise AssertionError('Installer overwrote unrelated map')
        assert map_file.read_bytes()==b'unrelated content' and digest(Path(entries[2]['destination']))==script_before
        shutil.copy2(entries[1]['source'],map_file)
        installer.restore()
        for e in entries:
            if e['existed']:assert digest(Path(e['destination']))==e['original_sha256']
            else:assert not Path(e['destination']).exists()
        installer.ROOT=original_root
    checks.append(('Installer upgrade' if upgrade else 'Installer first install')+', unrelated-change refusal before mutation and original restoration in a temporary game copy')
    for e in (real_manifest or {'entries':[]})['entries']:
        assert digest(Path(e['source']))==e['installed_sha256']
        if e['existed']:assert digest(Path(e['backup']))==e['original_sha256']
    report={'version':'0.2.15','offline_checks':checks,'dependencies':deps,'in_game_verification':'not performed; reserved for user','game_started':False}
    (ROOT/'research/v02-offline-checks.json').write_text(json.dumps(report,indent=2))
    for c in checks:print('PASS:',c)
if __name__=='__main__':run()
