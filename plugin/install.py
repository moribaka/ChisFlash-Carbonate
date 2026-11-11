#!/usr/bin/env python3
"""把 chisflash_native.py 安装到 FlashGBX 插件目录并在 settings.ini 中启用。

用法:
    python install.py
    python install.py --config "<FlashGBX 配置目录>"
    python install.py --uninstall
    python install.py --dry-run
"""
import argparse
import configparser
import json
import os
import shutil
import sys
from pathlib import Path

PLUGIN_FILE = "chisflash_native.py"
SECTION = "Plugins"


def candidate_dirs():
    """按平台列出 FlashGBX 可能使用的配置目录。"""
    home = Path.home()
    candidates = []
    if sys.platform == "win32":
        local = os.environ.get("LOCALAPPDATA")
        if local:
            candidates.append(Path(local) / "FlashGBX")
            candidates.append(Path(local) / "Programs" / "FlashGBX" / "config")
        roaming = os.environ.get("APPDATA")
        if roaming:
            candidates.append(Path(roaming) / "FlashGBX")
    elif sys.platform == "darwin":
        candidates.append(home / "Library" / "Application Support" / "FlashGBX")
        candidates.append(Path("/Applications/FlashGBX.app/Contents/MacOS/config"))
    else:
        candidates.append(home / ".config" / "FlashGBX")
        candidates.append(home / "FlashGBX")
        candidates.append(Path("/usr/share/FlashGBX/config"))
    return candidates


def pick_config_dir(explicit):
    if explicit:
        return Path(explicit).expanduser(), True
    for path in candidate_dirs():
        if (path / "settings.ini").is_file() or path.is_dir():
            return path, True
    return candidate_dirs()[0], False


def set_enabled(settings_path, enabled):
    """在 settings.ini 的 [Plugins] 段写入 {"enabled": true/false}。"""
    parser = configparser.RawConfigParser()
    parser.optionxform = str
    if settings_path.is_file():
        parser.read(settings_path, encoding="UTF-8")
    if not parser.has_section(SECTION):
        parser.add_section(SECTION)
    parser.set(SECTION, PLUGIN_FILE, json.dumps({"enabled": enabled}))
    with open(settings_path, "w", encoding="UTF-8") as handle:
        parser.write(handle)


def drop_enabled(settings_path):
    parser = configparser.RawConfigParser()
    parser.optionxform = str
    if not settings_path.is_file():
        return False
    parser.read(settings_path, encoding="UTF-8")
    if parser.has_section(SECTION) and parser.has_option(SECTION, PLUGIN_FILE):
        parser.remove_option(SECTION, PLUGIN_FILE)
        with open(settings_path, "w", encoding="UTF-8") as handle:
            parser.write(handle)
        return True
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", help="FlashGBX 配置目录，默认自动查找")
    ap.add_argument("--uninstall", action="store_true", help="删除插件文件并清除启用状态")
    ap.add_argument("--dry-run", action="store_true", help="只打印将要做的操作")
    args = ap.parse_args()

    config_dir, found = pick_config_dir(args.config)
    plugins_dir = config_dir / "plugins"
    target = plugins_dir / PLUGIN_FILE
    settings = config_dir / "settings.ini"
    source = Path(__file__).with_name(PLUGIN_FILE)

    print("配置目录:", config_dir, "" if found else "(未检测到已有 FlashGBX，按默认位置创建)")

    if args.uninstall:
        if args.dry_run:
            print("将删除:", target)
            print("将从 settings.ini 移除启用项:", settings)
            return 0
        if target.exists():
            target.unlink()
            print("已删除:", target)
        else:
            print("插件文件不存在:", target)
        print("已清除启用项" if drop_enabled(settings) else "settings.ini 中无启用项")
        print("重启 FlashGBX 生效。")
        return 0

    if not source.is_file():
        print("找不到插件源文件:", source)
        return 1
    if not (config_dir / "settings.ini").is_file():
        print("提示: 该目录下没有 settings.ini。如果你从未运行过 FlashGBX，请先启动一次再安装。")

    if args.dry_run:
        print("将复制:", source, "->", target)
        print("将写入:", settings, "[%s] %s = {\"enabled\": true}" % (SECTION, PLUGIN_FILE))
        return 0

    plugins_dir.mkdir(parents=True, exist_ok=True)
    if target.exists() and target.read_bytes() != source.read_bytes():
        backup = target.with_suffix(".py.bak")
        shutil.copy2(target, backup)
        print("已备份旧插件:", backup)
    shutil.copy2(source, target)
    print("已安装:", target)
    set_enabled(settings, True)
    print("已启用:", settings)
    print()
    print("下一步:")
    print("  1. 完全退出并重新打开 FlashGBX")
    print("  2. 在 Plugins 菜单确认插件为已启用状态")
    print("  3. 设备需要已经刷入 RC33-NATIVE 固件，USB 产品名显示为 ChisFlash Carbonate")
    print()
    print("注意: 若设备仍显示为 Joey Jr，说明刷的是旧的 Joey 兼容固件，本插件不会接管它。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
