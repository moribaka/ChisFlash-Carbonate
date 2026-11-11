#include "main.h"
#include "cart_adapter.h"
#ifdef FLASHGBX_COMPAT
#include "flashgbx.h"
#include "fgx_state.h"
#define CANCEL_WRITE() if (flashgbx_bus_cancelled) break
#else
#define CANCEL_WRITE()
#endif

#if defined(__SDCC) && defined(ROM_WRITE_DATA_FAST)
// 卡带总线仅由主循环调用；工作区不会被 USB/Timer4 中断使用。
// 指针仍保留通用地址空间类型，避免改变调用方的缓冲区语义。
static uint32_t __data rom_write_addr;
static uint8_t * __data rom_write_buf;
static uint16_t __data rom_write_remaining;
#ifdef ROM_WRITE_XDATA_FAST
static uint8_t __xdata * __data rom_write_xbuf;
#endif

#ifdef ROM_WRITE_ADDR_FAST
// 卡带内部递增低 16 位；只有低位进位时才更新外部高位地址。
#define ROM_WRITE_ADVANCE_ADDRESS() do { \
    ++KEIL_BYTE32(rom_write_addr, 3); \
    if (KEIL_BYTE32(rom_write_addr, 3) == 0) { \
        ++KEIL_BYTE32(rom_write_addr, 2); \
        if (KEIL_BYTE32(rom_write_addr, 2) == 0) { \
            ++KEIL_BYTE32(rom_write_addr, 1); \
            if (KEIL_BYTE32(rom_write_addr, 1) == 0) { \
                ++KEIL_BYTE32(rom_write_addr, 0); \
            } \
            CART_WRITE_A(KEIL_BYTE32(rom_write_addr, 1)); \
        } \
    } \
} while (0)
#else
#define ROM_WRITE_ADVANCE_ADDRESS() do { \
    rom_write_addr++; \
    CART_WRITE_A(KEIL_BYTE32(rom_write_addr, 1)); \
} while (0)
#endif

// 两种缓冲区读取路径共用同样的写脉冲、等待和地址进位。
#define ROM_WRITE_FINISH_WORD() do { \
    CART_WR = 0; \
    NOP3(); \
    NOP2(); \
    CART_WR = 1; \
    ROM_WRITE_ADVANCE_ADDRESS(); \
    rom_write_remaining--; \
} while (0)
#endif

#if defined(__SDCC) && defined(ROM_WRITE_XDATA_FAST) && defined(FLASHGBX_COMPAT)
#include "rom_stream_kernels.h"
/* STC 的 MOVX @Ri 固定访问低 256 字节；构建器将普通 XRAM 分配从 0x40 开始。 */
uint8_t __xdata __at (0x0000) fgx_read_page[64];
/* 位反转表位于独立RAM页；CRC仍使用0x1c00页，普通变量不得占用。 */
uint8_t __xdata __at (0x1b00) board_reverse_ram[256];
static uint8_t xdata reverse_ram_ready;
uint8_t cart_reverse_table_prepare(void) __reentrant {
    uint16_t i;
    if(reverse_ram_ready) return 1;
    for(i=0;i<256;i++) {
        if(flashgbx_bus_cancelled) return 0;
        board_reverse_ram[i]=board_reverse_table[i];
    }
    reverse_ram_ready=1;
    return 1;
}
void cart_romRead_stream_begin(uint32_t addr)
{
#ifdef FLASHGBX_COMPAT
    if(!cart_reverse_table_prepare()) return;
#endif
    
    CART_RD=1; CART_CS1=1;
    cart_gbaRomAddrSetup(addr);
    cart_setDirection_ad(0);
}
void cart_romRead_stream_end(void)
{
    CART_RD=1; CART_CS1=1; 
    cart_setDirection_a(0);
}
/* 连续读取 32 字。先采样后释放 RD，将查表和搬运排入 RD 高电平期间。
   高电平期间至少十条指令；低电平的五个 NOP 覆盖旧路径的两个 NOP 和查表地址装载。 */
