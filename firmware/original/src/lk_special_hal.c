/* SPDX-License-Identifier: GPL-3.0-only
 * 碳酸钙 STC8H 硬件适配层，供保留来源的 LK 特殊算法调用。 */
#include "lk_special_hal.h"
uint8_t special_failed;
static uint8_t xdata word_buffer[2];

void lk_delay_us(uint16_t us) FGX_STACK {
    while(us-- && !flashgbx_cancelled()) { NOP40(); }
}
void lk_conn_send_u16(uint16_t value) FGX_STACK {
    if(!fgx_send_word(value)) special_failed=1;
}
void lk_dmg_cart_write_byte(uint32_t address,uint16_t value) FGX_STACK {
    if(_timeout_check()) return;
    word_buffer[0]=(uint8_t)value;
    cart_gbcWrite(address,word_buffer,1);
}
uint8_t lk_dmg_cart_read_byte(uint16_t address) FGX_STACK {
    if(_timeout_check()) return 0xff;
    cart_gbcRead(address,word_buffer,1); return word_buffer[0];
}
void fgx_dmg_flash_write(uint16_t address,uint8_t value,uint8_t pin) FGX_STACK {
    if(flashgbx_cancelled()) return;
    if(pin<=1) { word_buffer[0]=value; cart_gbcWrite(address,word_buffer,1); return; }
    CART_WR=1; P16=0;
    cart_setDirection_ad(1); cart_setDirection_a(1);
    CART_WRITE_AD_L(address); CART_WRITE_AD_H(address>>8); CART_WRITE_A(value);
    NOP20();
    if(pin==2) {
        SetP4nPushPullMode(PIN_5); CART_IRQ=0; NOP40(); CART_IRQ=1;
    } else {
        CART_CS2=0; CART_WR=0; NOP40(); CART_WR=1; CART_CS2=1;
    }
    cart_setDirection_a(0); cart_setDirection_ad(0);
}
void lk_dmg_flash_write_byte(uint32_t address,uint16_t value) FGX_STACK {
    if(!_timeout_check()) fgx_dmg_flash_write(address,value,fgx_state.byte[4]);
}
void lk_agb_cart_write_short(uint32_t address,uint16_t value) FGX_STACK {
    if(_timeout_check()) return;
    word_buffer[0]=value; word_buffer[1]=value>>8;
    cart_romWrite(address,word_buffer,1);
}
uint16_t lk_agb_cart_read_short(uint32_t address) FGX_STACK {
    if(_timeout_check()) return 0xffff;
    cart_romRead(address,word_buffer,1); return ((uint16_t)word_buffer[1]<<8)|word_buffer[0];
}
static void verify_value(uint32_t address,uint16_t value,uint8_t status,uint8_t agb) FGX_STACK {
    uint16_t left=20000,readback=0;
    if(_timeout_check()) return;
    do {
        if(flashgbx_cancelled()) { special_failed=1; return; }
        readback=agb?lk_agb_cart_read_short(address):lk_dmg_cart_read_byte(address);
        if(status ? ((readback&fgx_state.word[5])==fgx_state.word[6]) : readback==value) {
            /* E201264使用专用方法10，但状态位仍须按Intel检查错误，不能仅凭就绪位报成功。 */
            if(status==1 && (fgx_state.byte[2]==2 || fgx_state.byte[3]==10) && (readback & ((fgx_state.word[5]&0xff00)?0x3a3a:0x003a))) {
                fgx_state.word[3]=readback; special_failed=1;
            }
            return;
        }
    } while(--left);
    fgx_state.word[3]=readback; special_failed=1;
}
void lk_dmg_verify_data(uint32_t address,uint16_t value) FGX_STACK { verify_value(address,(uint8_t)value,0,0); }
void lk_agb_verify_data(uint32_t address,uint16_t value) FGX_STACK { verify_value(address,value,0,1); }
void lk_dmg_verify_status_register(uint32_t address) FGX_STACK { verify_value(address,0,1,0); }
void lk_agb_verify_status_register(uint32_t address) FGX_STACK { verify_value(address,0,1,1); }
/* E8提交前读取XSR，只检查就绪位；提交后才检查完整错误状态。 */
void lk_agb_verify_ready_register(uint32_t address) FGX_STACK { verify_value(address,0,2,1); }
void lk_dmg_change_bank(uint16_t bank) FGX_STACK {
    uint8_t i;
    if(!fgx_state.bank_count) lk_dmg_cart_write_byte(0x2000,bank);
    else for(i=0;i<fgx_state.bank_count;i++) {
        if(fgx_state.bank_mode[i]==0) lk_dmg_cart_write_byte(fgx_state.bank_address[i],bank);
        else lk_dmg_cart_write_byte(bank,fgx_state.bank_address[i]);
    }
}
