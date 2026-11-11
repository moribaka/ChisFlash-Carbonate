"""将本次新生成的总线汇编转换为原版 PCB，保留实际采样等待时钟。"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parent
ports={'0':'7','7':'2','2':'0','60':'61','61':'60','70':'27'}
for name in ('rom_stream_kernels.h','rom_crc_kernel.h','rom_crc_legacy_kernel.h','gbc_kernels.h'):
    p=ROOT/'src'/name
    source=ROOT/'reference_modified'/name if name=='rom_crc_legacy_kernel.h' else p
    s=source.read_text(encoding='utf-8')
    s=re.sub(r'\b_P(0|7|2|60|61|70)\b',lambda m:'_P'+ports[m[1]],s)
    if name=='rom_crc_legacy_kernel.h':
        # 此文件其它 MOVC 是 CRC 查表，不能删掉。
        s=s.replace('        mov dptr,#_board_reverse_table\n        movc a,@a+dptr\n','')
        s=re.sub(r'    00985\$:\n        mov dptr,#_board_reverse_table\n.*?        mov dptr,#_crc_state',
                 '    00985$:\n        mov dptr,#_crc_state',s,flags=re.S)
    else:
        s=s.replace('        mov dptr,#_board_reverse_table\n','').replace('        movc a,@a+dptr\n','')
    if name=='rom_stream_kernels.h':
        # 原版无需反序，把三个指针更新移入 RD 低电平等待；加一个 NOP 即满足五时钟。
        begin=s.index('static void rom_kernel_verify_pipeline(')
        head,body=s[:begin],s[begin:]
        pointer='        inc dptr\n        mov r0,dpl\n        mov r1,dph\n'
        assert body.count(pointer)==1
        body=body.replace(pointer,'')
        old='        clr _P60\n        mov a,r5\n        cjne a,_P7,00993$'
        assert body.count(old)==1
        body=body.replace(old,'        clr _P60\n'+pointer+'        mov a,r5\n        nop\n        cjne a,_P7,00993$')
        s=head+body
    if name=='gbc_kernels.h':
        old='        mov r6,a\n'+'        nop\n'*3+'        jb _flashgbx_gbc_fast_timing'
        new='        mov r6,a\n'+'        nop\n'*8+'        jb _flashgbx_gbc_fast_timing'
        assert s.count(old)==1
        s=s.replace(old,new)
        assert s.count('        orl a,#0x01')==9
        s=s.replace('        orl a,#0x01','        orl a,#0x80')
        s=s.replace('        nop\n'*27+'    05912$:', '        nop\n'*32+'    05912$:')
    assert '_board_reverse_table' not in s,name
    p.write_text(s,encoding='utf-8')
