#ifndef __COMPILER_COMPAT_H__
#define __COMPILER_COMPAT_H__
#ifdef __SDCC
#define bit __bit
#define code __code
#define xdata __xdata
#define _nop_() __asm nop __endasm
#define COMPILER_ISR(v) __interrupt(v)
#define KEIL_BYTE16(value, index) (((uint8_t *)&(value))[1 - (index)])
#define KEIL_BYTE32(value, index) (((uint8_t *)&(value))[3 - (index)])
#else
#include <intrins.h>
#define COMPILER_ISR(v) interrupt v
#define KEIL_BYTE16(value, index) (((uint8_t *)&(value))[index])
#define KEIL_BYTE32(value, index) (((uint8_t *)&(value))[index])
#endif
#endif
