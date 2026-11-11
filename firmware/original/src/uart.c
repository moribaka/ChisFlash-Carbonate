#include <string.h>
#include "main.h"
#include "usb.h"
#include "uart.h"
#include "cart_adapter.h"
#ifdef FLASHGBX_COMPAT
#include "flashgbx.h"
#define UART_RX_EP_INDEX 4
#define UART_RX_FIFO FIFO4
#else
#define UART_RX_EP_INDEX 1
#define UART_RX_FIFO FIFO1
#endif
#define UART_EP_INDEX 1
#define UART_FIFO FIFO1

#define SIZE_CMD_HEADER 3
#define SIZE_RESPON_HEADER 2
#define SIZE_BASE_ADDRESS 4
#define SIZE_BYTE_COUNT 2
#define SIZE_CRC 2
#define SIZE_BUFF_SIZE 2
#define SIZE_LATENCY 1

// // 命令头
// typedef struct
// {
//     uint16_t cmdSize;
//     uint8_t cmdCode;
//     uint8_t payload[]; // 最后两个字节是crc
// } Desc_cmdHeader_t;

// // 命令身 写
// typedef struct
// {
//     uint32_t baseAddress;
//     uint8_t payload[]; // 最后两个字节是crc
// } Desc_cmdBody_write_t;

// // 命令身 读
// typedef struct
// {
//     uint32_t baseAddress;
//     uint16_t readSize;
//     uint16_t crc16;
// } Desc_cmdBody_read_t;

// // 响应包
// typedef struct
// {
//     uint16_t crc16;
//     uint8_t payload[];
// } Desc_respon_t;

uint16_t cmdBuf_i_wr = 0;
uint16_t cmdBuf_i_rd = 0;
uint8_t xdata cmdBuf[5500];

uint8_t xdata responCrc[2] = {0, 0};
uint8_t xdata responBuf[EP1IN_SIZE];

BOOL cmdEnd = 0;
BOOL currentRts = 0;
BOOL currentDtr = 0;

/* 整帧执行期间保持 OUT 端点关闭，不能让下一帧覆盖当前缓冲。 */
static volatile bit legacy_running;
#ifdef __SDCC
#define UART_STACK __reentrant
#else
#define UART_STACK
#endif

// Desc_cmdHeader_t *uart_cmd[0] = (Desc_cmdHeader_t *)cmdBuf;
void endpointClear(void) UART_STACK;
void uart_clearRecvBuf();

#ifdef FLASHGBX_COMPAT
static bit flashgbx_is_probe_byte(uint8_t value)
{
    return value == 0x44 || value == 0x55 || value == 0x68 ||
           value == 0x69 || value == 0x6A || value == 0x6B ||
           value == 0x6C || value == 0xAA || value == 0x4B ||
           value == 0x4C || value == 0xFE || value == 0xF1;
}
#endif

void romGetID();
void romEraseChip();
// void romEraseBlock();
// void romEraseSector();
void romProgram();
void romWrite();
void romRead();
void ramWrite();
void ramRead();
void ramProgramFlash();
void ramWrite_forFram();
void ramRead_forFram();

void gbcWrite();
void gbcRead();
void gbcRomProgram();
void gbcWrite_forFram();
void gbcRead_forFram();
#ifdef GBC_READ_DIAGNOSTICS
void gbcBusProbe();
#endif

void cart_power();
void cart_phi();

// 流控回调
void uart_setControlLine(BOOL rts, BOOL dtr)
{
    if (((currentRts == 0) && (rts != 0)) ||
        ((currentDtr == 0) && (dtr != 0))
#ifdef FLASHGBX_COMPAT
        || (currentRts != rts) || (currentDtr != dtr)
#endif
        )
    {
        LED_ACT = 1;
        legacy_running = 0;
        cmdBuf_i_wr = 0;
        cmdBuf_i_rd = 0;
        // memset(cmdBuf, 0, sizeof(cmdBuf));
        
        
        CART_CS1 = 1;
        CART_CS2 = 1;
        CART_RD = 1;
        CART_WR = 1;
        cmdEnd = 1;
#ifdef FLASHGBX_COMPAT
        flashgbx_reset();
#endif

        usb_write_reg(INDEX, UART_EP_INDEX);
#ifdef FLASHGBX_COMPAT
        /* CDC 控制线变化不重置主机的数据序号，清 FIFO 时必须保留 DATA0/DATA1。
           只有 USB 复位或 CLEAR_FEATURE 等标准请求才能同步重置序号。 */
        usb_write_reg(INCSR1, INFLUSH);
        usb_write_reg(INDEX, UART_RX_EP_INDEX);
        usb_write_reg(OUTCSR1, OUTFLUSH);
#endif
        usb_write_reg(OUTCSR1, 0);
    }

    currentRts = rts;
    currentDtr = dtr;
}

