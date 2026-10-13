"""Local-only native bridge signatures, for the exact protected executable."""
import struct,sys
from pathlib import Path
from game_build import verify_executable

# Each entry was traced in the pinned binary: native Dark validation, its actual
# collision map, native sparse hint lookup/setter, projection and viewport data.
PINS = [(0x13e4e0, 32), (0x13ebc0, 26), (0x13ec4e, 14), (0x13ecb3, 47),
        (0xee3a8, 45), (0xeefa0, 50), (0x259910, 28), (0x2598f0, 6), (0x259900, 6),
        (0x259986, 81), (0x20c839, 60), (0x20eae7, 15), (0x20eb44, 25)]

def generate(executable, output, pins=PINS, namespace='dtplacement'):
    verify_executable(executable)
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'work/python'))
    import pefile
    pe=pefile.PE(str(executable));base=pe.OPTIONAL_HEADER.ImageBase
    relocations={r.rva for block in pe.DIRECTORY_ENTRY_BASERELOC for r in block.entries if r.type==3}
    lines=['#pragma once','#include <stdint.h>','#include <string.h>','namespace '+namespace+' {',
           'inline bool intact(uintptr_t image){']
    for index,(rva,length) in enumerate(pins):
        raw=pe.get_data(rva,length)
        lines.append('BYTE expected%d[]={%s};'%(index,','.join('0x%02x'%v for v in raw)))
        for relocation in sorted(relocations):
            if rva<=relocation<rva+length:
                offset=relocation-rva
                if offset+4>length:raise RuntimeError('Truncated relocation in placement signature')
                value=struct.unpack_from('<I',raw,offset)[0]-base
                lines.append('{uint32_t pointer=(uint32_t)(image+0x%x);memcpy(expected%d+%d,&pointer,4);}'%(value,index,offset))
        lines.append('if(memcmp((void*)(image+0x%x),expected%d,sizeof(expected%d)))return false;'%(rva,index,index))
    lines.append('return true;} }')
    Path(output).parent.mkdir(parents=True,exist_ok=True)
    Path(output).write_text('\n'.join(lines),encoding='utf-8')
