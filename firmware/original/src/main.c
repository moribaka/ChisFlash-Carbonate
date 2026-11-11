#include "main.h"
#include "usb.h"
#include "uart.h"
#ifdef FLASHGBX_COMPAT
#include "flashgbx.h"
#endif

void TIMER4_ISR(void) COMPILER_ISR(TMR4_VECTOR)
{
#ifdef FLASHGBX_COMPAT
    flashgbx_tick();
#endif
    /* RUN 只表示 USB 已就绪，读写和定时器不再让它闪烁。 */
    LED_RUN = (DeviceState != DEVSTATE_CONFIGURED);
}

void main()
{
    EnableAccessXFR(); // 使能扩展寄存器(XFR)

    CLK_SYSCLKO_SwitchP16(); // 设置系统时钟输出端口: MCLKO (P1.6)
    CLK_SYSCLKO_Divider(0);  // 分频系数 0~127, 0:不输出

    // gpio
    LED_RUN = 1;
    LED_ACT = 1;
    LED_UNUSED_WR = 1;
    CART_CS1 = 1;
    CART_CS2 = 1;
    CART_RD = 1;
    CART_WR = 1;
    CART_IRQ = 1;
    PWR_5V = 0;
    PWR_CART = 0;
    // PWR_5V P33 推挽
    SetP3nPushPullMode(PIN_3);
    SetP3nSlewRateFast(PIN_3);
    // 原版 LED_ACT P42、LED_UNUSED_WR P43、LED_RUN P44 准双向
    // CART_IRQ P45 准双向
    // CART_CS2 P46 推挽
    SetP4nQuasiMode(PIN_2 | PIN_3 | PIN_4 | PIN_5);
    SetP4nPushPullMode(PIN_6);
    SetP4nSlewRateFast(PIN_6);
    // 原版 PWR_CART P63 总使能，P33 选择电压
    // CART_CS1 P61 推挽
    // CART_RD P60 推挽
    // CART_WR P62 推挽
    SetP6nPushPullMode(PIN_0 | PIN_1 | PIN_2 | PIN_3);
    SetP6nSlewRateFast(PIN_0 | PIN_1 | PIN_2 | PIN_3);
    // 原版 A16..23 / D0..7，P0.0..7 顺接
    SetP0nQuasiMode(PIN_ALL);     // 设置P0为准双向口模式
    SetP0nSlewRateFast(PIN_ALL);  // 设置P0快速翻转速度
    EnableP0nPullUp(PIN_ALL);     // 设置P0内部上拉电阻
    SetP0nDrivingStrong(PIN_ALL); // 驱动电流strong
    // 原版 AD8..15，P2.0..7 顺接
    SetP2nQuasiMode(PIN_ALL);     // 设置P2为准双向口模式
    SetP2nSlewRateFast(PIN_ALL);  // 设置P2快速翻转速度
    EnableP2nPullUp(PIN_ALL);     // 设置P2内部上拉电阻
    SetP2nDrivingStrong(PIN_ALL); // 驱动电流strong
    // 原版 AD0..7，P7.0..7 顺接
    SetP7nQuasiMode(PIN_ALL);     // 设置P7为准双向口模式
    SetP7nSlewRateFast(PIN_ALL);  // 设置P7快速翻转速度
    EnableP7nPullUp(PIN_ALL);     // 设置P7内部上拉电阻
    SetP7nDrivingStrong(PIN_ALL); // 驱动电流strong
    // phi
    SetP1nPushPullMode(PIN_6); // 设置P0为准双向口模式
    SetP1nSlewRateFast(PIN_6); // 设置P0快速翻转速度

    board_setPower(1);

    // timer 4
    TIMER4_TimerMode();       // 设置定时器4为定时模式
    TIMER4_12TMode();         // 设置定时器4为12T模式
    TIMER4_EnableInt();       // 使能定时器4中断
    TIMER4_SetPrescale(28);   // 设置定时器4的8位预分频 44236800/29=1525406.897
    TIMER4_SetReload16(1977); // 设置定时器4的16位重载值 (65536-1977)/(1525406.897/12)=0.5000029839s
    TIMER4_Run();             // 定时器4开始运行

#ifdef FLASHGBX_COMPAT
    flashgbx_reset();
#endif
    usb_init();

    EnableGlobalInt(); // 使能全局中断

    ///////////////////////

    while (1)
    {
        uart_cmdHandler();
    }
}

uint32_t reverse4(uint32_t d)
{
#ifdef __SDCC
    return d;
#else
    uint32_t ret;

    ((uint8_t *)&ret)[0] = ((uint8_t *)&d)[3];
    ((uint8_t *)&ret)[1] = ((uint8_t *)&d)[2];
    ((uint8_t *)&ret)[2] = ((uint8_t *)&d)[1];
    ((uint8_t *)&ret)[3] = ((uint8_t *)&d)[0];

    return ret;
#endif
}

uint16_t reverse2(uint16_t w)
{
#ifdef __SDCC
    return w;
#else
    uint16_t ret;

    ((uint8_t *)&ret)[0] = ((uint8_t *)&w)[1];
    ((uint8_t *)&ret)[1] = ((uint8_t *)&w)[0];

    return ret;
#endif
}

uint16_t reverse2_forIRQ(uint16_t w)
{
#ifdef __SDCC
    return w;
#else
    uint16_t ret;

    ((uint8_t *)&ret)[0] = ((uint8_t *)&w)[1];
    ((uint8_t *)&ret)[1] = ((uint8_t *)&w)[0];

    return ret;
#endif
}
