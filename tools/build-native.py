"""Offline build: derives footprints locally; never opens or starts the game."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
from game_build import ORIGINAL_SHA256, BUNDLED_API_SHA256, verify_executable, select_api, verify_hd
import xml.etree.ElementTree as ET
from placement_pins import generate as placement_signatures
from construction_pins import generate as construction_signatures
from gfx_pins import generate as gfx_signatures, GFX
from project_paths import COMPILER, SDK, game_exe, game_dir, original_building_xml
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build():
    exe=game_exe()
    os.environ['DARK_TRIBE_GAME_DIR']=str(game_dir()) # the native tests verify the pinned sprites from this folder
    game_build=verify_executable(exe)
    sdk=SDK;compiler=COMPILER
    for need in (compiler,sdk/'S4ModApi.lib',sdk/'S4ModApi.dll',sdk/'S4ModApi.h'):
        if not need.is_file():raise RuntimeError('Missing '+str(need)+'; run python tools/setup.py first')
    generated=ROOT/'work/native-generated';generated.mkdir(parents=True,exist_ok=True)
    placement_signatures(exe,generated/'placement-signatures.hpp')
    construction_signatures(exe,generated/'construction-signatures.hpp')
    gfx_signatures(generated/'gfx-signatures.hpp') # pinned byte ranges of the game's own sprites (private, ignored folder)
    original=original_building_xml()
    dark=ET.parse(original).find("race[@id='RACE_DARK']")
    kinds={'BUILDING_MUSHROOMFARM':49,'BUILDING_DARKTEMPLE':50,'BUILDING_FORTRESS':51,'BUILDING_MANACOPTERHALL':80}
    header=['#pragma once','namespace dt {struct Cell {int x,y;};struct Footprint {int count;const Cell*cells;};']
    footprints=[]
    for b in dark:
        if b.attrib['id'] not in kinds:continue
        k=kinds[b.attrib['id']];hx=int(b.findtext('iHotSpotX'));hy=int(b.findtext('iHotSpotY'))
        masks=b.find('blockPosLines')
        if masks is None:raise RuntimeError('Missing occupancy mask for '+b.attrib['id'])
        cells=sorted({(31-bit-hx,row-hy) for tag in ('blockPosLines','buildingPosLines','digPosLines') for mask in [b.find(tag)] if mask is not None for row,v in enumerate(mask) for bit in range(32) if int(v.text)&(1<<bit)})
        if not cells:raise RuntimeError('Empty footprint')
        header.append('static const Cell cells%d[]={%s};'%(k,','.join('{%d,%d}'%c for c in cells)))
        header.append('static const Footprint fp%d={%d,cells%d};'%(k,len(cells),k));footprints.append(k)
    for label,tag in [('placement','buildingPosLines'),('solid','blockPosLines')]:
        for b in dark:
            if b.attrib['id'] not in kinds:continue
            k=kinds[b.attrib['id']];hx=int(b.findtext('iHotSpotX'));hy=int(b.findtext('iHotSpotY'));mask=b.find(tag)
            cells=sorted({(31-bit-hx,row-hy) for row,v in enumerate(mask) for bit in range(32) if int(v.text)&(1<<bit)})
            if not cells:raise RuntimeError('Missing native '+tag)
            header.append('static const Cell '+label+'Cells%d[]={%s};'%(k,','.join('{%d,%d}'%c for c in cells)))
            header.append('static const Footprint '+label+'Fp%d={%d,'% (k,len(cells))+label+'Cells%d};'%k)
        header.append('inline const Footprint*'+label+'Footprint(int k){switch(k){'+''.join('case %d:return &'%k+label+'Fp%d;'%k for k in footprints)+'default:return nullptr;}}')
    if set(footprints)!=set(kinds.values()):raise RuntimeError('Missing Dark building definitions')
    header.append('inline const Footprint*footprint(int k){switch(k){'+''.join('case %d:return &fp%d;'%(k,k) for k in footprints)+'default:return nullptr;}}}')
    (generated/'footprints.hpp').write_text('\n'.join(header))
    runtime=ROOT/'runtime/native';runtime.mkdir(parents=True,exist_ok=True)
    common=[str(compiler),'-std=c++17','-O2','-static','-fms-extensions','-Wall','-Wextra']
    subprocess.run(common+['-shared','-I'+str(sdk),'-I'+str(generated),str(ROOT/'native/dark-tribe.cpp'),str(sdk/'S4ModApi.lib'),'-lgdi32','-lmsimg32','-lbcrypt','-o',str(runtime/'DarkTribe-0.2.15.dll')],check=True)
    subprocess.run(common+['-municode',str(ROOT/'native/inject.cpp'),'-o',str(runtime/'inject.exe')],check=True)
    test=generated/'economy-tests.exe'
    subprocess.run(common+[str(ROOT/'native/economy-tests.cpp'),'-o',str(test)],check=True)
    subprocess.run([str(test)],check=True)
    for name in ['queue-tests','placement-tests','panel-tests','aircraft-tests','interaction-tests','construction-tests','hd-overlay-tests','file-hash-tests','placement-hints-tests','memory-tests','unit-panel-tests','gfx-tests']:
        binary=generated/(name+'.exe')
        subprocess.run(common+['-I'+str(generated),str(ROOT/'native'/(name+'.cpp')),'-lgdi32','-lmsimg32','-lbcrypt','-o',str(binary)],check=True)
        result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
        print(result.stdout.strip() or 'PASS: '+name)
    preview=generated/'panel-preview.exe'
    subprocess.run(common+['-I'+str(generated),str(ROOT/'native/panel-preview.cpp'),'-lgdi32','-lmsimg32','-lbcrypt','-o',str(preview)],check=True)
    art=ROOT/'art';art.mkdir(exist_ok=True)
    bitmap=generated/'panel-preview.bmp'
    subprocess.run([str(preview),str(bitmap)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
    from PIL import Image
    Image.open(bitmap).convert('RGB').save(art/'panel-preview.png')
    fortress_bitmap=generated/'fortress-preview.bmp'
    subprocess.run([str(preview),str(fortress_bitmap),'fortress'],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
    Image.open(fortress_bitmap).convert('RGB').save(art/'fortress-preview.png')
    hd_preview=generated/'hd-panel-preview.exe';hd_bitmap=generated/'hd-panel-preview.bmp'
    subprocess.run(common+['-I'+str(generated),str(ROOT/'native/hd-panel-preview.cpp'),'-lgdi32','-lmsimg32','-lbcrypt','-o',str(hd_preview)],check=True)
    subprocess.run([str(hd_preview),str(hd_bitmap)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
    Image.open(hd_bitmap).convert('RGB').save(art/'hd-panel-preview.png')
    # The same previews with the game's own sprites: game-derived, so only in the ignored work/ folder.
    private=ROOT/'work/previews';private.mkdir(parents=True,exist_ok=True);gfx=str(GFX)+chr(92)
    for args,name in [([str(preview),str(bitmap),gfx],'panel-preview-native.png'),([str(preview),str(fortress_bitmap),'fortress',gfx],'fortress-preview-native.png'),([str(hd_preview),str(hd_bitmap),gfx],'hd-panel-preview-native.png')]:
        subprocess.run(args,check=True,creationflags=subprocess.CREATE_NO_WINDOW);Image.open(args[1]).convert('RGB').save(private/name)
    if not (runtime/'S4ModApi.dll').exists() or digest(runtime/'S4ModApi.dll')!=digest(sdk/'S4ModApi.dll'):shutil.copy2(sdk/'S4ModApi.dll',runtime/'S4ModApi.dll')
    for p in sdk.glob('COPYING*'):shutil.copy2(p,runtime/p.name)
    if digest(runtime/'S4ModApi.dll')!=BUNDLED_API_SHA256:raise RuntimeError('Bundled API pin changed')
    (runtime/'NoUpdateForS4ModApi.txt').write_text('Pinned S4ModApi 2.3 for Dark Tribe 0.2. Automatic replacement disabled.\n')
    third=ROOT/'third_party/S4ModApi';third.mkdir(parents=True,exist_ok=True)
    shutil.copy2(sdk/'S4ModApi.h',third/'S4ModApi.h')
    manifest={'version':'0.2.15','game_executable':str(exe),'game_sha256':game_build['sha256'],'game_code_sha256':ORIGINAL_SHA256,'game_large_address_aware':game_build['large_address_aware'],'api_version':'2.3','files':{name:digest(runtime/name) for name in ('DarkTribe-0.2.15.dll','inject.exe','S4ModApi.dll')},'offline_tests':'passed','in_game_tests':'left to user'}
    _,manifest['loaded_api']=select_api(exe,runtime/'S4ModApi.dll')
    manifest['hd_renderer']=verify_hd(exe)
    (ROOT/'runtime/manifest.json').write_text(json.dumps(manifest,indent=2))
    (ROOT/'research/ui-queue-checks.json').write_text(json.dumps(dict(version='0.2.15',queue_model='passed',placement_reservations='passed',panel_hit_targets='passed',panel_render='art/panel-preview.png',fortress_panel_render='art/fortress-preview.png',aircraft_model='passed',interaction_model='passed',construction_model='passed',hd_owned_window_fixture='passed',hd_dpi_and_input_model='passed',pre_hook_file_hash_fixture='passed',native_check_count=11,per_tick_read_cache='passed',construction_flags_and_terrain='passed',automatic_dark_hint_scan='passed',sparse_hint_cache_reuse='passed',game_started=False),indent=2))
    print('Built x86 native economy, loader and passed offline economy checks. No game launched.')
if __name__=='__main__':build()
