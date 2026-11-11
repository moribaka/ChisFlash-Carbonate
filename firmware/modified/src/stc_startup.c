#include "main.h"

#ifdef __SDCC
// 在 C 全局变量赋初值之前清零片内 8KB XRAM。
// STC 片内 MOVX @Ri 只有 8 位地址，清零必须使用 16 位 DPTR。
// 配合 --no-xinit-opt，让后续变量赋初值也使用 DPTR。
uint8_t __sdcc_external_startup(void) __naked
{
    __asm
        anl _AUXR, #0xfd
        mov dptr, #0x0000
        mov r0, #0x20
        mov r1, #0x00
        clr a
    00001$:
        movx @dptr, a
        inc dptr
        djnz r1, 00001$
        djnz r0, 00001$
        mov dpl, #0x00
        ret
    __endasm;
}
#endif
