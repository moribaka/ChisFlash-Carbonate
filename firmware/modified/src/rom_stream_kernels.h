/* 由 prepare_rom_kernels.py 生成。
   调用方保证 1..256 字、有效采样期间不跨低 16 位地址边界；count=0 表示 256。
   保留所用寄存器、每字取消检查、读写等待；工作区在每次总线调用时重新初始化。 */
static uint8_t xdata rom_verify_match;
static void rom_kernel_write(uint8_t count) __naked {
    (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar5
        push ar6
        push ar7
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
    00981$:
        jb _flashgbx_bus_cancelled,00982$
        mov dpl,r0
        mov dph,r1
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph
        mov dph,#>_board_reverse_ram
        mov dpl,r5
        movx a,@dptr
        mov _P0,a
        mov dpl,r6
        movx a,@dptr
        mov _P7,a
        nop
        nop
        nop
        clr _P62
        nop
        nop
        nop
        nop
        nop
        setb _P62
        djnz r7,00981$
    00982$:
        pop ar7
        pop ar6
        pop ar5
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
static void rom_kernel_read(uint8_t count) __naked {
    (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar5
        push ar6
        push ar7
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
    00981$:
        jb _flashgbx_bus_cancelled,00982$
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
        clr _P61
        nop
        nop
        mov dph,#>_board_reverse_ram
        mov dpl,_P0
        movx a,@dptr
        mov r5,a
        mov dpl,_P7
        movx a,@dptr
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
        setb _P61
        djnz r7,00981$
    00982$:
        pop ar7
        pop ar6
        pop ar5
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
static void rom_kernel_verify(uint8_t count) __naked {
    (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar5
        push ar6
        push ar7
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
    00981$:
        jb _flashgbx_bus_cancelled,00986$
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
        clr _P61
        nop
        nop
        mov dpl,r0
        mov dph,r1
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph
        mov dph,#>_board_reverse_ram
        mov dpl,r5
        movx a,@dptr
        cjne a,_P0,00983$
        mov dpl,r6
        movx a,@dptr
        cjne a,_P7,00984$
        setb _P61
        djnz r7,00981$
        sjmp 00982$
    00983$:
        mov a,_P0
        sjmp 00985$
    00984$:
        mov a,_P7
    00985$:
        mov dpl,a
        mov dph,#>_board_reverse_ram
        movx a,@dptr
        mov dptr,#_cart_verify_actual
        movx @dptr,a
    00986$:
        mov dptr,#_rom_verify_match
        clr a
        movx @dptr,a
    00982$:
        pop ar7
        pop ar6
        pop ar5
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
/* 写脉冲期间预取下一字，数据引脚保持当前值；页末保留五个 NOP。
   指针准备覆盖数据建立等待；核对时用预计值装载覆盖 RD 高电平等待，
   用六条反序转换指令覆盖低电平采样等待。 */
static void rom_kernel_write_pipeline(uint8_t count) __naked {
    (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar5
        push ar6
        push ar7
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
        mov dpl,r0
        mov dph,r1
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph
    00991$:
        jb _flashgbx_bus_cancelled,00992$
        mov dph,#>_board_reverse_ram
        mov dpl,r5
        movx a,@dptr
        mov _P0,a
        mov dpl,r6
        movx a,@dptr
        mov _P7,a
        mov dpl,r0
        mov dph,r1
        nop
        clr _P62
        djnz r7,00993$
        nop
        nop
        nop
        nop
        nop
        setb _P62
        sjmp 00992$
    00993$:
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph
        setb _P62
        sjmp 00991$
    00992$:
        pop ar7
        pop ar6
        pop ar5
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
static void rom_kernel_verify_pipeline(uint8_t count) __naked {
    (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar5
        push ar6
        push ar7
        mov r7,dpl
        mov r0,_rom_write_xbuf
        mov r1,(_rom_write_xbuf + 1)
    00991$:
        jb _flashgbx_bus_cancelled,00996$
        mov dpl,r0
        mov dph,r1
        movx a,@dptr
        mov r5,a
        inc dptr
        movx a,@dptr
        mov r6,a
        inc dptr
        mov r0,dpl
        mov r1,dph
        mov dph,#>_board_reverse_ram
        clr _P61
        mov dpl,r5
        movx a,@dptr
        nop
        nop
        cjne a,_P0,00993$
        mov dpl,r6
        movx a,@dptr
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
        mov dpl,a
        mov dph,#>_board_reverse_ram
        movx a,@dptr
        mov dptr,#_cart_verify_actual
        movx @dptr,a
    00996$:
        mov dptr,#_rom_verify_match
        clr a
        movx @dptr,a
    00992$:
        pop ar7
        pop ar6
        pop ar5
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
