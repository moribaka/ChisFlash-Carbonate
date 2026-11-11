"""从原碳酸钙工程移植的 SDCC 源码执行完整构建。"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import argparse
import sys

ROOT = Path(__file__).resolve().parent
def _find_sdcc():
	"""SDCC_ROOT 优先，其次常见安装位置，最后退回 PATH 中的 sdcc。"""
	for candidate in (os.environ.get('SDCC_ROOT'), r'C:\dev\sdcc-4.6.0', '/usr/share/sdcc', '/usr/local/share/sdcc'):
		if candidate and (Path(candidate) / 'bin').is_dir():
			return Path(candidate)
	return Path(os.environ.get('SDCC_ROOT', 'sdcc'))

SDCC = _find_sdcc()
SDCC_BIN = SDCC / 'bin' / ('sdcc.exe' if os.name == 'nt' else 'sdcc')

def _find_runtime_bin():
	"""SDCC 的 Windows 构建依赖 MinGW/UCRT 运行时 DLL，找到就加进 PATH。"""
	for root in (os.environ.get('MSYS2_ROOT'), r'C:\msys64', r'C:\tools\msys64', r'D:\msys64', r'E:\msys64'):
		if not root:
			continue
		for sub in ('ucrt64/bin', 'mingw64/bin', 'clang64/bin'):
			path = Path(root) / sub
			if path.is_dir():
				return path
	return None

_extra_bin = _find_runtime_bin()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--variant', choices=['original_flashgbx_rc32', 'pcb_bus_gbcfix_20261027', 'pcb_bus_inputprobe_20261027', 'pcb_bus_startupfix_20261027', 'pcb_bus_xdatafast_20261027', 'pcb_bus_addrfast_20261027', 'pcb_bus_rxfast_20261027', 'pcb_bus_datafast_20261027', 'pcb_bus_fast_20261027', 'pcb_bus_fix_20261027', 'usb_vector_fix_20261027'], default='pcb_bus_gbcfix_20261027')
variant = parser.parse_args().variant
base_variant = 'pcb_bus_startupfix_20261027' if variant in ['pcb_bus_inputprobe_20261027', 'pcb_bus_gbcfix_20261027', 'original_flashgbx_rc32'] else variant
BUILD = ROOT / ('build_' + variant)
BUILD.mkdir(exist_ok=True)
if variant == 'original_flashgbx_rc32':
    # 构建失败不能留下旧 HEX 冒充新产物；历史试验固件已有独立归档。
    (BUILD / ('carbonate_source_sdcc_' + variant + '.hex')).unlink(missing_ok=True)
SOURCES = ['main', 'usb', 'usb_desc', 'usb_req_class', 'usb_req_std',
           'uart', 'cart_adapter', 'board']
if base_variant=='pcb_bus_startupfix_20261027':
    SOURCES.append('stc_startup')
env = os.environ.copy()
env['SDCC_HOME'] = str(SDCC)
env['PATH'] = os.pathsep.join([str(SDCC / 'bin')] + ([str(_extra_bin)] if _extra_bin else []) + [env.get('PATH', '')])
common = [str(SDCC_BIN), '-mmcs51', '--model-large', '-Isrc', '--std-c11']
if base_variant in ['pcb_bus_fast_20261027','pcb_bus_datafast_20261027','pcb_bus_rxfast_20261027','pcb_bus_addrfast_20261027','pcb_bus_xdatafast_20261027','pcb_bus_startupfix_20261027']:
    common.append('--opt-code-speed')
if base_variant in ['pcb_bus_datafast_20261027','pcb_bus_rxfast_20261027','pcb_bus_addrfast_20261027','pcb_bus_xdatafast_20261027','pcb_bus_startupfix_20261027']:
    common.append('-DROM_WRITE_DATA_FAST=1')
if base_variant in ['pcb_bus_rxfast_20261027','pcb_bus_addrfast_20261027','pcb_bus_xdatafast_20261027','pcb_bus_startupfix_20261027']:
    common.append('-DUSB_RX_PACKET_FAST=1')
if base_variant in ['pcb_bus_addrfast_20261027','pcb_bus_xdatafast_20261027','pcb_bus_startupfix_20261027']:
    common.append('-DROM_WRITE_ADDR_FAST=1')
if base_variant in ['pcb_bus_xdatafast_20261027','pcb_bus_startupfix_20261027']:
    common.append('-DROM_WRITE_XDATA_FAST=1')
if base_variant=='pcb_bus_startupfix_20261027':
    # STC 的片内 MOVX @Ri 仅寻址低 256 字节，不能用 P2 给 XINIT 分页。
    common.append('--no-xinit-opt')
    if variant not in ['pcb_bus_gbcfix_20261027','original_flashgbx_rc32']:
        common.append('-DGBC_READ_DIAGNOSTICS=1')
if variant=='pcb_bus_inputprobe_20261027':
    common.append('-DGBC_PROBE_INPUT_MODES=1')
if variant=='pcb_bus_gbcfix_20261027':
    common.append('-DGBC_READ_NO_EXTRA_PULLUP=1')
if variant=='original_flashgbx_rc32':
    subprocess.run([sys.executable,str(ROOT/'prepare_lk_special.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_crc32_table.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_rom_kernels.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_rom_crc_kernel.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_gbc_kernels.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_original_generated.py')],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'prepare_speed_kernels.py')],cwd=ROOT,check=True)
    common.append('-DGBC_READ_NO_EXTRA_PULLUP=1')
    common.append('-DFLASHGBX_COMPAT=1')
    SOURCES.append('flashgbx')
    SOURCES.append('gba_save')
    SOURCES.extend(['lk_special','lk_special_hal','dmg_single_fast'])
log = []

def run(args):
    result = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, text=True)
    log.append(' '.join(args) + '\n' + result.stdout + result.stderr)
    (BUILD / 'build.log').write_text('\n'.join(log), encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{args[-1]} 构建失败，见 {BUILD / "build.log"}')

objects = []
for name in SOURCES:
    obj = BUILD / (name + '.rel')
    if obj.exists():
        obj.unlink()
    segment_flags = ['--codeseg', 'FAST_CODE'] if name in ('lk_special_hal','board') else []
    # 通用协议解释器优先控制体积；总线内核仍保留原有速度优化。
    if name == 'flashgbx': segment_flags += ['--opt-code-size']
    run(common + segment_flags + ['-c', 'src/' + name + '.c', '-o', str(obj)])
    assert obj.exists() and obj.stat().st_size > 0
    objects.append(str(obj))
ihx = BUILD / ('carbonate_source_sdcc_' + variant + '.ihx')
run(common + ['--out-fmt-ihx', '--iram-size', '256', '--xram-loc', '64' if variant == 'original_flashgbx_rc32' else '0',
              '--xram-size', '7104' if variant == 'original_flashgbx_rc32' else '8192',
              '-Wl-bFAST_CODE=0xE400', '--code-size', '61440' if variant == 'original_flashgbx_rc32' else '65528', '-o', str(ihx)] + objects)
hex_path = ihx.with_suffix('.hex')
hex_path.write_bytes(ihx.read_bytes())
metadata = {
    'firmware': hex_path.name,
    'sha256': hashlib.sha256(hex_path.read_bytes()).hexdigest(),
    'bytes': hex_path.stat().st_size,
    'sourceSha256': {str(p.relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in sorted((ROOT / 'src').iterdir()) if p.suffix in ('.c', '.h')},
    'hardwareValidated': False,
    'hardwareProfile':'original_source',
    'deviceName':'ChisFlash Original',
}
(BUILD / 'manifest.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps({'file': str(hex_path), 'sha256': metadata['sha256'], 'bytes': metadata['bytes']}))