// usb 接收回调
#if defined(__SDCC) && defined(USB_RX_PACKET_FAST) && defined(FLASHGBX_COMPAT)
/* 仅在 USB ISR 内调用。DPTR 连续复制 FIFO1 到 XRAM，保留两次外设忙检查。
   内嵌汇编显式保留所用寄存器，不依赖编译器推断修改集合。 */
static void uart_fifo_to_xram(uint8_t xdata *target,uint8_t count) __naked
{
    (void)target; (void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar7
        mov r0,dpl
        mov r1,dph
        mov dptr,#_uart_fifo_to_xram_PARM_2
        movx a,@dptr
        mov r7,a
        mov dpl,r0
        mov dph,r1
        jz 00983$
    00981$:
        mov a,_USBADR
        jb acc.7,00981$
        mov _USBADR,#0xa4
    00982$:
        mov a,_USBADR
        jb acc.7,00982$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        djnz r7,00981$
    00983$:
        pop ar7
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
#endif
/* 复制期间由 USB ISR 独占间接接口；入口等待遗留事务，每字节保留读就绪检查。
   使用普通读取，每组四字节减少循环，不启用 AUTORD。 */
#if defined(__SDCC) && defined(FLASHGBX_COMPAT)
static void uart_fifo_to_xram_burst(uint8_t xdata *target,uint8_t count) __naked {
    (void)target;(void)count;
    __asm
        push acc
        push psw
        push dpl
        push dph
        push ar0
        push ar1
        push ar6
        push ar7
        mov r0,dpl
        mov r1,dph
        mov dptr,#_uart_fifo_to_xram_burst_PARM_2
        movx a,@dptr
        mov r7,a
        mov dpl,r0
        mov dph,r1
        jz 00973$
    00970$:
        mov a,_USBADR
        jb acc.7,00970$
        mov a,r7
        anl a,#3
        mov r6,a
        mov a,r7
        rr a
        rr a
        anl a,#0x3f
        mov r7,a
        jz 00972$
    00971$:
        mov _USBADR,#0xa4
    00974$:
        mov a,_USBADR
        jb acc.7,00974$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        mov _USBADR,#0xa4
    00975$:
        mov a,_USBADR
        jb acc.7,00975$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        mov _USBADR,#0xa4
    00976$:
        mov a,_USBADR
        jb acc.7,00976$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        mov _USBADR,#0xa4
    00977$:
        mov a,_USBADR
        jb acc.7,00977$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        djnz r7,00971$
    00972$:
        mov a,r6
        jz 00973$
    00978$:
        mov _USBADR,#0xa4
    00979$:
        mov a,_USBADR
        jb acc.7,00979$
        mov a,_USBDAT
        movx @dptr,a
        inc dptr
        djnz r6,00978$
    00973$:
        pop ar7
        pop ar6
        pop ar1
        pop ar0
        pop dph
        pop dpl
        pop psw
        pop acc
        ret
    __endasm;
}
#endif
void uart_cmdRecv()
{
    uint8_t cnt1, cnt2;
    uint16_t cnt;
    uint16_t remainSize;
    uint16_t packSize;
#if defined(__SDCC) && defined(USB_RX_PACKET_FAST)
    uint16_t received;
    uint8_t __xdata *writePtr;
#endif

    remainSize = sizeof(cmdBuf) - cmdBuf_i_wr;

    cnt1 = usb_read_reg(OUTCOUNT1);
    cnt2 = usb_read_reg(OUTCOUNT2);
    cnt = ((cnt2 & 0x07) << 8) | cnt1;

    if (cnt > remainSize)
        return;

#if defined(__SDCC) && defined(USB_RX_PACKET_FAST)
    // 主循环在 USB ISR 返回前不会运行，一次发布整包计数与逐字更新等价。
    // 保留原 USB 间接寄存器的两次忙检测，消除每字节函数调用和计数搬运。
    received = cnt;
    writePtr = cmdBuf + cmdBuf_i_wr;
#if defined(FLASHGBX_COMPAT)
    if(cnt<=255) {
        if(flashgbx_rx_burst) uart_fifo_to_xram_burst(writePtr,(uint8_t)cnt);
        else uart_fifo_to_xram(writePtr,(uint8_t)cnt);
    }
    else if(cnt==256) {
        /* OUT4 的 FIFO 可容纳更多数据，仍通过已验证的普通读取路径复制。 */
        if(flashgbx_rx_burst) {
            uart_fifo_to_xram_burst(writePtr,128);
            uart_fifo_to_xram_burst(writePtr+128,128);
        } else {
            uart_fifo_to_xram(writePtr,128);
            uart_fifo_to_xram(writePtr+128,128);
        }
    }
    else
#endif
    while (cnt--)
    {
        while (USBADR & 0x80)
            ;
        USBADR = UART_RX_FIFO | 0x80;
        while (USBADR & 0x80)
            ;
        *writePtr++ = USBDAT;
    }
    cmdBuf_i_wr += received;
#else
    while (cnt--)
    {
        cmdBuf[cmdBuf_i_wr] = usb_read_reg(UART_RX_FIFO);
        cmdBuf_i_wr++;
    }
#endif

#ifdef FLASHGBX_COMPAT
    /* FlashGBX 命令必须在 USB FIFO 已经复制到 cmdBuf 后再解析。
       之前在读 FIFO 前检查 active，实际 A1 等首包会被漏掉。 */
    flashgbx_mark_receive();
    if (flashgbx_is_active())
    {
        if (!flashgbx_packet_complete())
            endpointClear();
        return;
    }
    /* FlashGBX 会先探测其它硬件协议。兼容变体需要消费这些探测包，
       否则 OUT 端点会保持 OPRDY，后续 A1 查询无法进入 FIFO。 */
    if (cmdBuf_i_wr > 0 && cmdBuf_i_wr <= 2 &&
        flashgbx_is_probe_byte(cmdBuf[0]))
    {
        endpointClear();
        uart_clearRecvBuf();
        return;
    }
#endif

    /* USB 是字节流：长度头自身也可能拆成一字节或两字节。 */
    if (cmdBuf_i_wr < SIZE_CMD_HEADER)
    {
        endpointClear();
        return;
    }
    packSize = reverse2(*((uint16_t *)cmdBuf));
    if (cmdBuf_i_wr < packSize)
        endpointClear();
}

void endpointClear(void) UART_STACK
{
    uint8_t saved_ie;
    if (legacy_running) return;
    /* 主循环和 ISR 共用 INDEX/USBADR/USBDAT，整组访问不可被 USB 中断插入。 */
    saved_ie = IE2;
    IE2 &= ~EUSB;
    usb_write_reg(INDEX, UART_RX_EP_INDEX);
    usb_write_reg(OUTCSR1, 0);
    IE2 = saved_ie;
}

/* 使用硬件 FIFO 状态等待；等待期间允许控制线复位取消当前操作。 */
static bit uart_waitTx(void) UART_STACK
{
    uint8_t saved_ie, csr;
    while (!cmdEnd)
    {
        saved_ie = IE2;
        IE2 &= ~EUSB;
        usb_write_reg(INDEX, UART_EP_INDEX);
        csr = usb_read_reg(INCSR1);
        IE2 = saved_ie;
        if (!(csr & (INFIFONE | INIPRDY))) return 1;
    }
    return 0;
}

void uart_responAck()
{
    uint8_t saved_ie;
    if (!uart_waitTx()) return;
    saved_ie = IE2;
    IE2 &= ~EUSB;
    if (!cmdEnd)
    {
        usb_write_reg(INDEX, UART_EP_INDEX);
        usb_write_reg(UART_FIFO, 0xaa);
        usb_write_reg(INCSR1, INIPRDY);
        ep1Busy = 1;
    }
    IE2 = saved_ie;
}

void uart_responData(uint8_t *dat, uint8_t len)
{
    uint8_t saved_ie;
    if (!uart_waitTx()) return;
    saved_ie = IE2;
    IE2 &= ~EUSB;
    if (!cmdEnd)
    {
        usb_write_reg(INDEX, UART_EP_INDEX);
        while (len--) usb_write_reg(UART_FIFO, *dat++);
        usb_write_reg(INCSR1, INIPRDY);
        ep1Busy = 1;
    }
    IE2 = saved_ie;
}

void uart_clearRecvBuf()
{
    cmdBuf_i_wr = 0;
    cmdBuf_i_rd = 0;
    // memset(cmdBuf, 0, sizeof(cmdBuf));
}

void uart_cmdHandler()
{
    uint8_t cmdCode, saved_ie;
    uint16_t received, frame_size;

#ifdef FLASHGBX_COMPAT
    /* 在协议入口覆盖所有动作，包括无普通读写包装的原生 CRC 内核。 */
    if (cmdBuf_i_wr) LED_ACT = 0;
    if (flashgbx_handle()) {
        LED_ACT = 1;
        return;
    }
    LED_ACT = 1;
#endif

    /* 先原子读取长度，收齐整帧后再读地址或执行动作，包含末尾 CRC 占位。
       5500 字节原缓冲足以容纳盖中盖 4096 字节 ROM 数据帧。 */
    saved_ie = IE2;
    IE2 &= ~EUSB;
    received = cmdBuf_i_wr;
    frame_size = received >= SIZE_CMD_HEADER ? reverse2(*((uint16_t *)cmdBuf)) : 0;
    if (received < SIZE_CMD_HEADER || frame_size < 5 ||
        frame_size > sizeof(cmdBuf) || received != frame_size)
    {
        IE2 = saved_ie;
        return;
    }
    legacy_running = 1;
    IE2 = saved_ie;
    if (received > 3)
    {
        LED_ACT = 0;
        cmdEnd = 0;

        cmdCode = cmdBuf[2];
        switch (cmdCode)
        {
        case 0xf0: // rom id获取
            romGetID();
            break;

        case 0xf1: // rom chip擦除
            romEraseChip();
            break;

            // case 0xf2: // rom blcok擦除
            //     romEraseBlock();
            //     break;

            // case 0xf3: // rom sector擦除
            //     romEraseSector();
            //     break;

        case 0xf4: // rom program
            romProgram();
            break;

        case 0xf5: // rom 写入透传
            romWrite();
            break;

        case 0xf6: // rom 读取透传
#ifdef FLASHGBX_COMPAT
            /* 旧设备把这帧作为正常的 62 字节读取；只有本固件解释尾部 CF 标记。
               普通客户端的零占位或校验尾不变，正常 F6 数据路径不受影响。 */
            if (received == 11 && cmdBuf[3] == 0 && cmdBuf[4] == 0 &&
                cmdBuf[5] == 0 && cmdBuf[6] == 0 &&
                cmdBuf[7] == 62 && cmdBuf[8] == 0 &&
                cmdBuf[9] == 'C' && cmdBuf[10] == 'F') {
                flashgbx_identify(1);
                break;
            }
#endif
            romRead();
            break;

        case 0xf7: // ram 写入透传
            ramWrite();
            break;

        case 0xf8: // ram 读取透传
            ramRead();
            break;

        case 0xf9: // 编程flash存档
            ramProgramFlash();
            break;

            // case 0xe7: // ram 带延迟写入
            //     ramWrite_forFram();
            //     break;

            // case 0xe8: // ram 带延迟读取
            //     ramRead_forFram();
            //     break;

        case 0xfa: // gbc 写入透传
            gbcWrite();
            break;

        case 0xfb: // gbc 读取透传
            gbcRead();
            break;

        case 0xfc: // gbc rom编程
            gbcRomProgram();
            break;

        case 0xea: // gbc 带延迟写入
            gbcWrite_forFram();
            break;

        case 0xeb: // gbc 带延迟读取
            gbcRead_forFram();
            break;

#ifdef GBC_READ_DIAGNOSTICS
        case 0xec: // 只读采样与端口状态排查，不参与正常烧录。
            gbcBusProbe();
            break;
#endif

        case 0xa0: // 卡带电源
            cart_power();
            break;

        case 0xa1: // phi分频输出
            cart_phi();
            break;

        default:
            break;
        }

        /* 内部旧函数可以清计数，但执行期间不能提前开放 OUT。 */
        saved_ie = IE2;
        IE2 &= ~EUSB;
        if (!cmdEnd) uart_clearRecvBuf();
        legacy_running = 0;
        if (!cmdEnd) endpointClear();
        IE2 = saved_ie;
        LED_ACT = 1;
    }
}

void romWaitForDone(uint32_t addr, uint16_t expectedValue)
{
    uint16_t value;
    while (1)
    {
        cart_romRead(addr, (uint8_t *)(&value), 1);
        value = reverse2(value);

        if ((value & 0x0080) == (expectedValue & 0x0080))
        {
            cart_romRead(addr, (uint8_t *)(&value), 1);
            return;
        }

        if (cmdEnd)
            return;
    }
}

// 获取rom id
// i 2B.包大小 0xf0 2B.CRC
//   05 00 f0 61 85
// o 2B.CRC 8B.数据
void romGetID()
{
    uint8_t id[8];
    uint8_t cmd[2] = {0, 0};
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);
    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    endpointClear();

    /* Issue Autoselect Command Sequence */
    cmd[0] = 0xaa;
    cart_romWrite(0x555, cmd, 1);
    cmd[0] = 0x55;
    cart_romWrite(0x2aa, cmd, 1);
    cmd[0] = 0x90;
    cart_romWrite(0x555, cmd, 1);

    //                                   e.g        S29GL256S  JS28F256
    cart_romRead(0x00, id + 0, 1); // Manufacture ID  0001h      0089h
    cart_romRead(0x01, id + 2, 1); // Device ID       227Eh      227Eh
    cart_romRead(0x0e, id + 4, 1); // Device ID       2222h      2222h = 256 Mb
    cart_romRead(0x0f, id + 6, 1); // Device ID       2201h      2201h

    /* Write Software RESET command */
    cmd[0] = 0xf0;
    cart_romWrite(0, cmd, 1);

    uart_clearRecvBuf();
    uart_responData(responCrc, 2);
    uart_responData(id, 8);
}

// 全片抹除
// i 2B.包大小 0xf1 2B.CRC
//   05 00 f1 a0 45
// o 0xaa
void romEraseChip()
{
    uint8_t cmd[2] = {0, 0};
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);
    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    endpointClear();

    /* Issue inlock sequence command */
    cmd[0] = 0xaa;
    cart_romWrite(0x555, cmd, 1);
    cmd[0] = 0x55;
    cart_romWrite(0x2aa, cmd, 1);
    cmd[0] = 0x80;
    cart_romWrite(0x555, cmd, 1);
    cmd[0] = 0xaa;
    cart_romWrite(0x555, cmd, 1);
    cmd[0] = 0x55;
    cart_romWrite(0x2aa, cmd, 1);
    /* Write Chip Erase Command to Base Address */
    cmd[0] = 0x10;
    cart_romWrite(0x555, cmd, 1);

    uart_clearRecvBuf();
    uart_responAck();
}

