"""File-only checks for the computer offence against the Dark Tribe (rounds r27..r32; no game launched)."""
import json,struct,sys
from pathlib import Path
from game_build import verify_executable
from project_paths import game_exe
from offence_pins import PINS,DATA_PINS,generate
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))
import capstone,pefile

# (rva, 'mnemonic operands') of the instruction at rva; operands print virtual
# addresses of the pinned image base 0x400000.
WHY=[ # The native AI can never target a Dark building.
    (0x4281d,'mov dword ptr [esi + 0x1c], 0x2e'),(0x42824,'mov dword ptr [esi + 0x20], 0x30'),  # enemy-building iterator: towers/castle only
    (0x4282b,'mov eax, dword ptr [ebx*4 + 0x12dcd54]'),(0x42832,'lea esi, [edi + 0xd70]'),
    (0x42862,'mov dword ptr [esi + 0x1c], 0x2e'),(0x42869,'mov dword ptr [esi + 0x20], 0x30'),
    (0x2f411,'cmp dword ptr [edx + 0x2c], 0xf'),(0x2f41e,'mov eax, dword ptr [esi + 0x5ac]'),(0x2f429,'jle 0x42f510'),  # empty offence cache: "No attack"
    (0x135cfa,'mov al, byte ptr [eax + 0x1dd]'),(0x135d00,'cmp al, 0xa'),(0x135d04,'cmp al, 0xf'),(0x135d08,'or dword ptr [edi + 0x14], 0xc'),  # group 0xC: military kinds only
    (0x19809a,'and al, 0xf'),(0x19809c,'cmp al, 3'),  # axe warrior/saboteur building search skips race 3
    (0x31740,'xor eax, eax'),(0x31742,'ret 8'),  # normal AI gardener agent is a stub
    (0x1458f4,'mov esi, dword ptr [eax + 0x14ef7ac]'),(0x145a15,'and cl, 5'),(0x145a81,'test esi, eax'),  # fire defence: ready farms, Dark cells, enemies
]
USED=[ # Everything offence-native.hpp calls or reads.
    (0x45023,'mov edx, dword ptr [ebp + 8]'),(0x45029,'mov ecx, dword ptr [esi + 4]'),(0x4502c,'call 0x445470'),
    (0x45046,'mov dword ptr [edx + 0x24], ecx'),(0x45049,'mov dword ptr [edx + 0x28], esi'),(0x45057,'mov dword ptr [esi + edi*4 + 0x68], edx'),
    (0x45066,'inc dword ptr [esi + eax*4 + 0x30]'),(0x4506e,'ret 4'),
    (0x45474,'cmp dword ptr [0x129339c], 0'),(0x45489,'cmp esi, 8'),(0x454ad,'mov eax, dword ptr [edi*4 + 0xff07a8]'),
    (0x46895,'and edx, 0xf3ff000f'),(0x468ba,'call dword ptr [eax + 0x2c]'),(0x468c7,'mov word ptr [esi + 0x35], 0'),(0x468e3,'jmp dword ptr [ebx*4 + 0x4469c8]'),
    (0x468ec,'mov al, 1'),(0x468f0,'ret 0xc'),(0x46346,'mov dword ptr [ecx + 0x50], eax'),(0x46361,'ret 4'),
    (0x4677e,'call 0x444ab0'),(0x46783,'cmp eax, 3'),(0x467ac,'ret 8'),
    (0x398b6,'mov dword ptr [esi], 0x102a980'),(0x398c1,'test byte ptr [ebp + 8], 1'),(0x398d6,'ret 4'),
    (0x3fa62,'movzx ecx, word ptr [eax]'),(0x3fa65,'mov edi, dword ptr [eax + 0x10]'),(0x3fa72,'and esi, 0xf'),(0x3fa77,'mov esi, dword ptr [ebx + esi*4 + 0x1c]'),
    (0x3fa81,'call 0x435370'),(0x3fa91,'call dword ptr [edx + 0x14]'),(0x3fa9e,'ret 4'),(0x35386,'mov esi, dword ptr [edi + 0x10]'),
    (0x33c1f,'call dword ptr [eax + 0x20]'),(0x33c29,'call 0x43fa50'),(0x33c3a,'call dword ptr [eax + 8]'),
    (0x136aab,'mov edi, ecx'),(0x136aaf,'cmp word ptr [edi + 0xc], 0x33'),(0x136abf,'cmp dword ptr [eax + 0x2d8], 0xb'),(0x136ad2,'cmp eax, 0xf'),(0x136ad7,'cmp eax, 4'),
    (0x136c4f,'cmp al, 3'),(0x136cb8,'mov byte ptr [edi + 0x25], al'),(0x136cc6,'call 0x56d030'),(0x136cd0,'call dword ptr [eax + 0x2c]'),(0x136cf2,'ret 4'),
    (0x2fb1a,'imul edx, ecx, 0x17c'),(0x2fb20,'mov eax, dword ptr [edx + 0x1292648]'),(0x2fb26,'mov ebx, dword ptr [eax]'),(0x2fb30,'mov ecx, dword ptr [0x1266b14]'),
    (0x2fb3f,'imul eax, dword ptr [eax + 4], 0x348'),(0x2fb82,'mov ecx, dword ptr [eax + 8]'),
    (0x2fbf4,'cmp eax, 0x28f5'),(0x2fbfb,'add eax, 0x51e'),(0x2fc12,'and eax, 0x100000'),(0x2fc17,'add eax, 0x200000'),
    (0x33b2f,'mov eax, dword ptr [0x117d118]'),(0x33b67,'add eax, dword ptr [0x117d10c]'),
    (0x342ba,'mov dword ptr [ebx + 0xb1c], ecx'),(0x342c6,'mov dword ptr [ebx + 0xb20], eax'),(0x42a80,'mov dword ptr [edi + 0x100], eax'),
    (0x2f4e0,'imul eax, ecx, 0x58'),(0x2f4e6,'cmp ecx, dword ptr [eax + 0x1266e60]'),
    (0x38563,'mov dword ptr [esi + edi*4 + 0xc], eax'),(0x3856a,'bts eax, edi'),(0x384e7,'cmp dword ptr [esi + 8], 0'),
    (0x39b69,'mov dword ptr [esi + 0xc], edx'),(0x399f0,'cmp dword ptr [eax*4 + 0x149b5d4], 3'),
    (0x394a0,'mov eax, dword ptr [eax*4 + 0x14eea74]'),(0x394d1,'movzx eax, word ptr [eax + 0x2a]'),(0x36702,'imul eax, eax, 0x43'),(0x36708,'mov eax, dword ptr [eax*4 + 0x1544cec]'),
    (0x145a38,'mov eax, dword ptr [0x12f1a18]'),(0x145a44,'movzx eax, word ptr [eax*4 + 0x12f1a24]'),
    (0x145cf6,'lea ecx, [edx + 0x1a4]'),(0x145d10,'call 0x44b6e0'),
    # AddMember core: old owner detach (its slot 6), rec+8 = new owner, tail link, result 3.
    (0x44ac6,'mov ecx, dword ptr [esi + 8]'),(0x44b01,'call dword ptr [eax + 0x18]'),(0x44b04,'mov dword ptr [esi + 8], edi'),(0x44b69,'ret 8'),
    (0x44a49,'dec dword ptr [ecx + eax*4 + 0x30]'),(0x44a57,'jmp 0x445710'),
    # Gardeners: job word, move order -> work centre (no WATCH bit), occupied cells skipped, WORK completion
    # greens with the gardener's owner, greening runs the ring walk then 0x13CE10(x, y, player) on Dark buildings.
    (0x180fcc,'mov word ptr [edx + 0x44], ax'),(0x179af6,'cmp dword ptr [esi + 8], 0xd'),(0x179b90,'test byte ptr [esi + 0xc], 4'),
    (0x179ba0,'mov dword ptr [ebx + 0x38], eax'),(0x179ba8,'mov dword ptr [ebx + 0x3c], ecx'),(0x179ed4,'mov eax, dword ptr [0x15630dc]'),
    (0x1795eb,'movzx eax, byte ptr [esi + 0x20]'),(0x17960a,'call dword ptr [eax + 0x10]'),
    (0xcbbf3,'mov eax, dword ptr [eax + 0x1c]'),(0xcbc10,'mov ecx, 0x14eea58'),(0xcbc15,'call 0x53ce10'),(0x13d00e,'cmp cl, 3'),(0x13d03b,'call 0x536a80'),
    # Recruitment: net 0x13AF -> owner test 0xF61E0 -> order 0xD5100 (signed byte eco+0x288+index, capped at 99);
    # the 16-tick eco pass needs spare carriers (+0x26 - +0x22 > +0x20) and an idle one (+0xAC), marks the carrier
    # role +0x2D with the target type; 0xD4280 sums the eco's pile list for a good (+0x130 + good*2).
    (0xf6980,'call 0x4f61e0'),(0xf6996,'call 0x4d5100'),(0xf61ef,'movzx ecx, byte ptr [eax + 0x12]'),(0xd513a,'movsx ecx, byte ptr [edi + 0x288]'),
    (0xd5159,'cmp edx, 0x64'),(0xd51bf,'ret 8'),(0xd52c0,'movzx eax, word ptr [esi + 0x22]'),(0xd52c4,'movzx ecx, word ptr [esi + 0x26]'),
    (0xd52ca,'movzx eax, word ptr [esi + 0x20]'),(0xd52d6,'movzx ecx, word ptr [esi + 0xac]'),(0xd54d3,'call dword ptr [eax + 0x74]'),(0xd54dc,'inc word ptr [esi + 0x22]'),
    (0xd428a,'movzx esi, word ptr [ecx + eax*2 + 0x130]'),(0xd42a9,'call dword ptr [edx + 0x28]'),(0xd42b5,'movzx esi, word ptr [ecx + 0x2a]'),(0xd42c2,'ret 4'),
    (0x170bad,'cmp byte ptr [esi + 0xa], 0x10'),(0x170bc4,'movzx edi, byte ptr [esi + 0x42]'),(0x170bc8,'call dword ptr [eax + 0x28]'),
    # Gardener state: role pointer +0x58, task byte role+4, greening job 0xAF, give-up after 0x38 slices -> default job,
    # and a failed WORK start that jumps to the next task without clearing the reserved cell (hence: classify by job).
    (0x180fd2,'mov ecx, dword ptr [edx + 0x58]'),(0x186dda,'mov byte ptr [ebx + 4], al'),(0x17a13c,'push 0xaf'),(0x179991,'cmp dword ptr [edi + 0x48], 0x38'),
    (0x1799b7,'call dword ptr [edi + 0x70]'),(0x17974e,'je 0x5798ae'),(0x1798ae,'mov eax, dword ptr [edi]'),
    # Orders: net event 0x1396 -> CGroupMgr slot 10 (count, ids, x, y, mode) with no owner test; the
    # unit filter is (flags & 0x2008000) == 0x2000000; the send gate needs TribeTable[event player]+0x34.
    (0xf6608,'mov ecx, 0x153fd30'),(0xf661b,'call dword ptr [eax + 0x28]'),(0x16a862,'and eax, 0x2008000'),(0x16a867,'cmp eax, 0x2000000'),
    (0x1a7b0e,'mov al, byte ptr [edx + 0x1e]'),(0x1a7b1e,'mov al, byte ptr [eax*4 + 0x149b608]'),(0x1a7b25,'test al, al'),
    # r28 territory. Owner of a cell (0x58A40, also S4ModApi LandscapeGetOwner): tile word per cell at
    # [0xEF1A18], owner = dword [0xEF1A20 + 20*tile] & 0xF; land region word at pool +4.
    (0x58a43,'mov ecx, dword ptr [0x1454138]'),(0x58a4d,'mov eax, dword ptr [0x12f1a18]'),(0x58a55,'movzx eax, word ptr [eax + ecx*2]'),
    (0x58a59,'lea eax, [eax + eax*4]'),(0x58a5c,'mov eax, dword ptr [eax*4 + 0x12f1a20]'),(0x58a63,'and eax, 0xf'),
    # Non-Dark placement (BuildBuilding 0x13E4E0): anchor flags & 0x5F (0x04 Dark included), anchor owner == player, footprint mask 0x5F.
    (0x13e623,'test byte ptr [eax + ebx], 0x5f'),(0x13e63d,'mov eax, dword ptr [eax*4 + 0x12f1a20]'),(0x13e644,'and eax, 0xf'),(0x13e647,'cmp eax, ebx'),(0x13e65d,'push 0x5f'),
    # Territory claim 0x20CD30: influenced cells get 0x80 (military influence, the whole territory) and SetOwner (tiling slot 7); no influence: clear 0x80 only.
    (0x20d1ac,'or byte ptr [ecx + eax], 0x80'),(0x20d205,'mov eax, dword ptr [0x11ce570]'),(0x20d20b,'call dword ptr [eax + 0x1c]'),(0x20d210,'and byte ptr [ecx + eax], 0x7f'),
    # Gardener search compares land regions only (no owner); SetDark toggles 0x04 only; WORK end clears the 0x20 reservation.
    (0x179fd3,'mov eax, dword ptr [eax*4 + 0x12f1a24]'),(0x17a042,'movzx eax, word ptr [eax*4 + 0x12f1a24]'),(0xcbba0,'or cl, 4'),(0xcbba5,'and cl, 0xfb'),(0x1795d9,'and byte ptr [eax + ecx], 0xdf'),
    # The human's Dark Gardeners darken land of any owner (mask 0x3F, no owner read).
    (0x175199,'test byte ptr [eax + esi], 0x3f'),
]
FIRE=[ # r30: the citadel fire (CDarkTempleRole, Temple 50 and Fortress 51) the computers keep away from.
    # 0x1458C0: owner p = byte +0x20 >> 4, centre = position +0x18 plus (1, 2), n = ready farms [0x10EF7AC + p*0x298];
    # no fire for n <= 0, radius 2n+16 for n = 1..3 and 24 (0x18) from 4 farms.
    (0x1458d4,'movzx eax, byte ptr [ebx + 0x20]'),(0x1458d8,'mov edi, dword ptr [ebx + 0x18]'),(0x1458db,'shr eax, 4'),(0x1458e1,'imul eax, eax, 0x298'),
    (0x1458e7,'movzx edx, di'),(0x1458ea,'sar edi, 0x10'),(0x1458ed,'inc edx'),(0x1458ee,'add edi, 2'),
    (0x1458fa,'test esi, esi'),(0x1458fc,'jle 0x545b8e'),(0x145941,'cmp esi, 4'),(0x145946,'mov dword ptr [ebp - 0xc], 0x18'),(0x14594f,'lea eax, [esi*2 + 0x10]'),
    # Victims: a walkable Dark cell (flags & 5 == 4) with a ground entity, alive, an enemy of the owner, class 1 or 4, group nibble set.
    (0x145a09,'mov ecx, dword ptr [0x15630d8]'),(0x145a18,'cmp cl, 4'),(0x145a24,'mov eax, dword ptr [0x15630dc]'),(0x145a64,'test dword ptr [edi + 4], 0x20000000'),
    (0x145a89,'mov al, byte ptr [edi + 0xa]'),(0x145a8c,'cmp al, 1'),(0x145a90,'cmp al, 4'),(0x145a98,'test byte ptr [edi + 0x14], 0xf'),
    # The hit: effect 0x4E at the cell, then future event 2 with 255 damage after (rand & 15) + 6 ticks.
    (0x145b35,'push 0x4e'),(0x145b41,'push 0xff'),(0x145b4c,'lea eax, [esi + 6]'),(0x145b50,'push 2'),(0x145b54,'call dword ptr [edx + 0xc]'),
    # CDarkTempleRole tick: one pass per call, rescheduled every 31 ticks.
    (0x145686,'call 0x5458c0'),(0x145696,'push 0x1f'),(0x145698,'call 0x56d5d0'),
    # Gardener search 0x179F80: the chosen cell goes to role +0x40/+0x44 before the mode test; mode 1 pushes job 0xAF, mode 0 job 0xAE.
    (0x17a116,'mov dword ptr [ebx + 0x40], esi'),(0x17a119,'mov dword ptr [ebx + 0x44], ecx'),(0x17a12d,'je 0x57a175'),(0x17a17e,'push 0xae'),
]
TOOLS=[ # r29: shovels for the gardeners at the computer's own toolsmith.
    # 0xD6A90(eco, good, amount), thiscall ret 8: find the good in the tool list eco+0x354..+0x358, the order byte at the
    # same index of eco+0x348; amount 0 clears, 100 sets continuous, otherwise add and clamp at 100 (and 0).
    (0xd6a94,'mov esi, dword ptr [ebp + 8]'),(0xd6a9b,'mov eax, dword ptr [ecx + 0x358]'),(0xd6aa3,'sub eax, dword ptr [ecx + 0x354]'),
    (0xd6aae,'mov edi, dword ptr [ecx + 0x354]'),(0xd6ab4,'movzx eax, byte ptr [edi + edx]'),(0xd6ab8,'cmp eax, esi'),
    (0xd6ada,'mov esi, dword ptr [ecx + 0x348]'),(0xd6ae0,'mov cl, byte ptr [esi + edx]'),(0xd6ae5,'mov edx, dword ptr [ebp + 0xc]'),
    (0xd6aea,'je 0x4d6b15'),(0xd6aec,'cmp edx, 0x64'),(0xd6af8,'add eax, edx'),(0xd6afa,'cmp eax, 0x64'),(0xd6aff,'mov cl, 0x64'),(0xd6b01,'mov byte ptr [esi], cl'),
    (0xd6b05,'ret 8'),(0xd6b0c,'add cl, dl'),(0xd6b0e,'mov byte ptr [esi], cl'),(0xd6b12,'ret 8'),(0xd6b15,'xor cl, cl'),(0xd6b1b,'ret 8'),
    # Net event 0x13A9 (CNetInputProcessor::Process 0xF6240): GetEntityWithOwner, ready, role kind 4, economy 0xF6200,
    # building type 0x15 (toolsmith) -> 0xD6A90(eco, good = low word, amount); 0x16 (weaponsmith) -> 0xD6960.
    (0xf687d,'mov ecx, dword ptr [edi + 8]'),(0xf6882,'mov edi, dword ptr [edi + 0xc]'),(0xf6885,'movzx ebx, cx'),(0xf688b,'call 0x4f6120'),
    (0xf689a,'test dword ptr [esi + 0x14], 0x2000000'),(0xf68ac,'call dword ptr [edx + 0x70]'),(0xf68af,'cmp eax, 4'),(0xf68ba,'call 0x4f6200'),
    (0xf68c7,'movzx ecx, word ptr [esi + 0xc]'),(0xf68cb,'cmp ecx, 0x15'),(0xf68d0,'push edi'),(0xf68d1,'push ebx'),(0xf68d4,'call 0x4d6a90'),(0xf68de,'cmp ecx, 0x16'),
    (0xf612b,'cmp byte ptr [eax + 0xa], 8'),(0xf6131,'test dword ptr [eax + 0x14], 0x4000000'),(0xf613e,'shr ecx, 4'),
    (0xf6204,'mov eax, dword ptr [ecx + 0x48]'),(0xf6209,'sar ecx, 0x10'),(0xf6218,'mov eax, dword ptr [0x15630c4]'),(0xf621d,'movzx eax, word ptr [eax + ecx*2]'),
    (0xf6224,'movzx eax, word ptr [eax*4 + 0x12f1a26]'),(0xf6230,'mov eax, dword ptr [eax*4 + 0x146c8a4]'),
    # SendProduceOrder (CEcoManager slot 0x10C = 0x50A70) posts 0x13A9 with the amount unchanged (lparam ebx = arg 3).
    # The AI's building manager sub_249400 never orders shovels for gardeners; it sends two shovel orders (0x1F):
    # 0x249835 for its diggers (7-d or 10-2d, only while d < 5 and no shovel is in stock: always positive) and
    # 0x249770, which CAN CLEAR OR LOWER the order: while a task is a built toolsmith (+0x54 0x15, +0x30 3, +0x10 2,
    # +0x14 != 3) and the task holds a toolsmith id [ebx+0x5c], amount = (10-d)-d with d = [ebx+0x54] = vt+0x98(2),
    # the economy's digger count (0x4F480: word eco+0x24+2*type). The AI recruits diggers while d < 10-d, so at its
    # steady state of 5 diggers the amount is 0 (0xD6A90 stores 0) and from 6 it is negative (added, clamped at 0).
    (0x50aa3,'mov ebx, dword ptr [ebp + 0x10]'),(0x50b09,'mov dword ptr [ebp - 0x30], 0x13a9'),(0x50b18,'mov dword ptr [ebp - 0x28], ebx'),
    (0x24982a,'push 0x1f'),(0x249835,'call dword ptr [eax + 0x10c]'),
    (0x24940e,'push 2'),(0x249422,'call dword ptr [eax + 0x98]'),(0x249428,'mov dword ptr [ebx + 0x54], eax'),
    (0x4f490,'movzx eax, word ptr [ecx + eax*2 + 0x24]'),
    (0x249720,'cmp dword ptr [eax + 0x14], 3'),(0x249726,'cmp dword ptr [eax + 0x10], 2'),(0x24972c,'cmp dword ptr [eax + 0x54], 0x15'),(0x249732,'cmp dword ptr [eax + 0x30], 3'),
    (0x249744,'mov esi, 0xa'),(0x249751,'sub esi, dword ptr [ebx + 0x54]'),(0x249758,'cmp dword ptr [ebx + 0x5c], eax'),
    (0x249763,'mov eax, esi'),(0x249765,'sub eax, dword ptr [ebx + 0x54]'),(0x249768,'push eax'),(0x249769,'push 0x1f'),(0x24976d,'push dword ptr [ebx + 0x5c]'),(0x249770,'call dword ptr [edx + 0x10c]'),
    (0x2497a8,'cmp dword ptr [ebx + 0x54], esi'),(0x2497ab,'jge 0x64983b'),
    # 0xD6A90 with a negative amount: add, a negative sum clears (js 0xD6B15), otherwise the lowered byte is stored.
    (0xd6af6,'jle 0x4d6b08'),(0xd6b08,'add eax, edx'),(0xd6b0a,'js 0x4d6b15'),
    # Tick order: the S4ModApi tick hook site 0xE9B18 comes before the net loop's event fetch (0xEF440) and dispatch
    # (NetInputProcessor, 0xE9B5E), so a cleared order the plugin puts back on its tick is in place for the entity update.
    (0xe9b36,'call 0x4ef440'),(0xe9b45,'lea eax, [ecx - 0x1389]'),(0xe9b5e,'call dword ptr [eax]'),
    # Consumer 0x14CE20 (CProductionBuildingRole slot 3 0x14CC20): economy of the building, then for type 0x15 a round robin
    # over the building's tool list; a tool is made only while its order byte is > 0, decremented unless it is 100.
    (0x14ce4e,'mov eax, dword ptr [0x15630c4]'),(0x14ce5a,'movzx eax, word ptr [eax*4 + 0x12f1a26]'),(0x14ce62,'mov ebx, dword ptr [eax*4 + 0x146c8a4]'),
    (0x14d008,'cmp ecx, 0x15'),(0x14d026,'mov eax, dword ptr [ebx + 0x348]'),(0x14d02c,'cmp byte ptr [eax + edx], 0'),(0x14d030,'ja 0x54d042'),
    (0x14d03b,'xor al, al'),(0x14d05d,'cmp al, 0x64'),(0x14d065,'dec al'),(0x14d067,'mov byte ptr [ecx], al'),
    # The economy save writes both byte vectors (order counts eco+0x348..+0x34C, tool list eco+0x354..+0x358).
    (0xd3f02,'mov bl, byte ptr [esi + 0x34c]'),(0xd3f0b,'sub bl, byte ptr [esi + 0x348]'),(0xd3f30,'mov edx, dword ptr [esi + 0x348]'),
    (0xd3f45,'mov bl, byte ptr [esi + 0x358]'),(0xd3f4e,'sub bl, byte ptr [esi + 0x354]'),(0xd3f70,'mov edx, dword ptr [esi + 0x354]'),
    # Per-player building list indexed by building type (CEcoManager 0x4F4A0 counts its economy's buildings of one type the
    # same way: [0x10EEA74 + (player*0x53 + type)*4], next word +0x2A, economy via the 0x11630C4 tile map).
    (0x4f4b0,'imul eax, eax, 0x53'),(0x4f4b5,'add eax, dword ptr [ebp + 8]'),(0x4f4b8,'mov edi, dword ptr [eax*4 + 0x14eea74]'),
    (0x4f4c9,'mov eax, dword ptr [0x15630c4]'),(0x4f4e8,'movzx edi, word ptr [ecx + 0x2a]'),(0x4f505,'movzx ecx, word ptr [ecx*4 + 0x12f1a26]'),
    # Role kind 4 = CProductionBuildingRole (slot 28 returns 4).
    (0x13ada0,'mov eax, 4'),(0x13ada5,'ret'),
]
SPEED=[ # r32: the settler speed byte. The SettlerValues.xml loader stores atoi(<speed>) (CRT 0x999BBA, base 10) at record +1
    # (0x1146930 + 36*(race*0x46 + type)); every <speed> element of the shipped file is empty, so soldiers and gardeners share
    # one walking speed (the convoy does not rely on it: the soaks also run gardeners slower).
    (0x183cdc,'call 0xd99bba'),(0x183ce1,'mov byte ptr [esi*4 + 0x1546931], al'),(0x999bc2,'push 0xa'),
]
def run():
    executable=game_exe();pin=verify_executable(executable)
    pe=pefile.PE(str(executable));base=pe.OPTIONAL_HEADER.ImageBase
    cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
    def at(rva):
        i=next(cs.disasm(pe.get_data(rva,16),base+rva),None)
        return (i.mnemonic+' '+i.op_str).strip() if i else ''
    for rva,text in WHY+USED+TOOLS+FIRE+SPEED:assert at(rva)==text,(hex(rva),at(rva),text)
    import re
    values=(executable.parent/'GameData/SettlerValues.xml').read_text(encoding='latin-1')
    blocks={}  # per settler entry: its <speed> texts (SETTLER_BOWMAN_02 has no <speed> in any race; its byte then comes from the loader's fallback node, not verified)
    for block in re.split(r'<settler id="',values)[1:]:blocks.setdefault(block.split('"')[0],[]).extend(re.findall(r'<speed>([^<]*)</speed>',block.split('</settler>')[0]))
    assert len(blocks)>=20 and all(not v.strip() for texts in blocks.values() for v in texts) and blocks.get('SETTLER_GARDENER')==['']*len(blocks['SETTLER_GARDENER']) and blocks['SETTLER_GARDENER'] and blocks.get('SETTLER_SWORDSMAN_01'),sorted(blocks)[:5]
    def listing(a,b):return [(i.address-base,(i.mnemonic+' '+i.op_str).strip()) for i in cs.disasm(pe.get_data(a,b-a),base+a)]
    # Territory: the claim loop and SetOwner never test the Dark bit; the gardener cell test and search never read the owner;
    # greening never touches the tiling or owners. Fixed pool tiles 0..8 carry owner p (unowned cells read 0).
    for a,b in [(0x20d150,0x20d21f),(0x1b79a0,0x1b7bab)]:assert not [x for x in listing(a,b) if x[1].endswith(', 4') and x[1].startswith('test byte')],hex(a)
    assert not [x for x in listing(0x20d150,0x20d21f) if x[1].endswith((', 0x5f',', 0x84',', 0x85'))]
    for a,b in [(0x179e70,0x179f71),(0x179f80,0x17a1b7)]:assert not [x for x in listing(a,b) if '0x12f1a20' in x[1]],hex(a)
    assert not [x for x in listing(0xcba70,0xcbcb0) if any(k in x[1] for k in ('0x11ce570','0x12f1a20','0x12f1a18'))]
    init=[x[1] for x in listing(0x1b1ea0,0x1b2000)]
    for p,v in enumerate([0x10,0x21,0x42,0x83,0x104,0x205,0x406,0x807,0x1008]):assert 'mov dword ptr [0x%x], 0x%x'%(0x12f1a20+20*p,v) in init,p
    def dword(rva):return struct.unpack('<I',pe.get_data(rva,4))[0]
    # Squad vtable slots used: 2 deleting dtor, 5 AddMember, 8 Command, 11 waypoint reset.
    squad=0xc2a91c
    assert [dword(squad+4*i)-base for i in (2,5,8,11)]==[0x398b0,0x46770,0x46880,0x46340]
    # Command jump table: case 0 returns at once (no waypoint, no member move).
    assert dword(0x469c8)==base+0x468ea
    assert dword(0xc5a308+10*4)==base+0x16a800  # CGroupMgr slot 10: the group move order
    assert dword(0xc5b51c)==base+0x178f30  # CGardenerRole vtable
    assert [dword(0xd64214+4*i) for i in range(10)]==[0x30,0,0x31,1,0x2f,2,0x2e,3,0x32,4]  # specialist order index: gardener 4 -> eco+0x28C
    # r29: CProductionBuildingRole slot 28 (kind 4, the 0x13A9 handler's test) and slot 3 (the consumer's caller);
    # CEcoManager slot 0x10C is SendProduceOrder; the economy merges copy the order bytes through 0xD6A90 only.
    assert dword(0xc59094+0x70)==base+0x13ada0 and dword(0xc59094+0xc)==base+0x14cc20 and dword(0xc2bc7c+0x10c)==base+0x50a70
    assert [x for x in listing(0x14cc20,0x14ccc0) if x[1]=='call 0x54ce20']
    # r30 citadel fire. CDarkTempleRole slot 3 (tick) is 0x1455E0 and the Manacopter Hall's tick (CManakopterHallRole
    # slot 3) 0x147A20 has no fire: it calls only the frame helper 0x61FE0, 0x169CA0 and the scheduler 0x16D5D0.
    assert dword(0xc59284+0xc)==base+0x1455e0 and dword(0xc593d0+0xc)==base+0x147a20
    hall=listing(0x147a20,0x147c35)
    assert {x[1] for x in hall if x[1].startswith('call ')}=={'call 0x461fe0','call 0x569ca0','call 0x56d5d0'} and 'push 0x4e' not in [x[1] for x in hall]
    # The spiral 0xD69968 (dx, dy, distance, direction) and its count words 0xD7CF3A + R*4 are static .data: the fire's
    # cells for R = 24 are exactly the 2,083 cells with dx*dx - dx*dy + dy*dy <= 576 (Euclidean, up to 27 hex steps), R = 18
    # exactly its integer disc; R = 20/22 lie inside theirs. citadel-fire.hpp's fire2 is this metric.
    def word(rva):return struct.unpack('<H',pe.get_data(rva,2))[0]
    counts={r:word(0xd7cf3a+r*4) for r in (16,18,20,22,24)}
    assert [counts[r] for r in (18,20,22,24)]==[1164,1422,1746,2082] and counts[16]==930,counts
    entries=[struct.unpack('<bbBB',pe.get_data(0xd69968+4*i,4)) for i in range(2084)]
    def e2(dx,dy):return dx*dx-dx*dy+dy*dy
    def hexd(dx,dy):return max(abs(dx),abs(dy)) if (dx>=0)==(dy>=0) else abs(dx)+abs(dy)
    for r in (18,20,22,24):
        cells={(dx,dy) for dx,dy,_,_ in entries[:counts[r]+1]};disc={(dx,dy) for dy in range(-40,41) for dx in range(-40,41) if e2(dx,dy)<=r*r}
        assert cells<=disc and (r not in (18,24) or cells==disc),r
    assert len(disc)==2083 and max(hexd(dx,dy) for dx,dy in disc)==27
    # Whole-section scans (skipdata, or the sweep stops at the first data): the ready-building counts 0x10EF624 + idx*8 are written
    # only on completion (0x136340), destruction (0x13C769), owner change (0x13F08E old owner, 0x13F128 new owner) and creation as
    # complete (0x13C4F8, relative to 0x10EEA58); the only 255-damage event-2 post is the fire's; job 0xAE/0xAF are pushed in the
    # gardener role only by the search (after the reservation write).
    text=next(x for x in pe.sections if x.Name.rstrip(b'\0')==b'.text');sweep=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);sweep.skipdata=True
    every=[(a-base,m,o) for a,_,m,o in sweep.disasm_lite(text.get_data(),base+text.VirtualAddress)]
    writers={a for a,m,o in every if m in ('inc','dec','add','sub','mov','or','and') and o.startswith('dword ptr [') and o.split(',')[0].endswith(('+ 0x14ef624]','+ 0x14ef7ac]'))}
    assert writers=={0x136340,0x13c769,0x13f08e,0x13f128},sorted(hex(a) for a in writers)
    assert at(0x13c4f8)=='inc dword ptr [ebx + eax*8 + 0xbcc]' and 0x10eea58+0xbcc==0x10ef624
    posts=[]
    for k,(a,m,o) in enumerate(every):
        if m=='call' and o.endswith('+ 0xc]'):
            prev=[x for x in every[max(0,k-10):k] if x[1]=='push']
            if ('push','2') in [(x[1],x[2]) for x in prev] and ('push','0xff') in [(x[1],x[2]) for x in prev]:posts.append(a)
    assert posts==[0x145b54],[hex(a) for a in posts]
    jobs=[(x[0],x[1]) for x in listing(0x178f30,0x17a1b7) if x[1] in ('push 0xae','push 0xaf')]
    assert jobs==[(0x17a13c,'push 0xaf'),(0x17a17e,'push 0xae')],jobs
    # Relocation-aware pins, whole instructions, three load bases.
    relocations={r.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for r in block.entries if r.type==3}
    generate(executable,ROOT/'work/native-generated/offence-signatures.hpp')
    for address in [base,0x360000,0x60000000]:
        rebased=pefile.PE(str(executable)).get_memory_mapped_image(ImageBase=address)
        for rva,length in PINS:
            raw=pe.get_data(rva,length)
            if rva not in DATA_PINS:assert sum(i.size for i in cs.disasm(raw,base+rva))==length,('partial instruction pin',hex(rva))
            expected=bytearray(raw)
            for relocation in relocations:
                if rva<=relocation<rva+length:
                    offset=relocation-rva;assert offset+4<=length,('truncated relocation',hex(rva))
                    struct.pack_into('<I',expected,offset,struct.unpack_from('<I',raw,offset)[0]-base+address)
            assert rebased[rva:rva+length]==expected,(hex(address),hex(rva))
    # Source contracts: the bridge never calls the API, Lua or locks; orders and
    # logging live on the tick in dark-tribe.cpp; the model has no engine reads.
    source=(ROOT/'native/dark-tribe.cpp').read_text(encoding='utf-8')
    bridge=(ROOT/'native/offence-native.hpp').read_text(encoding='utf-8')
    model=(ROOT/'native/offence-model.hpp').read_text(encoding='utf-8')
    for forbidden in ['lua','AcquireSRWLock','SendWarriors','api->','MagicIncreaseMana','SettlersAdd','0xe3a80']:assert forbidden not in bridge,forbidden
    # Destruction is the gardeners' greening; the plugin's own destroy is only the all-green fallback.
    assert 'if(!dark){' in model and 'ops.destroy(c,c.target)' in model and 'GardenerType' in bridge and 'image+0xd5100' in bridge
    assert 'g.work=job==0xaf?Work::Working' in bridge and 'kept(keep,px,py)' in bridge and 'ops.sites(c,sites)' in model and 'role+0x2d' not in bridge
    # r28: styles chosen with the plugin's generator, short delays, own-territory greening, allied calls, recruitment reasons.
    assert 'chooseStyle(c,o,n)' in model and 'firstAttack(cfg.delay)' in model and 'ops.ownDark(' in model and 'ops.announce(' in model and 'c.random.next()' in model
    assert 'this->owner(px,py)!=owner' in bridge and 'read(image+0xef1a20+tile*20u,head)' in bridge and 'image+0xef1a20+tile*20u)&15' in bridge and 'owner(px,py)!=p' in bridge
    assert 'return !(f&0x09)&&!ground(x,y);' in bridge and 'nativeOffence.scanTerritory(16);' in source and '(f&0x25)!=0x04' in bridge
    assert 'OFFENCE_RECRUIT_WAIT' in source and 'offenceCall.caller==c.player' in source and 'dtoffence::styleName(c.style)' in source
    assert 'DriftLimit' in model and 'if(!secure&&!parkedNow)return;' in model and 'allow=parkedNow?((c.style==Style::Allied||!ops.leads(c,c.target))?0:std::max(0,dark-2)):MaxGardeners' in model and 'LostAvoid' in model and 'bool leads(' in source and 'c.topUps<budget' in model and 't-c.parkedSince<HoldMax' in model and 'EscortStall' in model and 'if(locked(b,f))continue;' in model and 'gardener called back' in model and 'off the ring' in source and 's.shovels-=value<BYTE>(e+0x42)' in bridge
    # Gardener group 7 from the static group chain at 0xC1A648 (type 0x1D on); carriers (type 1) have no entry.
    tbl={};typ=0x1d;a=0xc1a64a
    while True:
        a+=4;tbl[typ]=pe.get_data(a-5,1)[0];nxt=pe.get_data(a-2,1)[0]
        if not nxt:break
        typ=nxt
    assert tbl.get(0x32)==7 and 1 not in tbl,tbl.get(0x32)
    for forbidden in ['windows.h','image+','api->','VirtualQuery']:assert forbidden not in model,forbidden
    assert 'nativeOffence.initialize(image)' in source and 'return 13;' in source and 'offenceTick(t)' in source
    assert 'bits&(1u<<p)' in bridge and '0xe66e28+DarkPlayer*0x58+0x38' in bridge
    # Squads are deleted only when empty; soldiers come only from this sector's reservoirs or state-5 squads.
    assert 'if(!read(tf+0xc,left)||left)return false;' in bridge and 'vt==image+0xc2a950&&read(pool+4,kind)&&kind==1' in bridge and 'kind==2&&state==5)take(tf,1)' in bridge
    # Reservoir slots used by AddMember/dissolve; factory state 6 -> squad type 2.
    assert dword(0xc2a950+0x14)==base+0x44ab0 and dword(0xc2a950+0x18)==base+0x44ba0 and dword(0xbf07a8+6*4)==2
    tick=source[source.index('struct OffenceOps {'):source.index('static HRESULT __stdcall frame(')]
    assert 'internalSend=true' in tick and 'SendWarriors(' in tick and 'S4_MOVEMENT_WATCH' not in tick and 'ids.data()+at' in tick
    # Plugin stop hands every mod squad back; dying buildings never count as standing.
    assert source.count('offenceStopLocked();')>=2 and 'nativeOffence.releaseAll()' in source and 'offencePinned=true;' in source
    assert 'standing(view.selected)' in source and '!standing(id)||isSite(id)' in source and '!standing((uint16_t)id)||isSite((uint16_t)id)' in source
    # r29: shovel orders only through 0xD6A90 on the home economy, with a ready toolsmith there, never on 100; the order
    # byte is the only state (no dt::State field); the decision is pure model code; logged on the tick.
    assert 'image+0xd6a90' in bridge and '((void*)eco,31,amount)' in bridge and 'k.toolsmiths<=0||amount<=0||amount>=100' in bridge and 'before<0||before>=100' in bridge
    assert 'value<uintptr_t>(role)==image+0xc59094' in bridge and 'image+0x10eea74+(p*0x53+21)*4' in bridge and 'read(image+0x11630c4,map)' in bridge and 'buildingEconomy(e)==eco' in bridge
    assert 'read(eco+0x354,list)' in bridge and 'read(eco+0x348,counts)' in bridge and 'listEnd-list>64' in bridge and 'i<countsEnd-counts' in bridge
    assert 'inline int shovelOrder(' in model and 'ordered>=ShovelContinuous?"continuous order"' in model and 'toolsmiths<=0?"no toolsmith"' in model and 'free>ShovelReserve?"spare shovels"' in model
    assert 'offenceTools(p,t,supply,wanted);' in source and 'nativeOffence.orderShovels(tools,supply.eco,amount)' in source and 'OFFENCE_TOOLS player=%u' in source and 'toolsmiths=%d shovelOrder=%d ironbars=%d coal=%d' in source
    assert '; toolsmith shovel orders r29; citadel fire keep-off and farm blockade r30; clean start maps r31; scaled styles and protected convoy r32")' in source
    # r30: one fire geometry (citadel-fire.hpp) for the director and the farm blockade; the fire is on from the native
    # ready-farm counter, the listed farms or a Farm site; every keep is Euclidean; the guard runs every director step.
    fire=(ROOT/'native/citadel-fire.hpp').read_text(encoding='utf-8')
    for text in ['constexpr int CentreDX=1,CentreDY=2;','constexpr int FireReach=24;','constexpr int FireLine=27;','constexpr int FarmLock=FireLine+FarmReach+2;','return dx*dx-dx*dy+dy*dy;','inline bool farmTooClose(']:assert text in fire,text
    for text in ['#include "citadel-fire.hpp"','using dtfire::FireLine;','GardenerKeep=FireLine+1','HallLock=FireLine+HallReach+2','FrontKeep=FireLine+SweepRadius','f.on=ready>0||f.listed>0||f.farmSites>0;',
                 'dtfire::farmTooClose(b.x,b.y,z.bx,z.by)','if(locked(b,f))continue;','if(guard(c,t,ops,m,f))return;','inline int soldierKeep(int n){return FireLine+spread(n);}','inline bool inFire(int x,int y,const Fire&f){return clear2(x,y,f)<=FireLine*FireLine;}',
                 '(g.aimX>=0&&inFire(g.aimX,g.aimY,f))','"fire defence back on"','"ring inside the fire defence"','"lured toward the fire defence"','"no safe assault cell"','e.round?dtfire::fire2(x,y,e.x,e.y)<e.r*e.r']:assert text in model,text
    for gone in ['FireRaidFarms','FireMargin','exposure(','zones(','detour(']:assert gone not in model,gone
    # r30 review: one resend gate per soldier; legs cover every shortest hex path (parallelogram) or turn at a corner;
    # marches and ways home plan again after an extraction; no straight way home through the fire; never handed back hot.
    for text in ['if(inFire(s.x,s.y,f)&&!pulled(c,s.uid,t))fresh.push_back(s);','for(auto&s:fresh)pull(c,s.uid,t); /* no way out','inline void corners(int ax,int ay,int bx,int by,','if(p>0&&p<1&&q>0&&q<1)return false;',
                 'template<class Stand> bool hexLeg(','c.replanAt=t+GuardResend;','template<class Ops> void remarch(','template<class Ops> void homeward(','"no way home outside the fire defence, holding"',
                 'if(done&&(!hot||t-c.since>HotHold)){','if(c.homeless){if(t>=c.retryAt)homeward(c,t,ops,m,f,s);return;}','hk=std::max(soldierKeep(std::max(1,a.count)),HuntKeep);']:assert text in model,text
    assert 'send(c,t,ops,m,s.homeX,s.homeY,true)' not in model and 'GuardResend)){c.fireAt' not in model
    assert 'image+0x10ef7ac+DarkPlayer*0x298' in bridge and '(job==0xae||job==0xaf)?rx:-1' in bridge and 'g.reservedX=job==0xaf?rx:-1' in bridge and '!accept(px,py)' in bridge and '(flags(x,y)&0x05)==0x04' in bridge
    assert 'static void offenceFire(OffenceOps&ops,uint32_t t){' in source and 'offenceFire(ops,t);' in source and 'OFFENCE_FIRE_STATE ready=%d listed=%d farmSites=%d on=%u centres=' in source and 'OFFENCE_FIRE_NATIVE player=%u soldiers=%d' in source and 'OFFENCE_FIRE player=%u' in source
    assert 'int readyFarms(){return nativeOffence.readyFarms();}' in source and 's.kind=state.construction[i].kind;' in source and '!dtoffence::inFire(px,py,f)' in source and '"CALL","FIRE","CONVOY"}' in source
    assert 'offenceFireNote[0]=0;' in source and 'random' not in fire
    # r29 review: the AI's clears are watched every tick; the watch is transient and puts back only the plugin's part.
    assert 'struct ShovelWatch {int owed=0,seen=-1;};' in model and 'if(now+1<w.seen)add=std::max(0,std::min(w.owed,w.seen)-now);' in model and 'inline void shovelNoted(' in model and 'inline int shovelTarget(' in model
    assert 'for(uint8_t p=2;p<=8;p++)offenceShovelGuard(p,t);' in source and 'static void offenceShovelGuard(uint8_t p,uint32_t t){' in source and 'nativeOffence.economyLive(p,eco)' in source and 'dtoffence::shovelNoted(watch,' in source
    assert source.index('for(uint8_t p=2;p<=8;p++)offenceShovelGuard(p,t);')<source.index('if(t%dtoffence::Period)return;OffenceOps ops;')
    assert 'offenceShovelWatch' not in (ROOT/'native/economy.hpp').read_text() and 'for(auto&w:offenceShovelWatch)w=dtoffence::ShovelWatch{};' in source
    assert 'back==eco&&read(eco+0x12,owner)&&owner==p' in bridge
    # r32: every style grows with the army (today's sizes the floor), gardeners with the soldiers a style takes, at most
    # gardenersWanted(free soldiers); the army marches as one convoy with its gardeners (one id per order), holds while
    # they lag, leaves stragglers after ConvoyStall, screens the target before they work, takes top-ups only near it,
    # walks to a hunt's gardeners first (rally) and goes home as a convoy; recruitment and shovel orders follow the scaled number.
    for text in ['inline int raidSize(int army){return std::max(0,std::min(army,std::max(RaidMax,army/3)));}','inline int huntSize(int army){return std::max(0,std::min(army,std::max(HuntMax,army/2)));}',
                 'inline int escortSize(int army){return std::max(0,std::min(army,std::max(SweepEscort,army/4)));}','inline int gardenersWanted(int soldiers){return std::min(MaxGardeners,GardenersWanted+std::max(0,soldiers-WantedFrom)/WantedPer);}',
                 'inline int gardenersFor(Style s,int soldiers=0){return std::min(MaxGardeners,gardenerBase(s)+std::max(0,soldiers)/GardenerPer);}','r.take=raidSize(army);','h.take=huntSize(army);','w.take=escortSize(army);','a.want=wantOf(Style::Assault,army);',
                 'template<class Ops> bool convoy(','template<class Ops,class Take> int gather(','ops.sendGardener(c,g,px,py);c.gdx[i]=px;','if(!legClear(g.x,g.y,px,py,f,GardenerKeep))continue;','"waiting for gardeners"','"gardeners left behind"','"rally to the gardeners"',
                 'bool arrived=there*10>=(int)m.size()*7&&together(c,ops,m);','home*10>=(int)m.size()*8&&together(c,ops,m)','held=c.lastOrder&&holders(c,m)>=needed(m)','distance(g.x,g.y,a.x,a.y)<=TopUpReach&&legClear(','template<class Free> bool screenPoint(',
                 'template<class Ops> void rearGuard(','if(f.on&&closer(a.x,a.y,f,soldierKeep(n))){','!closer(cx,cy,f,soldierKeep(n))&&','int want=c.escort?0:c.want;','alive+pending>=wanted?','int missing=wanted-alive-std::max(0,pending)']:assert text in model,text
    assert model.count('ops.moveGardeners(')==1 and model.count('distance(g.x,g.y,a.x,a.y)<=TopUpReach')==2 and 'GardenersWanted-alive' not in model
    assert 'OFFENCE_CONVOY player=%u sector=%u phase=%d style=%s target=%u kind=%u door=%d,%d army=%u gardeners=%d gap=%d limit=%d left=%d wait=%u at=%d,%d reason=%s' in source and 'chance=%u gap=%d wait=%u reason=%s' in source
    assert 'int wanted=dtoffence::gardenersWanted(soldiers);' in source and 'supply.carriers,&why,wanted)' in source and 'tools.toolsmiths,&why,wanted);' in source and 'supply.shovels,wanted);' in source
    tools=source[source.index('static void offenceTools('):source.index('static void offenceRecruit(')]
    assert 'dtoffence::shovelOrder(' in tools and 'supply.eco' in tools and '0xe66b10' not in tools.lower() and 'offenceShovelGuard' in tools
    report=dict(version='0.2.15',round='r32',game_sha256=pin['sha256'],game_started=False,live_process_read=False,
        native_targets='types 0x2E..0x30 only (0x4281D) and group nibble 0xC only for house kinds 10/15 (0x135D08): Dark buildings are never AI targets',
        native_damage_paths_for_dark_buildings='none (soldiers, war machines, axe warriors/saboteurs, spells); gardener greening and 0x136A80 only',
        mod_squad='0x45020(manager, 6) + Command(0) + AddMember from the sector reservoirs/guard squads; dissolve 0x3FA50 + slot 2',
        destroy='gardeners (type 50) green the ring of walkable footprint cells; native collapse 0xCBB60 -> 0x13CE10 -> 0x136A80(building, gardener owner); 0x136A80 called by the plugin only when the ring is all green and the building still stands',
        gardeners='authored at map start; native recruitment 0xD5100 (carrier + shovel) one at a time, keeping 2 shovels',
        shovels='r29: while gardeners are missing and free shovels are at the reserve, the home economy shovel order (byte eco+0x348 at the index of good 31 in eco+0x354) is topped up to at most 4 via 0xD6A90 (the 0x13A9 toolsmith handler), only with a ready toolsmith (type 0x15, CProductionBuildingRole) in that economy, never on a continuous 100 order; the toolsmith (0x14D008) makes nothing without an order; the AI never orders shovels for gardeners (0x249835 only for its diggers) and its building manager can clear or lower the order (0x249770 sends 10-2*diggers: 0 at its 5-digger steady state, negative above), so the plugin watches the byte every tick and puts back its own part after a drop of two or more (transient watch, before the net dispatch and the entity update)',
        citadel_fire='r30: CDarkTempleRole (Temple 50, Fortress 51; the Manacopter Hall has none) runs 0x1458C0 every 31 ticks per building: while the owner\'s ready-farm counter [0x10EF7AC + p*0x298] > 0 it kills (255 damage, 100% from 4 farms) hostile units with a selection group on walkable Dark cells (flags & 5 == 4) of the door\'s land sector within the Euclidean disc dx*dx - dx*dy + dy*dy <= R*R around position + (1,2), R = 16+2n or 24 (spiral 0xD69968, 2,083 cells for R = 24, up to 27 hex steps). The director keeps every computer unit outside FireLine 27 while the counter, the listed farms or a Farm site say the fire is on: soldier destinations soldierKeep(N) = 27 + formation spread, gardener cells 28, legs routed, farms within 33 and Halls within 44 never targeted, a guard every 10 ticks pulls units out (gardener aim cell role +0x40/+0x44 under job 0xAE/0xAF)',
        timing='first attack after AttackDelay*840/3 within 3..8 minutes (native agent 0x2FAF0 uses AttackDelay*840), checks every 256..383 ticks at 50% +25% per failure, 600..1499 ticks rest after a campaign',
        styles='raid, assault, pincer, sweep, hunt, allied (plugin generator, never the game LCG 0xE66B10); r32: raid max(10, army/3), hunt max(16, army/2), sweep escort max(8, army/4), each at most the army, assault/pincer/allied the whole army; gardeners base (raid 4, hunt 2, sweep 10, allied 6, assault/pincer 8) + soldiers taken/15, at most gardenersWanted(free soldiers) = min(20, 12 + max(0, soldiers-40)/10)',
        convoy='r32: the army is ordered every 30 ticks to the point of its leg 10 tiles ahead of the convoy rear (army centre or gardeners centre), the gardeners one id per order to cells around the point 3 behind it; the army holds while a gardener is farther than 8 + formation spread from its centre and leaves stragglers after 600 ticks; legs end only with the convoy together; at the target the army screens towards the nearest Dark fighter (within 6 of the door) and the gardeners wait 4 behind it until the screen holds; top-ups only within 20 of the army; a hunt walks to its gardeners (rally) first; the way home is a convoy. SettlerValues.xml leaves every <speed> empty (atoi 0 at record +1, 0x183CE1): one walking speed for all settler types',
        checked_instructions=len(WHY)+len(USED)+len(TOOLS)+len(FIRE),generated_signature_regions=len(PINS),relocation_data_copy_bases=['0x400000','0x360000','0x60000000'],
        gameplay_attack_verified=False,gameplay_greening_verified=False,gameplay_recruitment_verified=False,gameplay_toolsmith_orders_verified=False,dark_land_region_verified=False)
    (ROOT/'research/offence-native-checks.json').write_text(json.dumps(report,indent=2))
    print('PASS: offence target filters, mod squad lifecycle, gardener orders/greening/recruitment, toolsmith shovel orders (r29), citadel fire keep-off (r30), scaled styles and convoy (r32), AI config/strength layout and relocation-aware pins; file/data-only checks')

if __name__=='__main__':run()
