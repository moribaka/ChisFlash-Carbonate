/* SPDX-License-Identifier: GPL-3.0-only
 * 基于 Lesserkuma 的 FlashGBX_LK_Firmware；保留 upstream_lk/ 中的原文与许可证。
 * 固定提交：5ad328bf222646f58e9f531fc52da1d89cacbf3c
 * 本地修改：SDCC 栈式函数、状态映射、取消检查及 Fujitsu 空白字判断。
 * 由 prepare_lk_special.py 生成。
 */
#include "lk_special_hal.h"
#define LK_MODE_DMG					1
#define LK_MODE_AGB					2
#define LK_MODE_ROM_READ			1
#define LK_MODE_ROM_WRITE			2
#define LK_MODE_RAM_READ			3
#define LK_MODE_RAM_WRITE			4
#define LK_FLASH_COMMAND_SET_AMD				1
#define LK_FLASH_COMMAND_SET_INTEL				2
#define LK_FLASH_METHOD_UNBUFFERED				1
#define LK_FLASH_METHOD_BUFFERED				2
#define LK_FLASH_METHOD_DMG_MMSA				3
#define LK_FLASH_METHOD_DMG_MBC6				4
#define LK_FLASH_METHOD_AGB_FLASH2ADVANCE		5
#define LK_FLASH_METHOD_AGB_FUJITSU_FASTMODE	6
#define LK_FLASH_METHOD_AGB_SAVE_FLASH			7
#define LK_FLASH_METHOD_AGB_PAGED				8
#define LK_FLASH_METHOD_DMG_DATEL_ORBITV2		9
#define LK_FLASH_METHOD_DMG_E201264				10
#define LK_FLASH_METHOD_AGB_GBAMP				11
#define LK_FLASH_METHOD_DMG_BUNG_16M			12
#define LK_FLASH_WE_PIN_WR						1
#define LK_FLASH_WE_PIN_AUDIO					2
#define LK_FLASH_WE_PIN_WR_RESET				3
#define LK_VAR32_ADDRESS					0x00
#define LK_VAR32_AUTO_POWEROFF_TIME			0x01
#define LK_VAR16_TRANSFER_SIZE				0x00
#define LK_VAR16_BUFFER_SIZE				0x01
#define LK_VAR16_DMG_ROM_BANK				0x02
#define LK_VAR16_STATUS_REGISTER			0x03
#define LK_VAR16_LAST_BANK_ACCESSED			0x04
#define LK_VAR16_STATUS_REGISTER_MASK		0x05
#define LK_VAR16_STATUS_REGISTER_VALUE		0x06
#define LK_VAR8_CART_MODE					0x00
#define LK_VAR8_DMG_ACCESS_MODE				0x01
#define LK_VAR8_FLASH_COMMAND_SET			0x02
#define LK_VAR8_FLASH_METHOD				0x03
#define LK_VAR8_FLASH_WE_PIN				0x04
#define LK_VAR8_FLASH_PULSE_RESET			0x05
#define LK_VAR8_FLASH_COMMANDS_BANK_1		0x06
#define LK_VAR8_FLASH_SHARP_VERIFY_SR		0x07
#define LK_VAR8_DMG_READ_CS_PULSE			0x08
#define LK_VAR8_DMG_WRITE_CS_PULSE			0x09
#define LK_VAR8_FLASH_DOUBLE_DIE			0x0A
#define LK_VAR8_DMG_READ_METHOD				0x0B
#define LK_VAR8_AGB_READ_METHOD				0x0C
#define LK_VAR8_CART_POWERED				0x0D
#define LK_VAR8_PULLUPS_ENABLED				0x0E
#define LK_VAR8_AUTO_POWEROFF_ENABLED		0x0F
#define LK_VAR8_AGB_IRQ_ENABLED				0x10
#define LK_VAR8_DMG_AUDIO_ENABLED			0x11
static const code u16 lk_agb_bootup_table[] = {
	0x479B, 0x7426, 0x11BC, 0x6D4F, 0x11BD, 0x32F1, 0x7FD9, 0x2CE7,
	0x5DA5, 0x11BD, 0x4610, 0x5DA4, 0x4E90, 0x6173, 0x2A84, 0x4E91,
	0x106A, 0x75FE, 0x29C8, 0x7839, 0x420E, 0x5D1B, 0x7838, 0x12A8,
	0x3F7D, 0x67B9, 0x26F3, 0x54EF, 0x7C23, 0x26F2, 0x6BC6, 0x4137,
	0x15AB, 0x730D, 0x6BC7, 0x3B4F, 0x5F24, 0x3DDA, 0x253F, 0x1749,
	0x3DDB, 0x70E6, 0x746C, 0x30F7, 0x531F, 0x6738, 0x531E, 0x1A51,
	0x1971, 0x5B7D, 0x4ED6, 0x1970, 0x3F27, 0x75CB, 0x3D62, 0x128C,
	0x74B8, 0x2FAD, 0x74B9, 0x64FD, 0x6C9A, 0x4F3A, 0x276D, 0x73EF,
	0x38B1, 0x4F3B, 0x571E, 0x7EA3, 0x6249, 0x3587, 0x1B7C, 0x3586,
	0x7AFB, 0x67E4, 0x5C92, 0x67E5, 0x2BCA, 0x438C, 0x2E6F, 0x587F,
	0x14B7, 0x2E6E, 0x4CB9, 0x6FA2, 0x38F0, 0x719E, 0x475A, 0x1F3C,
	0x6AD8, 0x475B, 0x5199, 0x3264, 0x7B41, 0x49EF, 0x5198, 0x1CD7
};
void lk_dmg_mbc6_mmsa_write_flash_bytes(void) FGX_STACK;
void lk_dmg_mbc7_write(u8 value) FGX_STACK;
void lk_dmg_mbc7_reinit(void) FGX_STACK;
void lk_dmg_mbc7_set_write_access(u8 enable) FGX_STACK;
void lk_dmg_mbc7_set_cmd(u16 address, u8 mode) FGX_STACK;
void lk_dmg_mbc7_read_eeprom(void) FGX_STACK;
void lk_dmg_mbc7_write_eeprom(void) FGX_STACK;
void lk_dmg_eeprom_write_bytes(u16 transfer_size, u16 buffer_offset, u32 sector_address) FGX_STACK;
void lk_dmg_flash_mbc6(u16 buffer_offset) FGX_STACK;
void lk_dmg_flash_E201264(u16 buffer_offset) FGX_STACK;
void lk_dmg_flash_bung_16m(u16 buffer_offset) FGX_STACK;
void lk_dmg_flash_datel_orbitv2(void) FGX_STACK;
void lk_dmg_flash_mmsa(u16 buffer_offset) FGX_STACK;
void lk_dmg_mmsa_flash_command(u16 addr, u8 data) FGX_STACK;
void lk_dmg_mmsa_access_mapper(void) FGX_STACK;
void lk_dmg_mmsa_access_rom(void) FGX_STACK;
void lk_dmg_mmsa_access_mbc(bool enable) FGX_STACK;
void lk_dmg_mmsa_disable_flash_write_protect(void) FGX_STACK;
void lk_dmg_mmsa_map_full(void) FGX_STACK;
void lk_agb_flash2advance_buffered(u16 transfer_size, u16 buffer_offset, u32 sector_address) FGX_STACK;
void lk_agb_bootup_sequence(void) FGX_STACK;
void lk_agb_flash_fujitsu_fastmode(void) FGX_STACK;
void lk_agb_flash_paged(u16 buffer_offset) FGX_STACK;
void lk_agb_flash_gbamp(void) FGX_STACK;
void lk_dmg_mbc6_mmsa_write_flash_bytes(void) FGX_STACK {
	RAW_DMG_DATA_DIR_OUT();
	for (u32 x = 0; x < fgx_state.word[LK_VAR16_TRANSFER_SIZE]; x++) {
		lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], cmdBuf[x]); ++fgx_state.wide[LK_VAR32_ADDRESS];
	}
	RAW_DMG_DATA_SET(0);
	RAW_DMG_DATA_DIR_IN();
}