// rom program
// i 2B.包大小 0xf4 4B.始地址 2B.buffer大小 nB.数据 2B.CRC
// o 0xaa
void romProgram()
{
    uint16_t packSize;
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t bufferWriteBytes;
#if defined(__SDCC) && defined(ROM_WRITE_XDATA_FAST)
    uint8_t __xdata *dataBuf;
#else
    uint8_t *dataBuf;
#endif
    uint16_t wrLen, writtenCount;
    uint8_t cmd[2] = {0, 0};

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令头接收完成
    while (cmdBuf_i_wr < (SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE))
        ;

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_BUFF_SIZE - SIZE_CRC;
    // 编程buff大小
    bufferWriteBytes = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE;

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE;
    writtenCount = 0;

    do
    {
        // 不能多字节编程
        if (bufferWriteBytes == 0)
        {
            // 剩余有效数据不足 1 word
            for (wrLen = 0; wrLen < 2;)
            {
                IE2 &= ~EUSB;                      // 禁用 USB 中断
                wrLen = cmdBuf_i_wr - cmdBuf_i_rd; // 8位单片机处理16位数据过程中遇上中断，被修改后回来，结果就可能异常，TMD
                IE2 |= EUSB;                       // 使能 USB 中断

                // 命令包收完了
                if (cmdBuf_i_wr == packSize)
                    break;

                // 被主机复位了
                if (cmdEnd)
                {
                    cmdEnd = 0;
                    return;
                }
            }

            /* Issue Load Write Buffer Command Sequence */
            /* Issue unlock command sequence */
            cmd[0] = 0xaa;
            cart_romWrite(0x555, cmd, 1);
            cmd[0] = 0x55;
            cart_romWrite(0x2aa, cmd, 1);
            /* Write Program Command */
            cmd[0] = 0xa0;
            cart_romWrite(0x555, cmd, 1);

            cart_romWrite_data(baseAddress >> 1,
                          dataBuf,
                          1);

            romWaitForDone(baseAddress >> 1,
                           reverse2(*((uint16_t *)dataBuf)));

            baseAddress += 2;
            dataBuf += 2;
            writtenCount += 2;
            cmdBuf_i_rd += 2;
        }
        // 可以多字节编程
        else
        {
            // 剩余数据不够1个write buffer
            for (wrLen = 0; wrLen < bufferWriteBytes;)
            {
                IE2 &= ~EUSB;                      // 禁用 USB 中断
                wrLen = cmdBuf_i_wr - cmdBuf_i_rd; // 8位单片机处理16位数据过程中遇上中断，被修改后回来，结果就可能异常，TMD
                IE2 |= EUSB;                       // 使能 USB 中断

                // 命令包收完了
                if (cmdBuf_i_wr == packSize)
                    break;

                // 被主机复位了
                if (cmdEnd)
                {
                    cmdEnd = 0;
                    return;
                }
            }

            wrLen = min(wrLen, byteCount - writtenCount);
            wrLen = min(wrLen, bufferWriteBytes);

            /* Issue Load Write Buffer Command Sequence */
            /* Issue unlock command sequence */
            cmd[0] = 0xaa;
            cart_romWrite(0x555, cmd, 1);
            cmd[0] = 0x55;
            cart_romWrite(0x2aa, cmd, 1);
            /* Issue Write to Buffer Command at Sector Address */
            cmd[0] = 0x25;
            cart_romWrite(baseAddress >> 1, cmd, 1);

            /* Write Number of Locations to program */
            *((uint16_t *)cmd) = reverse2(wrLen / 2 - 1);
            cart_romWrite(baseAddress >> 1, cmd, 1);

            /* Load Data into Buffer */
            cart_romWrite_data(baseAddress >> 1,
                          dataBuf,
                          wrLen / 2);

            /* Issue Program Buffer to Flash command */
            cmd[0] = 0x29;
            cart_romWrite(baseAddress >> 1, cmd, 1);

            romWaitForDone(
                (baseAddress + wrLen - 2) >> 1,
                reverse2(*((uint16_t *)(dataBuf + wrLen - 2))));

            baseAddress += wrLen;
            dataBuf += wrLen;
            writtenCount += wrLen;
            cmdBuf_i_rd += wrLen;
        }
    } while (writtenCount < byteCount);

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

// rom写入透传
// i 2B.包大小 0xf5 4B.始地址 nB.数据 2B.CRC
// o 0xaa
void romWrite()
{
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t wrLen, writtenCount;
    uint8_t *dataBuf;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_CRC;
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;

    

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;
    writtenCount = 0;

    cart_gbaRomAddrSetup(baseAddress);
    do
    {
        // 剩余有效数据不足 1 word，等下一个usb数据包
        for (wrLen = 0; wrLen < 2;)
        {
            IE2 &= ~EUSB;
            wrLen = cmdBuf_i_wr - cmdBuf_i_rd;
            IE2 |= EUSB;

            // 命令包收完了
            if (cmdBuf_i_wr == packSize)
                break;

            // 被主机复位了
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        }

        wrLen = min(wrLen, byteCount - writtenCount);
        wrLen = wrLen & 0xfffe; // 按word对齐

        cart_romWrite_continuous(baseAddress,
                                 dataBuf,
                                 wrLen / 2);

        baseAddress += wrLen / 2;
        dataBuf += wrLen;
        writtenCount += wrLen;
        cmdBuf_i_rd += wrLen;
    } while (writtenCount < byteCount); // 数据还没发完

    
    CART_CS1 = 1;

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

// rom 读取透传
// i 2B.包大小 0xf6 4B.始地址 2B.读取数量 2B.CRC
// o 2B.CRC nB.数据
void romRead()
{
    uint32_t baseAddress; // 基地址
    uint16_t byteCount;   // 读取总数量
    uint16_t rdLen, i;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    uart_responData(responCrc, 2);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 读取总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));

    for (i = 0; i < byteCount;)
    {
        rdLen = byteCount - i;
        rdLen = min(rdLen, (sizeof(responBuf) - 2)); // 把端点fifo写满会发不出去，原因未知

        cart_romRead(
            (baseAddress + i) >> 1,
            responBuf,
            rdLen / 2);

        uart_responData(responBuf, rdLen);

        i += rdLen;
    }

    uart_clearRecvBuf();
    endpointClear();
}

