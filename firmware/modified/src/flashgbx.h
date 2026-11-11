#ifndef __FLASHGBX_H__
#define __FLASHGBX_H__

#ifdef FLASHGBX_COMPAT
bit flashgbx_is_active(void);
void flashgbx_reset(void);
void flashgbx_identify(uint8_t padded);
void flashgbx_mark_receive(void);
bit flashgbx_packet_complete(void);
bit flashgbx_cancelled(void);
extern volatile bit flashgbx_bus_cancelled;
extern volatile bit flashgbx_stream_polling;
extern uint8_t xdata crc_state[4];
extern uint8_t xdata flashgbx_crc_fast_mode;
extern uint8_t xdata flashgbx_program_fast_mode;
extern bit flashgbx_rx_burst;
void flashgbx_tick(void);
#ifdef __SDCC
#define FGX_STACK __reentrant
#else
#define FGX_STACK reentrant
#endif
bit flashgbx_handle(void) FGX_STACK;
#endif

#endif