void cart_romRead_stream64(void) __naked
{
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar5
        push ar6
        push ar7
        mov r0,#_fgx_read_page
        mov r7,#32
        mov dph,#>_board_reverse_ram
    00971$:
        jb _flashgbx_bus_cancelled,00972$
        clr _P61
        nop
        nop
        nop
        nop
        nop
        mov r5,_P0
        mov r6,_P7
        setb _P61
        mov dpl,r5
        movx a,@dptr
        movx @r0,a
        inc r0
        mov dpl,r6
        movx a,@dptr
        movx @r0,a
        inc r0
        djnz r7,00971$
    00972$:
        pop ar7
        pop ar6
        pop ar5
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
static bit rom_stream_fast_fits(uint32_t address,uint16_t count) FGX_STACK {
    uint16_t low=(uint16_t)address;
    if(!count || count>256) return 0;
    return !low || count<=(uint16_t)(0-low);
}
#endif

void cart_setDirection_ad(uint8_t dir)
{
    if (dir == 0)
    {
        SetP7nQuasiMode(PIN_ALL); // 新板 AD8..15
        SetP0nQuasiMode(PIN_ALL); // 新板 AD0..7
        PORT_AD_L = 0xff;
        PORT_AD_H = 0xff;
    }
    else
    {
        SetP7nPushPullMode(PIN_ALL); // 新板 AD8..15
        SetP0nPushPullMode(PIN_ALL); // 新板 AD0..7
    }
}

void cart_setDirection_a(uint8_t dir)
{
    if (dir == 0)
    {
        SetP2nQuasiMode(PIN_ALL);
        PORT_A = 0xff;
    }
    else
    {
        SetP2nPushPullMode(PIN_ALL);
    }
}

// uint16_t cart_readBus_ad()
// {
//     // return (GPIOB->IDR) & 0x0000ffff;
// }

// uint8_t cart_readBus_a()
// {
//     // return (GPIOA->IDR) & 0x000000ff;
// }

// void cart_writeBus_ad(uint16_t value)
// {
//     // GPIOB->ODR = value;
// }

// void cart_writeBus_a(uint8_t value)
// {
//     // uint32_t _ = GPIOA->ODR;
//     // GPIOA->ODR = (_ & 0xffffff00) | value;
// }
void cart_gbaRomAddrSetup(uint32_t addr)
{
    // latch base address
    cart_setDirection_a(1);
    cart_setDirection_ad(1);

    CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
    CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));
    CART_WRITE_A(KEIL_BYTE32(addr, 1));

    NOP2();       // 44ns
    CART_CS1 = 0; // cs1=0
}

void cart_romRead_continuous(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    // read bus
    cart_setDirection_ad(0);
    for (i = 0; i < len; i++)
    {
        // NOP6(); // tACC >110ns 132ns 可以放到后面？
        addr++; // tacc >110ns 这里有12周期 264ns 当做tacc了

        CART_RD = 0; // rd=0
        NOP2();      // tOE >25ns, 44ns

        *buf = CART_READ_AD_L();
        buf++;
        *buf = CART_READ_AD_H();
        buf++;

        CART_RD = 1; // rd=1

        CART_WRITE_A(KEIL_BYTE32(addr, 1));
    }
}

void cart_romRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    

    cart_gbaRomAddrSetup(addr);
    cart_romRead_continuous(addr, buf, len);

    // release bus
    CART_CS1 = 1; // cs1=1
    
    cart_setDirection_a(0);
}

void cart_romWrite_continuous(uint32_t addr, uint8_t *buf, uint16_t len)
{
#if defined(__SDCC) && defined(ROM_WRITE_DATA_FAST)
    rom_write_addr = addr;
    rom_write_buf = buf;
    rom_write_remaining = len;
    while (rom_write_remaining != 0)
    {
        CANCEL_WRITE();
        CART_WRITE_AD_L(*rom_write_buf);
        rom_write_buf++;
        CART_WRITE_AD_H(*rom_write_buf);
        rom_write_buf++;

        ROM_WRITE_FINISH_WORD();
    }
#else
    uint16_t i;
    // write bus
    // NOP5(); // twc >100ns 110ns
    for (i = 0; i < len; i++)
    {
        CART_WRITE_AD_L(*buf);
        buf++;
        CART_WRITE_AD_H(*buf);
        buf++;

        CART_WR = 0;
        NOP3(); // data setup >45ns 66ns
        NOP2(); // twp >35ns 44ns
        CART_WR = 1;
        // NOP5(); // twc >100ns 110ns 可以取消？

        addr++;
        CART_WRITE_A(KEIL_BYTE32(addr, 1));
    }
#endif
}