// ram写入
// i 2B.包大小 0xf7 4B.基地址 nB.写入数据 2B.CRC
// o 0xaa
void ramWrite()
{
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t wrLen, writtenCount;
    uint8_t *dataBuf;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_CRC;
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;
    writtenCount = 0;

    do
    {
        // 剩余有效数据不足 1 byte，等下一个usb数据包
        for (wrLen = 0; wrLen < 1;)
        {
            IE2 &= ~EUSB;
            wrLen = cmdBuf_i_wr - cmdBuf_i_rd;
            IE2 |= EUSB;

            // 命令包收完了
            if (cmdBuf_i_wr == packSize)
                break;

            // 被主机复位了
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        }

        wrLen = min(wrLen, byteCount - writtenCount);

        cart_ramWrite(baseAddress, dataBuf, wrLen);

        baseAddress += wrLen;
        dataBuf += wrLen;
        writtenCount += wrLen;
        cmdBuf_i_rd += wrLen;
    } while (writtenCount < byteCount); // 数据还没发完

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

// ram 读取
// i 2B.包大小 0xf8 4B.基地址 2B.读取数量 2B.CRC
// o 2B.CRC nB.数据
void ramRead()
{
    uint32_t baseAddress; // 基地址
    uint16_t byteCount;   // 读取总数量
    uint16_t rdLen, i;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    uart_responData(responCrc, 2);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 读取总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));

    for (i = 0; i < byteCount;)
    {
        rdLen = byteCount - i;
        rdLen = min(rdLen, (sizeof(responBuf) - 1)); // 把端点fifo写满会发不出去，原因未知

        cart_ramRead(baseAddress, responBuf, rdLen);

        uart_responData(responBuf, rdLen);

        baseAddress += rdLen;
        i += rdLen;
    }

    uart_clearRecvBuf();
    endpointClear();
}

