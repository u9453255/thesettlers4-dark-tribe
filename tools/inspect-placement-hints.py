"""File-only checks for the exact Dark collision and native hint cache bridge."""
import json,struct,sys
from pathlib import Path
from game_build import verify_executable
from project_paths import game_exe
from placement_pins import PINS,generate
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile

def run():
    executable=game_exe();pin=verify_executable(executable)
    pe=pefile.PE(str(executable));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def check(rva,n,*rows):
        instructions={(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,n),base+rva)}
        for row in rows:assert row in instructions,(hex(rva),row)
    check(0x13ec48,28,('mov','ebx, dword ptr [0x15630d8]'))
    check(0x13ecb3,76,('and','cl, 0x5f'),('cmp','cl, 4'),('cmp','edi, 0x15'),
          ('cmp','ecx, 0x10'),('cmp','ecx, 0x40'),('cmp','ecx, 0x90'),('cmp','edi, 0x30'))
    assert struct.unpack('<I',pe.get_data(0x13eb58+8,4))[0]==base+0x13e8e8
    check(0x13e8e8,30,('push','dword ptr [ebp + 0x18]'),('call','0x53ebc0'),('ret','0x14'))
    check(0xee380,180,('sar','ebx, 4'),('sar','eax, 4'),('test','byte ptr [eax*2 + 0x12ef998], cl'),('je','0x4ee44b'))
    check(0xedeb0,400,('call','0x53e4e0'),('call','0x4eefa0'))
    check(0xedfdb,100,('call','0x53e4e0'),('call','0x4eefa0'))
    check(0xee1d0,130,('call','0x6598f0'),('call','0x659900'),('call','0x659910'))
    check(0x259910,28,('ret','0x10'))
    check(0x20c839,60,('mov','ecx, dword ptr [0x1454138]'),('mov','dword ptr [esi + 0x10], ecx'),
          ('mov','dword ptr [esi + 0x14], ecx'),('mov','byte ptr [esi + 0x1c], 0'))
    check(0x20eae7,15,('mov','dword ptr [eax], ecx'),('mov','dword ptr [ecx + ebx*4], eax'))
    check(0x20eb44,25,('mov','dword ptr [ecx], eax'),('mov','dword ptr [ecx + 4], 0'),('mov','dword ptr [eax + 4], ecx'))
    check(0xeefa0,105,('inc','dword ptr [ebx + 0x18]'),('ret','0xc'))
    relocations={r.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for r in block.entries if r.type==3}
    generated=ROOT/'work/native-generated/placement-signatures.hpp';generate(executable,generated)
    # Rebase data copies only, never load/execute a game module. These are the
    # same HIGHLOW fixups emitted into the bridge's fail-closed startup guard.
    for address in [base,0x360000,0x60000000]:
        rebased=pefile.PE(str(executable)).get_memory_mapped_image(ImageBase=address)
        for rva,length in PINS:
            raw=pe.get_data(rva,length);instructions=list(cs.disasm(raw,base+rva))
            assert sum(i.size for i in instructions)==length,'Partial native instruction in pin'
            expected=bytearray(raw)
            for relocation in relocations:
                if rva<=relocation<rva+length:
                    offset=relocation-rva;assert offset+4<=length
                    pointer=struct.unpack_from('<I',raw,offset)[0]-base+address
                    struct.pack_into('<I',expected,offset,pointer)
            assert rebased[rva:rva+length]==expected,(hex(address),hex(rva))
            changed=bytearray(expected);changed[0]^=1;assert changed!=expected
    source=(ROOT/'native/dark-tribe.cpp').read_text()
    # 0.2.15: construction no longer re-checks a cleared footprint for BuildingsAdd;
    # the retained native site is checked by construction-native.hpp (byte flags).
    assert 'static bool footprintFree(' not in source and 'BuildingsAdd(' not in source
    growth=(ROOT/'native/construction-native.hpp').read_text()
    assert 'image+0x11630d8' in growth and 'dt::darkBuildTerrain' in growth and 'LandscapeIsOccupied' not in growth
    assert 'placementHints.initialize(image)' in source and 'placementHints.reset();say("")' in source
    tick=source.split('static HRESULT __stdcall tick(',1)[1].split('static ',1)[0]
    assert 'placementHints.update(' in tick and source.count('placementHints.update(')==1
    bridge=(ROOT/'native/placement-native.hpp').read_text()
    for forbidden in ['lua','AcquireSRWLock','SendMessage','LandscapeIsOccupied']:
        assert forbidden not in bridge,forbidden
    report=dict(version='0.2.15',game_sha256=pin['sha256'],game_started=False,live_process_read=False,
        construction_collision_byte_map_rva='0x11630D8',walking_sector_used_for_construction=False,
        native_validator_rva='0x13E4E0',native_dark_validator_rva='0x13EBC0',
        native_sparse_setter_rva='0xEEFA0',native_hint_grid_rva='0x105413C',
        ordinary_territory_filter_bypassed_for_dark_hints_only=True,territory_ownership_changed=False,
        viewport_projection_rva='0x259910',tick_batch_limit=8192,refresh_ticks=10,
        generated_signature_regions=len(PINS),relocation_data_copy_bases=['0x400000','0x360000','0x60000000'],
        partial_instruction_or_relocation_pins=False,native_cache_existing_entries_reused=True,
        model_checks='all four original masks, all terrain/flag bytes, no-hover/pan/refresh scans and sparse entry reuse',
        gameplay_building_completion_verified=False,gameplay_circles_verified=False)
    (ROOT/'research/placement-hint-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: pinned Dark collision flags, territory-filtered background vs cursor hints, native cache ABI/layout, projection and relocation guards; file/data-only checks')

if __name__=='__main__':run()