void lk_dmg_mbc7_write(u8 value) FGX_STACK {
	lk_dmg_cart_write_byte(0xA080, 0x80 + (value << 1));
	lk_dmg_cart_write_byte(0xA080, 0xC0 + (value << 1));
}

void lk_dmg_mbc7_reinit(void) FGX_STACK {
	lk_dmg_cart_write_byte(0xA080, 0x00);
	lk_dmg_cart_write_byte(0xA080, 0x80);
	_delay_ms(15); // seems to work from at least 6 ms, so 15 ms should be safe
}

void lk_dmg_mbc7_set_write_access(u8 enable) FGX_STACK {
	lk_dmg_mbc7_reinit();
	lk_dmg_mbc7_write(0);
	lk_dmg_mbc7_write(1);
	lk_dmg_mbc7_write(0);
	lk_dmg_mbc7_write(0);
	if (enable == true) {
		lk_dmg_mbc7_write(1);
		lk_dmg_mbc7_write(1);
	} else {
		lk_dmg_mbc7_write(0);
		lk_dmg_mbc7_write(0);
	}
	for (u8 x=0; x < 6; x++) {
		lk_dmg_mbc7_write(0);
	}
}

void lk_dmg_mbc7_set_cmd(u16 address, u8 mode) FGX_STACK {
	// Re-init and commit previous action(?)
	lk_dmg_mbc7_reinit();
	
	// Command sequence
	lk_dmg_mbc7_write(0);
	lk_dmg_mbc7_write(1);
	if (mode == LK_MODE_RAM_READ) {
		lk_dmg_mbc7_write(1);
		lk_dmg_mbc7_write(0);
	} else { /*if (mode == LK_MODE_RAM_WRITE) {*/
		lk_dmg_mbc7_write(0);
		lk_dmg_mbc7_write(1);
	}
	
	// Set address; bitwise, 80/C0=0, 82/C2=1
	for (u8 x = 0; x < 8; x++) {
		if (((address << x) >> 7) & 1) {
			lk_dmg_mbc7_write(1);
		} else {
			lk_dmg_mbc7_write(0);
		}
	}
}

