#include "main.h"
#include "flashgbx.h"
void lk_delay_us(uint16_t us) __reentrant;
static uint8_t power_mode;
static bit bus_saved;
/* 只在实际切换电源时访问，不进入读写热路径。 */
static struct {
    uint8_t p0m0,p0m1,p2m0,p2m1,p7m0,p7m1;
    uint8_t p0pu,p2pu,p7pu,p4pu,p6pu,p1pu;
    uint8_t p4m0,p4m1,p6m0,p6m1,p1m0,p1m1,clock;
} xdata bus;
uint8_t board_getPower(void) { return power_mode; }
static void suspend_bus(void) {
    bus.p0m0=P0M0; bus.p0m1=P0M1; bus.p2m0=P2M0; bus.p2m1=P2M1;
    bus.p7m0=P7M0; bus.p7m1=P7M1;
    bus.p0pu=P0PU; bus.p2pu=P2PU; bus.p7pu=P7PU;
    bus.p4pu=P4PU; bus.p6pu=P6PU; bus.p1pu=P1PU;
    bus.p4m0=P4M0; bus.p4m1=P4M1; bus.p6m0=P6M0; bus.p6m1=P6M1;
    bus.p1m0=P1M0; bus.p1m1=P1M1; bus.clock=MCLKOCR;
    CART_CS1=1; CART_CS2=1; CART_RD=1; CART_WR=1;
    MCLKOCR &= 0x80;
    SetP0nHighZInputMode(PIN_ALL); SetP2nHighZInputMode(PIN_ALL); SetP7nHighZInputMode(PIN_ALL);
    SetP4nHighZInputMode(PIN_5|PIN_6); SetP6nHighZInputMode(PIN_0|PIN_1|PIN_2);
    SetP1nHighZInputMode(PIN_6);
    P0PU=0; P2PU=0; P7PU=0;
    P4PU &= ~(PIN_5|PIN_6); P6PU &= ~(PIN_0|PIN_1|PIN_2); P1PU &= ~PIN_6;
    bus_saved=1;
}
static void resume_bus(void) {
    if(!bus_saved) return;
    /* 先确保控制线无效，再恢复驱动；不改 LED 和电源控制脚的模式。 */
    CART_CS1=1; CART_CS2=1; CART_RD=1; CART_WR=1;
    P0M0=bus.p0m0; P0M1=bus.p0m1; P2M0=bus.p2m0; P2M1=bus.p2m1;
    P7M0=bus.p7m0; P7M1=bus.p7m1;
    P4M0=(P4M0&~0x60)|(bus.p4m0&0x60); P4M1=(P4M1&~0x60)|(bus.p4m1&0x60);
    P6M0=(P6M0&~7)|(bus.p6m0&7); P6M1=(P6M1&~7)|(bus.p6m1&7);
    P1M0=(P1M0&~0x40)|(bus.p1m0&0x40); P1M1=(P1M1&~0x40)|(bus.p1m1&0x40);
    P0PU=bus.p0pu; P2PU=bus.p2pu; P7PU=bus.p7pu;
    P4PU=(P4PU&~0x60)|(bus.p4pu&0x60); P6PU=(P6PU&~7)|(bus.p6pu&7);
    P1PU=(P1PU&~0x40)|(bus.p1pu&0x40); MCLKOCR=bus.clock;
    bus_saved=0;
}
void board_setPower(uint8_t mode) {
    uint8_t saved_ie;
    if(mode>2) mode=0;
    if(mode==power_mode) return;
    saved_ie=IE2; IE2 &= ~EUSB;
    if(power_mode) suspend_bus();
    PWR_CART=0;
    power_mode=0;
    IE2=saved_ie;
    /* 软件关电源后保留200ms隔离窗口，避免总线反灌或残压维持换组锁存。 */
    if(bus_saved) {
        lk_delay_us(50000); lk_delay_us(50000); lk_delay_us(50000); lk_delay_us(50000);
    }
    saved_ie=IE2; IE2 &= ~EUSB;
    if(bus_saved && flashgbx_bus_cancelled) mode=0;
    if(mode) {
        PWR_5V=(mode==2); PWR_CART=1;
        power_mode=mode;
        IE2=saved_ie;
        lk_delay_us(1000);
        saved_ie=IE2; IE2 &= ~EUSB;
        if(bus_saved && flashgbx_bus_cancelled) { PWR_CART=0; power_mode=0; }
        else resume_bus();
    }
    IE2=saved_ie;
}

