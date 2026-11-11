"""生成 IEEE CRC32 分字节查找表，放在代码区，不占用卡带缓冲 RAM。"""
from pathlib import Path

ROOT=Path(__file__).resolve().parent
values=[]
for index in range(256):
    value=index
    for _ in range(8): value=(value>>1)^(0xedb88320 if value&1 else 0)
    values.append(value)
text='/* 由 prepare_crc32_table.py 生成；IEEE CRC32，反射多项式 0xEDB88320。 */\n'
for byte in range(4):
    text+=f'const uint8_t code crc32_byte{byte}[256] = {{\n'
    for start in range(0,256,16):
        text+='    '+','.join(f'0x{(v>>(8*byte))&255:02x}' for v in values[start:start+16])+',\n'
    text+='};\n'
for byte in range(4):
    # 新板数据线反序；在 CRC 状态和表中保持同样位序，消除逐字节的反序查表。
    def reverse(v): return v
    text+=f'const uint8_t code __at (0x{0xe000+byte*256:04x}) crc32_wire{byte}[256] = {{\n'
    for start in range(0,256,16):
        text+='    '+','.join(f'0x{reverse((values[reverse(i)]>>(8*byte))&255):02x}' for i in range(start,start+16))+',\n'
    text+='};\n'
(ROOT/'src'/'crc32_table.h').write_text(text,encoding='utf-8')
