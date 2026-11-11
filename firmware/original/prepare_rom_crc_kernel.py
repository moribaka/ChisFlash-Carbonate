"""生成不跨卡带地址边界的 CRC 内核，比较代码查表和片内 RAM 查表。"""
from pathlib import Path
ROOT=Path(__file__).resolve().parent
regs=['acc','b','psw','dpl','dph',*('ar'+str(i) for i in range(8))]
save='\n'.join('        push '+r for r in regs)
restore='\n'.join('        pop '+r for r in reversed(regs))

def update(rotation,ram,arm_next=False):
    a,b,c,d=[(i+rotation)%4+4 for i in range(4)]
    arm='        clr _P61\n' if arm_next else ''
    if ram:
        # 将索引写入 DPL 后四次查表共用该地址，省去反复装入累加器。
        return f'''        xrl a,r{a}
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar{b},a
        inc dph
        movx a,@dptr
        xrl ar{c},a
{arm}        inc dph
        movx a,@dptr
        xrl ar{d},a
        inc dph
        movx a,@dptr
        mov r{a},a'''
    return f'''        xrl a,r{a}
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar{b},a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar{c},a
{arm}        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar{d},a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r{a},a'''

def word(index,rotation,ram,prefetched=False,arm_next=False):
    wait='' if prefetched else '''        clr _P61
        nop
        nop
        nop
        nop
        nop
'''
    return f'''        jnb _flashgbx_bus_cancelled,{7000+index*10:05d}$
        ljmp 00985$
    {7000+index*10:05d}$:
{wait}        mov a,_P0
        mov b,_P7
        setb _P61
{update(rotation,ram)}
        mov a,b
{update(rotation+1,ram,arm_next)}
'''

def kernel(ram):
    kind='ram' if ram else 'code'
    table='_crc_ram_table' if ram else '_crc32_wire0'
    body=''.join(word(i,(i%2)*2,ram,i>0,i<15) for i in range(16))
    tail16=''.join(word(i,(i%2)*2,ram,i>16,i<23) for i in range(16,24))
    tail8=''.join(word(i,(i%2)*2,ram,i>24,i<27) for i in range(24,28))
    tail4=''.join(word(i,(i%2)*2,ram,i>28,i<29) for i in range(28,30))
    return f'''void cart_rom_crc_{kind}_kernel(void) __naked {{
    __asm
        .globl _crc_state
        .globl _crc32_wire0
        .globl _crc_ram_table
{save}
        mov dptr,#_crc_state
        movx a,@dptr
        mov r4,a
        inc dptr
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        movx a,@dptr
        mov r7,a
        mov r0,_rom_write_remaining
        mov r1,(_rom_write_remaining+1)
        mov a,r0
        jz 00979$
        inc r1
    00979$:
        mov dptr,#_rom_crc_flags
        movx a,@dptr
        mov r3,a
        mov dptr,#{table}
        anl a,#0x80
        jnz 00980$
        ljmp 00984$
    00980$:
{body}
        djnz r0,00987$
        djnz r1,00987$
        ljmp 00984$
    00987$:
        ljmp 00980$
    00984$:
        mov a,r3
        anl a,#16
        jnz 00969$
        ljmp 00970$
    00969$:
{tail16}
    00970$:
        mov a,r3
        anl a,#8
        jnz 00971$
        ljmp 00972$
    00971$:
{tail8}
    00972$:
        mov a,r3
        anl a,#4
        jnz 00973$
        ljmp 00974$
    00973$:
{tail4}
    00974$:
        mov a,r3
        anl a,#2
        jnz 00986$
        ljmp 00988$
    00986$:
{word(30,0,ram)}
        mov a,r4
        mov r2,a
        mov a,r6
        mov r4,a
        mov a,r2
        mov r6,a
        mov a,r5
        mov r2,a
        mov a,r7
        mov r5,a
        mov a,r2
        mov r7,a
    00988$:
        jb _flashgbx_bus_cancelled,00985$
        mov a,r3
        anl a,#1
        jz 00985$
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov a,_P0
        setb _P61
{update(0,ram)}
        mov a,r4
        mov ar4,ar5
        mov ar5,ar6
        mov ar6,ar7
        mov r7,a
    00985$:
        mov dptr,#_crc_state
        mov a,r4
        movx @dptr,a
        inc dptr
        mov a,r5
        movx @dptr,a
        inc dptr
        mov a,r6
        movx @dptr,a
        inc dptr
        mov a,r7
        movx @dptr,a
{restore}
        ret
    __endasm;
}}
'''

text='''/* 由 prepare_rom_crc_kernel.py 生成。每轮三十二字节；调用方保证不跨低十六位地址边界。
   各分段之间保留物理位序的 CRC 状态，命令结束时统一转换输出。
   后续采样等待由前一个字的 CRC 尾部计算覆盖；片段首字保留五个 NOP。
   片段末尾不会预读额外数据，每字检查断线，不省略校验。 */
'''+kernel(True)
(ROOT/'src/rom_crc_kernel.h').write_text(text,encoding='utf-8')
