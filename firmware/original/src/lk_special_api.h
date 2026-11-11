#ifndef LK_SPECIAL_API_H
#define LK_SPECIAL_API_H
#include "lk_special_hal.h"
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
#endif
