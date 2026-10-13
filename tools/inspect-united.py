"""File-only inventory of SU components and the shared API; never loads their DLLs."""
import hashlib,json,sys
from pathlib import Path
from game_build import verify_executable,select_api
from project_paths import bundled_api, game_dir
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import pefile

def run():
    game=game_dir()
    executable=verify_executable(game/'S4_Main.exe')
    api_path,api_pin=select_api(game/'S4_Main.exe',bundled_api())
    files=[]
    for p in [api_path,*[game/'Plugins'/name for name in ('SettlersUnited.asi','S4HD-Patch.asi','S4WarriorsLib.asi','ExtraZoom.asi','SettlersUnitedLogger.asi')]]:
        if not p.is_file():continue
        data=p.read_bytes();pe=pefile.PE(data=data);assert pe.FILE_HEADER.Machine==0x14c
        version=None
        for group in getattr(pe,'FileInfo',[]):
            for item in group:
                for table in getattr(item,'StringTable',[]):
                    value=table.entries.get(b'FileVersion')
                    if value:version=value.decode()
        files.append(dict(name=p.name,version=version,sha256=hashlib.sha256(data).hexdigest(),
                          imports_shared_api=any(d.dll.lower()==b's4modapi.dll' for d in pe.DIRECTORY_ENTRY_IMPORT)))
    pe=pefile.PE(str(api_path))
    exports={s.name.decode() for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name}
    assert {'S4CreateInterface','lua_beginblock','lua_endblock','lua_getglobal','lua_getnumber'}<=exports
    assert bytes.fromhex('9f4b1005d35204498d6fa7c3012eabdd') in api_path.read_bytes()
    report=dict(version='0.2.15',executable=executable,selected_api=api_pin,components=files,
                required_api_guid_and_lua_exports_present=True,game_started=False,live_process_read=False,
                game_files_written=False,enabled_optional_features_verified=False,gameplay_compatibility_verified=False)
    (ROOT/'research/settlers-united-files.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PASS: file-only SU component versions/hashes and required shared API interface/exports')
if __name__=='__main__':run()
