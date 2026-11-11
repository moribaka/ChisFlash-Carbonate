# 固件源码

碳酸钙使用 STC8H 系列 MCU，固件用 SDCC 编译（8051，`-mmcs51 --model-large`）。

| 目录 | 板型 | HEX 变体名 | 身份串 |
| --- | --- | --- | --- |
| `modified/` | 改版 PCB（总线修复版） | `pcb_bus_flashgbx_20261028` | `ChisFlash FW C15 RC33-NATIVE` |
| `original/` | 原版 PCB | `original_flashgbx_rc32` | `ChisFlash Original FW C15 RC33-O-NATIVE` |

两套源码的硬件映射不同，**不能混刷**。USB 身份为 `0483:0721`，产品名分别是 `ChisFlash Carbonate` 与 `ChisFlash Carbonate Original`。

## 构建

需要 Python 3.11+ 与 SDCC 4.6.0（`sdcc` 可执行文件需在 `SDCC_ROOT/bin` 下）：

```bash
export SDCC_ROOT=/path/to/sdcc-4.6.0        # Windows: set SDCC_ROOT=C:\dev\sdcc-4.6.0

python modified/build.py --variant pcb_bus_flashgbx_20261028
python original/build.py --variant original_flashgbx_rc32
```

产物写入 `build_<variant>/`：

- `carbonate_source_sdcc_<variant>.hex`：可直接用 STC-ISP 烧写
- `manifest.json`：HEX 的 SHA-256、字节数与全部源文件摘要
- `build.log`：完整编译输出

`build_<variant>/` 是可再生成产物，不纳入版本管理。

## 构建时自动生成的文件

`build.py` 在编译前会运行下列脚本，从仓库内的固定输入重新生成代码，因此不要手工修改生成结果：

| 脚本 | 生成内容 |
| --- | --- |
| `prepare_lk_special.py` | 从 `upstream_lk/` 固定提交生成特殊卡带算法 `src/lk_special.c` |
| `prepare_crc32_table.py` | CRC32 查表 |
| `prepare_rom_kernels.py` / `prepare_rom_crc_kernel.py` | ROM 读写与 CRC 内核 |
| `prepare_gbc_kernels.py` | GB / GBC 内核 |
| `prepare_speed_kernels.py` | 速度优化内核 |

改版板生成的内核直接使用；原版板还要经过 `prepare_original_generated.py`，把总线内核的端口映射改成原版 PCB 的走线，并参考 `reference_modified/rom_crc_legacy_kernel.h` 生成旧版 CRC 内核。因此 `reference_modified/` 是构建输入，不能删。

两步都可重复执行：生成脚本每次从模板重建内核，转换脚本再就地改写，连续构建多次结果一致。

## 源码结构

```
src/main.c              初始化与主循环
src/usb*.c              USB CDC 实现与描述符（身份、产品名在这里）
src/flashgbx.c          FlashGBX 兼容协议入口（55 AA 身份、A1 固件信息、动态参数）
src/cart_adapter.c      卡带总线时序与读写内核
src/lk_special*.c       从上游 LK 固件移植的特殊卡带算法
src/gba_save.c          GBA 存档处理
src/board.c             板级 GPIO 映射
upstream_lk/            Lesserkuma 上游原始文件与许可证（只读）
reference_modified/     原版板构建用的参考内核（仅 original/ 需要）
```

## 许可

固件包含 Lesserkuma `FlashGBX_LK_Firmware`（GPL-3.0-only）的算法移植，按 GPL-3.0 提供源码与构建脚本。详见 `THIRD_PARTY_NOTICES.md` 与仓库根目录 `LICENSE`。
