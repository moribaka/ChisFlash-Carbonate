#ifndef FGX_STATE_H
#define FGX_STATE_H
#include "main.h"
#include "flashgbx.h"
typedef struct { uint32_t address; uint16_t value; } FlashCommand;
typedef struct {
    uint32_t wide[2];
    uint16_t word[7];
    uint8_t byte[18];
    FlashCommand commands[6];
    uint8_t configured;
    uint8_t selected_voltage;
    uint32_t auto_ticks;
    uint8_t bank_count;
    uint32_t bank_address[3];
    uint8_t bank_mode[3];
} FgxState;
extern FgxState xdata fgx_state;
bit fgx_send_word(uint16_t value) FGX_STACK;
#endif
