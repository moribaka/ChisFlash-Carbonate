/* 由 prepare_rom_crc_kernel.py 生成。CRC 字节保持物理数据线位序，输出时恢复。
   每轮处理两个字，轮换逻辑 CRC 字节所对应的寄存器，避免逐字节搬移。
   先采样再释放 RD；计算覆盖高电平等待，五个 NOP 保持低电平采样等待。
   表固定在四个连续代码页，低地址始终为零；所有工作寄存器均保存。 */
void cart_rom_crc_legacy_kernel(void) __naked {
    __asm
        .globl _crc_state
        .globl _crc32_wire0
        .globl _crc32_wire1
        .globl _crc32_wire2
        .globl _crc32_wire3
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
        mov r4,#0xff
        mov r5,#0xff
        mov r6,#0xff
        mov r7,#0xff
        mov r0,_rom_write_remaining
        mov r1,(_rom_write_remaining+1)
        mov a,r0
        jz 00979$
        inc r1
    00979$:
        mov dptr,#_rom_crc_flags
        movx a,@dptr
        mov r3,a
        mov dptr,#_crc32_wire0
        anl a,#2
        jnz 00980$
        ljmp 00984$
    00980$:
        jnb _flashgbx_bus_cancelled,00700$
        ljmp 00985$
    00700$:
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov a,_P0
        mov b,_P7
        setb _P61
        xrl a,r4
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar5,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r4,a
        mov a,b
        xrl a,r5
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar4,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r5,a
        inc _rom_write_addr
        mov a,_rom_write_addr
        jnz 00702$
        inc (_rom_write_addr+1)
        mov a,(_rom_write_addr+1)
        jnz 00702$
        inc (_rom_write_addr+2)
        mov a,(_rom_write_addr+2)
        jnz 00701$
        inc (_rom_write_addr+3)
    00701$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov _P2,a
        mov dptr,#_crc32_wire0
    00702$:

        jnb _flashgbx_bus_cancelled,00710$
        ljmp 00985$
    00710$:
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov a,_P0
        mov b,_P7
        setb _P61
        xrl a,r6
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar4,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar5,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r6,a
        mov a,b
        xrl a,r7
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar4,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar5,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r7,a
        inc _rom_write_addr
        mov a,_rom_write_addr
        jnz 00712$
        inc (_rom_write_addr+1)
        mov a,(_rom_write_addr+1)
        jnz 00712$
        inc (_rom_write_addr+2)
        mov a,(_rom_write_addr+2)
        jnz 00711$
        inc (_rom_write_addr+3)
    00711$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov _P2,a
        mov dptr,#_crc32_wire0
    00712$:

        djnz r0,00987$
        djnz r1,00987$
        ljmp 00984$
    00987$:
        ljmp 00980$
    00984$:
        mov a,r3
        anl a,#4
        jnz 00986$
        ljmp 00988$
    00986$:
        jnb _flashgbx_bus_cancelled,00720$
        ljmp 00985$
    00720$:
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov a,_P0
        mov b,_P7
        setb _P61
        xrl a,r4
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar5,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r4,a
        mov a,b
        xrl a,r5
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar4,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r5,a
        inc _rom_write_addr
        mov a,_rom_write_addr
        jnz 00722$
        inc (_rom_write_addr+1)
        mov a,(_rom_write_addr+1)
        jnz 00722$
        inc (_rom_write_addr+2)
        mov a,(_rom_write_addr+2)
        jnz 00721$
        inc (_rom_write_addr+3)
    00721$:
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov _P2,a
        mov dptr,#_crc32_wire0
    00722$:

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
        xrl a,r4
        mov r2,a
        mov dph,#>_crc32_wire0
        movc a,@a+dptr
        xrl ar5,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar6,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        xrl ar7,a
        mov a,r2
        inc dph
        movc a,@a+dptr
        mov r4,a
        mov a,r4
        mov ar4,ar5
        mov ar5,ar6
        mov ar6,ar7
        mov r7,a
    00985$:
        mov dptr,#_board_reverse_table
        mov a,r4
        movc a,@a+dptr
        mov r4,a
        mov a,r5
        movc a,@a+dptr
        mov r5,a
        mov a,r6
        movc a,@a+dptr
        mov r6,a
        mov a,r7
        movc a,@a+dptr
        mov r7,a
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
        ret
    __endasm;
}
