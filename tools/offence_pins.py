"""Private local signatures for the computer offence against the Dark Tribe (rounds r27..r33)."""
from placement_pins import generate as generate_regions
# Whole-instruction code ranges and relocated data words the plugin relies on
# (research/AI_OFFENCE_0215.md; tools/inspect-offence.py checks each one).
PINS=[# Mod squad lifecycle: factory 0x45020 (+ 0x45470 guard), Command 0x46880
      # (+ slot 11 waypoint reset and the command-0 case), AddMember 0x46770 ->
      # 0x44AB0, deleting dtor 0x398B0, dissolve 0x3FA50 (record lookup 0x35370).
      (0x45020,0x51),(0x45470,0x2f),(0x46880,0x6a),(0x468ea,0x9),(0x46340,0x24),(0x46770,0x36),
      (0x398b0,0x29),(0x3fa50,0x51),(0x35370,0x6b),
      # Credited destroy 0x136A80: campaign Fortress branch, destroyer store, deletion, role slot 11.
      (0x136a80,0x60),(0x136cb5,0x40),
      # Attack agent timing/config (0x2FB1A, 0x2FBF1), squad size globals and native dissolve
      # sequence in CAIAgentSquads, F/Str stores, sector region, Dark fighter value compare.
      (0x2fb1a,0x34),(0x2fbf1,0x34),(0x33b29,0x44),(0x33c15,0x28),(0x342ba,0x12),(0x42a78,0xe),(0x2f4e0,0xc),
      # CAIMain player table and AI bit, round robin gate, player AI player field and race split.
      (0x38548,0x28),(0x384e7,0xb),(0x39b69,0x3),(0x399f0,0x11),
      # Per-player building and settler lists, land region lookup, enemy masks.
      (0x394a0,0xa),(0x394ca,0xb),(0x36702,0xd),(0x145a38,0x17),(0x4282b,0xd),
      # The Temple role's parked-settler vector.
      (0x145cf6,0x1f),
      # Vtables: CAITaskForceSquad slots 0..11, reservoir, CAIMain, normal player/sector AI.
      (0xc2a91c,0x30),(0xc2a950,0x2c),(0xc2a8a4,4),(0xc2aa0c,4),(0xc2b218,4),
      # Bodies behind the calls (code review): reservoir AddMember/RemoveMember, squad
      # RemoveMember, base dtor and task-force unlink (round-robin cursor), the middle of
      # 0x136A80 (role slot 25, race-3 economy skip) and the removal 0x16D030 head;
      # the Command case-0 jump-table entry and the factory's state 6 -> type table entry.
      (0x44ab0,0xbc),(0x44ba0,0x6d),(0x467e0,0x58),(0x449e0,0x7c),(0x45710,0x5d),(0x136b6d,0x148),(0x16d030,0x82),
      (0x469c8,4),(0xbf07c0,4),
      # Gardeners (r4): role vtable, job word writer, the move order setting the work
      # centre (event 0x11 command 0xD, WATCH bit 4), the cell test skipping occupied
      # cells, WORK completion greening with the gardener's owner (CDarkTribe slot 4),
      # and greening -> ring walk (slot 7) -> 0x13CE10(x, y, player) collapse.
      (0xc5b51c,4),(0x180fcc,4),(0x179af6,4),(0x179b87,0x24),(0x179ed4,0xb),(0x1795eb,0x22),(0xcbbf6,0x24),
      # Native gardener recruitment: order 0xD5100 (specialist table 0xD64214), shovel
      # stock 0xD4280, economy owner test 0xF61E0, eco pass carrier tests and the
      # in-flight conversion count (eco +0x22).
      (0xd5100,0xc2),(0xd4280,0x45),(0xf61e0,0x1a),(0xd52c0,0x1d),(0xd54d3,0xd),(0xd64214,0x28),
      # Gardener give-up: 57 empty slices, then the default job; a failed WORK start keeps its reserved cell.
      (0x179991,4),(0x17974e,6),
      # Free stock as the pile search 0x170B80 counts it: stock (vtbl +0x28) minus reservations (byte +0x42).
      (0x170bc4,4),
      # r28 territory: the cell owner reader 0x58A40 (pool header dword & 0xF), non-Dark placement
      # (BuildBuilding anchor mask 0x5F + owner == player, footprint mask 0x5F), the territory claim
      # (0x80 influence, SetOwner via the tiling, no Dark test), the gardener search comparing land
      # regions only, SetDark toggling only 0x04, and the WORK end clearing the 0x20 reservation.
      (0x58a40,0x28),(0x13e623,0x2c),(0x13e65d,2),(0x20d1aa,6),(0x20d205,0xf),(0x179fcc,0x13),(0x17a03b,0x12),(0xcbb8b,0x22),(0x1795d9,4),
      # r29 toolsmith shovel orders: the order 0xD6A90 (tool list eco+0x354, order bytes eco+0x348, 100 =
      # continuous), the net handler 0x13A9 (GetEntityWithOwner 0xF6120, ready flag, role kind 4, economy
      # 0xF6200, type 0x15 -> 0xD6A90), the consumer (eco lookup, type 0x15 round robin, decrement unless
      # 100), the AI's own digger shovel order, the eco save of both vectors, the per-type building list
      # count of CEcoManager 0x4F4A0, and CProductionBuildingRole slot 28 (kind 4).
      (0xd6a90,0x8e),(0xf687d,0x73),(0xf6120,0x28),(0xf6200,0x38),(0x14ce2b,0x4a),(0x14d008,0x61),(0x24981c,0x1f),
      (0xd3f02,0x83),(0x4f4b0,0x72),(0x13ada0,6),(0xc59104,4),
      # r29 review: the AI building manager also clears or lowers the shovel order: digger count d = vt+0x98(2)
      # (0x249422), the built-toolsmith task test and SendProduceOrder(toolsmith, SHOVEL, 10-2*d) (0x249720..0x249776),
      # and its digger recruitment while d < 10-d (0x2497A3), i.e. a steady state of 5 diggers where the amount is 0.
      (0x24940e,0x1d),(0x249720,0x56),(0x2497a3,8),
      # r30 citadel fire keep-off: the fire pass 0x1458C0 (owner's ready-farm counter [0x10EF7AC + p*0x298] at
      # 0x1458F4, centre = position + (1,2), radius 2n+16 or 24), the victim cell test (flags & 5 == 4), the enemy
      # mask / class 1 or 4 / group nibble tests, the Temple role tick's per-pass call and 31-tick reschedule
      # (0x16D5D0(0x1F)), the 255-damage event-2 post, CDarkTempleRole slot 3 (the tick), the gardener search's
      # reservation write (+0x40/+0x44) and its job 0xAF / 0xAE pushes, and the spiral count words for R = 16..24.
      (0x1458c0,0x99),(0x145a00,0x21),(0x145a71,0x2b),(0x145685,0x18),(0x145b3a,0x1d),(0xc59290,4),
      (0x17a0e1,0x3b),(0x17a12a,0x17),(0x17a175,0xe),(0xd7cf7a,0x22),
      # r33 escorted gardeners (WATCH): the group order copies the mode into the event unchanged (0x16A9E3), the gardener's
      # order handler tests event +0xC & 4 (pinned above at 0x179B87) and pushes job 0xB0 without touching the work centre,
      # else job 0xAE with the work centre; both store the destination at role +0x18 (0x179BC5); the job id table entry
      # 0xB0 = JOB_GARDENER_FORCE_WALK (GO_TO_POS, WAIT in GameData/jobInfo.xml).
      (0x16a9e3,0x15),(0x179bab,0x1d),(0xc049d0,4),
      # r33 defence: the native intruder test 0x1B17E0 (owner of the cell == player; re-implemented in the bridge) and
      # CAIAgentIntruderCombat's intruder type table 0xBF0438 (46, 48, 49, 47, 50, 45: no Dark type).
      (0x1b1808,0x15),(0xbf0438,0x18)]
DATA_PINS={0xc2a91c,0xc2a950,0xc2a8a4,0xc2aa0c,0xc2b218,0x469c8,0xbf07c0,0xc5b51c,0xd64214,0xc59104,0xc59290,0xd7cf7a,0xc049d0,0xbf0438}
def generate(executable,output):generate_regions(executable,output,PINS,'dtoffence')
