#ifndef __BOARD_H__
#define __BOARD_H__
void board_setPower(uint8_t mode);
uint8_t board_getPower(void);
uint8_t board_reverse8(uint8_t value);
extern const uint8_t code board_reverse_table[256];
// PCB 三组总线按位反接，在读写物理端口时转换，协议缓冲仍保持原字节顺序。
#define CART_WRITE_A(value) (PORT_A = board_reverse_table[(uint8_t)(value)])
#define CART_WRITE_AD_L(value) (PORT_AD_L = board_reverse_table[(uint8_t)(value)])
#define CART_WRITE_AD_H(value) (PORT_AD_H = board_reverse_table[(uint8_t)(value)])
#define CART_READ_A() board_reverse_table[PORT_A]
#define CART_READ_AD_L() board_reverse_table[PORT_AD_L]
#define CART_READ_AD_H() board_reverse_table[PORT_AD_H]
#endif