void ramProgramFlash()
{
    uint16_t packSize;
    uint32_t baseAddress;
    uint16_t byteCount;
    uint8_t *dataBuf;
    uint16_t wrLen, writtenCount;
    uint8_t cmd;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令头接收完成
    while (cmdBuf_i_wr < (SIZE_CMD_HEADER + SIZE_BASE_ADDRESS))
        ;

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_CRC;
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE;
    writtenCount = 0;

    do
    {
        // 剩余有效数据不足 1 byte
        for (wrLen = 0; wrLen < 1;)
        {
            IE2 &= ~EUSB;
            wrLen = cmdBuf_i_wr - cmdBuf_i_rd;
            IE2 |= EUSB;

            // 命令包收完了
            if (cmdBuf_i_wr == packSize)
                break;

            // 被主机复位了
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        }

        cmd = 0xaa;
        cart_ramWrite(0x5555, &cmd, 1);
        cmd = 0x55;
        cart_ramWrite(0x2aaa, &cmd, 1);
        cmd = 0xa0;
        cart_ramWrite(0x5555, &cmd, 1); // FLASH_COMMAND_PROGRAM
        cart_ramWrite((uint16_t)(baseAddress), dataBuf, 1);

        // wait for done
        do
        {
            cart_ramRead((uint16_t)(baseAddress), &cmd, 1);
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        } while (cmd != *dataBuf);

        baseAddress++;
        dataBuf++;
        writtenCount++;
        cmdBuf_i_rd++;
    } while (writtenCount < byteCount);

    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

////////////////////////////////////////////////////////////
/// 下面是gbc的功能
////////////////////////////////////////////////////////////

void gbcWrite()
{
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t wrLen, writtenCount;
    uint8_t *dataBuf;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_CRC;
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;

    

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS;
    writtenCount = 0;

    do
    {
        // 剩余有效数据不足 1 byte，等下一个usb数据包
        for (wrLen = 0; wrLen < 1;)
        {
            IE2 &= ~EUSB;
            wrLen = cmdBuf_i_wr - cmdBuf_i_rd;
            IE2 |= EUSB;

            // 命令包收完了
            if (cmdBuf_i_wr == packSize)
                break;

            // 被主机复位了
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        }

        wrLen = min(wrLen, byteCount - writtenCount);

        cart_gbcWrite(baseAddress, dataBuf, wrLen);

        baseAddress += wrLen;
        dataBuf += wrLen;
        writtenCount += wrLen;
        cmdBuf_i_rd += wrLen;
    } while (writtenCount < byteCount); // 数据还没发完

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

void gbcRead()
{
    uint32_t baseAddress; // 基地址
    uint16_t byteCount;   // 读取总数量
    uint16_t rdLen, i;
    uint16_t packSize;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    uart_responData(responCrc, 2);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 读取总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));

    for (i = 0; i < byteCount;)
    {
        rdLen = byteCount - i;
        rdLen = min(rdLen, (sizeof(responBuf) - 1)); // 把端点fifo写满会发不出去，原因未知

        cart_gbcRead(baseAddress, responBuf, rdLen);

        uart_responData(responBuf, rdLen);

        baseAddress += rdLen;
        i += rdLen;
    }

    // 返回数据
    uart_clearRecvBuf();
    endpointClear();
}

