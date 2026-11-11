/* 由 prepare_gbc_kernels.py 生成；总线采样供逐字节核对和 CRC 共用。 */
bit flashgbx_dmg_pipeline=1;
bit flashgbx_gbc_batch_fast=1;
bit flashgbx_gbc_fast_timing=1;
bit cart_gbc_hold_writes;
static bit gbc_write_open;
static bit gbc_io_pulse,gbc_io_hold,gbc_io_slow,gbc_io_matched,gbc_io_write_pulse;

static void gbc_write_kernel(void) __naked {
    __asm
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

/* 输入 R0:R1 为原始地址；返回 A 为反序数据，地址加一。其它通用寄存器保持。 */
void gbc_io_sample(void) __naked {
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
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        jnb _gbc_io_slow,04102$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
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
}

/* C 包装负责总线方向及上拉；以下两段保存全部寄存器，允许 Timer/USB 中断。 */
static bit gbc_verify_kernel(void) __naked {
    __asm
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
        jb _flashgbx_dmg_pipeline,04900$
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
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
        clr _P61
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
        nop
        nop
        nop
        jb _flashgbx_gbc_fast_timing,04604$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    04604$:
        mov a,_P2
        setb _P70
        cjne a,ar6,04602$
        inc r0
        cjne r0,#0,04601$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    04601$:
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
    04200$:
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
        mov c,_gbc_io_matched
        ret
    __endasm;
}

static void gbc_crc_kernel(void) __naked {
    __asm
        .globl _crc_state
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
        jb _flashgbx_dmg_pipeline,04800$
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
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
        ljmp 06000$
    06000$:
        jnb _flashgbx_bus_cancelled,06001$
        ljmp 04302$
    06001$:
        djnz r2,06002$
        djnz r3,06002$
        xrl a,r4
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        mov r4,a
        ljmp 06101$
    06002$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
        xrl a,r4
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        mov r4,a
        jb _flashgbx_gbc_fast_timing,06003$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    06003$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,06004$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    06004$:
        ljmp 06020$
    06020$:
        jnb _flashgbx_bus_cancelled,06021$
        ljmp 04302$
    06021$:
        djnz r2,06022$
        djnz r3,06022$
        xrl a,r5
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        mov r5,a
        ljmp 06102$
    06022$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
        xrl a,r5
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        mov r5,a
        jb _flashgbx_gbc_fast_timing,06023$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    06023$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,06024$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    06024$:
        ljmp 06040$
    06040$:
        jnb _flashgbx_bus_cancelled,06041$
        ljmp 04302$
    06041$:
        djnz r2,06042$
        djnz r3,06042$
        xrl a,r6
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        mov r6,a
        ljmp 06103$
    06042$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
        xrl a,r6
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar7,a
        inc dph
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        mov r6,a
        jb _flashgbx_gbc_fast_timing,06043$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    06043$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,06044$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    06044$:
        ljmp 06060$
    06060$:
        jnb _flashgbx_bus_cancelled,06061$
        ljmp 04302$
    06061$:
        djnz r2,06062$
        djnz r3,06062$
        xrl a,r7
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        mov r7,a
        ljmp 06100$
    06062$:
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        clr _P70
        mov a,b
        xrl a,r7
        mov dpl,a
        mov dph,#>_crc_ram_table
        movx a,@dptr
        xrl ar4,a
        inc dph
        movx a,@dptr
        xrl ar5,a
        inc dph
        movx a,@dptr
        xrl ar6,a
        inc dph
        movx a,@dptr
        mov r7,a
        jb _flashgbx_gbc_fast_timing,06063$
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    06063$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,06064$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    06064$:
        ljmp 06000$
    06100$:
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
    04301$:
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
static void gbc_write_kernel_fast(void) __naked {
    __asm
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
        mov r0,_rom_write_addr
        mov r1,(_rom_write_addr+1)
        mov r2,_rom_write_remaining
        mov r3,(_rom_write_remaining+1)
        mov r4,_rom_write_xbuf
        mov r5,(_rom_write_xbuf+1)
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        mov _P7,a
        mov a,r2
        jz 04700$
        inc r3
    04700$:
        jb _flashgbx_bus_cancelled,04704$
        clr _P60
        setb _P16
        jnb _gbc_io_write_pulse,04701$
        setb _P60
    04701$:
        mov dptr,#_board_reverse_table
        mov a,r0
        movc a,@a+dptr
        mov _P0,a
        mov dpl,r4
        mov dph,r5
        movx a,@dptr
        inc dptr
        mov r4,dpl
        mov r5,dph
        mov dptr,#_board_reverse_table
        movc a,@a+dptr
        mov _P2,a
    04702$:
        nop
        nop
        nop
        nop
        nop
        jnb _gbc_io_write_pulse,04703$
        clr _P60
    04703$:
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
        inc r0
        cjne r0,#0,04705$
        inc r1
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        mov _P7,a
    04705$:
        djnz r2,04700$
        djnz r3,04700$
    04704$:
        mov _P2,#0xff
        mov _P0,#0xff
        mov _P7,#0xff
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

static void gbc_read_kernel(void) __naked {
    __asm
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
        jb _flashgbx_dmg_pipeline,05900$
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
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    05910$:
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
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    05912$:
        mov a,_P2
        setb _P70
        inc r0
        cjne r0,#0,05913$
        inc r1
        mov b,a
        mov dptr,#_board_reverse_table
        mov a,r1
        movc a,@a+dptr
        orl a,#0x01
        mov _P7,a
        mov a,b
    05913$:
        ljmp 05910$
    05800$:
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
