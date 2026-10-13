"""Static managed metadata/IL evidence for the pinned HD renderer; never loads it."""
import json,sys
from pathlib import Path
from game_build import verify_hd
from project_paths import game_dir, game_exe
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))

def run():
    source=(ROOT/'native/dark-tribe.cpp').read_text(encoding='utf-8')
    overlay=(ROOT/'native/hd-overlay.hpp').read_text(encoding='utf-8')
    file_hash=(ROOT/'native/file-hash.hpp').read_text(encoding='utf-8')
    assert 'return dtio::hashFile(p,out);' in source
    assert 'CreateFileW(' in file_hash and 'CloseHandle(file)' in file_hash
    for save_hook_original in ['createW(', 'closeHandle(']:assert save_hook_original not in file_hash
    startup=source.split('static DWORD WINAPI start(',1)[1]
    assert startup.index('hashFile(file,actual)')<startup.index('createW=peek<CreateW>')
    frame=source.split('static HRESULT __stdcall frame(',1)[1].split('static BOOL __stdcall blt',1)[0]
    assert 'if(hdActive.load())return 0' in frame and 'v.buildMenu' in frame
    cached=source.split('static bool hdSnapshot(',1)[1].split('static DWORD WINAPI hdPanelThread',1)[0]
    assert 'TryAcquireSRWLockShared' in cached and 'panelInput(shown.data,x,y,message)' in cached
    for forbidden in ['api->','luaConstant(','signature(','entity(']:assert forbidden not in cached and forbidden not in overlay,forbidden
    for required in ['WS_EX_NOACTIVATE','WS_EX_TOOLWINDOW','WS_EX_LAYERED','MA_NOACTIVATE','ULW_COLORKEY','GetCurrentProcessId()','SW_HIDE','logicalX','logicalY']:
        assert required in overlay,required
    pins=verify_hd(game_exe())
    report=dict(version='0.2.15',hd_files=pins,cached_rendering_and_queued_input=True,
                owned_nonactivating_layered_panel=True,dpi_draw_and_hit_mapping_shared=True,
                startup_hash_independent_of_save_hooks=True,hash_check_before_save_hook_initialization=True,
                game_started=False,live_process_read=False,game_modules_loaded=False,gameplay_verified=False)
    if pins:
        import dnfile
        from dncil.cil.body import CilMethodBody
        from dncil.cil.body.reader import CilMethodBodyReaderBytes
        pe=dnfile.dnPE(str(game_dir()/'Plugins/S4HD-Manager.dll'))
        def body(type_name,method):
            for t in pe.net.mdtables.TypeDef:
                if str(t.TypeName)!=type_name:continue
                for ix in t.MethodList:
                    m=ix.row
                    if str(m.Name)==method:
                        return m,CilMethodBody(CilMethodBodyReaderBytes(pe.get_data(m.Rva,15000))).instructions
            raise AssertionError((type_name,method))
        def token_name(operand):
            v=operand.value;row=pe.net.mdtables.tables[v>>24].rows[(v&0xffffff)-1]
            return str(row.Name)
        active,ins=body('CppInterface','IsHDActive')
        assert active.Signature.value.hex()=='000020828902'
        assert [str(i.opcode) for i in ins]==['ldsfld','ret'] and token_name(ins[0].operand)=='drawHD'
        blocked,ins=body('CppInterface','GetMouseBlockState')
        assert blocked.Signature.value==active.Signature.value
        assert [str(i.opcode) for i in ins]==['ldsfld','ldfld','ret'] and token_name(ins[1].operand)=='isVideoRunning'
        _,ins=body('DX11RenderState','CreateSizeIndependentResources')
        windowed=[index for index,i in enumerate(ins) if str(i.opcode)=='stfld' and token_name(i.operand)=='IsWindowed']
        assert len(windowed)==1 and str(ins[windowed[0]-2].opcode)=='ldc.i4.1'
        report.update(renderer='DX11 windowed swapchain',status_exports='stdcall bool, no arguments; field reads only',
                      is_hd_active_rva=hex(active.Rva),video_block_status_rva=hex(blocked.Rva),native_dll_hash_guard=True)
    (ROOT/'research/hd-rendering-checks.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PASS: pinned HD renderer metadata, status export ABI, windowed swapchain and cached/non-activating panel contracts')
if __name__=='__main__':run()