void lk_dmg_mbc7_read_eeprom(void) FGX_STACK {
	for (u32 x = 0; x < fgx_state.word[LK_VAR16_TRANSFER_SIZE]; x += 2) {
		lk_dmg_mbc7_set_cmd(fgx_state.wide[LK_VAR32_ADDRESS], LK_MODE_RAM_READ); ++fgx_state.wide[LK_VAR32_ADDRESS];
		u16 data = 0;
		for (u8 y = 0; y < 16; y++) {
			lk_dmg_mbc7_write(0);
			data = (data << 1) | (lk_dmg_cart_read_byte(0xA080) & 1);
		}
		lk_conn_send_u16(data);
	}
}

void lk_dmg_mbc7_write_eeprom(void) FGX_STACK {
    u16 offset, value;
    u8 bit_index;
    lk_dmg_mbc7_set_write_access(true);
    for (offset=0; offset<fgx_state.word[LK_VAR16_TRANSFER_SIZE]; offset+=2) {
        lk_dmg_mbc7_set_cmd(fgx_state.wide[LK_VAR32_ADDRESS], LK_MODE_RAM_WRITE);
        ++fgx_state.wide[LK_VAR32_ADDRESS];
        value=cmdBuf[offset];
        value=(value<<8)|cmdBuf[offset+1];
        for(bit_index=0;bit_index<16;bit_index++) {
            lk_dmg_mbc7_write((value&0x8000)?1:0);
            value<<=1;
        }
    }
    lk_dmg_mbc7_set_write_access(false);
}

