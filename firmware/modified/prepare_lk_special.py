"""从固定版本的官方 LK 源码生成特殊卡带算法，保留 GPL 来源与修改说明。"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parent
UP=ROOT/'upstream_lk'
source=(UP/'LK.c').read_text(encoding='utf-8-sig')
header=(UP/'LK.h').read_text(encoding='utf-8-sig')
NAMES=['lk_dmg_mbc6_mmsa_write_flash_bytes','lk_dmg_mbc7_write','lk_dmg_mbc7_reinit',
       'lk_dmg_mbc7_set_write_access','lk_dmg_mbc7_set_cmd','lk_dmg_mbc7_read_eeprom','lk_dmg_mbc7_write_eeprom',
       'lk_dmg_eeprom_write_bytes','lk_dmg_flash_mbc6','lk_dmg_flash_E201264','lk_dmg_flash_bung_16m',
       'lk_dmg_flash_datel_orbitv2','lk_dmg_flash_mmsa','lk_dmg_mmsa_flash_command','lk_dmg_mmsa_access_mapper',
       'lk_dmg_mmsa_access_rom','lk_dmg_mmsa_access_mbc','lk_dmg_mmsa_disable_flash_write_protect','lk_dmg_mmsa_map_full',
       'lk_agb_flash2advance_buffered','lk_agb_bootup_sequence','lk_agb_flash_fujitsu_fastmode','lk_agb_flash_paged','lk_agb_flash_gbamp']

def extract(name):
    match=re.search(r'^void '+name+r'\([^\n]*?\)\s*\{',source,re.M)
    assert match,name
    start=match.end()-1; depth=0
    tokens=re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{|\}',source[start:],re.S)
    for token in tokens:
        if token[0]=='{': depth+=1
        elif token[0]=='}':
            depth-=1
            if depth==0: return source[match.start():start+token.end()]
    raise AssertionError(name)

blocks=[extract(n) for n in NAMES]
prototypes=[re.sub(r'\s*\{.*',' FGX_STACK;',b, count=1, flags=re.S).replace('()','(void)') for b in blocks]
defines='\n'.join(line for line in header.splitlines() if re.match(r'#define LK_(VAR|FLASH_|MODE_)',line))
table=re.search(r'static const __lk_fwconst u16 lk_agb_bootup_table\[\] = \{.*?\};',header,re.S)[0].replace('__lk_fwconst','code')
notice='/* SPDX-License-Identifier: GPL-3.0-only\n * 基于 Lesserkuma 的 FlashGBX_LK_Firmware；保留 upstream_lk/ 中的原文与许可证。\n * 固定提交：'+(UP/'COMMIT').read_text().strip()+'\n * 本地修改：SDCC 栈式函数、状态映射、取消检查及 Fujitsu 空白字判断。\n * 由 prepare_lk_special.py 生成。\n */\n'
text=notice+'#include "lk_special_hal.h"\n'+defines+'\n'+table+'\n'+'\n'.join(prototypes)+'\n'
for block in blocks:
    if block.startswith('void lk_agb_flash2advance_buffered'):
        # E8预写阶段读XSR，仅判断就绪；两片D0提交后的SR检查保留。
        block=block.replace('lk_agb_verify_status_register(sector_address);',
                            'lk_agb_verify_ready_register(sector_address);',1)
        block=block.replace('lk_agb_verify_status_register(sector_address + 1);',
                            'lk_agb_verify_ready_register(sector_address + 1);',1)
    if block.startswith('void lk_dmg_mbc7_write_eeprom'):
        # 保留上游 EWEN/WRITE/EWDS 次序，把多级移位拆开，避免 SDCC 临时寄存器截断。
        block='''void lk_dmg_mbc7_write_eeprom(void) {
    u16 offset, value;
    u8 bit_index;
    lk_dmg_mbc7_set_write_access(true);
    for (offset=0; offset<_lk_var16[LK_VAR16_TRANSFER_SIZE]; offset+=2) {
        lk_dmg_mbc7_set_cmd(_lk_var32[LK_VAR32_ADDRESS], LK_MODE_RAM_WRITE);
        ++_lk_var32[LK_VAR32_ADDRESS];
        value=data_buffer[offset];
        value=(value<<8)|data_buffer[offset+1];
        for(bit_index=0;bit_index<16;bit_index++) {
            lk_dmg_mbc7_write((value&0x8000)?1:0);
            value<<=1;
        }
    }
    lk_dmg_mbc7_set_write_access(false);
}'''
    # 原代码的 int 为 32 位，8051 int 只有 16 位，字节拼接后继续移位需显式扩宽。
    block=re.sub(r'(data_buffer\[[^]]+\]) << 8',r'((u32)\1) << 8',block)
    # SDCC 的 8051 寄存器分配在“调用参数内的 32 位后自增”上会丢失另一参数。
    # 地址更新移到调用之后；这些参数均不读取同一地址变量，协议语义不变。
    block=re.sub(r'(lk_\w+)\(_lk_var32\[LK_VAR32_ADDRESS\]\+\+, ([^;]+)\);',
                 r'\1(_lk_var32[LK_VAR32_ADDRESS], \2); ++_lk_var32[LK_VAR32_ADDRESS];',block)
    block=block.replace('lk_dmg_cart_write_byte((_lk_var32[LK_VAR32_ADDRESS]++) & 0xFFFF, data_buffer[x]);',
                        'lk_dmg_cart_write_byte(_lk_var32[LK_VAR32_ADDRESS] & 0xFFFF, data_buffer[x]); ++_lk_var32[LK_VAR32_ADDRESS];')
    block=re.sub(r'^(void \w+)\(\)',r'\1(void)',block)
    block=re.sub(r'RAW_AGB_ADDR_SET\(_lk_var32\[LK_VAR32_ADDRESS\]\);\s*PIN_CS_L\(\);\s*RAW_AGB_DATA_SET\((.*?)\);\s*PIN_WR_L\(\);\s*_delay_200ns\(\);\s*PIN_WR_H\(\);\s*PIN_CS_H\(\);',
                 r'lk_agb_cart_write_short(_lk_var32[LK_VAR32_ADDRESS], \1);',block,flags=re.S)
    block=block.replace('_lk_var32','fgx_state.wide').replace('_lk_var16','fgx_state.word').replace('_lk_var8','fgx_state.byte')
    block=re.sub(r'_lk_flashcmd_addr\[([^]]+)\]',r'fgx_state.commands[\1].address',block)
    block=re.sub(r'_lk_flashcmd_data\[([^]]+)\]',r'fgx_state.commands[\1].value',block)
    block=block.replace('data_buffer[x] != 0xFFFF','data != 0xFFFF')
    block=block.replace('data_buffer','cmdBuf')
    block=block.replace(' {',' FGX_STACK {',1)
    # 取消只阻止后续写操作；原始协议次序和地址更新保留。
    text+=block+'\n\n'
(ROOT/'src'/'lk_special.c').write_text(text,encoding='utf-8')
(ROOT/'src'/'lk_special_api.h').write_text('#ifndef LK_SPECIAL_API_H\n#define LK_SPECIAL_API_H\n#include "lk_special_hal.h"\n'+'\n'.join(prototypes)+'\n#endif\n',encoding='utf-8')
print('Generated',len(NAMES),'upstream algorithms')
