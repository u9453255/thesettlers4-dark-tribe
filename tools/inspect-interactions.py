"""Pinned file-only evidence for construction, income and aircraft interaction."""
import hashlib,json,re,struct,sys
from pathlib import Path
from game_build import ORIGINAL_SHA256, verify_executable
from project_paths import game_exe
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile

def run():
    executable=game_exe()
    game_build=verify_executable(executable)
    pe=pefile.PE(str(executable));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def rows(rva,n):return [(i.mnemonic,i.op_str) for i in cs.disasm(pe.get_data(rva,n),base+rva)]
    assert pe.get_data(0x389c0,6)==bytes.fromhex('558bec8b4d08')
    production=rows(0x389c0,69)
    for row in [('imul','eax, ecx, 0x17c'),('mov','eax, dword ptr [eax + 0x1292648]'),
                ('add','ecx, dword ptr [eax + 0x2c]'),('xor','ecx, 0x27356334'),('ret','4')]:
        assert row in production,row
    # CAIMain owns this direct income path; it never calls the old 0x36380 hook.
    assert not any(op=='call' for op,arg in production)
    table=struct.unpack('<I',pe.get_data(0xc2a8d4,4))[0]
    assert table==base+0x389c0
    catchup=rows(0x3b303,64)
    assert ('call','0x436380') in catchup and ('cmp','eax, dword ptr [esp + 0x10]') in catchup
    deletion=rows(0x1c5edf,32)
    assert ('call','0x536a80') in deletion and ('jmp','dword ptr [eax + 0x10]') in deletion
    assert ('jmp','0x536960') in deletion
    destruction=rows(0x136bc1,128)
    assert ('push','0x3d') in destruction and ('push','0x4d') in destruction
    # Source-level callback contracts: gameplay actions live on ticks; rendering
    # and input only copy snapshots/enqueue intent, and never block on state.
    source=(ROOT/'native/dark-tribe.cpp').read_text(encoding='utf-8')
    # The previous interpretation of mode 0 was wrong: CBuilding +0x10
    # is cleanup without native deletion scheduling. Mode 1 clears masks,
    # schedules removal, and mode 2 additionally emits destruction graphics.
    assert struct.unpack('<I',pe.get_data(0xc585e4+0x10,4))[0]==base+0x1361d0
    cleanup=rows(0x1361d0,273)
    assert ('call','0x56d030') not in cleanup and ('ret','') in cleanup
    cancel=rows(0x136960,286)
    assert ('call','0x536d00') in cancel and ('call','0x56d030') in cancel
    scheduling=rows(0x16d030,455)
    assert ('or','eax, 0x20000000') in scheduling and ('call','0x56db20') in scheduling
    pool_release=rows(0x16d200,65)
    assert ('mov','dword ptr [esi*4 + 0x129bc38], 0') in pool_release
    # One Dark Temple and one Dark Fortress: a completed base counts until native
    # removal marks it. 0x16D030 sets UID bit 0x20000000 (+4) and the dying flag
    # 0x4000000 (+0x14) together, before it queues the deletion (0x16DB20); the
    # pool release later zeroes the slot. Lua Buildings.CrushBuilding (S4ModApi
    # BuildingsCrush, the mod's demolish) refuses flagged entities and removes a
    # building through mode 1 (0x136960 -> 0x16D030); mode 2 destruction
    # (0x136A80) also ends in 0x16D030.
    names=[f'{m} {a}' for m,a in scheduling]
    queued=names.index('call 0x56db20')
    uid,dying=names.index('or eax, 0x20000000'),names.index('or eax, 0x4000000')
    assert 'mov dword ptr [esi + 4], eax' in names[uid+1:queued] and 'mov dword ptr [esi + 0x14], eax' in names[dying+1:queued]
    name,crush=struct.unpack('<II',pe.get_data(0xd65cb8,8))
    assert pe.get_data(name-base,14)==b'CrushBuilding\x00' and crush==base+0x1c4cf0
    crush_rows=rows(0x1c4cf0,0x7f)
    for row in [('cmp','byte ptr [ecx + 0xa], 8'),('test','dword ptr [ecx + 0x14], 0x4000000'),('je','0x536960')]:
        assert row in crush_rows,row
    assert ('call','0x56d030') in rows(0x136a80,0x275)
    construction=source.split('struct ConstructionOps {',1)[1].split('static void aircraftJobs',1)[0]
    model=(ROOT/'native/construction-model.hpp').read_text(encoding='utf-8')
    assert 'FoundationCancelMode=1' in model and 'ops.remove(j,FoundationCancelMode)' in model
    assert 'BuildingsDelete(j.site,mode)' in construction and 'BuildingsDelete(id,dt::FoundationCancelMode)' in construction
    assert 'BuildingsDelete(j.site,0)' not in source and 'BuildingsDelete(j.site,2)' not in source
    assert 'p[32]==0x13' in construction and 'p[32]!=0x13' in construction and '!owned(j.site)' not in construction
    assert 'ops.mana()<j.cost' in model and 'tick+growthTicks(j.kind)' in model
    assert 'j.refunded&&ops.sameSite(j)' in construction and 'dt::foundationIdentity' in construction
    assert 'uid&0x60000000' in model and 'ConstructionEvent::LegacyCancelled' in model
    # Successful construction completes the same site in place; it is never deleted/recreated.
    assert 'BuildingsAdd(' not in source and 'nativeConstruction.finish(j)' in construction
    assert 'if(j.refunded)return' in model and 'BUILD_CANCEL' in construction and 'BUILD_STATE' in construction
    # Limit: counted in the existing discovery scan (built: unmarked UID, no dying
    # flag, not a site; jobs: not refunded); a second site is refused before
    # payment with mode 1; command 1 and the tiles read the tick's counts.
    limit=source.split('static dt::BuildingLimit buildingLimit;',1)[1].split('static void aircraftJobs',1)[0]
    assert 'dt::countJobs(limit,state)' in limit and 'dt::discoverConstruction(j,t,ops,limit)' in limit and 'buildingLimit=limit;' in limit
    assert '!(peek<uint32_t>((uintptr_t)p+0x14)&0x4000000))limit.countBuilt(k)' in limit and limit.count('for(uint32_t id=1;id<65536;id++)')==1
    assert 'if(buildingLimit.exists((uint16_t)c.arg)){say(dt::limitNotice((uint16_t)c.arg));continue;}' in source
    assert 'view.buildLimit[c]=buildingLimit.state(' in source
    assert 'refuseConstruction(j,ops,ConstructionEvent::Duplicate)' in model and 'if(!j.refunded)limit.countBuilding(j.kind);' in model
    assert 'return refuseConstruction(j,ops,ConstructionEvent::Unpaid);' in model
    # Native farm credit has a read-only signature guard, never a mod detour.
    farm_guard=source.split('static bool nativeFarmIncomeIntact(){',1)[1].split('static void position',1)[0]
    assert 'image+0x389c0' in farm_guard and 'memcmp' in farm_guard
    assert 'VirtualProtect' not in farm_guard and 'VirtualAlloc' not in farm_guard
    for obsolete in ['installFarmIncomeHook', 'farmIncome(', 'pendingHarvest', 'spawnCourier', 'routeHarvest', 'discoverFarms', 'static void deliveries', 'farmRecord', 'c.type==4']:
        assert obsolete not in source,obsolete
    assert 'running.load()&&!incomeBypass&&player==1&&amount>0' in source
    assert source.count('MagicIncreaseMana(')==1  # Checked cost/refund transactions only.
    assert 'dt::retireLegacyFarmState(state)' in source and 'snap.pending=' not in source
    panel=(ROOT/'native/panel.hpp').read_text(encoding='utf-8')
    for obsolete in ['stock', 'cargo', 'courier', 'Hire courier', 'Cargo menu']:
        assert obsolete not in panel,obsolete
    assert 'passengerLines' in panel and 'Unload passengers' in panel
    assert 'c>=0&&buildable(v,c)?Action{1,BuildTypes[c]}' in panel and 'v.mana>=BuildCosts[c]&&!v.buildLimit[c]' in panel
    callbacks=source.split('static HRESULT __stdcall frame(',1)[1].split('static DWORD WINAPI start',1)[0]
    for forbidden in ['luaConstant(', 'DarkTribeFlyTo(', 'SendWarriors(', 'AcquireSRWLockExclusive(&stateLock)', 'AcquireSRWLockShared(&stateLock)']:
        assert not re.search(r'(?<!Try)'+re.escape(forbidden),callbacks),forbidden
    transport=source.split('static void requestBoarding(',1)[1].split('static const char* passengerName',1)[0]
    assert 'for(auto unit:selectedUnits)' in transport and 'for(uint32_t id=1;id<65536' not in transport
    assert 'eligiblePassenger(o.unit)' in transport and 'dt::sameEntity(o.craft' in transport
    assert 'ground+2*(y*576+x)' in source
    report=dict(version='0.2.15',game_sha256=game_build['sha256'],game_code_sha256=ORIGINAL_SHA256,game_started=False,live_process_read=False,
        native_direct_income_rva='0x389C0',old_hook_is_ai_catchup=True,
        direct_income_bypasses_old_hook=True,native_farm_income_untouched=True,
        custom_couriers_removed=True,obsolete_inventory_ui_removed=True,legacy_farm_state_retired=True,foundation_delete_mode=1,
        mode0_is_cleanup_without_entity_removal=True,mode1_clears_masks_and_schedules_deletion=True,
        pending_deletion_not_rediscovered=True,refunded_retained_foundation_not_recharged=True,
        legacy_paid_foundation_recovered_without_charge=True,
        old_delete_mode_creates_destruction_objects=True,all_four_costs_checked=True,
        boarding_selected_units_only=True,boarding_and_passenger_identity_checks=True,
        callbacks_use_nonblocking_cached_state=True,sidecar_schema=2,
        one_dark_temple_one_dark_fortress=True,limited_kinds=[50,51],
        removal_marks_uid_and_dying_flag='RVA 0x16D030 (UID 0x20000000, flags 0x4000000) before deletion queue 0x16DB20',
        lua_crush_building='RVA 0x1C4CF0 -> mode 1 0x136960 -> 0x16D030',
        second_site_refused_before_payment='native mode 1, refused record kept until the site identity disappears',
        building_limit_gameplay_verified=False,
        gameplay_verified=False)
    (ROOT/'research/interaction-native-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: native farm income untouched, obsolete courier/UI removal, pinned catch-up path, native foundation cancellation/lifecycle, one Temple and one Fortress (removal marking, crush path, refusal before payment), affordability, selected boarding and cached callbacks')

if __name__=='__main__':run()