void lk_dmg_eeprom_write_bytes(u16 transfer_size, u16 buffer_offset, u32 sector_address) FGX_STACK {
	RAW_DMG_DATA_DIR_OUT();
	if (fgx_state.byte[LK_VAR8_FLASH_COMMANDS_BANK_1] == 1) {
		lk_dmg_change_bank(1);
	}
	lk_dmg_flash_write_byte(fgx_state.commands[0].address, fgx_state.commands[0].value);			// AAA=AA
	lk_dmg_flash_write_byte(fgx_state.commands[1].address, fgx_state.commands[1].value);			// 555=55
	lk_dmg_flash_write_byte(fgx_state.commands[2].address, fgx_state.commands[2].value);			// AAA=A0
	if (fgx_state.byte[LK_VAR8_FLASH_COMMANDS_BANK_1] == 1) {
		lk_dmg_change_bank(fgx_state.word[LK_VAR16_LAST_BANK_ACCESSED]);
	}
	
	for (u32 x = 0; x < transfer_size; x++) {
		lk_dmg_flash_write_byte(sector_address+x, cmdBuf[buffer_offset+x]);
	}
	_delay_ms(4);

	fgx_state.wide[LK_VAR32_ADDRESS] += transfer_size;
	RAW_DMG_DATA_SET(0);
	RAW_DMG_DATA_DIR_IN();
}

void lk_dmg_flash_mbc6(u16 buffer_offset) FGX_STACK {
	RAW_DMG_DATA_DIR_OUT();
	
	// Set Sub-Bank A (0x4000-0x5FFF) to flash bank 1
	// Set Sub-Bank B (0x6000-0x7FFF) to flash bank 2
	lk_dmg_cart_write_byte(0x2800, 0x08);
	lk_dmg_cart_write_byte(0x3800, 0x08);
	lk_dmg_cart_write_byte(0x2000, 0x01);
	lk_dmg_cart_write_byte(0x3000, 0x02);
	
	// Flash write command sequence
	lk_dmg_cart_write_byte(0x7555, 0xAA);
	lk_dmg_cart_write_byte(0x4AAA, 0x55);
	lk_dmg_cart_write_byte(0x7555, 0xA0);
	
	// Restore bank
	lk_dmg_cart_write_byte(0x2800, 0x08);
	lk_dmg_cart_write_byte(0x3800, 0x08);
	lk_dmg_cart_write_byte(0x2000, fgx_state.word[LK_VAR16_DMG_ROM_BANK]);
	lk_dmg_cart_write_byte(0x3000, fgx_state.word[LK_VAR16_DMG_ROM_BANK]);
	
	// Write data
	for (u16 x = 0; x < 128; x++) {
		lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], cmdBuf[buffer_offset+x]); ++fgx_state.wide[LK_VAR32_ADDRESS];
	}
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS]-1, 0x00);
	
	// Status register check
	lk_dmg_verify_status_register(fgx_state.wide[LK_VAR32_ADDRESS]-1);
}

void lk_dmg_flash_E201264(u16 buffer_offset) FGX_STACK {
	lk_dmg_cart_write_byte(0x4000, 0xFF);
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 0xE0);
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 128 - 1);
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 0x00);
	for (u16 x = 0; x < 128; x++) {
		lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS]+x, cmdBuf[buffer_offset+x]);
	}
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 0x0C);
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 128 - 1);
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], 0x00);
	fgx_state.wide[LK_VAR32_ADDRESS] += 128;
	lk_dmg_cart_write_byte(0x4000, 0x70);
	lk_dmg_verify_status_register(fgx_state.wide[LK_VAR32_ADDRESS] - 1);
	lk_dmg_cart_write_byte(0x4000, 0xFF);
}

void lk_dmg_flash_bung_16m(u16 buffer_offset) FGX_STACK {
	lk_dmg_change_bank(2);
	lk_dmg_flash_write_byte(0x6AAA, 0xAA);
	lk_dmg_change_bank(1);
	lk_dmg_flash_write_byte(0x5554, 0x55);
	lk_dmg_change_bank(2);
	lk_dmg_flash_write_byte(0x6AAA, 0xA0);
	lk_dmg_change_bank(fgx_state.word[LK_VAR16_LAST_BANK_ACCESSED]);
	for (u16 x = 0; x < 128; x++) {
		lk_dmg_flash_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], cmdBuf[buffer_offset+x]); ++fgx_state.wide[LK_VAR32_ADDRESS];
	}
	_delay_us(100);
	lk_dmg_verify_status_register(fgx_state.wide[LK_VAR32_ADDRESS] - 1);
}

