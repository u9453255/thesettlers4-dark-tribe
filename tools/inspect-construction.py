"""File-only checks for 0.2.14 in-place Dark construction (no game launched)."""
import json,struct,sys
from pathlib import Path
from game_build import verify_executable
from project_paths import game_exe
from construction_pins import PINS,generate
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile
DATA_PINS={0xc59214,0xc591bc,0xc59284,0xc58ae8,0xc593d0,0x14236c,0xc585e4+0x40,0xc585e4+0x10,0xc58ae8+0x2c}

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
    aircraft_model=(ROOT/'native/aircraft-model.hpp').read_text(encoding='utf-8')
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
    # r35: Dark-land sight hands greened cells back to the fog merge (RVA 0x1AE3F0). The merge counts stable cycles in
    # fog byte bits 6-7 (paddusb 0x40, and 0xC0), skips the land byte-3 write for an 8-cell group all at 0xC0, and resets
    # pending to current. Slot 2 (0x1AE600) stores the landscape at +8 and the fog byte map at +0xC.
    merge=rows(0x1ae3f0,0x19b)
    ordered(merge,('mov','eax, dword ptr [ecx + 0xc]'),('mov','eax, dword ptr [ecx + 8]'),(('mov',f'edx, {hex(base+0xc1bd30)}')),
            ('pcmpeqb','mm3, mm0'),('pand','mm3, qword ptr [edx]'),('paddusb','mm1, mm3'),('pand','mm1, mm7'),('pcmpeqb','mm3, mm1'),
            ('cmp','eax, -1'),('je','0x5ae538'),('mov','byte ptr [edi + 3], al'))
    assert pe.get_data(0xc1bd30,40)==bytes([0x40]*8+[0x18]*8+[7]*8+[0x38]*8+[0xc0]*8)
    check(0x1ae600,0x20,('mov','dword ptr [esi + 8], eax'),('mov','dword ptr [esi + 0xc], eax'))
    assert dword(0xc60a84+2*4)==base+0x1ae600 and dword(0xc60a84+5*4)==base+0x1aeab0
    check(0x1aeab0,0x80,('call','0x5ae3f0'))
    assert 'image+0x1ae438' in source and 'darkSight(land,fog,sightShown.data(),cells)' in source and 'sightShown.clear()' in source
    assert 'fog[i]&=0x3f' in aircraft_model and 'land[4*i+3]&=0xc0' in aircraft_model
    # r39: the native unload (0x16B990) posts events 0x19 and 0x11/0xD per passenger; the settler forwards events to role
    # slot 14 (+0x38); the Dark Gardener role's handler (0x174F10) knows only 0xD and 0x11 and otherwise re-adds the unit;
    # the soldier (0x18A020) and computer Gardener (0x179D1A) handlers place the unit (0x182A40, stdcall id,x,y, ret 0xC),
    # re-index it (0x1BB560, thiscall) and clear 0x8000 | 0x2100. The exit record is (byte +0x4E + 3) % 6 of 0xD69988.
    assert dword(0xc5ad04+14*4)==base+0x174f10 and dword(0xc5bf20+27*4)==base+0x1818d0 and dword(0xc5b51c+14*4)==base+0x179a80 and dword(0xc5c3ac+14*4)==base+0x189f70
    check(0x1818d0,0x14,('mov','ecx, dword ptr [edx + 0x58]'),('call','dword ptr [eax + 0x38]'))
    ordered(rows(0x174f10,0x40),('cmp','eax, 0xd'),('cmp','eax, 0x11'),('test','dword ptr [esi + 0x14], 0x20000000'),('push','1'),('call','0x56d5d0'))
    for at in (0x18a020,0x179d1a):
        ordered(rows(at,0x50),('call','0x582a40'),('and','eax, 0xffff7fff'),('or','eax, 0x2100'))
    check(0x18a03c,8,('call','0x5bb560'))
    ordered(rows(0x16b990,0x160),('movsx','eax, byte ptr [esi + 0x4e]'),('add','eax, 3'),('idiv','ebx'),('mov','dword ptr [ebp - 0x24], 0x19'),('mov','dword ptr [ebp - 0x24], 0x11'),('mov','dword ptr [ebp - 0x20], 0xd'))
    assert ('ret','0xc') in rows(0x182a40,0xd0) and ('mov','word ptr [eax + ebx*2], cx') in rows(0x182acc,0x10)
    check(0x1bb560,0x20,('cmp','dword ptr [esi + 0x1c], 0xffff'),('call','0x5bb440'),('jmp','0x5bb330'))
    assert 'image+0xc5ad04' in source and '((Place)(image+0x182a40))(r.second,ex,ey)' in source and '((Index)(image+0x1bb560))(u)' in source and '(bits&~0x8000u)|0x2100u' in source and 'placeUnloaded(c.entity,riders,x,y,ex,ey);' in source
    assert source.index('int ex=x,ey=y;{int k=dt::unloadExit(p[0x4e]);')<source.index('((Unload)(image+0x16b990))(p);')<source.index('placeUnloaded(c.entity,riders,x,y,ex,ey);')
    # r43: the AI's people manager sets the carrier reserve through CEcoManager +0x138 (0x51C10: word eco+0x20 =
    # clamp(v, 5, min(999, carriers eco+0x26))) as clamp(min(7n + 10, carriers - 10), 5, 80); the plugin redirects that one call (0x2567B8) to
    # reserveHook, which raises the value to carrierKeep(completed buildings) and calls the native setter. Pins: the call
    # site, the setter, the formula's clamp, the vtable slots and the building count table (completion increments
    # 0x10EF624 + (p*0x53 + type)*8).
    assert pe.get_data(0x2567ac,22)==bytes.fromhex('8b4328578b40248b48188b01ff90380100005f5e5bc3')
    ordered(rows(0x51c10,0x2e),('mov','eax, dword ptr [ecx + 4]'),('mov','edi, dword ptr [ebp + 8]'),('movzx','ecx, word ptr [esi + 0x20]'),('cmp','edi, 5'),('mov','eax, 5'),('mov','word ptr [esi + 0x20], ax'),('ret','4'))
    ordered(rows(0x25676e,0x50),('call','dword ptr [eax + 0x98]'),('cmp','ecx, 0xa'),('lea','edi, [eax - 0xa]'),('cmovge','edi, esi'),('cmp','edi, 5'),('mov','edi, 5'),('mov','eax, 0x50'),('cmovg','edi, eax'),('call','dword ptr [eax + 0x138]'))
    assert ('mov','eax, dword ptr [ecx + 8]') in rows(0x51a70,3) and ('imul','eax, eax, 0x53') in rows(0x136330,0x18) and ('inc',f'dword ptr [eax*8 + {hex(base+0x10ef624)}]') in rows(0x136330,0x18)
    assert pe.get_data(0xc2bc7c+0x138,4)==(base+0x51c10).to_bytes(4,'little') and pe.get_data(0xc2bc7c+0x80,4)==(base+0x51a70).to_bytes(4,'little')
    assert 'static int __fastcall reserveHook(BYTE*eco,void*,int value){' in source and 'static bool installReserveHook(){' in source and 'if(!installReserveHook()){fatal=true;' in source
    assert 'set=dtoffence::carrierReserve(value,b,carriers,room);' in source and 'image+0x10ef624+(uintptr_t)p*0x298' in source and 'at[12]=0xe8;' in source and 'FlushInstructionCache(GetCurrentProcess(),at+12,6)' in source and 'CARRIER_RESERVE' in source and 'for(auto&r:reserveSeen)r=ReserveSeen{};' in source
    # r42: the Alt+9 reveal clears only the pending bits 3-5 of the fog bytes every tick (the merge then shows level 0);
    # base bits and counters stay (saves honest, off = natural re-fogging); keys polled on the tick, foreground + in game.
    assert 'if(fog[i]&0x38){fog[i]&=0xc7;n++;}' in aircraft_model and 'revealTick(t);if(t%5==0){aircraftJobs(t);darkSightTick(t);}' in source
    assert 'pid==GetCurrentProcessId()&&inGame()&&(GetAsyncKeyState(VK_MENU)&0x8000)&&(GetAsyncKeyState(\'9\')&0x8000)' in source and 'revealAll=false;revealWas=false;revealLogAt=0;' in source
    assert source.count('GetAsyncKeyState')==2 and 'MAP_REVEAL' in source
    # r41: the Manacopter update heals a landed aircraft on Dark land to DTManakopterHitpoints every 10 ticks; the plugin
    # NOPs that one store (88 47 21 at 0x16B594). The other hit-point writers (constructor, damage) stay.
    ordered(rows(0x16b550,0x4c),('cmp','edx, -0xa'),('cmp','dword ptr [edi + 0x7c], -0x32'),('push','0xa'),('call','0x56d5d0'),('test','byte ptr [eax + ecx], 4'),('je','0x56b71a'),
            ('mov',f'al, byte ptr [{hex(base+0xdcdb24)}]'),('mov','byte ptr [edi + 0x21], al'),('jmp','0x56b71a'))
    assert pe.get_data(0x16b594,3)==bytes([0x88,0x47,0x21]) and ('mov','byte ptr [ebx + 0x21], cl') in rows(0x16af74,3) and ('mov','byte ptr [ecx + 0x21], bl') in rows(0x16b4d6,3)
    assert 'static bool installNoHealing(){' in source and 'at[11]=0x90;at[12]=0x90;at[13]=0x90;' in source and 'FlushInstructionCache(GetCurrentProcess(),at+11,3)' in source and 'if(!installNoHealing()){fatal=true;' in source
    # r51: the Ubisoft services job guard (research/CRASH_DIAGNOSTICS_0215.md Round r51). The SDK's step runner 0x468520
    # asserts when a job's step sequence holds no step method; with no assert callback (dword 0x122A6CC) the handler
    # 0x341660 writes to 0xDEADBAAD (three field crashes, each about 35 s after the game started). The plugin replaces the
    # branch at 0x46854C (33 bytes, reached only by falling through both null checks) with: count the job, park it through
    # its own state setter 0x4CC550 with Waiting (1), leave through the function's epilogue 0x46866B.
    import re
    ubi=source[source.index('static bool installUbiStepGuard(){'):source.index('static void ubiServicesNote(){')]
    def arr(name):
        m=re.search(r'BYTE '+name+r'\[(\d+)\]=\{([^}]*)\}',ubi);vals=[int(v,0) for v in m.group(2).split(',')];assert len(vals)==int(m.group(1)),name;return bytes(vals)
    def reloc(b,off,rva):return b[:off]+(base+rva).to_bytes(4,'little')+b[off+4:]
    assert pe.get_data(0x468520,16)==reloc(arr('head'),7,0xd62444) and pe.get_data(0x468530,28)==arr('decide') and pe.get_data(0x46854c,33)==reloc(arr('broken'),3,0xb20358) and pe.get_data(0x46866b,17)==arr('tail')
    assert pe.get_data(0x4cc550,35)==reloc(arr('setHead'),10,0xd62444) and pe.get_data(0x4cc718,21)==arr('setTail')
    for rva in (0x468527,0x46854f,0x4cc55a):assert rva in {r.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for r in block.entries if r.type==3},hex(rva)
    for address in (base,0xad0000,0x60000000):  # the installer's memcmp at other load addresses (the field crash ran at 0xAD0000)
        image_bytes=pefile.PE(str(executable)).get_memory_mapped_image(ImageBase=address)
        at=lambda b,off,rva:b[:off]+(address+rva).to_bytes(4,'little')+b[off+4:]
        assert image_bytes[0x468520:0x468530]==at(arr('head'),7,0xd62444) and image_bytes[0x46854c:0x46856d]==at(arr('broken'),3,0xb20358) and image_bytes[0x4cc550:0x4cc573]==at(arr('setHead'),10,0xd62444),hex(address)
        assert image_bytes[0x468530:0x46854c]==arr('decide') and image_bytes[0x46866b:0x46867c]==arr('tail') and image_bytes[0x4cc718:0x4cc72d]==arr('setTail'),hex(address)
    patched=[(i.mnemonic,i.op_str) for i in cs.disasm(arr('patch'),base+0x46854c)]
    assert patched==[('mov','ecx, dword ptr [ebp + 8]'),('mov','eax, dword ptr [ecx]'),('mov','dword ptr [0], eax'),('lock inc','dword ptr [0]'),('push','1'),('call',hex(base+0x4cc550)),('jmp',hex(base+0x46866b)),('nop',''),('nop',''),('nop',''),('nop','')],patched
    ordered(rows(0x468539,0x13),('cmp','dword ptr [esi], 0'),('jne',hex(base+0x4685ea)),('cmp','dword ptr [esi + 8], 0'),('jne',hex(base+0x4685ea)))
    assert pe.get_data(0xb20320,55)==b'm_pStepMethod != nullptr || m_pStepMethodEx != nullptr\x00' and ('call',hex(base+0x341660)) in rows(0x4685cd,5)
    assert ('je',hex(base+0x341676)) in rows(0x341660,0x16) and ('mov','dword ptr [0xdeadbaad], 0') in rows(0x341676,10) and ('call','eax') in rows(0x468667,4)
    for i in cs.disasm(pe.get_data(0x468520,0x15c),base+0x468520):  # nothing outside the dead branch lands inside the replaced bytes
        if (i.mnemonic.startswith('j') or i.mnemonic=='call') and i.op_str.startswith('0x') and 0x46854c<=int(i.op_str,16)-base<0x46856d:assert 0x46854c<=i.address-base<0x4685ea,hex(i.address)
    ordered(rows(0x4cc565,0xe),('mov','edi, dword ptr [ebp + 8]'),('mov','esi, ecx'),('push','edi'),('push','dword ptr [esi + 0xc]'),('call',hex(base+0x4f7f20)))
    assert ('mov','dword ptr [esi + 0xc], edi') in rows(0x4cc71b,3) and ('ret','4') in rows(0x4cc72a,3)
    ordered(rows(0x4f7f72,0x17),('cmp','edi, 3'),('jne',hex(base+0x4f7f89)),('cmp','esi, 1'),('je',hex(base+0x4f816d)),('cmp','esi, 4'),('je',hex(base+0x4f816d)))  # Running -> Waiting or Complete
    ordered(rows(0x45404f,0xe),('lea','ecx, [ebx + 0x20]'),('push','ebx'),('call',hex(base+0x468520)))
    ordered(rows(0x45426d,0xa),('cmp','dword ptr [ebx + 0xc], 3'),('je',hex(base+0x454040)))  # the step loop repeats while Running
    ordered(rows(0x45fbb7,0x2c),('cmp','dword ptr [esi + 0xc], 2'),('push','3'),('call',hex(base+0x4cc550)),('call',hex(base+0x453c60)),('cmp','eax, 4'),('cmp','eax, 1'),('cmp','eax, 2'))
    assert 'if(!installUbiStepGuard()){fatal=true;' in source and 'return 18;' in source and 'dtdiag::beat(true,true);ubiServicesNote();' in source and 'UBISERVICES_JOB_PARKED' in source and 'Ubisoft services job guard r51' in source
    assert source.index('if(!installReserveHook())')<source.index('if(!installUbiStepGuard())')<source.index('Version 0.2.15 native callbacks installed')
    # r12: own idle units on the reserved shape are ordered off (throttled); the
    # mod's own moves are ignored by the send callback.
    assert 'ops.evict(j)' in model and 'void blockers(' in growth and 'internalSend' in source and 'SITE_EVICT' in source
    assert 'bool wild=q&&cls==0x80' in growth and 'if(wild&&i>=0)continue;' in growth  # r14: wild animals only block the anchor
    assert 'if(!reserved)reserve(' in growth and 'removePiles(r,p)' in growth and 'economy(p)' in growth
    assert 'SiteSpace::MobileBlocked' in model and 'ConstructionEvent::LegacyCancelled' in model
    # r30 citadel fire blockade and start-map farm removal. Lua CrushBuilding
    # (S4ModApi BuildingsCrush, the player's own demolition) jumps to the mode-1
    # removal 0x136960, which skips the economy notify for race 3 and calls
    # Kill 0x16D030(id, attacker 0). Kill calls CBuilding slot 4 (0x1361D0) on
    # the same tick; slot 4 calls 0x13C680, which decrements the ready counter
    # [0x10EF624+idx*8] (role slot +0x70 true) or the site counter. Attacker 0
    # skips Kill's statistics; the Farm role's slot 11 (0x145050) credits
    # MushroomFarmsDestroyed only to a non-zero building byte +0x25.
    ordered(rows(0x1c4d4c,0x23),('mov','ecx, dword ptr [ecx*4 + 0x129bc38]'),('cmp','byte ptr [ecx + 0xa], 8'),('test','dword ptr [ecx + 0x14], 0x4000000'),('je','0x536960'),('ret',''))
    ordered(rows(0x136a26,0x58),('cmp','al, 3'),('je','0x536a68'),('push','0'),('push','eax'),('call','0x56d030'),('jmp','dword ptr [eax + 0x2c]'))
    assert rows(0x16d1c7,7)==[('mov','eax, dword ptr [esi]'),('mov','ecx, esi'),('call','dword ptr [eax + 0x10]')]
    assert dword(0xc585e4+0x10)==base+0x1361d0 and dword(0xc58ae8+0x2c)==base+0x145050
    assert rows(0x1362a9,10)==[('movzx','eax, word ptr [esi + 8]'),('push','eax'),('call','0x53c680')]
    ordered(rows(0x13c719,0x5d),('call','dword ptr [eax + 0x70]'),('dec','dword ptr [esi*8 + 0x14ef624]'),('dec','dword ptr [0x14eea70]'))
    assert ('dec','dword ptr [esi*8 + 0x14ef620]') in rows(0x13c77f,7)
    ordered(rows(0x145050,0x23),('movzx','eax, byte ptr [eax + 0x25]'),('test','eax, eax'),('je','0x545070'),('inc','dword ptr [eax]'))
    blockade=(ROOT/'native/blockade-model.hpp').read_text(encoding='utf-8')
    fire=(ROOT/'native/citadel-fire.hpp').read_text(encoding='utf-8')
    hints=(ROOT/'native/placement-native.hpp').read_text(encoding='utf-8')
    assert 'FarmLock=FireLine+FarmReach+2' in fire and 'dtfire::farmTooClose(' in blockade and 'constexpr' not in blockade  # one shared geometry
    assert 'refuseConstruction(j,ops,ConstructionEvent::FireBlocked)' in model and 'if(fire)fire->add(j.kind,j.x,j.y);' in model
    sweep=source.split('static bool fireSweep(',1)[1].split('static void construction(',1)[0]
    assert 'api->BuildingsCrush(id)' in sweep and 'FARM_FIRE_REMOVED' in sweep and '0x136a80' not in sweep.lower() and 'BuildingsDelete' not in sweep
    assert 'fireStartPending=!restored;' in source and source.count('fireStartPending=')==3
    assert 'dt::discoverConstruction(j,t,ops,limit,&fire);' in source and 'if(start||load)fireSweep(farms,fire,load);' in source
    # r30 review: a restored save sweeps once too (pre-r30 farms inside a field), logged with reason=load
    assert 'fireLoadPending=restored;' in source and source.count('fireLoadPending=')==3 and 'load=fireLoadPending&&!start;' in source
    assert 'load?"load":"start"' in sweep and 'if(!start&&!load)for(auto id:farms)' in source
    assert 'placementHints.blockWith(&fireBlockade);' in source and 'say(dt::fireNotice(j.kind))' in source
    assert 'self.blockade->blocked(type,x,y)' in hints
    # r34 site selection crash fix (field pid 8768: 0x143E07 read 0x8E on a new,
    # unpaid Farm site clicked in the input batch that created it). The slot-22
    # entry is the panel's only reference (no direct call), the panel is
    # thiscall(role, building, flag) with `ret 8`, and the plugin repeats its
    # economy lookup exactly before choosing the transient safe view.
    image=pe.get_memory_mapped_image();needle=struct.pack('<I',base+0x143d10);refs=[];at=image.find(needle)
    while at>=0:refs.append(at);at=image.find(needle,at+1)
    assert refs==[0xc59214],[hex(r) for r in refs]
    assert dword(0xc59214)==base+0x143d10 and 0xc59214 in relocations and rows(0x1440eb,3)==[('ret','8')]
    assert ('mov','esi, ecx') in rows(0x143d3b,2) and ('mov','eax, dword ptr [ebp + 8]') in rows(0x143d3d,3)
    ordered(rows(0x143d3b,0x48),('movzx','eax, word ptr [esi + 6]'),('mov','eax, dword ptr [eax*4 + 0x129bc38]'),('mov','eax, dword ptr [eax + 0x48]'),
            ('sar','edx, 0x10'),('imul','edx, dword ptr [0x1454138]'),('add','edx, eax'),('mov','eax, dword ptr [0x15630c4]'),('movzx','eax, word ptr [eax + edx*2]'),
            ('lea','eax, [eax + eax*4]'),('movzx','edi, word ptr [eax*4 + 0x12f1a26]'))
    ordered(rows(0x143dc2,0x55),('mov','eax, dword ptr [esi + 0x178]'),('movsx','ebx, byte ptr [eax + 0x1df]'),('mov','ecx, dword ptr [edi*4 + 0x146c8a4]'),('call','0x4d4280'),
            ('mov','eax, dword ptr [esi + 0x178]'),('movsx','ecx, byte ptr [eax + 0x1de]'),('mov','eax, dword ptr [edi*4 + 0x146c8a4]'),('cmp','word ptr [eax + ecx*2 + 0x24], 0'))
    for pile,call,good in [(0x143e81,0x143eb8,'7'),(0x143ee8,0x143f1f,'0x20'),(0x143f4f,0x143f86,'0xe')]:
        assert rows(pile,7)==[('movzx','eax, word ptr [esi + 0x%x]'%(0x1b0+2*((pile-0x143e81)//0x67)))],hex(pile)
        assert rows(call,14)==[('mov','ecx, dword ptr [edi*4 + 0x146c8a4]'),('push',good),('call','0x4d4280')],hex(call)
    assert ('cmp','dword ptr [eax + 0x1ec], 0') in rows(0x143e2d,7)
    guard=growth.split('struct PanelGuard {',1)[1]
    for text in ['module+0xc59214','module+0x143d10','*slot!=module+0x143d10','image+0xe9bc38+4u*id','image+0x1054138','image+0x11630c4','image+0xef1a26+20u*tile',
                 'image+0x106c8a4+4u*eco','role+0x178','role+0x1b0','dt::InfoInhabitant','dt::InfoTool','__fastcall panel(void*role,void*,void*building,uint32_t flag)',
                 'using Panel=void(__thiscall*)(void*,void*,uint32_t)','dt::withPanelView(','dt::panelCall(s)','VirtualProtect(slot,4,PAGE_READWRITE,&old)']:
        assert text in guard,text
    for forbidden in ['log(','fopen','new ','malloc','std::vector','std::string','AcquireSRWLock','lua','api->','snprintf']:
        assert forbidden not in guard,forbidden
    panel_model=(ROOT/'native/site-panel-model.hpp').read_text(encoding='utf-8')
    assert 'constexpr unsigned BuildingInfoSize=0x32c,InfoInhabitant=0x1de,InfoTool=0x1df;' in panel_model
    assert '*info=savedInfo;memcpy(piles,saved,sizeof(saved));' in panel_model and 'out[InfoInhabitant]=0;out[InfoTool]=0;' in panel_model
    assert source.index('nativeConstruction.initialize(image)')<source.index('dtconstruction::PanelGuard::install(image)')<source.index('Version 0.2.15 native callbacks installed')
    assert 'SITE_PANEL_GUARD views=' in source and 'site selection crash fix r34' in source
    assert 'panelGuard();' in (ROOT/'native/construction-tests.cpp').read_text(encoding='utf-8')
    report=dict(version='0.2.15',game_sha256=pin['sha256'],game_started=False,live_process_read=False,
        successful_construction='same native site completes in place; no delete/recreate',
        site_role_vtable_rva='0xC591BC',phase_dispatch='2..8; mod freezes at 1',grow_kinds=[49,80],
        grow_completion='phase-8 sequence RVA 0x142312 (entity map, flags 0x2000000, RVA 0x1362F0)',
        ordinary_completion='RVA 0x1424C0 + 0x1429F0 when an economy exists; else pile removal mirror + phase-8 sequence',
        reservation_rva='0x136470 once, skipped when native phases already reserved',
        unit_blockers='waited for without timeout refund, as native phase 7',
        site_panel_guard='CBuildingSiteRole slot 22 (entry RVA 0xC59214 -> 0x143D10) wrapped once at startup; no economy at the entrance: transient info copy without inhabitant/tool and piles 0, restored after the call',
        ubiservices_step_guard='r51: the ubiservices step runner 0x468520 crashes (assert handler 0x341660 writes 0xDEADBAAD, no assert callback) when a job step has no method; the branch at 0x46854C (33 bytes) parks the job instead: state setter 0x4CC550 with Waiting (1, legal from Running), epilogue 0x46866B; the runner re-queues it (0x45FBD5)',
        generated_signature_regions=len(PINS),relocation_data_copy_bases=['0x400000','0x360000','0x60000000'],
        gameplay_completion_verified=False,economy_presence_on_dark_land_verified=False)
    (ROOT/'research/construction-inplace-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: in-place construction ABI, phase dispatch/grow path, native completion sequences, callee conventions and relocation-aware pins; file/data-only checks')

if __name__=='__main__':run()
