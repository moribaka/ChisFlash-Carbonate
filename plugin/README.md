# FlashGBX 插件

`chisflash_native.py` 是单文件插件，为官方 FlashGBX 增加两个设备后端：

| 后端 | 适用设备 | USB 产品名 |
| --- | --- | --- |
| `CarbonateDevice` | 碳酸钙（RC33-NATIVE 固件） | `ChisFlash Carbonate` / `ChisFlash Carbonate Original` |
| `GbxDevice` | 盖中盖旧设备 | `GBA Burner for ChisFlash` |

目标版本：**FlashGBX 5.2.1**。官方 FlashGBX 的文件不被修改。

## 安装

```bash
python install.py                          # 自动查找配置目录
python install.py --config "D:\FlashGBX"   # 手动指定
python install.py --uninstall              # 卸载
python install.py --dry-run                # 只看会做什么
```

脚本做的事：把 `chisflash_native.py` 复制进 `<配置目录>/plugins/`，并在 `settings.ini` 的 `[Plugins]` 段写入 `{"enabled": true}`。旧文件会被备份为 `.py.bak`。

也可以手工安装：把 `chisflash_native.py` 放进插件目录，启动 FlashGBX 时选择“Load”，之后在 `Plugins` 菜单里管理。

## 工作方式

1. 读取系统缓存里的 USB 产品名来分配后端，**不靠串口试发命令**区分设备，避免误探测。
2. 只有产品名匹配碳酸钙的设备，才会发送 `55 AA` 身份查询，核对以换行结束的 `ChisFlash ... FW C15 RC33...` 身份串，再发送 `A1` 读取固件信息（协议级别、供电能力等）。
3. 碳酸钙后端继承官方通用 `LK_Device`，复用其读写、擦除、校验、卡带配置与完整的烧录 worker；不继承 `hw_JoeyJr.GbxDevice`，也不把固件标记成 L 系列。
4. `SupportsFirmwareUpdates()` 返回 `False`，`FirmwareUpdateAvailable()` 返回 `False`，第三方固件升级入口不存在——不会出现误点升级把 BennVenn 固件写进 STC8 的情况。

## 能力与边界

支持：

- GB / GBC / GBA ROM 读取、擦除、写入、校验
- 动态传输参数、大页分块、自定义 Flash 命令表、长 CRC、Bank 切换（沿用固件里的兼容指令）
- 存档读写走官方通用流程

不支持（会明确报错，不会静默降级）：

- 卡带供电的软件切换（碳酸钙无此硬件能力，插件如实报告）
- 逐次 CLK 脉冲、AUDIO / RESET 写脚
- RTC 调整
- 第三方固件升级

盖中盖后端只走基础 F6 协议：ROM 读写、ID / CFI、标准 AMD 编程可用；Intel、特殊卡带、EEPROM / Flash 存档写入、RTC 未适配。

## 兼容性提醒

插件依赖 FlashGBX 的内部接口（`HW_DEVICES`、`LK_Device`、`hw_JoeyJr`），这些不是稳定 API。FlashGBX 升级后请先做一次只读连接测试，再批量使用。

若设备在 FlashGBX 里显示为 `Joey Jr`，说明烧录器刷的是旧的 Joey 兼容固件（`0483:5740`），本插件不会接管它。请刷 `releases/RC33-NATIVE/` 里的原生固件。

## 版本记录

| 版本 | 说明 |
| --- | --- |
| 0.1.1 | 盖中盖原生后端，实机只读通过 |
| 0.2.0 | 增加碳酸钙独立后端；固定 64 字节身份读取在实机超时 |
| 0.2.1 | 身份查询改为已验证的短响应查询，实机 COM9 连通，连续两次读取 16 KiB 一致 |