void lk_dmg_flash_datel_orbitv2(void) FGX_STACK {
	for (u16 x = 0; x < fgx_state.word[LK_VAR16_TRANSFER_SIZE]; x++) {
		lk_dmg_cart_write_byte(0x7FE1, 0x02);
		lk_dmg_cart_write_byte(0x5555, 0xAA);
		lk_dmg_cart_write_byte(0x2AAA, 0x55);
		lk_dmg_cart_write_byte(0x5555, 0xA0);
		lk_dmg_cart_write_byte(0x7FE1, fgx_state.wide[LK_VAR32_ADDRESS] >> 24);
		lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS] & 0xFFFF, cmdBuf[x]); ++fgx_state.wide[LK_VAR32_ADDRESS];
		_delay_us(20);
	}
}

void lk_dmg_flash_mmsa(u16 buffer_offset) FGX_STACK {
	// Thanks to nocash et. al. for the initial research
	#ifdef DISABLE_INTERRUPTS
		DISABLE_INTERRUPTS();
	#endif
	lk_dmg_mmsa_access_mbc(true);
	lk_dmg_cart_write_byte(0x2100, 1);
	lk_dmg_mmsa_access_mbc(false);
	lk_dmg_mmsa_access_mapper();
	lk_dmg_mmsa_disable_flash_write_protect();
	lk_dmg_mmsa_map_full();
	lk_dmg_mmsa_flash_command(0x5555, 0xAA);
	lk_dmg_mmsa_flash_command(0x2AAA, 0x55);
	lk_dmg_mmsa_flash_command(0x5555, 0xA0);
	lk_dmg_mmsa_access_mbc(true);
	lk_dmg_cart_write_byte(0x2100, fgx_state.word[LK_VAR16_DMG_ROM_BANK]);
	lk_dmg_mmsa_access_mbc(false);
	lk_dmg_mmsa_access_rom();
	for (u8 x = 0; x < 128; x++) {
		lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS], cmdBuf[buffer_offset+x]); ++fgx_state.wide[LK_VAR32_ADDRESS];
	}
	lk_dmg_cart_write_byte(fgx_state.wide[LK_VAR32_ADDRESS] - 1, 0xFF);
	_delay_ms(4);
	lk_dmg_verify_status_register(fgx_state.wide[LK_VAR32_ADDRESS] - 1);
	#ifdef ENABLE_INTERRUPTS
		ENABLE_INTERRUPTS();
	#endif
}

void lk_dmg_mmsa_flash_command(u16 addr, u8 data) FGX_STACK {
	lk_dmg_cart_write_byte(0x120, 0x0F);
	lk_dmg_cart_write_byte(0x125, addr >> 8);
	lk_dmg_cart_write_byte(0x126, addr & 0xFF);
	lk_dmg_cart_write_byte(0x127, data);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
}

void lk_dmg_mmsa_access_mapper(void) FGX_STACK {
	lk_dmg_cart_write_byte(0x120, 0x09);
	lk_dmg_cart_write_byte(0x121, 0xAA);
	lk_dmg_cart_write_byte(0x122, 0x55);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
}

void lk_dmg_mmsa_access_rom(void) FGX_STACK {
	lk_dmg_cart_write_byte(0x120, 0x08);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
}

void lk_dmg_mmsa_access_mbc(bool enable) FGX_STACK {
	if (enable) {
		lk_dmg_cart_write_byte(0x120, 0x11);
		lk_dmg_cart_write_byte(0x13F, 0xA5);
	} else {
		lk_dmg_cart_write_byte(0x120, 0x10);
		lk_dmg_cart_write_byte(0x13F, 0xA5);
	}
	_delay_us(10);
}