void cart_romWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    

    cart_gbaRomAddrSetup(addr);
    cart_romWrite_continuous(addr, buf, len);

    // release bus
    CART_CS1 = 1; // cs1=1
    
    cart_setDirection_a(0);
    cart_setDirection_ad(0);
}

#if defined(__SDCC) && defined(ROM_WRITE_XDATA_FAST)
uint8_t __xdata cart_verify_actual;
#ifdef FLASHGBX_COMPAT
static uint8_t xdata rom_crc_flags;
uint8_t __xdata __at (0x1c00) crc_ram_table[1024];
extern const uint8_t __code __at (0xe000) crc32_wire0[256];
extern const uint8_t __code __at (0xe100) crc32_wire1[256];
extern const uint8_t __code __at (0xe200) crc32_wire2[256];
extern const uint8_t __code __at (0xe300) crc32_wire3[256];
static bit crc_tables_ready;
static bit cart_crc_tables_prepare(void) {
    uint16_t i;
    if(crc_tables_ready) return 1;
    for(i=0;i<256;i++) {
        if(flashgbx_bus_cancelled) return 0;
        crc_ram_table[i]=crc32_wire0[i];
        crc_ram_table[256+i]=crc32_wire1[i];
        crc_ram_table[512+i]=crc32_wire2[i];
        crc_ram_table[768+i]=crc32_wire3[i];
    }
    crc_tables_ready=1;
    return 1;
}
#include "rom_crc_legacy_kernel.h"
#include "rom_crc_kernel.h"
bit cart_romCRC32(uint32_t addr,uint32_t length) FGX_STACK {
    uint32_t count,boundary;
    uint8_t mode=flashgbx_crc_fast_mode,i;
    if(!length) {
        crc_state[0]=0xff;crc_state[1]=0xff;crc_state[2]=0xff;crc_state[3]=0xff;
        return !flashgbx_bus_cancelled;
    }
    if(mode!=2) {
        
        cart_gbaRomAddrSetup(addr);cart_setDirection_ad(0);
        rom_write_addr=addr;
        rom_write_remaining=(uint16_t)(length>>2);
        rom_crc_flags=(length>=4?2:0)|(uint8_t)(length&1)|(length&2?4:0);
        cart_rom_crc_legacy_kernel();
        CART_RD=1;CART_CS1=1;cart_setDirection_a(0);
        return !flashgbx_bus_cancelled;
    }
    if(mode==2 && !cart_crc_tables_prepare()) return 0;
    crc_state[0]=0xff;crc_state[1]=0xff;crc_state[2]=0xff;crc_state[3]=0xff;
    while(length) {
        if(flashgbx_bus_cancelled) return 0;
        /* 分段保证内核不跨卡带的低 16 位自动递增边界，省去每字地址运算。 */
        boundary=(0x10000UL-(uint16_t)addr)<<1;
        count=length<boundary?length:boundary;
        
        cart_gbaRomAddrSetup(addr);cart_setDirection_ad(0);
        rom_write_remaining=(uint16_t)(count>>5);
        rom_crc_flags=(count>=32?0x80:0)|(uint8_t)(count&31);
        cart_rom_crc_ram_kernel();
        CART_RD=1;CART_CS1=1;cart_setDirection_a(0);
        if(flashgbx_bus_cancelled) return 0;
        addr+=(count+1)>>1;
        length-=count;
    }
    for(i=0;i<4;i++) crc_state[i]=board_reverse_table[crc_state[i]];
    return !flashgbx_bus_cancelled;
}
#endif
/* 逐字节读回整页并直接对照 USB 缓冲，省去中转缓冲的写入和二次读取。 */
bit cart_romVerify_data(uint32_t addr, uint8_t __xdata *expected, uint16_t len)
{
#ifdef FLASHGBX_COMPAT
    if(!cart_reverse_table_prepare()) return 0;
#endif
    uint8_t matched=1,value;
    
    cart_gbaRomAddrSetup(addr);
    cart_setDirection_ad(0);
    rom_write_addr=addr;
    rom_write_xbuf=expected;
    rom_write_remaining=len;
#ifdef FLASHGBX_COMPAT
    if(rom_stream_fast_fits(addr,len)) {
        rom_verify_match=1;
        if(flashgbx_program_fast_mode) rom_kernel_verify_pipeline((uint8_t)len);
        else rom_kernel_verify((uint8_t)len);
        matched=rom_verify_match;
    } else
#endif
    while(rom_write_remaining) {
#ifdef FLASHGBX_COMPAT
        if(flashgbx_bus_cancelled) { matched=0; break; }
#endif
        NOP10(); CART_RD=0; NOP2();
        value=CART_READ_AD_L();
        if(value!=*rom_write_xbuf++) { cart_verify_actual=value; matched=0; break; }
        value=CART_READ_AD_H();
        if(value!=*rom_write_xbuf++) { cart_verify_actual=value; matched=0; break; }
        CART_RD=1;
        ROM_WRITE_ADVANCE_ADDRESS();
        --rom_write_remaining;
    }
    CART_RD=1; CART_CS1=1; 
    cart_setDirection_a(0);
    return matched!=0;
}

