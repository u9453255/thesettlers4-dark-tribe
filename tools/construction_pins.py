"""Private local signatures for native reservation, rise drawing and completion."""
from placement_pins import generate as generate_regions
# Complete instruction/data ranges; executable full-file pins remain mandatory.
PINS=[(0x136470,36),(0x1364f2,24),(0x1362f0,37),(0x136317,48),
      (0x1363c6,66),(0x140b20,57),(0x141bf0,128),(0x141c70,73),
      (0x141cd0,45),(0x141d3e,26),(0x141b0e,42),(0x14217a,47),
      (0x621c0,68),(0x62201,82),(0x14556a,55),(0x14799a,53),
      (0x142d50,42),(0x142d91,53),(0x142ddc,27),(0x142e0a,45),(0x143120,39),
      (0xc591bc,32),(0xc59284,32),(0xc58ae8,32),(0xc593d0,32),
      # 0.2.14 mirrored completion: phase table/7/8, phase-5 pair, grow init,
      # CBuilding pile detach slot and native object deletion entry.
      (0x14236c,28),(0x142267,0xab),(0x142312,0x4f),(0x1424c0,0x30),
      (0x1429f0,0x1a8),(0x141a50,0x40),(0x143680,0x35),(0x143b83,0xbd),
      # 0.2.14 r4: phase-4 claim mirrored for ordinary sites (object clear, bit 8, anchor).
      (0x141f1b,0x10),(0x141fe7,0x1c),(0x142181,0x28),
      (0x137100,0x33),(0xc585e4+0x40,4),(0x16d030,0x24)]
def generate(executable,output):generate_regions(executable,output,PINS,'dtconstruction')
