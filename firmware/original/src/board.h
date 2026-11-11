#ifndef __BOARD_H__
#define __BOARD_H__
void board_setPower(uint8_t mode);
uint8_t board_getPower(void);
// 原版三组总线按位顺接，直接读写端口。
#define CART_WRITE_A(value) (PORT_A = (uint8_t)(value))
#define CART_WRITE_AD_L(value) (PORT_AD_L = (uint8_t)(value))
#define CART_WRITE_AD_H(value) (PORT_AD_H = (uint8_t)(value))
#define CART_READ_A() (PORT_A)
#define CART_READ_AD_L() (PORT_AD_L)
#define CART_READ_AD_H() (PORT_AD_H)
#endif
