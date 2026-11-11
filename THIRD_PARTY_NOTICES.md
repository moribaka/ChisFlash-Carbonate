# 来源与许可证

本仓库整体以 **GPL-3.0** 发布，完整许可证见根目录 `LICENSE`。各部分来源如下。

## 1. Lesserkuma — FlashGBX_LK_Firmware（GPL-3.0-only）

固件中的特殊卡带算法移植自 Lesserkuma 的 LK 固件：

- 上游：https://github.com/Lesserkuma/FlashGBX_LK_Firmware
- 固定提交：`5ad328bf222646f58e9f531fc52da1d89cacbf3c`（见 `firmware/*/upstream_lk/COMMIT`）
- 原作者：Lesserkuma
- 许可证：GPL-3.0-only，全文保留于 `firmware/*/upstream_lk/LICENSE`

原始文件完整保留在 `firmware/*/upstream_lk/`。本地派生代码为 `src/lk_special.c`、`src/lk_special_api.h` 与硬件适配层 `src/lk_special_hal.*`，由 `prepare_lk_special.py` 从固定上游版本重新生成。

本地修改包括：适配 STC8H GPIO、SDCC 栈式调用、状态与缓冲区映射、断连取消、超时错误、Flash2Advance 总线调用封装、Fujitsu 空白字判断，以及 8051 上 16 位 int 与参数自增问题。

## 2. Lesserkuma — FlashGBX（GPL-3.0-or-later）

`plugin/chisflash_native.py` 是 FlashGBX 的插件，运行时导入并继承其 `LK_Device`、`hw_JoeyJr`、`RomFileAGB`、`RomFileDMG` 等模块：

- 上游：https://github.com/Lesserkuma/FlashGBX
- 目标版本：5.2.1
- 许可证：GPL-3.0-or-later

插件不复制、不修改 FlashGBX 的安装文件，只在其进程内注册新的设备后端。因此插件的分发同样受 GPL 约束，源码即本仓库中的单个 `.py` 文件。

## 3. STC8H 与 USB 示例代码

固件基于 STC8H 系列 MCU，`firmware/*/src/stc8h_def*.h`、`stc_startup.c`、USB 相关文件保留了原 STC 示例代码的作者与来源说明。这部分代码按其原始条款使用。

## 4. BennVenn

本项目不使用 BennVenn 的任何代码。`0483:5740` 的 Joey 兼容身份与协议帧格式是按公开文档与实测行为实现的兼容层，用于让既有工具识别设备。

“Joey Jr” 是 BennVenn 的产品名称。本项目与 BennVenn 无隶属或授权关系，相关名称仅用于说明兼容性。

## 5. 厂商卡带配置文件

`flashgbx/cartridge-profiles/fc_DMG_ChisFlash-MBC5-PLUS.txt` 是 ChisFlash 自家卡带（MBC5）的 FlashGBX 卡带配置，随固件适配发布。

## 再分发注意

如果你分发本仓库编译出的 HEX 或修改版插件，需要同时按 GPL-3.0 提供对应源码。仓库已包含固件源码、构建脚本与插件源码，直接分发本仓库或指向本仓库即可满足。
