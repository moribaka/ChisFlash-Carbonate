# ChisFlash Carbonate（碳酸钙）

ChisFlash 碳酸钙 GB / GBC / GBA 卡带烧录器的固件源码，以及让官方 [FlashGBX](https://github.com/Lesserkuma/FlashGBX) 原生识别这台设备的插件。

不需要改官方 FlashGBX 的任何文件，插件放在配置目录里即可。

- 固件：`RC33-NATIVE`（改版 PCB）/ `RC33-O-NATIVE`（原版 PCB）
- 插件：`0.2.1`，目标 FlashGBX `5.2.1`
- USB 身份：`0483:0721`，产品名 `ChisFlash Carbonate` / `ChisFlash Carbonate Original`
- 许可：GPL-3.0，来源与义务见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

> 当前验证状态：本仓库的固件与插件已在实体碳酸钙上完成**连接与只读**验证（连续两次读取 16 KiB 一致，卡带标题 `LK MULTIMENU`、代码 `AGBJ`）。**实体卡带的擦除与写入尚未验证**，请先用不重要的卡带自行测试。

## 烧录器该用哪种方式连电脑

| 方式 | 设备身份 | 需要装插件 | 体验 |
| --- | --- | --- | --- |
| **原生身份 + 本插件（推荐）** | `0483:0721`，显示 `ChisFlash Carbonate` | 需要 | 名称、能力、供电信息都正确；**没有升级提示**，也不存在误刷第三方固件的入口 |
| Joey 兼容旧固件 | `0483:5740`，被当成 Joey Jr | 不需要 | 开箱即用，能读写；但**每次连接都会弹一次“有固件更新”对话框** |
| 厂商自己的上位机 | `0483:0721` 或旧身份 | 不需要 | 与 FlashGBX 无关，二者互不影响 |

### 如果你在用 Joey 兼容固件

FlashGBX 会把设备认成 BennVenn Joey Jr，并提示“A firmware update for your Joey Jr is available”。

**这个对话框必须点“否”。** 它指向的是 BennVenn 官方固件，刷进碳酸钙的 STC8 芯片会导致设备无法使用，只能拆机上 ISP 救回来。

FlashGBX 5.2.1 的界面上没有“忽略固件更新”复选框，想彻底不再被问，只能手工在 `settings.ini` 的 `[General]` 段加一行：

```ini
SkipFirmwareUpdate = enabled
```

这也是推荐改用原生固件 + 插件的主要原因：插件路径直接把这个入口关掉了，用户不会点错。

## 快速开始

### 1. 刷固件

按你的 PCB 板型选择对应 HEX，用 STC-ISP 写进烧录器：

| 板型 | HEX | USB 产品名 |
| --- | --- | --- |
| 改版 PCB | `releases/RC33-NATIVE/ChisFlash_Native_Modified_PCB.hex` | `ChisFlash Carbonate` |
| 原版 PCB | `releases/RC33-NATIVE/ChisFlash_Native_Original_PCB.hex` | `ChisFlash Carbonate Original` |

**两种板型的固件不能混刷。** 刷写后 USB 身份会变化，Windows 可能重新分配 COM 号。校验值见 [SHA256SUMS.txt](releases/RC33-NATIVE/SHA256SUMS.txt)。

### 2. 安装插件

```bash
python plugin/install.py
```

脚本会自动找到 FlashGBX 配置目录（Windows 默认 `%LOCALAPPDATA%\FlashGBX`），把 `chisflash_native.py` 放进 `plugins/` 并在 `settings.ini` 的 `[Plugins]` 段启用。也可以用 `--config` 指定目录、用 `--uninstall` 卸载。

装完重启 FlashGBX，在 `Plugins` 菜单里能看到插件为已启用。

### 3. 连接

打开 FlashGBX，设备列表出现 `ChisFlash Carbonate` 即可读写、擦除、校验卡带。插件复用官方 `LK_Device` 的通用流程，卡带配置、界面、worker 都是原样的。

## 从源码构建固件

需要 Python 3.11+ 和 SDCC 4.6.0（默认找 `C:\dev\sdcc-4.6.0`，可用环境变量 `SDCC_ROOT` 指定）：

```bash
python firmware/modified/build.py --variant pcb_bus_flashgbx_20261028   # 改版 PCB
python firmware/original/build.py --variant original_flashgbx_rc32      # 原版 PCB
```

产物写入各板目录下的 `build_<variant>/`，并生成带 SHA-256 的 `manifest.json`。构建前会自动运行 `prepare_*.py` 重新生成特殊卡带算法、CRC 表和 ROM 内核代码。

已核对：按上面两条命令重新编译，得到的 HEX 与 `releases/RC33-NATIVE/` 里的文件 **SHA-256 完全一致**（改版 `d99e4861…`、原版 `26fc8440…`），可重复构建。

## 目录结构

```
plugin/                        FlashGBX 插件与安装脚本
firmware/modified/             改版 PCB 固件源码、构建脚本
firmware/original/             原版 PCB 固件源码、构建脚本
firmware/*/upstream_lk/        来自 Lesserkuma 的上游算法源文件与许可证
releases/RC33-NATIVE/          已编译 HEX 与校验清单
flashgbx/cartridge-profiles/   ChisFlash 卡带配置（fc_DMG_ChisFlash-MBC5-PLUS.txt）
```

## 插件做了什么

- 按 USB 产品名分流后端：只有产品名匹配碳酸钙时才发 `55 AA` 身份查询，核对 `ChisFlash ... FW C15 RC33...` 后再走后续流程；不会对其它设备乱发探测命令。
- 碳酸钙后端继承官方通用 `LK_Device`，不继承 `hw_JoeyJr`，也不把固件伪装成 L 系列。
- 动态传输、大页分块、自定义 Flash 命令表、长 CRC、Bank 切换继续由固件里的兼容指令实现。
- `SupportsFirmwareUpdates()` 返回 False，第三方固件升级入口关闭。

### 能力边界

- 碳酸钙没有软件控制的卡带供电切换，插件如实报告该能力为不支持。
- 逐次 CLK 脉冲、AUDIO/RESET 写脚、RTC 等特性未适配，触发时明确报不支持，不会静默降级。
- 插件同时保留了旧设备“盖中盖”的基础原生后端（`GBA Burner for ChisFlash` 产品名），走 F6 初始化，功能范围小于碳酸钙。

## 已知问题

- **实体卡擦写未验证**：写入流程只在固件 HEX 模型上跑通了官方完整 worker，还没有对真实卡带动手。测试前请确认卡带可牺牲。
- **存在刷错的固件**：`0483:5740` 的 Joey 兼容固件仍然可用，但它会引发升级弹窗，且误点会变砖。新设备请优先刷原生固件。
- 插件依赖 FlashGBX 的内部接口（`HW_DEVICES`、`LK_Device`）。FlashGBX 升级后建议先跑一次只读连接再批量使用。

## 许可与来源

本仓库以 GPL-3.0 发布。固件包含 Lesserkuma `FlashGBX_LK_Firmware`（GPL-3.0-only，固定提交 `5ad328bf222646f58e9f531fc52da1d89cacbf3c`）的特殊卡带算法移植，插件继承 Lesserkuma `FlashGBX`（GPL-3.0-or-later）的通用设备流程。原始文件与许可证保留在 `firmware/*/upstream_lk/`，详细说明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

本项目与 BennVenn 无关，“Joey Jr” 是其产品名称，此处仅用于说明兼容关系。刷写固件有风险，请自行承担。
