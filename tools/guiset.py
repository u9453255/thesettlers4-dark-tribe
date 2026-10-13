"""Lists the game's native menu layouts (Menu/GUISet<Race>.DAT), read-only.

Format (verified on the pinned History Edition files): uint32 version, uint32
menu count, 8 bytes, then uint32 offsets of each menu. A menu is a 16-byte
header (uint32 id, uint16 y, width, height, background sprite, element count,
flags) followed by 36-byte elements of uint16 fields: x, y, w, h (relative to
the menu), sprite, element id, 0, pressed sprite, ..., field 12 low byte =
type (2 tab, 0/3 button, 4 text, 6 image, 10 bar), field 13 = font index for
text (the native GUI font table), field 16 = text alignment. Sprite numbers
index the race GUI file (9.gfx for Romans).
Usage: python tools/guiset.py [Romans|Vikings|Mayas|Trojans] [menu ...]
"""
import struct,sys
from project_paths import game_dir
def load(name='Romans'):
    b=(game_dir()/f'Menu/GUISet{name}.DAT').read_bytes()
    n=struct.unpack_from('<I',b,4)[0];offs=list(struct.unpack_from('<%dI'%n,b,16))+[len(b)]
    menus=[]
    for m in range(n):
        o=offs[m];e=offs[m+1];mid,y,w,h,spr,cnt,fl=struct.unpack_from('<I6H',b,o)
        menus.append(dict(i=m,id=mid,y=y,w=w,h=h,spr=spr,cnt=cnt,fl=fl,els=[struct.unpack_from('<18H',b,p) for p in range(o+16,e-35,36)]))
    return menus
if __name__=='__main__':
    race=sys.argv[1] if len(sys.argv)>1 else 'Romans';pick={int(a) for a in sys.argv[2:]}
    for M in load(race):
        if pick and M['i'] not in pick:continue
        print(f"menu {M['i']} id={M['id']} y={M['y']} w={M['w']} h={M['h']} sprite={M['spr']} elements={len(M['els'])}")
        if pick:
            for e in M['els']:print('  xywh=%s sprite=%d pressed=%d type=%d font=%d align=%d'%(e[:4],e[4],e[7],e[12]&0xff,e[13],e[16]))