/* 读写总线只在主循环使用，复用工作区以免挤占中断栈。
   保留明确的地址稳定等待和 /RD 低电平等待；仅消除通用指针与高地址重复更新。 */
void cart_romRead_data(uint32_t addr, uint8_t __xdata *buf, uint16_t len)
{
#ifdef FLASHGBX_COMPAT
    if(!cart_reverse_table_prepare()) return;
#endif
    
    cart_gbaRomAddrSetup(addr);
    cart_setDirection_ad(0);
    rom_write_addr = addr;
    rom_write_xbuf = buf;
    rom_write_remaining = len;
#ifdef FLASHGBX_COMPAT
    if(rom_stream_fast_fits(addr,len)) rom_kernel_read((uint8_t)len);
    else
#endif
    while (rom_write_remaining != 0)
    {
        CANCEL_WRITE();
        NOP10();
        CART_RD = 0;
        NOP2();
        *rom_write_xbuf++ = CART_READ_AD_L();
        *rom_write_xbuf++ = CART_READ_AD_H();
        CART_RD = 1;
        ROM_WRITE_ADVANCE_ADDRESS();
        --rom_write_remaining;
    }
    CART_CS1 = 1;
    
    cart_setDirection_a(0);
}

// 仅供已知位于 XRAM 的 USB 数据缓冲区使用；通用指针接口保留。
void cart_romWrite_data(uint32_t addr, uint8_t __xdata *buf, uint16_t len)
{
#ifdef FLASHGBX_COMPAT
    if(!cart_reverse_table_prepare()) return;
#endif
    
    cart_gbaRomAddrSetup(addr);

    rom_write_addr = addr;
    rom_write_xbuf = buf;
    rom_write_remaining = len;
#ifdef FLASHGBX_COMPAT
    if(rom_stream_fast_fits(addr,len)) {
        if(flashgbx_program_fast_mode) rom_kernel_write_pipeline((uint8_t)len);
        else rom_kernel_write((uint8_t)len);
    }
    else
#endif
    while (rom_write_remaining != 0)
    {
        CANCEL_WRITE();
        CART_WRITE_AD_L(*rom_write_xbuf);
        rom_write_xbuf++;
        CART_WRITE_AD_H(*rom_write_xbuf);
        rom_write_xbuf++;
        ROM_WRITE_FINISH_WORD();
    }

    CART_CS1 = 1;
    
    cart_setDirection_a(0);
    cart_setDirection_ad(0);
}
#endif

void cart_ramRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    

    cart_setDirection_a(0);
    cart_setDirection_ad(1);

    // read bus
    for (i = 0; i < len; i++)
    {
        CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
        CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));

        CART_CS2 = 0; // cs2=0
        CART_RD = 0;  // rd=0

        // taa 105ns toe 25ns
        NOP20(); // 440ns

        *buf = CART_READ_A();

        CART_RD = 1;  // rd=1
        CART_CS2 = 1; // cs2=1

        addr++;
        buf++;
    }
    cart_setDirection_ad(0);
    
}

