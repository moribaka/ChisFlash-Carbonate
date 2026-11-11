"""生成 GB 直接逐字节核对和 CRC 内核，保留旧读时序及每字节取消检查。"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent
registers = ['acc', 'b', 'psw', 'dpl', 'dph'] + ['ar'+str(i) for i in range(8)]
save = '\n'.join('        push '+r for r in registers)
restore = '\n'.join('        pop '+r for r in reversed(registers))
wait40 = '\n'.join('        nop' for _ in range(40))
wait120 = '\n'.join('        nop' for _ in range(120))

def nops(count):return '\n'.join('        nop' for _ in range(count))

# STC8H 手册第 331～332 页：普通 MOV/XRL/INC 为 1 时钟，片内 MOVX 读为 2。
# 下面 CRC 更新为 21 时钟，前置 MOV A,B 为 1；再补 18 NOP，覆盖旧 A15 的 40 时钟等待。
crc_update='''        xrl a,r4
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl a,r5
        mov r4,a
        inc dph
        movx a,@dptr
        xrl a,r6
        mov r5,a
        inc dph
        movx a,@dptr
        xrl a,r7
        mov r6,a
        inc dph
        movx a,@dptr
        mov r7,a'''

# A15 读法在采样后把 A15 拉高；其它高地址位只需在低字节进位时更新。
# OR 0x01 对应改板的 P70，原版转换器会改成 P27 对应的 0x80。
cache_high='''        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
'''

# CRC累加器每字节旋转逻辑角色，四字节后回到原位；省去每字节三次搬运。
def crc_rotated(phase):
    a,b,c,d=[(i+phase)%4+4 for i in range(4)]
    return f'''        xrl a,r{a}
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar{b},a
        inc dph
        movx a,@dptr
        xrl ar{c},a
        inc dph
        movx a,@dptr
        xrl ar{d},a
        inc dph
        movx a,@dptr
        mov r{a},a
'''

pipeline_crc='''        jb _flashgbx_dmg_pipeline,04800$
        ljmp 04301$
    04800$:
        jb _gbc_io_hold,04801$
        ljmp 04301$
    04801$:
        jnb _gbc_io_slow,04802$
        ljmp 04301$
    04802$:
        mov a,r1
        jnb acc.7,04803$
        ljmp 04301$
    04803$:
        lcall _gbc_io_sample
'''+cache_high+'''        ljmp 06000$
'''
for phase in range(4):
    entry=6000+phase*20
    next_entry=6000+((phase+1)%4)*20
    final=6100+(phase+1)%4
    pipeline_crc+=f'''    {entry:05d}$:
        jnb _flashgbx_bus_cancelled,{entry+1:05d}$
        ljmp 04302$
    {entry+1:05d}$:
        djnz r2,{entry+2:05d}$
        djnz r3,{entry+2:05d}$
{crc_rotated(phase)}        ljmp {final:05d}$
    {entry+2:05d}$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
{crc_rotated(phase)}        jb _flashgbx_gbc_fast_timing,{entry+3:05d}$
{nops(21)}
    {entry+3:05d}$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,{entry+4:05d}$
        inc r1
{cache_high}    {entry+4:05d}$:
        ljmp {next_entry:05d}$
'''
pipeline_crc+='''    06100$:
        ljmp 04302$
    06101$:
        mov a,r4
        mov ar4,ar5
        mov ar5,ar6
        mov ar6,ar7
        mov r7,a
        ljmp 04302$
    06102$:
        mov a,r4
        mov ar4,ar6
        mov r6,a
        mov a,r5
        mov ar5,ar7
        mov r7,a
        ljmp 04302$
    06103$:
        mov a,r7
        mov ar7,ar6
        mov ar6,ar5
        mov ar5,ar4
        mov r4,a
        ljmp 04302$
'''

# 预取原始期望字节并转成总线位序需 13 时钟，补 27 NOP；原版板去掉反序后补足差额。
pipeline_verify=f'''        jb _flashgbx_dmg_pipeline,04900$
        ljmp 04200$
    04900$:
        jb _gbc_io_hold,04901$
        ljmp 04200$
    04901$:
        jnb _gbc_io_slow,04902$
        ljmp 04200$
    04902$:
        mov a,r1
        jnb acc.7,04903$
        ljmp 04200$
    04903$:
{cache_high}        clr _P61
    04600$:
        jb _flashgbx_bus_cancelled,04603$
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov dpl,r4
        mov dph,r5
        movx a,@dptr
        inc dptr
        mov r4,dpl
        mov r5,dph
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov r6,a
{nops(3)}
        jb _flashgbx_gbc_fast_timing,04604$
{nops(24)}
    04604$:
        mov a,_P2
        setb _P70
        cjne a,ar6,04602$
        inc r0
        cjne r0,#0,04601$
        inc r1
{cache_high}    04601$:
        djnz r2,04600$
        djnz r3,04600$
        setb _gbc_io_matched
        ljmp 04204$
    04602$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dptr,#_cart_verify_actual
        movx @dptr,a
    04603$:
        clr _gbc_io_matched
        ljmp 04204$
'''

text = f'''/* 由 prepare_gbc_kernels.py 生成；总线采样供逐字节核对和 CRC 共用。 */
bit flashgbx_dmg_pipeline=1;
bit flashgbx_gbc_batch_fast=1;
bit flashgbx_gbc_fast_timing=1;
bit cart_gbc_hold_writes;
static bit gbc_write_open;
static bit gbc_io_pulse,gbc_io_hold,gbc_io_slow,gbc_io_matched,gbc_io_write_pulse;

static void gbc_write_kernel(void) __naked {{
    __asm
{save}
        mov r0,_rom_write_addr
        mov r1,(_rom_write_addr+1)
        mov r2,_rom_write_remaining
        mov r3,(_rom_write_remaining+1)
        mov r4,_rom_write_xbuf
        mov r5,(_rom_write_xbuf+1)
        mov a,r2
        jz 04400$
        inc r3
    04400$:
        jb _flashgbx_bus_cancelled,04404$
        clr _P60
        setb _P16
        jnb _gbc_io_write_pulse,04401$
        setb _P60
    04401$:
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        mov a,r1
        movc a,@a+dptr
        mov _P7,a
        mov dpl,r4
        mov dph,r5
        movx a,@dptr
        inc dptr
        mov r4,dpl
        mov r5,dph
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov _P2,a
        inc r0
        cjne r0,#0,04402$
        inc r1
    04402$:
        nop
        nop
        nop
        nop
        nop
        jnb _gbc_io_write_pulse,04403$
        clr _P60
    04403$:
        clr _P16
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        setb _P16
        setb _P60
        mov _P2,#0xff
        mov _P0,#0xff
        mov _P7,#0xff
        djnz r2,04400$
        djnz r3,04400$
    04404$:
{restore}
        ret
    __endasm;
}}

/* 输入 R0:R1 为原始地址；返回 A 为反序数据，地址加一。其它通用寄存器保持。 */
void gbc_io_sample(void) __naked {{
    __asm
        jnb _gbc_io_pulse,04100$
        setb _P60
    04100$:
        jb _gbc_io_hold,04106$
        ljmp 04101$
    04106$:
        mov a,r1
        jnb acc.7,04107$
        ljmp 04101$
    04107$:
        clr _P61
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        mov a,r1
        movc a,@a+dptr
        mov _P7,a
{wait40}
        jnb _gbc_io_slow,04102$
{wait120}
    04102$:
        mov a,_P2
        setb _P70
        sjmp 04103$
    04101$:
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        mov a,r1
        movc a,@a+dptr
        mov _P7,a
        nop
        nop
        nop
        nop
        nop
        jnb _gbc_io_pulse,04104$
        clr _P60
    04104$:
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov a,_P2
        setb _P61
        jnb _gbc_io_pulse,04103$
        setb _P60
    04103$:
        inc r0
        cjne r0,#0,04105$
        inc r1
    04105$:
        ret
    __endasm;
}}

/* C 包装负责总线方向及上拉；以下两段保存全部寄存器，允许 Timer/USB 中断。 */
static bit gbc_verify_kernel(void) __naked {{
    __asm
{save}
        mov r0,_rom_write_addr
        mov r1,(_rom_write_addr+1)
        mov r2,_rom_write_remaining
        mov r3,(_rom_write_remaining+1)
        mov r4,_rom_write_xbuf
        mov r5,(_rom_write_xbuf+1)
        mov a,r2
        jz 04210$
        inc r3
    04210$:
{pipeline_verify}    04200$:
        jb _flashgbx_bus_cancelled,04203$
        lcall _gbc_io_sample
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov b,a
        mov dpl,r4
        mov dph,r5
        movx a,@dptr
        inc dptr
        mov r4,dpl
        mov r5,dph
        xrl a,b
        jnz 04202$
        djnz r2,04200$
        djnz r3,04200$
        setb _gbc_io_matched
        sjmp 04204$
    04202$:
        mov dptr,#_cart_verify_actual
        mov a,b
        movx @dptr,a
    04203$:
        clr _gbc_io_matched
    04204$:
{restore}
        mov c,_gbc_io_matched
        ret
    __endasm;
}}

static void gbc_crc_kernel(void) __naked {{
    __asm
        .globl _crc_state
{save}
        mov r0,_rom_write_addr
        mov r1,(_rom_write_addr+1)
        mov r2,_rom_write_remaining
        mov r3,(_rom_write_remaining+1)
        mov a,r2
        jz 04300$
        inc r3
    04300$:
        mov r4,#0xff
        mov r5,#0xff
        mov r6,#0xff
        mov r7,#0xff
{pipeline_crc}    04301$:
        jb _flashgbx_bus_cancelled,04302$
        lcall _gbc_io_sample
        xrl a,r4
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl a,r5
        mov r4,a
        inc dph
        movx a,@dptr
        xrl a,r6
        mov r5,a
        inc dph
        movx a,@dptr
        xrl a,r7
        mov r6,a
        inc dph
        movx a,@dptr
        mov r7,a
        djnz r2,04301$
        djnz r3,04301$
    04302$:
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
# 数据页内部保持未选中期间的总线值，只在低地址进位时更新高地址。
# 控制字节及非 WR 接法沿用各自路径；全部 WR/CS/CLK 边沿和五时钟写等待保持。
begin=text.index('static void gbc_write_kernel(void)')
end=text.index('/* 输入 R0:R1',begin)
fast=text[begin:end].replace('gbc_write_kernel(void)','gbc_write_kernel_fast(void)')
high='''        mov a,r1
        movc a,@a+dptr
        mov _P7,a
'''
assert fast.count(high)==1
fast=fast.replace(high,'')
advance='''        inc r0
        cjne r0,#0,04402$
        inc r1
'''
assert fast.count(advance)==1
fast=fast.replace(advance,'')
fast=fast.replace('        mov a,r2\n', '        mov dptr,#_board_reverse_table\n'+high+'        mov a,r2\n',1)
idle='''        mov _P2,#0xff
        mov _P0,#0xff
        mov _P7,#0xff
'''
assert fast.count(idle)==1
fast=fast.replace(idle,'''        inc r0
        cjne r0,#0,04405$
        inc r1
        mov dptr,#_board_reverse_table
'''+high+'''    04405$:
''')
fast=fast.replace('    04404$:\n','    04404$:\n'+idle)
for index in range(6):fast=fast.replace(f'0440{index}$',f'0470{index}$')
text+=fast

pipeline_read=f'''        jb _flashgbx_dmg_pipeline,05900$
        ljmp 05800$
    05900$:
        jb _gbc_io_hold,05901$
        ljmp 05800$
    05901$:
        jnb _gbc_io_slow,05902$
        ljmp 05800$
    05902$:
        mov a,r1
        jnb acc.7,05903$
        ljmp 05800$
    05903$:
        lcall _gbc_io_sample
{cache_high}    05910$:
        jnb _flashgbx_bus_cancelled,05914$
        ljmp 05802$
    05914$:
        djnz r2,05911$
        djnz r3,05911$
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dpl,r4
        mov dph,r5
        movx @dptr,a
        ljmp 05802$
    05911$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dpl,r4
        mov dph,r5
        movx @dptr,a
        inc dptr
        mov r4,dpl
        mov r5,dph
{nops(27)}
    05912$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,05913$
        inc r1
{cache_high}    05913$:
        ljmp 05910$
'''

# 原生读回沿用gbc_io_sample的三种读法和CS脉冲，不缩短读等待。
text+=f'''static void gbc_read_kernel(void) __naked {{
    __asm
{save}
        mov r0,_rom_write_addr
        mov r1,(_rom_write_addr+1)
        mov r2,_rom_write_remaining
        mov r3,(_rom_write_remaining+1)
        mov r4,_rom_write_xbuf
        mov r5,(_rom_write_xbuf+1)
        mov a,r2
        jnz 05803$
        sjmp 05804$
    05803$:
        inc r3
    05804$:
{pipeline_read}    05800$:
        jb _flashgbx_bus_cancelled,05802$
        lcall _gbc_io_sample
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dpl,r4
        mov dph,r5
        movx @dptr,a
        inc dptr
        mov r4,dpl
        mov r5,dph
        djnz r2,05800$
        djnz r3,05800$
    05802$:
{restore}
        ret
    __endasm;
}}
'''
(ROOT/'src/gbc_kernels.h').write_text(text, encoding='utf-8')