void lk_dmg_mmsa_disable_flash_write_protect(void) FGX_STACK {
	// stat.bit0
	lk_dmg_cart_write_byte(0x120, 0x0A);
	lk_dmg_cart_write_byte(0x125, 0x62);
	lk_dmg_cart_write_byte(0x126, 0x04);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
	// stat.bit1
	lk_dmg_cart_write_byte(0x120, 0x02);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
}

void lk_dmg_mmsa_map_full(void) FGX_STACK {
	lk_dmg_cart_write_byte(0x120, 0x04);
	lk_dmg_cart_write_byte(0x13F, 0xA5);
	_delay_us(10);
}

void lk_agb_flash2advance_buffered(u16 transfer_size, u16 buffer_offset, u32 sector_address) FGX_STACK {
	u16 buffer_size = (fgx_state.word[LK_VAR16_BUFFER_SIZE] >> 1);
	RAW_AGB_ADDR_DIR_OUT();
	
	lk_agb_cart_write_short(sector_address, fgx_state.commands[0].value);			// SA=E8
	lk_agb_cart_write_short(sector_address + 1, fgx_state.commands[0].value);		// SA+1=E8
	lk_agb_verify_ready_register(sector_address);
	lk_agb_verify_ready_register(sector_address + 1);
	lk_agb_cart_write_short(sector_address, (buffer_size >> 1) - 1);		// SA=BS/2
	lk_agb_cart_write_short(sector_address + 1, (buffer_size >> 1) - 1);	// SA+1=BS/2
	
	for (u16 x = 0; x < transfer_size; x += 2) {							// PA=PD
		lk_agb_cart_write_short(fgx_state.wide[LK_VAR32_ADDRESS], ((u32)cmdBuf[buffer_offset+x+1]) << 8 | cmdBuf[buffer_offset+x]);
		
		fgx_state.wide[LK_VAR32_ADDRESS]++;
	}

	// Write buffer to flash
	lk_agb_cart_write_short(sector_address, fgx_state.commands[3].value);			// SA=D0
	lk_agb_cart_write_short(sector_address + 1, fgx_state.commands[3].value);		// SA+1=D0
	lk_agb_verify_status_register(sector_address);
	lk_agb_verify_status_register(sector_address + 1);
	lk_agb_cart_write_short(sector_address, fgx_state.commands[4].value);			// SA=FF
	lk_agb_cart_write_short(sector_address + 1, fgx_state.commands[4].value);		// SA+1=FF
}

void lk_agb_bootup_sequence(void) FGX_STACK { // Thanks TachY3n
	u16 x, y, z, value = 0;
	u32 b, hash = 0;

	// 3D Memory GBA Video cartridges need this
	PIN_WR_L();
	PIN_RD_L();
	_delay_us(1);
	PIN_RD_H();
	PIN_WR_H();

	value = lk_agb_cart_read_short(0x5A);
	value = (value & 0x80) ? 0 : 0xF000;
	lk_agb_cart_read_short(0xFF0FF0 + value);
	lk_agb_cart_read_short(0xFF0FF1 + value);
	lk_agb_cart_read_short(0xFF0FF2 + value);
	lk_agb_cart_read_short(0xFF0FF3 + value);
	lk_agb_cart_read_short(0xFF0FF4 + value);
	lk_agb_cart_read_short(0xFF0FF5 + value);
	lk_agb_cart_read_short(0xFF0FF6 + value);
	lk_agb_cart_read_short(0xFF0FF7 + value);
	lk_agb_cart_read_short(0xFF0FF8 + value);
	lk_agb_cart_read_short(0xFF0FF9 + value);
	
	for (x = 0x9C; x < 0xB8; x += 2) {
		for (u8 y = 0; y < 2; y++) {
			if ((x == 0x9C) && (y == 0)) continue;
			value = lk_agb_cart_read_short(x >> 1);
			b = (value >> (y * 8)) & 0xFF;
			hash = (hash >> 3) | (hash << 29);
			hash = hash ^ b;
			b = b << 8;
			hash = hash ^ b;
			b = b << 8;
			hash = hash ^ b;
			b = b << 8;
			hash = hash ^ b;
		}
	}
	hash <<= 27;
	hash >>= 30;
	
	u32 tindex = 6 * hash + ((lk_agb_cart_read_short(0x9E >> 1) & 0xFF) & 3) * 0x18;

	for (x = 2; x < 42; ++x) {
		lk_agb_cart_read_short(x);
	}

	for (y = 0; y < 6; ++y) {
		u32 address = lk_agb_bootup_table[tindex++];
		lk_agb_cart_read_short(address);
		for (z = 10 * y; z < 10 * (y + 1); ++z) {
			lk_agb_cart_read_short(x + z);
		}
	}
	for (z = 10 * y; z < 10 * (y + 1); ++z) {
		lk_agb_cart_read_short(x + z);
	}
}

