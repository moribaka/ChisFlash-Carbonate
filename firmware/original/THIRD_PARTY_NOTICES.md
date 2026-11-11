# 来源与许可证

本目录固件包含 Lesserkuma 的 FlashGBX LK 固件特殊卡带算法：

- 上游：https://github.com/Lesserkuma/FlashGBX_LK_Firmware
- 固定版本：`upstream_lk/COMMIT`
- 原作者：Lesserkuma
- 许可证：GPL-3.0-only，全文见 `upstream_lk/LICENSE`

原始文件保留于 `upstream_lk/`，本地派生代码为 `src/lk_special.c`、`src/lk_special_api.h` 及硬件适配层 `src/lk_special_hal.*`。`prepare_lk_special.py` 可从固定上游版本重新生成算法文件。

固件基于 STC8H MCU，`src/stc8h_def*.h`、`src/stc_startup.c` 与 USB 相关文件保留原 STC 示例代码的作者与来源说明。

仓库整体许可证与其它上游说明见根目录 `THIRD_PARTY_NOTICES.md`。
