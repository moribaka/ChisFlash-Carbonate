/* AMD 普通 WR 逐字写入：逐字保留上传的三条解锁命令和原始 ROM 数据。
   只缓存物理端口值、复用方向设置；每字需要两次连续完整读回相同才继续。
   本文件由发布准备脚本替换板型标记，不能直接用于另一种 PCB。 */
#if defined(__SDCC) && defined(FLASHGBX_COMPAT)
extern uint8_t xdata cmdBuf[5500];
#define GS_ENCODE(value) ((uint8_t)(value))
#define GS_DECODE(value) GS_ENCODE(value)
#define GS_AD_OUT() do { SetP7nPushPullMode(PIN_ALL); SetP2nPushPullMode(PIN_ALL); } while(0)
#define GS_AD_IN() do { SetP7nQuasiMode(PIN_ALL); SetP2nQuasiMode(PIN_ALL); PORT_AD_L=0xff; PORT_AD_H=0xff; } while(0)

static uint8_t xdata gs_prefix[15],gs_target[5],gs_reset[2];

static void gs_poll_backoff(void) __reentrant
{
    uint8_t delay;
    /* 前十六次允许快速完成；持续忙时补足等待，避免快内核缩短原超时窗口。 */
    for(delay=0;delay<32;delay++) { NOP40(); }
}

static void gs_write_word(const uint8_t xdata *p) __reentrant
{
    uint8_t saved_ie;
    if(flashgbx_bus_cancelled) return;
    CART_RD=1; CART_WR=1; CART_CS1=1;
    PORT_AD_L=p[0]; PORT_AD_H=p[1]; PORT_A=p[2];
    NOP2(); CART_CS1=0;
    PORT_AD_L=p[3]; PORT_AD_H=p[4]; NOP5();
    /* WR 低脉冲不能被 USB 取消拆开；恢复中断前完成整个写周期。 */
    saved_ie=IE2; IE2&=~EUSB;
    if(!flashgbx_bus_cancelled) { CART_WR=0; NOP5(); CART_WR=1; }
    IE2=saved_ie;
    CART_CS1=1;
}

static uint16_t gs_read_word(void) __reentrant
{
    uint8_t low,high;
    if(flashgbx_bus_cancelled) return 0;
    CART_RD=1; CART_WR=1; CART_CS1=1;
    GS_AD_OUT();
    PORT_AD_L=gs_target[0]; PORT_AD_H=gs_target[1]; PORT_A=gs_target[2];
    NOP2(); CART_CS1=0;
    GS_AD_IN(); NOP5();
    if(flashgbx_bus_cancelled) { CART_CS1=1; return 0; }
    CART_RD=0; NOP5();
    low=PORT_AD_L; high=PORT_AD_H;
    CART_RD=1; CART_CS1=1;
    return low|((uint16_t)high<<8);
}

bit cart_gbaProgram_single(void) __reentrant
{
    uint16_t offset,attempts,sample=0,expected;
    uint32_t address;
    uint8_t j,matches,result=0,saved_ie;
    for(j=0;j<3;j++) {
        address=fgx_state.commands[j].address;
        gs_prefix[j*5]=GS_ENCODE((uint8_t)address);
        gs_prefix[j*5+1]=GS_ENCODE((uint8_t)(address>>8));
        gs_prefix[j*5+2]=GS_ENCODE((uint8_t)(address>>16));
        gs_prefix[j*5+3]=GS_ENCODE((uint8_t)fgx_state.commands[j].value);
        gs_prefix[j*5+4]=GS_ENCODE((uint8_t)(fgx_state.commands[j].value>>8));
    }
    rom_write_addr=fgx_state.wide[0];
    CART_RD=1; CART_WR=1; CART_CS1=1;
    cart_setDirection_a(1);
    for(offset=0;offset<fgx_state.word[0];offset+=2) {
        if(flashgbx_bus_cancelled) goto finished;
        gs_target[0]=GS_ENCODE((uint8_t)rom_write_addr);
        gs_target[1]=GS_ENCODE((uint8_t)(rom_write_addr>>8));
        gs_target[2]=GS_ENCODE((uint8_t)(rom_write_addr>>16));
        gs_target[3]=GS_ENCODE(cmdBuf[offset]);
        gs_target[4]=GS_ENCODE(cmdBuf[offset+1]);
        expected=gs_target[3]|((uint16_t)gs_target[4]<<8);
        GS_AD_OUT();
        gs_write_word(gs_prefix); gs_write_word(gs_prefix+5); gs_write_word(gs_prefix+10);
        gs_write_word(gs_target);
        matches=0; attempts=20000;
        while(attempts--) {
            if(flashgbx_bus_cancelled) goto finished;
            if(attempts<19984) gs_poll_backoff();
            sample=gs_read_word();
            if(sample==expected) { if(++matches==2) break; }
            else matches=0;
        }
        if(matches!=2) {
            if(flashgbx_bus_cancelled) goto finished;
            fgx_state.word[3]=GS_DECODE((uint8_t)sample)|((uint16_t)GS_DECODE((uint8_t)(sample>>8))<<8);
            gs_reset[0]=0xf0; gs_reset[1]=0;
            cart_romWrite(0,gs_reset,1);
            goto finished;
        }
        ++rom_write_addr;
    }
    /* 取消后旧事务不能把地址写回新会话。 */
    saved_ie=IE2; IE2&=~EUSB;
    if(!flashgbx_bus_cancelled) { fgx_state.wide[0]=rom_write_addr; result=1; }
    IE2=saved_ie;
finished:
    CART_RD=1; CART_WR=1; CART_CS1=1;
    cart_setDirection_a(0); cart_setDirection_ad(0);
    return result;
}
#undef GS_AD_IN
#undef GS_AD_OUT
#undef GS_DECODE
#undef GS_ENCODE
#endif
