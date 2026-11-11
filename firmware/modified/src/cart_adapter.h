#ifndef __CART_ADAPTER_H_
#define __CART_ADAPTER_H_

void cart_setDirection_ad(uint8_t dir);
void cart_setDirection_a(uint8_t dir);

// uint16_t cart_readBus_ad();
// uint8_t cart_readBus_a();

// void cart_writeBus_ad(uint16_t value);
// void cart_writeBus_a(uint8_t value);

void cart_gbaRomAddrSetup(uint32_t addr);

void cart_romRead(uint32_t addr, uint8_t *buf, uint16_t len);
void cart_romRead_continuous(uint32_t addr, uint8_t *buf, uint16_t len);

void cart_romWrite(uint32_t addr, uint8_t *buf, uint16_t len);
void cart_romWrite_continuous(uint32_t addr, uint8_t *buf, uint16_t len);
#if defined(__SDCC) && defined(ROM_WRITE_XDATA_FAST)
void cart_romWrite_data(uint32_t addr, uint8_t __xdata *buf, uint16_t len);
void cart_romRead_data(uint32_t addr, uint8_t __xdata *buf, uint16_t len);
extern uint8_t __xdata cart_verify_actual;
bit cart_romVerify_data(uint32_t addr, uint8_t __xdata *expected, uint16_t len);
#ifdef FLASHGBX_COMPAT
bit cart_romCRC32(uint32_t addr,uint32_t length) __reentrant;
extern uint8_t __xdata __at (0x0000) fgx_read_page[64];
uint8_t cart_reverse_table_prepare(void) __reentrant;
void cart_romRead_stream_begin(uint32_t addr);
void cart_romRead_stream64(void);
void cart_romRead_stream_end(void);
#endif
#else
#define cart_romWrite_data cart_romWrite
#define cart_romRead_data cart_romRead
#endif

void cart_ramRead(uint32_t addr, uint8_t *buf, uint16_t len);
void cart_ramWrite(uint32_t addr, uint8_t *buf, uint16_t len);

void cart_gbcRead(uint32_t addr, uint8_t *buf, uint16_t len);
void cart_gbcWrite(uint32_t addr, uint8_t *buf, uint16_t len);
#if defined(__SDCC) && defined(FLASHGBX_COMPAT)
void cart_gbcWrite_block(uint16_t addr,const uint8_t __xdata *buf,uint16_t len);
bit cart_gbcVerify_block(uint16_t addr,const uint8_t __xdata *expected,uint16_t len) __reentrant;
bit cart_gbcCRC32(uint16_t addr,uint16_t len) __reentrant;
extern bit flashgbx_dmg_pipeline;
extern bit flashgbx_gbc_batch_fast;
extern bit flashgbx_gbc_fast_timing;
extern bit cart_gbc_hold_writes;
void cart_gbcWrite_finish(void);
void cart_gbcRead_block(uint16_t addr,uint8_t __xdata *buf,uint16_t len) __reentrant;
bit cart_gbcProgram_single(void) __reentrant;
bit cart_gbaProgram_single(void) __reentrant;
#endif
#ifdef GBC_READ_DIAGNOSTICS
void cart_gbcProbe(uint32_t addr, uint8_t xdata *samples, uint8_t input_mode);
#endif

#endif
