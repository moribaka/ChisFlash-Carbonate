"""生成不跨低 16 位地址边界、最多 256 字的总线内核。"""
from pathlib import Path
ROOT=Path(__file__).resolve().parent
registers=['acc','psw','dpl','dph','ar0','ar1','ar5','ar6','ar7']
save='\n'.join('        push '+r for r in registers)
restore='\n'.join('        pop '+r for r in reversed(registers))
load='''        mov dpl,r0
        mov dph,r1
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph'''
write=f'''{load}
        mov dptr,#_board_reverse_table
        mov a,r5
        movc a,@a+dptr
        mov _P0,a
        mov a,r6
        movc a,@a+dptr
        mov _P7,a
        nop
        nop
        nop
        clr _P62
{chr(10).join('        nop' for _ in range(5))}
        setb _P62'''
read_wait='\n'.join('        nop' for _ in range(10))+'\n        clr _P61\n        nop\n        nop'
read=f'''{read_wait}
        mov dptr,#_board_reverse_table
        mov a,_P0
        movc a,@a+dptr
        mov r5,a
        mov a,_P7
        movc a,@a+dptr
        mov r6,a
        mov dpl,r0
        mov dph,r1
        mov a,r5
        movx @dptr,a
        inc dptr
        mov a,r6
        movx @dptr,a
        inc dptr
        mov r0,dpl
        mov r1,dph
        setb _P61'''
verify=f'''{read_wait}
{load}
        mov dptr,#_board_reverse_table
        mov a,r5
        movc a,@a+dptr
        cjne a,_P0,00983$
        mov a,r6
        movc a,@a+dptr
        cjne a,_P7,00984$
        setb _P61'''
text='''/* 由 prepare_rom_kernels.py 生成。
   调用方保证 1..256 字、有效采样期间不跨低 16 位地址边界；count=0 表示 256。
   保留所用寄存器、每字取消检查、读写等待；工作区在每次总线调用时重新初始化。 */
static uint8_t xdata rom_verify_match;
'''
for name,body in [('write',write),('read',read),('verify',verify)]:
    tail=''
    if name=='verify':
        tail='''        sjmp 00982$
    00983$:
        mov a,_P0
        sjmp 00985$
    00984$:
        mov a,_P7
    00985$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dptr,#_cart_verify_actual
        movx @dptr,a
    00986$:
        mov dptr,#_rom_verify_match
        clr a
        movx @dptr,a
'''
    cancel='00986$' if name=='verify' else '00982$'
    text+=f'''static void rom_kernel_{name}(uint8_t count) __naked {{
    (void)count;
    __asm
{save}
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
    00981$:
        jb _flashgbx_bus_cancelled,{cancel}
{body}
        djnz r7,00981$
{tail}    00982$:
{restore}
        ret
    __endasm;
}}
'''
pipeline_write=f'''        mov dptr,#_board_reverse_table
        mov a,r5
        movc a,@a+dptr
        mov _P0,a
        mov a,r6
        movc a,@a+dptr
        mov _P7,a
        mov dpl,r0
        mov dph,r1
        nop
        clr _P62
        djnz r7,00993$
{chr(10).join('        nop' for _ in range(5))}
        setb _P62
        sjmp 00992$
    00993$:
{chr(10).join(load.splitlines()[2:])}
        setb _P62
        sjmp 00991$'''
pipeline_verify=f'''{load}
        mov dptr,#_board_reverse_table
        clr _P61
        mov a,r5
        movc a,@a+dptr
        cjne a,_P0,00993$
        mov a,r6
        movc a,@a+dptr
        cjne a,_P7,00994$
        setb _P61
        djnz r7,00991$
        sjmp 00992$
    00993$:
        mov a,_P0
        sjmp 00995$
    00994$:
        mov a,_P7
    00995$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov dptr,#_cart_verify_actual
        movx @dptr,a
    00996$:
        mov dptr,#_rom_verify_match
        clr a
        movx @dptr,a'''
text+='''/* 写脉冲期间预取下一字，数据引脚保持当前值；页末保留五个 NOP。
   指针准备覆盖数据建立等待；核对时用预计值装载覆盖 RD 高电平等待，
   用六条反序转换指令覆盖低电平采样等待。 */
'''
for name,body in [('write',pipeline_write),('verify',pipeline_verify)]:
    initial=load+'\n' if name=='write' else ''
    cancel='00996$' if name=='verify' else '00992$'
    text+=f'''static void rom_kernel_{name}_pipeline(uint8_t count) __naked {{
    (void)count;
    __asm
{save}
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
{initial}    00991$:
        jb _flashgbx_bus_cancelled,{cancel}
{body}
    00992$:
{restore}
        ret
    __endasm;
}}
'''
(ROOT/'src'/'rom_stream_kernels.h').write_text(text,encoding='utf-8')
