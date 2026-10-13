"""Pins the game's own GUI and Dark Tribe sprites drawn by the Dark menus.

Reads the installed game's Gfx files (read-only, offline) and writes
work/native-generated/gfx-signatures.hpp: for each sprite the file offset,
byte length and SHA-256 of its encoded data, its palette offset, and the
SHA-256 of each palette file. The DLL reads exactly these bytes at startup,
checks the hashes and decodes them; on any mismatch it keeps the drawn style.
No game data is written into the repository or the DLL.

Format (Settlers IV .gfx, verified on the pinned History Edition files):
every index/palette file has a 20-byte header followed by little-endian
uint32 entries. A .gil entry is the byte offset of an image in the .gfx.
An image header is 12 bytes (uint16 width, height, left, top, then a type
byte; type 32 = raw palette indices, otherwise run-length coded), or 8 bytes
with byte-sized fields when the first uint16 exceeds 860. Run-length data:
0 n = n transparent pixels, 1 n = n shadow pixels, any other byte = one
palette index. Palettes are RGB565. GUI files (.pil/.pa6) give a palette per
image; object files (.jil/.dil/.pi4/.p46) give one per job.
"""
import hashlib,struct
from pathlib import Path
from project_paths import game_dir
GFX=game_dir()/'Gfx'
# name, file, index. Order is irrelevant; names are used by native/gfx-assets.hpp.
SPRITES=[
 ('panel',9,589),('page',9,590),('strip',9,592),('separator',9,852),('bar',9,79),('healthFrame',9,76),
 ('demolish',9,108),('demolishOn',9,109),('next',9,106),('nextOn',9,107),('cancel',9,156),('cancelOn',9,157),
 ('back',9,280),('backOn',9,281),('down',9,121),('downOn',9,119),('up',9,122),('upOn',9,120),
 ('unload',9,74),('unloadOn',9,75),('houseTab',9,343),('houseTabOn',9,342),('sword',9,452),('stop',9,270),
 # Barracks recruitment controls (Menu/GUISetRomans.DAT menu 46, elements 1766..1771:
 # infinite, zero, up 5, down 5; up/down 1 are 'up'/'down' above) and the pressed
 # bow tab whose gold corners are the native selection bracket (#486 plus brackets).
 ('infinite',9,124),('infiniteOn',9,116),('zero',9,123),('up5',9,125),('down5',9,117),('bowOn',9,487),
 # Empty background of the native resource/statistics panel (GUISetRomans.DAT
 # menu 1, y 600, 210x168): the Dark mana section covers that panel with it.
 ('resourcePanel',9,587),
 ('farm',13,11),('temple',13,73),('fortress',13,75),('hall',13,79),
 # Dark settlers by the engine's animation name (resolved below), not by index.
 ('warrior',23,'SML01_WALK'),('thrower',23,'BML01_WALK'),('gardener',23,'DG_WALK'),('shaman',23,'SHM_WALK'),('farmer',23,'MF_WALK'),
 ('slave',23,'SS_WALK'),('pilot',23,'MCM_WALK'),
 ('manacopter',36,307),('crystal',0,65),
]
EXE=game_dir()/'S4_Main.exe'
# S4_Main.exe keeps one name->id table for settler types and animation jobs
# (pairs of char* name, uint32 id) starting at RVA 0xC00974 with
# SETTLER_NO_SETTLER=1. An animation id minus one is its job number in the
# race's settler .gfx (verified in game: DG_WALK is the Dark Gardener sprite,
# SHM_WALK the Shaman, SS_WALK the enslaved settler, MF_WALK the farmer).
ANIM_TABLE=0xC00974
def animation_ids():
    import sys;sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'work/python'));import pefile
    pe=pefile.PE(str(EXE),fast_load=True);base=pe.OPTIONAL_HEADER.ImageBase;b=EXE.read_bytes()
    def text(va):
        try:o=pe.get_offset_from_rva(va-base)
        except Exception:return None
        e=b.find(bytes(1),o);t=b[o:e];return t.decode() if 0<len(t)<64 and all(32<c<127 for c in t) else None
    o=pe.get_offset_from_rva(ANIM_TABLE);ids={}
    first=struct.unpack_from('<II',b,o)
    if text(first[0])!='SETTLER_NO_SETTLER' or first[1]!=1:raise RuntimeError('animation name table moved')
    while True:
        ptr,val=struct.unpack_from('<II',b,o);n=text(ptr)
        if not n:break
        ids[n]=val;o+=8
    return ids
def first_frame(f,job):
    """First image of `job` (through .jil/.dil) in the most upright facing
    direction facing the viewer: the tallest opaque box relative to its width, so the figure
    fills a native 30x47 portrait cell."""
    jil=ints(GFX/f'{f.n}.jil');dil=ints(GFX/f'{f.n}.dil')
    if job>=len(jil) or not jil[job]:raise RuntimeError(f'job {job} missing in {f.n}.jil')
    best=None
    for direction in (1,2,3): # facing the viewer, like native portraits
        d=(jil[job]-20)//4+direction
        if not dil[d]:continue
        i=(dil[d]-20)//4;w,h=f.opaque_box(i)
        if w and (best is None or h/w>best[0]):best=(h/w,i)
    if not best:raise RuntimeError(f'job {job} has no frames')
    return best[1]