void cart_ramWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    

    cart_setDirection_a(1);
    cart_setDirection_ad(1);

    // write bus
    for (i = 0; i < len; i++)
    {
        CANCEL_WRITE();
        CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
        CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));

        CART_CS2 = 0; // cs2=0

        CART_WRITE_A(*buf);

        CART_WR = 0;
        NOP20(); // 440ns
        CART_WR = 1;

        CART_CS2 = 1;

        addr++;
        buf++;
    }

    cart_setDirection_a(0);
    cart_setDirection_ad(0);
    
}

//
// for gb/gbc
//

void cart_gbcRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
#ifdef GBC_READ_NO_EXTRA_PULLUP
    uint8_t saved_pullup;
#endif

    
#ifdef FLASHGBX_COMPAT
    P16=0; /* Game Boy Camera 等卡带读取时要求 CLK 为低。 */
#endif
    cart_setDirection_a(0);
#ifdef GBC_READ_NO_EXTRA_PULLUP
    // 新板实测：额外上拉开启时 GB 读值不稳；只在读取期间关闭，随后恢复。
    saved_pullup = P2PU;
    DisableP2nPullUp(PIN_ALL);
#endif
    cart_setDirection_ad(1);

    CART_CS1 = 0; // cs1=0
#ifdef FLASHGBX_COMPAT
    if(fgx_state.byte[8]) CART_CS1=1;
#endif

    // read bus
    for (i = 0; i < len; i++)
    {
#ifdef FLASHGBX_COMPAT
        if(fgx_state.byte[8]) CART_CS1=1;
        if(fgx_state.byte[11] && !fgx_state.byte[8] && addr<0x8000) {
            CART_RD=0; P16=0;
            CART_WRITE_AD_L((uint8_t)addr); CART_WRITE_AD_H((uint8_t)(addr>>8));
            NOP40();
            if(fgx_state.byte[11]==2) { NOP40(); NOP40(); NOP40(); }
            *buf++=CART_READ_A(); CART_AD15=1; ++addr;
            continue;
        }
#endif
        CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
        CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));

        addr++; // tacc >110ns 这里有12周期 264ns 当做tacc了
        NOP5(); // twc >100ns 110ns 可以取消？ 我也不知道为什么要加这么多延时，但256n老是卡，本来可以更快的

#ifdef FLASHGBX_COMPAT
        if(fgx_state.byte[8]) CART_CS1=0;
#endif
        CART_RD = 0; // rd=0
        NOP5();      // tOE >25ns, twc >100ns 110ns
        *buf = CART_READ_A();
        CART_RD = 1; // rd=1
#ifdef FLASHGBX_COMPAT
        if(fgx_state.byte[8]) CART_CS1=1;
#endif

        buf++;
    }

    CART_RD = 1;
    CART_CS1 = 1; // cs1=1

#ifdef GBC_READ_NO_EXTRA_PULLUP
    P2PU = saved_pullup;
#endif
    cart_setDirection_ad(0);
    
}

void cart_gbcWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    
    cart_setDirection_a(1);
    cart_setDirection_ad(1);

    CART_CS1 = 0;

    // write bus
    for (i = 0; i < len; i++)
    {
        CANCEL_WRITE();
#ifdef FLASHGBX_COMPAT
        P16=1;
        if(fgx_state.byte[9]) CART_CS1=1;
#endif
        CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
        CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));
        CART_WRITE_A(*buf);

        addr++; // 这里有12周期 264ns
        NOP5(); // twc >100ns 110ns 可以取消？ 我也不知道为什么要加这么多延时，但256n老是卡，本来可以更快的

#ifdef FLASHGBX_COMPAT
        if(fgx_state.byte[9]) CART_CS1=0;
        P16=0;
#endif
        CART_WR = 0;
        NOP5(); // twp >35ns 110ns
        CART_WR = 1;
#ifdef FLASHGBX_COMPAT
        P16=1;
        if(fgx_state.byte[9]) CART_CS1=1;
#endif

        buf++;
    }

    CART_CS1 = 1;

    cart_setDirection_a(0);
    cart_setDirection_ad(0);
    
}