void gbcRomProgram()
{
    uint16_t packSize;
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t bufferWriteBytes;
    uint8_t *dataBuf;
    uint16_t wrLen, writtenCount;
    uint8_t cmd;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令头接收完成
    while (cmdBuf_i_wr < (SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE))
        ;

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_BUFF_SIZE - SIZE_CRC;
    // 编程buff大小
    bufferWriteBytes = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE;

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BUFF_SIZE;
    writtenCount = 0;

    do
    {
        // 不能多字节编程编程
        if (bufferWriteBytes == 0)
        {
            // 剩余有效数据不足 1 byte
            for (wrLen = 0; wrLen < 1;)
            {
                IE2 &= ~EUSB;                      // 禁用 USB 中断
                wrLen = cmdBuf_i_wr - cmdBuf_i_rd; // 8位单片机处理16位数据过程中遇上中断，被修改后回来，结果就可能异常，TMD
                IE2 |= EUSB;                       // 使能 USB 中断

                // 命令包收完了
                if (cmdBuf_i_wr == packSize)
                    break;

                // 被主机复位了
                if (cmdEnd)
                {
                    cmdEnd = 0;
                    return;
                }
            }

            cmd = 0xaa;
            cart_gbcWrite(0xaaa, &cmd, 1);
            cmd = 0x55;
            cart_gbcWrite(0x555, &cmd, 1);
            cmd = 0xa0;
            cart_gbcWrite(0xaaa, &cmd, 1); // FLASH_COMMAND_PROGRAM
            cart_gbcWrite(baseAddress, dataBuf, 1);

            // wait for done
            do
            {
                cart_gbcRead(baseAddress, &cmd, 1);

                // 被主机复位了
                if (cmdBuf_i_wr == 0)
                {
                    return;
                }
            } while (cmd != *dataBuf);

            baseAddress += 1;
            dataBuf += 1;
            writtenCount += 1;
            cmdBuf_i_rd += 1;
        }
        // 可以多字节编程
        else
        {
            // 剩余数据不够1个write buffer
            for (wrLen = 0; wrLen < bufferWriteBytes;)
            {
                IE2 &= ~EUSB;                      // 禁用 USB 中断
                wrLen = cmdBuf_i_wr - cmdBuf_i_rd; // 8位单片机处理16位数据过程中遇上中断，被修改后回来，结果就可能异常，TMD
                IE2 |= EUSB;                       // 使能 USB 中断

                // 命令包收完了
                if (cmdBuf_i_wr == packSize)
                    break;

                // 被主机复位了
                if (cmdEnd)
                {
                    cmdEnd = 0;
                    return;
                }
            }

            wrLen = min(wrLen, byteCount - writtenCount);
            wrLen = min(wrLen, bufferWriteBytes);

            cmd = 0xaa;
            cart_gbcWrite(0xaaa, &cmd, 1);
            cmd = 0x55;
            cart_gbcWrite(0x555, &cmd, 1);
            cmd = 0x25;
            cart_gbcWrite(baseAddress, &cmd, 1);

            cmd = wrLen - 1;
            cart_gbcWrite(baseAddress, &cmd, 1);

            cart_gbcWrite(baseAddress,
                          dataBuf,
                          wrLen);

            cmd = 0x29;
            cart_gbcWrite(baseAddress, &cmd, 1);

            // wait for done
            do
            {
                cart_gbcRead(baseAddress + wrLen - 1, &cmd, 1);

                // 被主机复位了
                if (cmdBuf_i_wr == 0)
                {
                    return;
                }
            } while (cmd != *(dataBuf + wrLen - 1));

            baseAddress += wrLen;
            dataBuf += wrLen;
            writtenCount += wrLen;
            cmdBuf_i_rd += wrLen;
        }

    } while (writtenCount < byteCount);

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

void gbcWrite_forFram()
{
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t wrLen, writtenCount;
    uint8_t *dataBuf;
    uint16_t packSize;
    uint8_t latency, ii;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令头接收完成
    while (cmdBuf_i_wr < (SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_LATENCY))
        ;

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 写入总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf))) - SIZE_CMD_HEADER - SIZE_BASE_ADDRESS - SIZE_LATENCY - SIZE_CRC;
    // 延迟周期
    latency = *(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS);
    // 数据
    dataBuf = cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_LATENCY;

    cmdBuf_i_rd = SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_LATENCY;
    writtenCount = 0;
    do
    {
        // 剩余有效数据不足 1 byte，等下一个usb数据包
        for (wrLen = 0; wrLen < 1;)
        {
            IE2 &= ~EUSB;
            wrLen = cmdBuf_i_wr - cmdBuf_i_rd;
            IE2 |= EUSB;

            // 命令包收完了
            if (cmdBuf_i_wr == packSize)
                break;

            // 被主机复位了
            if (cmdEnd)
            {
                cmdEnd = 0;
                return;
            }
        }

        wrLen = min(wrLen, byteCount - writtenCount);

        cart_gbcWrite(baseAddress, dataBuf, 1); // 逐个字节写
        for (ii = 0; ii < latency; ii++)
            NOP(1);

        baseAddress++;
        dataBuf++;
        writtenCount++;
        cmdBuf_i_rd++;
    } while (writtenCount < byteCount); // 数据还没发完

    // 回复ack
    uart_responAck();
    uart_clearRecvBuf();
    endpointClear();
}

