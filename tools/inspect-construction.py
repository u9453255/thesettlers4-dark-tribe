"""File-only checks for 0.2.14 in-place Dark construction (no game launched)."""
import json,struct,sys
from pathlib import Path
from game_build import verify_executable
from project_paths import game_exe
from construction_pins import PINS,generate
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile
DATA_PINS={0xc591bc,0xc59284,0xc58ae8,0xc593d0,0x14236c,0xc585e4+0x40}

def run():
    executable=game_exe();pin=verify_executable(executable)
    pe=pefile.PE(str(executable));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def rows(rva,n):return [(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,n),base+rva)]
    def check(rva,n,*expected):
        found=rows(rva,n)
        for row in expected:assert row in found,(hex(rva),row)
        return found
    def ordered(found,*expected):
        at=0
        for row in expected:at=found.index(row,at)+1
    def dword(rva):return struct.unpack('<I',pe.get_data(rva,4))[0]
    # Site role vtable: tick slot 3, init slot 6, request detach slot 11.
    assert dword(0xc591bc+3*4)==base+0x141cc0 and dword(0xc591bc+6*4)==base+0x141a50 and dword(0xc591bc+11*4)==base+0x142d50
    # Tick dispatches phases 2..8 only, so phase 1 is inert (mod freeze).
    check(0x141d3e,26,('movzx','eax, byte ptr [edi + 0x18c]'),('add','eax, -2'),('cmp','eax, 6'),('ja','0x542361'))
    table=[dword(0x14236c+4*i)-base for i in range(7)]
    assert table==[0x141d58,0x141d97,0x141dcf,0x1421d1,0x14220c,0x14226e,0x142312],table
    # Init: Farm 0x31 and Hall 0x50 take the builder-free grow path (phase 6).
    check(0x141a50,0x100,('or','eax, 0x1001000'),('call','0x543680'))
    check(0x143680,0x35,('cmp','eax, 0x31'),('je','0x543b83'),('cmp','eax, 0x50'))
    check(0x143b83,0xbd,('mov','byte ptr [eax + 0x18c], 6'),('mov','word ptr [eax + edx*2], cx'))
    # Phase 7 reserves after units leave; phase 8 writes the entity map, flags, completion.
    ordered(rows(0x142267,0xab),('call','0x60d870'),('call','0x536470'),('mov','byte ptr [edi + 0x18c], 8'))
    phase8=rows(0x142312,0x4f)
    ordered(phase8,('mov','eax, dword ptr [0x15630dc]'),('mov','word ptr [eax + edx*2], cx'),('mov','eax, dword ptr [0x15630e0]'),
            ('and','eax, 0xfeffffff'),('or','eax, 0x2000000'),('call','0x5362f0'))
    # Phase 5 completion pair, both thiscall(role, entity).
    ordered(rows(0x1421f3,0x14),('call','0x5424c0'),('call','0x5429f0'))
    finish=rows(0x1429f0,0x1a8)
    for row in [('mov','eax, dword ptr [0x12f1a18]'),('movzx','ebx, word ptr [eax*4 + 0x12f1a26]'),('je','0x542b84'),
                ('movzx','eax, word ptr [esi + 0x1b0]'),('movzx','eax, word ptr [esi + 0x1b2]'),('movzx','eax, word ptr [esi + 0x1b4]'),
                ('call','dword ptr [edx + 0x40]'),('dec','dword ptr [0x11cdb4c]'),('call','0x56d030'),('call','0x5362f0'),('ret','4')]:
        assert row in finish,row
    assert ('ret','4') in rows(0x1424c0,0x123)
    # Callee conventions used by the mod.
    assert ('ret','4') in rows(0x137100,0x33) and dword(0xc585e4+0x40)==base+0x137100
    assert ('ret','8') in rows(0x16d030,0x1c9)
    assert ('ret','') in rows(0x1362f0,0x171) and ('ret','') in rows(0x142d50,0xf0)
    # Info table stride/base used for the role identity check.
    check(0x141a50,0x40,('imul','eax, edx, 0x32c'),('lea','eax, [eax + 0x149c600]'))
    relocations={r.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for r in block.entries if r.type==3}
    generate(executable,ROOT/'work/native-generated/construction-signatures.hpp')
    for address in [base,0x360000,0x60000000]:
        rebased=pefile.PE(str(executable)).get_memory_mapped_image(ImageBase=address)
        for rva,length in PINS:
            raw=pe.get_data(rva,length)
            if rva not in DATA_PINS:
                assert sum(i.size for i in cs.disasm(raw,base+rva))==length,('partial instruction pin',hex(rva))
            expected=bytearray(raw)
            for relocation in relocations:
                if rva<=relocation<rva+length:
                    offset=relocation-rva;assert offset+4<=length
                    struct.pack_into('<I',expected,offset,struct.unpack_from('<I',raw,offset)[0]-base+address)
            assert rebased[rva:rva+length]==expected,(hex(address),hex(rva))
    source=(ROOT/'native/dark-tribe.cpp').read_text(encoding='utf-8')
    growth=(ROOT/'native/construction-native.hpp').read_text(encoding='utf-8')
    model=(ROOT/'native/construction-model.hpp').read_text(encoding='utf-8')
    assert 'BuildingsAdd(' not in source and 'nativeConstruction.initialize(image)' in source
    for forbidden in ['lua','AcquireSRWLock','SendWarriors','LandscapeIsOccupied','BuildingsDelete']:
        assert forbidden not in growth,forbidden
    # 0.2.14 field report: a mushroom planted beside a growing Farm (object routine
    # RVA 0x163510 sets keep-clear bit 0x40 around it) cancelled it. Reserved
    # cells are checked like native phase 7: entity occupancy only.
    assert ('or','byte ptr [eax + ebx], 0x40') in rows(0x163598,4)
    assert 'if(!claimed)for(' in growth and 'lastBlock' in source
    # r4: ordinary sites (Temple/Fortress) enter the building layer like native
    # phase 4 so the renderer draws their rise; the +0x314 clear runs only when info+6 is set.
    check(0x141f1b,0x10,('cmp','byte ptr [edx + 6], 0'),('je','0x54205f'))
    check(0x141fe7,0x1b,('mov','word ptr [eax + esi*2], cx'),('or','byte ptr [eax + esi], 8'))
    check(0x142181,0x2a,('mov','eax, dword ptr [0x15630dc]'),('mov','word ptr [eax + edx*2], cx'),('call','0x536470'))
    ordered(rows(0x135ac0,0x60),('inc','eax'),('add','edx, edx'))
    assert 'claimOrdinary(j,r)' in growth and 'info+0x314' in growth and 'growthTicks(j.kind)' in model
    # r5: site panel (slot 22) reads the entrance economy for the inhabitant and
    # for each material pile; pause (slot 8) for flag 0x1000. Guards in growth.
    assert dword(0xc591bc+22*4)==base+0x143d10 and dword(0xc591bc+8*4)==base+0x142bf0
    check(0x143def,0x28,('movsx','ecx, byte ptr [eax + 0x1de]'),('cmp','word ptr [eax + ecx*2 + 0x24], 0'))
    check(0x143eb8,0x10,('mov','ecx, dword ptr [edi*4 + 0x146c8a4]'),('call','0x4d4280'))
    check(0x142c42,0x10,('test','eax, 0x1000'),('je','0x542d07'))
    assert 'useSafeInfo(j,r)' in growth and 'activeFlag(p,false)' in growth and 'safeCopies[k][0x1de]=0' in growth
    # r5: passenger arrival mirror and native fog cell reveal.
    check(0x189cd3,0x17,('mov','byte ptr [ebx + 0x42], 0xa'),('call','0x56d720'))
    assert ('ret','8') in rows(0x16d720,0x9c) and ('ret','8') in rows(0x16d5d0,0x147)
    # r6: aircraft sight uses the native temporary vision circle like settlers
    # (band pass RVA 0x1AE395 -> 0x1AE100, pending bits merged/faded by the cycle).
    ordered(rows(0x1ae37f,0x20),('push','ecx'),('push','eax'),('push','edx'),('call','0x5ae100'))
    assert ('ret','0xc') in rows(0x1ae100,0x109) and ('mov','ecx, 0x19') in rows(0x1ae306,0x10)
    assert 'image+0x16d720' in source and 'image+0x1ae100' in source
    assert 'image+0x1aed30' not in source and 'image+0x1aeef0' not in source and 'revealCircle(land' not in source
    # r12: own idle units on the reserved shape are ordered off (throttled); the
    # mod's own moves are ignored by the send callback.
    assert 'ops.evict(j)' in model and 'void blockers(' in growth and 'internalSend' in source and 'SITE_EVICT' in source
    assert 'bool wild=q&&cls==0x80' in growth and 'if(wild&&i>=0)continue;' in growth  # r14: wild animals only block the anchor
    assert 'if(!reserved)reserve(' in growth and 'removePiles(r,p)' in growth and 'economy(p)' in growth
    assert 'SiteSpace::MobileBlocked' in model and 'ConstructionEvent::LegacyCancelled' in model
    report=dict(version='0.2.15',game_sha256=pin['sha256'],game_started=False,live_process_read=False,
        successful_construction='same native site completes in place; no delete/recreate',
        site_role_vtable_rva='0xC591BC',phase_dispatch='2..8; mod freezes at 1',grow_kinds=[49,80],
        grow_completion='phase-8 sequence RVA 0x142312 (entity map, flags 0x2000000, RVA 0x1362F0)',
        ordinary_completion='RVA 0x1424C0 + 0x1429F0 when an economy exists; else pile removal mirror + phase-8 sequence',
        reservation_rva='0x136470 once, skipped when native phases already reserved',
        unit_blockers='waited for without timeout refund, as native phase 7',
        generated_signature_regions=len(PINS),relocation_data_copy_bases=['0x400000','0x360000','0x60000000'],
        gameplay_completion_verified=False,economy_presence_on_dark_land_verified=False)
    (ROOT/'research/construction-inplace-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: in-place construction ABI, phase dispatch/grow path, native completion sequences, callee conventions and relocation-aware pins; file/data-only checks')

if __name__=='__main__':run()
