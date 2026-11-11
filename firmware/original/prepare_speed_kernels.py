"""对已经生成及完成PCB映射的GBA内核消除冗余工作。"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parent
path=ROOT/'src/rom_stream_kernels.h'
text=path.read_text(encoding='utf-8')
for kind in ('write','verify'):
    begin=text.index('static void rom_kernel_'+kind+'_pipeline(')
    end=text.find('static void ',begin+1)
    if end<0:end=len(text)
    part=text[begin:end]
    loads='        mov r0,_rom_write_xbuf\n        mov r1,(_rom_write_xbuf + 1)\n'
    assert part.count(loads)==1
    part=part.replace(loads,'        mov dpl,_rom_write_xbuf\n        mov dph,(_rom_write_xbuf + 1)\n')
    pointer='        mov dpl,r0\n        mov dph,r1\n'
    if kind=='write':
        assert part.count(pointer)==2
        part=part.replace(pointer,'',1)
        part=part.replace(pointer,'        nop\n        nop\n',1)
        part=part.replace('        mov r0,dpl\n        mov r1,dph\n','')
    else:
        assert part.count(pointer)==1
        part=part.replace(pointer,'')
        copies='        mov r0,dpl\n        mov r1,dph\n'
        assert part.count(copies)==1
        part=part.replace(copies,'        nop\n        nop\n')
    text=text[:begin]+part+text[end:]
path.write_text(text,encoding='utf-8')
