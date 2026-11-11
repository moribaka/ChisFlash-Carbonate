"""对已经生成及完成PCB映射的GBA内核消除冗余工作。"""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parent
path=ROOT/'src/rom_stream_kernels.h'
text=path.read_text(encoding='utf-8')
def ram_lookup(part):
    part=part.replace('        mov dptr,#_board_reverse_table\n        movc a,@a+dptr\n',
                      '        mov dpl,a\n        mov dph,#>_board_reverse_ram\n        movx a,@dptr\n')
    part=part.replace('        mov dptr,#_board_reverse_table\n','        mov dph,#>_board_reverse_ram\n')
    part=re.sub(r'        mov a,([^\n]+)\n        movc a,@a\+dptr\n',
                r'        mov dpl,\1\n        movx a,@dptr\n',part)
    assert '_board_reverse_table' not in part and 'movc a,@a+dptr' not in part
    return part
text=ram_lookup(text)
begin=text.index('static void rom_kernel_verify_pipeline(')
head,part=text[:begin],text[begin:]
before='        clr _P61\n        mov dpl,r5\n        movx a,@dptr\n'
assert part.count(before)==1
part=part.replace(before,before+'        nop\n        nop\n')
text=head+part
path.write_text(text,encoding='utf-8')