void gbcRead_forFram()
{
    uint32_t baseAddress;
    uint16_t byteCount;
    uint16_t rdLen, readCount, i;
    uint16_t packSize;
    uint8_t latency, ii;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;
    uart_responData(responCrc, 2);

    // 基地址
    baseAddress = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
    // 读取总数量
    byteCount = reverse2(*((uint16_t *)(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS)));
    // 延迟周期
    latency = *(cmdBuf + SIZE_CMD_HEADER + SIZE_BASE_ADDRESS + SIZE_BYTE_COUNT);

    for (readCount = 0; readCount < byteCount;)
    {
        rdLen = byteCount - readCount;
        rdLen = min(rdLen, (sizeof(responBuf) - 1)); // 把端点fifo写满会发不出去，原因未知

        for (i = 0; i < rdLen; i++)
        {
            cart_gbcRead(baseAddress, responBuf + i, 1); // 逐个字节读
            for (ii = 0; ii < latency; ii++)
                NOP(1);
            baseAddress++;
        }

        uart_responData(responBuf, rdLen);

        readCount += rdLen;
    }

    uart_clearRecvBuf();
    endpointClear();
}

#ifdef GBC_READ_DIAGNOSTICS
// 本次排查使用 11 字节帧：长度、EC、地址 4 字节、保留 2 字节、CRC 2 字节。
void gbcBusProbe()
{
    uint16_t packSize;
    uint32_t address;
    uint8_t i;
    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);
    while (cmdBuf_i_wr < packSize)
        ;
    if (packSize == 11)
    {
        address = reverse4(*((uint32_t *)(cmdBuf + SIZE_CMD_HEADER)));
        cart_gbcProbe(address, responBuf, cmdBuf[7]);
        for (i = 0; i < 3; i++)
            responBuf[i + 3] = responBuf[i];
        uart_responData(responCrc, 2);
        uart_responData(responBuf, 16);
    }
    uart_clearRecvBuf();
    endpointClear();
}
#endif

// 卡带电源
// i 2B.包大小 0xa0 1B.pwr 2B.CRC
//   06 00 a0 xx 00 00
// o 无返回
void cart_power()
{
    uint16_t packSize;
    uint8_t val;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;

    val = cmdBuf[3];

    board_setPower(val);

    uart_clearRecvBuf();
    endpointClear();
}

// phi分频输出
// i 2B.包大小 0xa1 1B.div 2B.CRC
//   06 00 a1 xx 00 00
// o 无返回
void cart_phi()
{
    uint16_t packSize;
    uint8_t val;

    KEIL_BYTE16(packSize, 0) = *(cmdBuf + 1);
    KEIL_BYTE16(packSize, 1) = *(cmdBuf + 0);

    // 等待命令接收完成
    while (cmdBuf_i_wr < packSize)
        ;

    val = cmdBuf[3];

    CLK_SYSCLKO_Divider(val); // 分频系数 0~127, 0:不输出

    uart_clearRecvBuf();
    endpointClear();
}