/* 普通 WR 接法的 Flash 缓冲数据阶段。每字节保留原来的 CS/CLK/WR 顺序，
   页内共用方向设置；高电平空闲时恢复原来的 FF 总线值。 */
#if defined(__SDCC) && defined(FLASHGBX_COMPAT)
#include "gbc_kernels.h"
void cart_gbcWrite_finish(void)
{
    cart_gbc_hold_writes=0;
    if(!gbc_write_open) return;
    CART_WR=1;CART_CS1=1;
    cart_setDirection_a(0);cart_setDirection_ad(0);
    gbc_write_open=0;
}
void cart_gbcWrite_block(uint16_t addr,const uint8_t __xdata *buf,uint16_t len)
{
    if(!gbc_write_open) {
        CART_WR=1;CART_RD=1;CART_CS1=1;
        cart_setDirection_a(1);cart_setDirection_ad(1);
        gbc_write_open=1;
    }
    if(len==1 && flashgbx_gbc_batch_fast) {
        /* 控制字节不启动通用页内核；WR、CLK、CS 和两个五时钟等待保持。 */
        if(!flashgbx_bus_cancelled) {
            CART_CS1=0;P16=1;
            if(fgx_state.byte[9]) CART_CS1=1;
            CART_WRITE_AD_L((uint8_t)addr);CART_WRITE_AD_H((uint8_t)(addr>>8));
            CART_WRITE_A(*buf);NOP5();
            if(fgx_state.byte[9]) CART_CS1=0;
            P16=0;CART_WR=0;NOP5();CART_WR=1;P16=1;CART_CS1=1;
            PORT_A=0xff;PORT_AD_L=0xff;PORT_AD_H=0xff;
        }
    } else if(len) {
        rom_write_addr=addr;rom_write_xbuf=(uint8_t __xdata *)buf;rom_write_remaining=len;
        gbc_io_write_pulse=fgx_state.byte[9]!=0;
        if(flashgbx_gbc_batch_fast) gbc_write_kernel_fast();
        else gbc_write_kernel();
    }
    if(!cart_gbc_hold_writes || flashgbx_bus_cancelled) cart_gbcWrite_finish();
}
static uint8_t gbc_io_begin(void) __reentrant
{
    uint8_t saved_pullup=P2PU;
    gbc_io_pulse=fgx_state.byte[8]!=0;
    gbc_io_hold=fgx_state.byte[11]!=0 && !gbc_io_pulse;
    gbc_io_slow=fgx_state.byte[11]==2;
    P16=0;cart_setDirection_a(0);
#ifdef GBC_READ_NO_EXTRA_PULLUP
    DisableP2nPullUp(PIN_ALL);
#endif
    cart_setDirection_ad(1);CART_CS1=0;
    if(gbc_io_pulse) CART_CS1=1;
    return saved_pullup;
}
static void gbc_io_end(uint8_t saved_pullup) __reentrant
{
    CART_RD=1;CART_CS1=1;
#ifdef GBC_READ_NO_EXTRA_PULLUP
    P2PU=saved_pullup;
#endif
    cart_setDirection_ad(0);
}
/* 普通WR逐字节编程使用上传的三条解锁命令，保留原配置及两次实际读回。 */
extern uint8_t fgx_dmg_single_kernel(void);
extern uint8_t xdata fgx_single_ok;
extern uint8_t xdata fgx_single_prefix[9];
bit cart_gbcProgram_single(void) __reentrant {
    uint8_t saved=gbc_io_begin();
    uint8_t reset=0xf0;
    uint8_t i;
    /* 只缓存本次传输上传的控制字节，不缓存ROM数据，不改变Flash写法。 */
    for(i=0;i<3;i++) {
        fgx_single_prefix[i*3+0]=board_reverse_table[(uint8_t)(fgx_state.commands[i].address)];
        fgx_single_prefix[i*3+1]=board_reverse_table[(uint8_t)(fgx_state.commands[i].address>>8)];
        fgx_single_prefix[i*3+2]=board_reverse_table[(uint8_t)(fgx_state.commands[i].value)];
    }
    
    cart_setDirection_a(1);
    fgx_dmg_single_kernel();
    CART_WR=1; cart_setDirection_a(0);
    gbc_io_end(saved);
    
    if(!fgx_single_ok && !flashgbx_cancelled()) {
        fgx_state.word[3]=cart_verify_actual;
        cart_gbcWrite(0,&reset,1);
    }
    return fgx_single_ok && !flashgbx_cancelled();
}
/* ROM原生读回使用相同采样等待；RAM和跨窗口请求仍由原接口处理。 */
void cart_gbcRead_block(uint16_t addr,uint8_t __xdata *buf,uint16_t len) __reentrant {
    uint8_t saved;
    if(!len) return;
    rom_write_addr=addr; rom_write_xbuf=buf; rom_write_remaining=len;
    saved=gbc_io_begin();
    gbc_read_kernel();
    gbc_io_end(saved);
}
bit cart_gbcVerify_block(uint16_t addr,const uint8_t __xdata *expected,uint16_t len) __reentrant
{
    uint8_t saved_pullup,matched;
    if(!len) return !flashgbx_bus_cancelled;
    rom_write_addr=addr;rom_write_xbuf=(uint8_t __xdata *)expected;rom_write_remaining=len;
    saved_pullup=gbc_io_begin();
    matched=gbc_verify_kernel();
    gbc_io_end(saved_pullup);
    return matched!=0 && !flashgbx_bus_cancelled;
}
bit cart_gbcCRC32(uint16_t addr,uint16_t len) __reentrant
{
    uint8_t saved_pullup,i;
    if(!cart_crc_tables_prepare()) return 0;
    crc_state[0]=crc_state[1]=crc_state[2]=crc_state[3]=0xff;
    if(!len) return !flashgbx_bus_cancelled;
    rom_write_addr=addr;rom_write_remaining=len;
    saved_pullup=gbc_io_begin();
    gbc_crc_kernel();
    gbc_io_end(saved_pullup);
    for(i=0;i<4;i++) crc_state[i]=board_reverse_table[crc_state[i]];
    return !flashgbx_bus_cancelled;
}

