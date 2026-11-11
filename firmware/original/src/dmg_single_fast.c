/* 基于RC26原生通道；RC27增加每帧命令缓存。只接受原配置的单字节协议，数据不改位序。
   FAST_CODE固定在0xE400，前0x400字节属于CRC控制表，禁止重叠。 */
#include "main.h"
#include "fgx_state.h"
#include "cart_adapter.h"
uint8_t xdata fgx_single_ok;
#pragma codeseg FAST_CODE
uint8_t fgx_dmg_single_kernel(void) __naked {
    __asm
        ar0 = 0x00
        ar1 = 0x01
        ar2 = 0x02
        ar3 = 0x03
        ar4 = 0x04
        ar5 = 0x05
        ar6 = 0x06
        ar7 = 0x07
        push acc
        push b
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar2
        push ar3
        push ar4
        push ar5
        push ar6
        push ar7
        mov dptr,#_fgx_single_ok
        clr a
        movx @dptr,a
        mov dptr,#_fgx_state
        movx a,@dptr
        mov r0,a
        inc dptr
        movx a,@dptr
        mov r1,a
        mov dptr,#(_fgx_state+8)
        movx a,@dptr
        mov r2,a
        inc dptr
        movx a,@dptr
        mov r3,a
        mov a,r2
        jz 05500$
        inc r3
    05500$:
        mov r4,#<_cmdBuf
        mov r5,#>_cmdBuf
    05501$:
        mov _P0M0,#0xff
        jnb _flashgbx_bus_cancelled,05601$
        ljmp 05580$
    05601$:
        clr _P61
        setb _P16
        mov dptr,#(_fgx_state+40)
        movx a,@dptr
        mov _P7,a
        mov dptr,#(_fgx_state+41)
        movx a,@dptr
        mov _P2,a
        mov dptr,#(_fgx_state+44)
        movx a,@dptr
        mov _P0,a
        nop
        nop
        nop
        nop
        nop
        mov b,_IE2
        anl _IE2,#0x7f
        jnb _flashgbx_bus_cancelled,05701$
        mov _IE2,b
        ljmp 05580$
    05701$:
        clr _P16
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        setb _P16
        setb _P61
        mov _IE2,b
        jnb _flashgbx_bus_cancelled,05602$
        ljmp 05580$
    05602$:
        clr _P61
        setb _P16
        mov dptr,#(_fgx_state+46)
        movx a,@dptr
        mov _P7,a
        mov dptr,#(_fgx_state+47)
        movx a,@dptr
        mov _P2,a
        mov dptr,#(_fgx_state+50)
        movx a,@dptr
        mov _P0,a
        nop
        nop
        nop
        nop
        nop
        mov b,_IE2
        anl _IE2,#0x7f
        jnb _flashgbx_bus_cancelled,05702$
        mov _IE2,b
        ljmp 05580$
    05702$:
        clr _P16
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        setb _P16
        setb _P61
        mov _IE2,b
        jnb _flashgbx_bus_cancelled,05603$
        ljmp 05580$
    05603$:
        clr _P61
        setb _P16
        mov dptr,#(_fgx_state+52)
        movx a,@dptr
        mov _P7,a
        mov dptr,#(_fgx_state+53)
        movx a,@dptr
        mov _P2,a
        mov dptr,#(_fgx_state+56)
        movx a,@dptr
        mov _P0,a
        nop
        nop
        nop
        nop
        nop
        mov b,_IE2
        anl _IE2,#0x7f
        jnb _flashgbx_bus_cancelled,05703$
        mov _IE2,b
        ljmp 05580$
    05703$:
        clr _P16
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        setb _P16
        setb _P61
        mov _IE2,b
        mov dpl,r4
        mov dph,r5
        movx a,@dptr
        inc dptr
        mov r4,dpl
        mov r5,dph
        mov r6,a
        jnb _flashgbx_bus_cancelled,05604$
        ljmp 05580$
    05604$:
        clr _P61
        setb _P16
        mov a,r0
        mov _P7,a
        mov a,r1
        mov _P2,a
        mov a,r6
        mov _P0,a
        nop
        nop
        nop
        nop
        nop
        mov b,_IE2
        anl _IE2,#0x7f
        jnb _flashgbx_bus_cancelled,05704$
        mov _IE2,b
        ljmp 05580$
    05704$:
        clr _P16
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        setb _P16
        setb _P61
        mov _IE2,b
        mov _P0,#0xff
        mov _P0M0,#0
        clr _P16
        clr _P61
        clr 0xd5
        mov r7,#0x20
        mov b,#0x4f
    05530$:
        jnb _flashgbx_bus_cancelled,05531$
        ljmp 05580$
    05531$:
        clr _P61
        lcall _gbc_io_sample
        setb _P60
        setb _P61
        dec r0
        cjne r0,#0xff,05532$
        dec r1
    05532$:
        cjne a,ar6,05533$
        jb 0xd5,05540$
        setb 0xd5
        sjmp 05534$
    05533$:
        clr 0xd5
    05534$:
        djnz r7,05530$
        djnz b,05530$
        mov dptr,#_cart_verify_actual
        movx @dptr,a
        inc r0
        cjne r0,#0,05580$
        inc r1
        sjmp 05580$
    05540$:
        setb _P61
        setb _P60
        inc r0
        cjne r0,#0,05541$
        inc r1
    05541$:
        djnz r2,05542$
        djnz r3,05542$
        mov dptr,#_fgx_single_ok
        mov a,#1
        movx @dptr,a
        sjmp 05580$
    05542$:
        ljmp 05501$
    05580$:
        mov b,_IE2
        anl _IE2,#0x7f
        jb _flashgbx_bus_cancelled,05581$
        mov dptr,#_fgx_state
        mov a,r0
        movx @dptr,a
        inc dptr
        mov a,r1
        movx @dptr,a
        inc dptr
        clr a
        movx @dptr,a
        inc dptr
        movx @dptr,a
    05581$:
        mov _IE2,b
        pop ar7
        pop ar6
        pop ar5
        pop ar4
        pop ar3
        pop ar2
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop b
        pop acc
        mov dptr,#_fgx_single_ok
        movx a,@dptr
        mov dpl,a
        ret
    __endasm;
}
