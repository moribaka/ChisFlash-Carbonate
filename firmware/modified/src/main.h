#ifndef __MAIN_H__
#define __MAIN_H__

// typedef unsigned char uint8_t;
// typedef unsigned int uint16_t ;
// typedef unsigned long uint32_t;

#include "compiler_compat.h"
#include <stdio.h>

#ifdef __SDCC
#include "STC8H_sdcc.h"
#else
#include "STC8H.h"
#endif
#ifdef __SDCC
#include "stc8h_def_sdcc.h"
#else
#include "stc8h_def.h"
#endif
#include "board.h"

#define PWR_5V P33
#define PWR_3V3 P63

#define LED_RUN P42
#define LED_ACT P43
#define LED_UNUSED_WR P44

#define CART_CS1 P60
#define CART_CS2 P46
#define CART_RD P61
#define CART_WR P62
#define CART_IRQ P45
#define CART_AD0 P07
#define CART_AD1 P06
#define CART_AD2 P05
#define CART_AD3 P04
#define CART_AD4 P03
#define CART_AD5 P02
#define CART_AD6 P01
#define CART_AD7 P00
#define CART_AD8 P77
#define CART_AD9 P76
#define CART_AD10 P75
#define CART_AD11 P74
#define CART_AD12 P73
#define CART_AD13 P72
#define CART_AD14 P71
#define CART_AD15 P70
#define CART_A16 P27
#define CART_A17 P26
#define CART_A18 P25
#define CART_A19 P24
#define CART_A20 P23
#define CART_A21 P22
#define CART_A22 P21
#define CART_A23 P20

#define PORT_A P2
#define PORT_AD_L P0
#define PORT_AD_H P7

extern BOOL ep1Busy;

uint32_t reverse4(uint32_t d);
uint16_t reverse2(uint16_t w);
uint16_t reverse2_forIRQ(uint16_t w);


#endif