#endif
#ifdef GBC_READ_DIAGNOSTICS
// 只读排查入口：同一 RD 周期采样三次，不发写脉冲，不切换供电。
void cart_gbcProbe(uint32_t addr, uint8_t xdata *samples, uint8_t input_mode)
{
#ifdef GBC_PROBE_INPUT_MODES
    uint8_t saved_pullup;
    saved_pullup = P2PU;
#endif
    
    cart_setDirection_a(0);
#ifdef GBC_PROBE_INPUT_MODES
    if (input_mode & 1)
    {
        SetP2nHighZInputMode(PIN_ALL);
    }
    if (input_mode & 2)
    {
        DisableP2nPullUp(PIN_ALL);
    }
#endif
    cart_setDirection_ad(1);
    CART_CS1 = 0;
    CART_WRITE_AD_L(KEIL_BYTE32(addr, 3));
    CART_WRITE_AD_H(KEIL_BYTE32(addr, 2));
#ifdef GBC_PROBE_INPUT_MODES
    // 模式对照时给地址统一的保守建立时间，避免地址建立影响输入模式比较。
    NOP40(); NOP40();
#endif
    NOP5();
    CART_RD = 0;
    NOP5();
    samples[0] = PORT_A;
    NOP40();
    samples[1] = PORT_A;
    NOP40(); NOP40(); NOP40(); NOP40(); NOP40();
    samples[2] = PORT_A;
    samples[6] = P2M0;
    samples[7] = P2M1;
    samples[8] = AUXR;
    samples[9] = P2PU;
    samples[10] = P2IE;
    samples[11] = P_SW1;
    samples[12] = CLKDIV;
    samples[13] = P2NCS;
    samples[14] = P2PD;
    samples[15] = (CART_RD ? 1 : 0) | (CART_WR ? 2 : 0)
                  | (CART_CS1 ? 4 : 0) | (CART_CS2 ? 8 : 0);
    CART_RD = 1;
    CART_CS1 = 1;
#ifdef GBC_PROBE_INPUT_MODES
    P2PU = saved_pullup;
    cart_setDirection_a(0);
#endif
    cart_setDirection_ad(0);
    
}
#endif

#include "gba_single_fast.h"
