#ifndef GBA_SAVE_H
#define GBA_SAVE_H
void gba_eeprom_read(uint16_t block, uint8_t *data, uint8_t type);
bit gba_eeprom_write(uint16_t block, uint8_t *data, uint8_t type);
void gba_rtc_read(uint8_t *data);
#endif