def ints(p):
    b=p.read_bytes();return list(struct.unpack_from('<%dI'%((len(b)-20)//4),b,20))
class GfxFile:
    def __init__(s,n):
        s.n=n;s.gfx=(GFX/f'{n}.gfx').read_bytes();s.gil=ints(GFX/f'{n}.gil')
        if (GFX/f'{n}.pil').exists():s.palName=f'{n}.pa6';s.pil=ints(GFX/f'{n}.pil');s.job=None
        else:
            s.palName=f'{n}.p46';s.pil=ints(GFX/f'{n}.pi4');jil=ints(GFX/f'{n}.jil');dil=ints(GFX/f'{n}.dil')
            starts=[]
            for k,jo in enumerate(jil):
                if not jo:continue
                d0=(jo-20)//4;nxt=[x for x in jil[k+1:] if x];d1=(nxt[0]-20)//4 if nxt else len(dil)
                starts+=[((dil[d]-20)//4,k) for d in range(d0,d1) if dil[d]]
            starts.sort();s.job=[None]*len(s.gil)
            for a,(g0,k) in enumerate(starts):
                for g in range(g0,starts[a+1][0] if a+1<len(starts) else len(s.gil)):s.job[g]=k
        s.pal=(GFX/s.palName).read_bytes()
    def header(s,i):
        o=s.gil[i];g=s.gfx
        if struct.unpack_from('<H',g,o)[0]>860:return dict(w=g[o],h=g[o+1],type=0,data=o+8)
        w,h=struct.unpack_from('<2H',g,o);return dict(w=w,h=h,type=g[o+8],data=o+12)
    def opaque_box(s,i):
        """Width and height of the opaque (non-shadow) pixels of image i."""
        h=s.header(i);p=h['data'];w=h['w'];n=w*h['h'];j=0;g=s.gfx;xs=[];ys=[]
        while j<n:
            if h['type']==32:v=2;p+=1
            else:
                v=g[p];p+=1
                if v<=1:j+=g[p];p+=1;continue
            xs.append(j%w);ys.append(j//w);j+=1
        return (max(xs)-min(xs)+1,max(ys)-min(ys)+1) if xs else (0,0)
    def palette_offset(s,i):
        raw=s.pil[s.job[i]] if s.job else s.pil[i]
        if raw<20 or (raw-20)%2:raise RuntimeError(f'bad palette offset {raw} for {s.n}:{i}')
        return (raw-20)//2
    def encoded(s,i):
        """Returns (offset,length) of header plus pixel data, walking the run-length stream."""
        h=s.header(i);p=h['data'];n=h['w']*h['h'];j=0;g=s.gfx
        if h['type']==32:p+=n
        else:
            while j<n:
                v=g[p];p+=1
                if v<=1:j+=g[p];p+=1
                else:j+=1
        # The final transparent run may extend past the last pixel; decoders clamp it.
        return s.gil[i],p-s.gil[i]
def generate(out):
    files={};pals={};rows=[];ids=None
    for name,n,i in SPRITES:
        f=files.get(n) or files.setdefault(n,GfxFile(n))
        if isinstance(i,str):
            ids=ids or animation_ids()
            if i not in ids:raise RuntimeError('unknown animation '+i)
            i=first_frame(f,ids[i]-1)
        h=f.header(i);off,length=f.encoded(i);po=f.palette_offset(i)
        if po+256>(len(f.pal)-20)//2:raise RuntimeError(f'palette range outside {f.palName}')
        if f.palName not in pals:pals[f.palName]=(len(pals),len(f.pal),hashlib.sha256(f.pal).digest())
        rows.append((name,pals[f.palName][0],n,i,off,length,po,h['w'],h['h'],hashlib.sha256(f.gfx[off:off+length]).digest()))
    hx=lambda d:'{'+','.join('0x%02x'%b for b in d)+'}'
    lines=['#pragma once','#include <stdint.h>','// Generated by tools/gfx_pins.py from the local game files. Do not publish.','namespace dtgfxpin {',
     'struct Palette {const wchar_t*name;uint32_t size;uint8_t sha[32];};',
     'struct Sprite {const char*name;uint8_t palette;uint16_t file,index;uint32_t offset,length,paletteOffset;uint16_t w,h;uint8_t sha[32];};',
     'static const Palette palettes[]={'+','.join('{L"%s",%d,%s}'%(k,v[1],hx(v[2])) for k,v in sorted(pals.items(),key=lambda kv:kv[1][0]))+'};',
     'static const Sprite sprites[]={'+',\n '.join('{"%s",%d,%d,%d,%d,%d,%d,%d,%d,%s}'%(r[:9]+(hx(r[9]),)) for r in rows)+'};',
     'constexpr int paletteCount=%d,spriteCount=%d;'%(len(pals),len(rows)),'}']
    Path(out).write_text('\n'.join(lines)+'\n')
    return rows
if __name__=='__main__':
    import sys
    for r in generate(sys.argv[1] if len(sys.argv)>1 else 'gfx-signatures.hpp'):print(r[:9])