void lk_agb_flash_fujitsu_fastmode(void) FGX_STACK {
	u16 data;
	lk_agb_cart_write_short(fgx_state.commands[0].address, fgx_state.commands[0].value);	// AAA=AAA9
	lk_agb_cart_write_short(fgx_state.commands[1].address, fgx_state.commands[1].value);	// 555=5556
	lk_agb_cart_write_short(fgx_state.commands[2].address, fgx_state.commands[2].value);	// AAA=2020
	for (u16 x = 0; x < fgx_state.word[LK_VAR16_TRANSFER_SIZE]; x += 2) {
		data = ((u32)cmdBuf[x + 1]) << 8 | cmdBuf[x];
		if (data != 0xFFFF) {
			lk_agb_cart_write_short(fgx_state.wide[LK_VAR32_ADDRESS], fgx_state.commands[3].value);	// AAA=A0A0
			lk_agb_cart_write_short(fgx_state.wide[LK_VAR32_ADDRESS], data);					// PA=PD
			lk_agb_verify_data(fgx_state.wide[LK_VAR32_ADDRESS], data);
			if (_timeout_check()) break;
		}
		fgx_state.wide[LK_VAR32_ADDRESS]++;
	}
	lk_agb_cart_write_short(fgx_state.commands[4].address, fgx_state.commands[4].value);	// 0=F0F0
}

void lk_agb_flash_paged(u16 buffer_offset) FGX_STACK {
	u16 data;
	u32 page_address = fgx_state.wide[LK_VAR32_ADDRESS];
	lk_agb_cart_write_short(fgx_state.commands[0].address, fgx_state.commands[0].value);			// 0=70
	lk_agb_verify_status_register(page_address);
	if (_timeout_check()) return;
	lk_agb_cart_write_short(fgx_state.commands[1].address, fgx_state.commands[1].value);			// 0=41
	for (u16 x = 0; x < fgx_state.word[LK_VAR16_BUFFER_SIZE]; x += 2) {
		data = ((u32)cmdBuf[buffer_offset + x + 1]) << 8 | cmdBuf[buffer_offset + x];
		lk_agb_cart_write_short(fgx_state.wide[LK_VAR32_ADDRESS], data);				// PA=PD
		fgx_state.wide[LK_VAR32_ADDRESS]++;
	}
	lk_agb_verify_status_register(page_address);
}

void lk_agb_flash_gbamp(void) FGX_STACK {
	u32 addr;
	u16 data;
	for (u16 x = 0; x < fgx_state.word[LK_VAR16_TRANSFER_SIZE]; x += 2) {
		lk_agb_cart_write_short(0x213C2, 0xAA);
		lk_agb_cart_write_short(0x10C3D, 0x55);
		lk_agb_cart_write_short(0x213C2, 0xA0);
		addr = fgx_state.wide[LK_VAR32_ADDRESS]++;
		addr = (addr >> 13 << 16) | (addr & 0x1FFF);
		data = ((u32)cmdBuf[x + 1]) << 8 | cmdBuf[x];
		lk_agb_cart_write_short(addr, data);
		lk_agb_verify_data(addr, data);
		if (_timeout_check()) break;
	}
}

