"""FlashGBX 5.2.1 的 ChisFlash 独立后端与盖中盖原生后端。"""
import copy
import json
import datetime
import re
import struct
import sys
import threading
import time
from types import SimpleNamespace
import zlib

import serial
import serial.tools.list_ports
from FlashGBX.app import HW_DEVICES
from FlashGBX.hw_JoeyJr import GbxDevice as _DeviceBase
from FlashGBX.LK_Device import LK_Device
from FlashGBX.RomFileAGB import RomFileAGB
from FlashGBX.RomFileDMG import RomFileDMG

PLUGIN_NAME = "ChisFlash 碳酸钙与盖中盖"
PLUGIN_VERSION = "0.2.1"
PLUGIN_DESCRIPTION = "碳酸钙独立身份及完整兼容指令后端；盖中盖基础原生后端。"
IDENTITY_REQUEST = b'\x55\xaa'
CARBONATE_PRODUCTS = ('ChisFlash Carbonate', 'ChisFlash Carbonate Original')


def _usb_products():
    """只读系统中的 USB 产品名称，不打开串口或向设备发送探测命令。"""
    ports = serial.tools.list_ports.comports()
    result = {p.device: (getattr(p, 'product', None) or '') for p in ports}
    if sys.platform != 'win32':
        return result
    import ctypes
    import uuid
    import winreg
    from ctypes import wintypes

    class PropertyKey(ctypes.Structure):
        _fields_ = [('guid', ctypes.c_ubyte * 16), ('pid', wintypes.DWORD)]

    # DEVPKEY_Device_BusReportedDeviceDesc：Windows 缓存的设备产品描述。
    key = PropertyKey((ctypes.c_ubyte * 16).from_buffer_copy(
        uuid.UUID('540b947e-8b40-45bc-a8a2-6a0b894cbda2').bytes_le), 4)
    cfg = ctypes.WinDLL('cfgmgr32')
    locate = cfg.CM_Locate_DevNodeW
    locate.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, wintypes.ULONG]
    locate.restype = wintypes.ULONG
    get_property = cfg.CM_Get_DevNode_PropertyW
    get_property.argtypes = [wintypes.DWORD, ctypes.POINTER(PropertyKey), ctypes.POINTER(wintypes.ULONG),
                            ctypes.c_void_p, ctypes.POINTER(wintypes.ULONG), wintypes.ULONG]
    get_property.restype = wintypes.ULONG
    parent = cfg.CM_Get_Parent
    parent.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.DWORD, wintypes.ULONG]
    parent.restype = wintypes.ULONG
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SYSTEM\CurrentControlSet\Enum\USB') as usb:
            for index in range(winreg.QueryInfoKey(usb)[0]):
                name = winreg.EnumKey(usb, index)
                if not name.upper().startswith('VID_0483&PID_0721'):
                    continue
                with winreg.OpenKey(usb, name) as device:
                    for item in range(winreg.QueryInfoKey(device)[0]):
                        instance = winreg.EnumKey(device, item)
                        try:
                            with winreg.OpenKey(device, instance + r'\Device Parameters') as params:
                                port = winreg.QueryValueEx(params, 'PortName')[0]
                            if port not in result:
                                continue
                            node = wintypes.DWORD()
                            if locate(ctypes.byref(node), 'USB\\' + name + '\\' + instance, 0):
                                continue
                            for _ in range(3):
                                text = ctypes.create_unicode_buffer(256)
                                size, kind = wintypes.ULONG(ctypes.sizeof(text)), wintypes.ULONG()
                                if get_property(node.value, ctypes.byref(key), ctypes.byref(kind), text,
                                                ctypes.byref(size), 0) == 0 and kind.value == 0x12:
                                    result[port] = text.value
                                    break
                                next_node = wintypes.DWORD()
                                if parent(ctypes.byref(next_node), node.value, 0):
                                    break
                                node = next_node
                        except OSError:
                            continue
    except OSError:
        pass
    return result


def _prepare_serial(raw):
    raw.rts = True
    raw.dtr = True
    raw.rts = False
    raw.dtr = False
    time.sleep(0.01)
    raw.reset_input_buffer()
    raw.reset_output_buffer()


def _identify_chisflash(raw):
    """仅对产品名称已确认的碳酸钙查询身份，使用以换行结束的短响应。"""
    packet = IDENTITY_REQUEST
    if raw.write(packet) != len(packet):
        raise IOError('设备身份请求未完整发送')
    raw.flush()
    reply = bytearray()
    deadline = time.monotonic() + 1
    while len(reply) < 96 and time.monotonic() < deadline:
        part = raw.read(1)
        if part:
            reply.extend(part)
            if part == b'\n':
                break
    if not reply.endswith(b'\n'):
        raise IOError('碳酸钙身份响应不完整')
    if not reply.startswith(b'\x01ChisFlash'):
        return None
    match = re.fullmatch(rb'\x01ChisFlash( Original)? FW C([0-9]+) (RC[^\r\n ]+)\r\n', bytes(reply))
    if match is None:
        raise IOError('设备身份响应不完整或无法识别')
    return {'board': 'original' if match[1] else 'modified',
            'protocol': int(match[2]), 'version': match[3].decode('ascii')}


class CarbonateDevice(LK_Device):
    """直接复用官方通用 worker 和协议传输，不依赖 Joey 设备身份。"""
    DEVICE_NAME = 'ChisFlash（碳酸钙）'
    DEVICE_LABEL_LONG = DEVICE_NAME
    DEVICE_LABEL_SHORT = '碳酸钙'
    USB_IDS = ((0x0483, 0x0721),)
    PCB_VERSIONS = {2: ''}
    MAX_BUFFER_READ = 0x2000
    MAX_BUFFER_WRITE = 0x800
    FWUPDATE_ACTION = None
    CLI_UPDATER_METHOD = None
    DEVICE_SUPPORT_MESSAGE = 'ChisFlash 碳酸钙固件及插件；固件升级请使用对应板型的刷写工具。'

    def __init__(self):
        self.DEVICE, self.FW, self.IDENTITY = None, {}, None
        self.PORT, self.MODE = '', None
        self.FW_VAR, self.SUPPORTED_CARTS = {}, {}
        self.INFO = {'action': None, 'last_action': None, 'dump_info': {}}
        self.ERROR_ARGS, self.CANCEL_ARGS = {}, {}
        self.ERROR = self.CANCEL = False
        self.LAST_CHECK_ACTIVE = 0

    def GetCandidatePorts(self, comports=None):
        products = _usb_products()
        return [port for port in super().GetCandidatePorts(comports) if products.get(port) in CARBONATE_PRODUCTS]

    def Initialize(self, flashcarts, port=None, max_baud=2000000):
        self.Close()
        for candidate in ([port] if port else self.GetCandidatePorts()):
            if _usb_products().get(candidate) not in CARBONATE_PRODUCTS:
                continue
            raw = None
            try:
                raw = self._OpenSerial(candidate, baudrate=9600, timeout=0.05, write_timeout=5)
                _prepare_serial(raw)
                identity = _identify_chisflash(raw)
                if identity is None:
                    raw.close()
                    continue
                self.DEVICE, self.PORT, self.IDENTITY = raw, candidate, identity
                self.BAUDRATE = 9600
                raw.timeout = self.DEVICE_TIMEOUT
                if not self.LoadFirmwareVersion():
                    raise IOError('碳酸钙固件信息与原生身份不一致')
                self.UpdateFlashCarts(flashcarts)
                return []
            except (OSError, serial.SerialException, ValueError, struct.error) as error:
                if raw is not None:
                    raw.close()
                self.DEVICE, self.FW, self.IDENTITY = None, {}, None
                self.ERROR_ARGS['connection'] = str(error)
        return False

    def _exact_info(self, count):
        data = bytearray()
        deadline = time.monotonic() + self.DEVICE_TIMEOUT
        while len(data) < count and time.monotonic() < deadline:
            data.extend(self.DEVICE.read(count - len(data)))
        if len(data) != count:
            raise IOError('碳酸钙固件信息响应不完整')
        return bytes(data)

    def LoadFirmwareVersion(self):
        if self.IDENTITY is None or self.DEVICE is None:
            return False
        self._write(self.DEVICE_CMD['QUERY_FW_INFO'])
        if self._exact_info(1) != b'\x08':
            return False
        family, protocol, pcb, timestamp = struct.unpack('>cHBI', self._exact_info(8))
        if family != b'C' or protocol != self.IDENTITY['protocol']:
            return False
        length = self._exact_info(1)[0]
        name = self._exact_info(length).decode('ascii')
        expected_name = 'ChisFlash Original' if self.IDENTITY['board'] == 'original' else 'ChisFlash'
        if name != expected_name:
            return False
        power, boot = self._exact_info(2)
        self.FW = {'cfw_id': 'C', 'fw_ver': protocol, 'pcb_ver': pcb,
                   'fw_ts': timestamp, 'fw_dt': datetime.datetime.fromtimestamp(timestamp).astimezone().isoformat(),
                   'pcb_name': name, 'ofw_ver': None, 'device_serial': None,
                   'cart_power_ctrl': bool(power & 1), 'cart_presence_switch': bool(power & 2),
                   'cart_mode_switch': bool(power & 4), 'bootloader_reset': bool(boot & 1)}
        return True

    def Close(self, cartPowerOff=False):
        raw = self.DEVICE
        if raw is None:
            return
        try:
            if cartPowerOff and raw.is_open and self.CanPowerCycleCart():
                self._set_fw_variable('AUTO_POWEROFF_TIME', 0)
                self._write(self.DEVICE_CMD['CART_PWR_OFF'], wait=True)
        finally:
            raw.close()
            self.DEVICE, self.MODE = None, None
            self.LAST_CHECK_ACTIVE = 0

    def GetFirmwareVersion(self, more=False):
        version = self.IDENTITY['version'] if self.IDENTITY else '未知'
        return f'{version}（协议 {self.FW.get("fw_ver", "未知")}）' if more else version

    def GetFullName(self): return self.DEVICE_NAME
    def GetFullNameExtended(self, more=False):
        return f'{self.DEVICE_NAME} · {self.GetFirmwareVersion(more)} · {self.PORT}'
    def GetSupportedModes(self): return ['DMG', 'AGB']
    def CanSetVoltageBySwitch(self): return False
    def CanSetVoltageByCode(self): return self.FW.get('pcb_ver') == 2
    def CanSetVoltageByAutoswitch(self): return False
    def CanPowerCycleCart(self): return self.FW.get('cart_power_ctrl', False)
    def IsSupported3dMemory(self): return True
    def IsClkConnected(self): return True
    def SupportsAudioAsWe(self): return True
    def ChangeBaudRate(self, value): pass
    def ResetLEDs(self): pass
    def SupportsFirmwareUpdates(self): return False
    def FirmwareUpdateAvailable(self): return False
    def GetFirmwareUpdaterClass(self): return None
    def SupportsBootloaderReset(self): return False
    def BootloaderReset(self): return False


class NativeProtocolError(IOError):
    pass


class UnsupportedNativeFeature(NotImplementedError):
    pass


def crc16(data):
    value = 0xffff
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xa001 if value & 1 else 0)
    return value


class NativeLink:
    """单个请求完整接收响应后才发送下一帧；不把占位响应头当作 CRC 校验。"""
    def __init__(self, port, cancelled=lambda: False):
        self.port = port
        self.cancelled = cancelled
        self.lock = threading.RLock()
        self.history = []

    @staticmethod
    def frame(command, body=b''):
        packet = struct.pack('<H', len(body) + 5) + bytes([command]) + bytes(body)
        # 原客户端保留两个零字节，未启用请求 CRC16。
        return packet + b'\x00\x00'

    @staticmethod
    def chunk_size(left, overhead, unit=1):
        size = min(left, 4096)
        size -= size % unit
        # 双协议固件会检查首字节，避开可能被识别为另一协议探测符的帧长度。
        reserved = {0x44, 0x55, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x4b, 0x4c}
        while size and (((size + overhead) & 255) >= 0xa0 or ((size + overhead) & 255) in reserved):
            size -= unit
        if not size:
            raise NativeProtocolError('盖中盖载荷长度无法组成有效帧')
        return size

    def exact(self, count, timeout):
        result = bytearray()
        deadline = time.monotonic() + timeout
        while len(result) < count:
            if self.cancelled():
                raise NativeProtocolError('操作已取消')
            part = self.port.read(count - len(result))
            if part:
                result.extend(part)
            elif time.monotonic() >= deadline:
                raise NativeProtocolError(f'盖中盖响应超时：需要 {count} 字节，收到 {len(result)} 字节')
        return bytes(result)

    def request(self, command, body=b'', read_length=None, ack=False, timeout=5):
        with self.lock:
            if command not in (0xf0, 0xf1, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xea, 0xeb):
                raise UnsupportedNativeFeature(f'盖中盖原生协议未提供该命令：0x{command:02X}')
            if self.cancelled():
                raise NativeProtocolError('操作已取消')
            packet = self.frame(command, body)
            self.history.append((command, len(packet)))
            written = self.port.write(packet)
            if written != len(packet):
                raise NativeProtocolError('盖中盖请求未完整发送')
            self.port.flush()
            if ack:
                response = self.exact(1, timeout)
                if response != b'\xaa':
                    raise NativeProtocolError(f'盖中盖 ACK 错误：{response.hex()}')
                return b''
            if read_length is not None:
                response = self.exact(read_length + 2, timeout)
                self.last_response_header = response[:2]
                return response[2:]
            return b''

    def read(self, mode, address, length):
        result = bytearray()
        while len(result) < length:
            size = min(4096, length - len(result))
            base = address + len(result)
            if mode == 'AGB':
                aligned = base & ~1
                count = (size + (base & 1) + 1) & ~1
                raw = self.request(0xf6, struct.pack('<IH', aligned, count), read_length=count)
                result.extend(raw[base & 1:(base & 1) + size])
            else:
                command = 0xf8 if mode == 'SRAM' else 0xfb
                result.extend(self.request(command, struct.pack('<IH', base, size), read_length=size))
        return bytes(result)

    def write(self, mode, address, data, buffer_size=None):
        data = bytes(data)
        unit = 2 if mode == 'AGB' else 1
        if len(data) % unit or (mode == 'AGB' and address & 1):
            raise ValueError('GBA 写入地址和数据必须按 16 位字对齐')
        offset = 0
        while offset < len(data):
            overhead = 11 if buffer_size is not None else 9
            size = self.chunk_size(len(data) - offset, overhead, unit)
            # 原生 F5 使用字地址，F4/F6 使用字节地址，不能共用同一换算。
            wire_address = (address + offset) // 2 if mode == 'AGB' and buffer_size is None else address + offset
            body = struct.pack('<I', wire_address)
            if buffer_size is not None:
                body += struct.pack('<H', buffer_size)
                command = 0xf4 if mode == 'AGB' else 0xfc
            else:
                command = 0xf5 if mode == 'AGB' else 0xf7 if mode == 'SRAM' else 0xfa
            self.request(command, body + data[offset:offset + size], ack=True, timeout=30)
            offset += size


class GbxDevice(_DeviceBase):
    DEVICE_NAME = 'ChisFlash（盖中盖原生）'
    DEVICE_LABEL_LONG = DEVICE_NAME
    DEVICE_LABEL_SHORT = '盖中盖原生'
    USB_IDS = ((0x0483, 0x0721),)
    MAX_BUFFER_READ = 4096
    MAX_BUFFER_WRITE = 4096
    FWUPDATE_ACTION = None
    CLI_UPDATER_METHOD = None
    DEVICE_SUPPORT_MESSAGE = '实验版盖中盖插件；当前支持标准 AMD ROM、普通 SRAM 及卡带识别。'

    def __init__(self):
        self.DEVICE = None
        self.PORT = ''
        self.MODE = 'AGB'
        self.FW = {'pcb_ver': 2, 'pcb_name': self.DEVICE_NAME, 'cfw_id': 'N', 'fw_ver': 15,
                   'fw_ts': 0, 'fw_dt': '原生协议不提供固件版本查询', 'cart_power_ctrl': False,
                   'bootloader_reset': False, 'cart_presence_switch': False, 'cart_mode_switch': False}
        # fw_ver 仅供复用上位机逻辑判断接口级别，不代表设备中安装了 LK 固件。
        self.FW_VAR = {name: 0 for name in self.DEVICE_VAR}
        self.FW_VAR.update(CART_MODE=2, STATUS_REGISTER_MASK=0x80, STATUS_REGISTER_VALUE=0x80)
        self.INFO = {'action': None, 'last_action': None, 'dump_info': {}}
        self.ERROR_ARGS, self.CANCEL_ARGS = {}, {}
        self.ERROR = self.CANCEL = False
        self.SUPPORTED_CARTS = {}
        self._commands = [(0, 0)] * 6
        self._input, self._reply = bytearray(), bytearray()
        self._payload = None
        self._busy = False
        self._lock = threading.RLock()
        self._native = None
        self.LAST_CHECK_ACTIVE = 0

    def GetCandidatePorts(self, serial_ports=None):
        ports = serial.tools.list_ports.comports() if serial_ports is None else serial_ports
        products = _usb_products()
        return [p.device for p in ports if (p.vid, p.pid) in self.USB_IDS and products.get(p.device) not in CARBONATE_PRODUCTS]

    def Initialize(self, flashcarts, port=None, max_baud=2000000):
        for candidate in ([port] if port else self.GetCandidatePorts()):
            if _usb_products().get(candidate) in CARBONATE_PRODUCTS:
                continue
            raw = None
            try:
                raw = serial.Serial(candidate, baudrate=9600, timeout=0.05, write_timeout=5)
                _prepare_serial(raw)
                self.DEVICE, self.PORT = raw, candidate
                self._native = NativeLink(raw, lambda: self.CANCEL)
                self._native.read('AGB', 0, 4)
                self.UpdateFlashCarts(flashcarts)
                return []
            except (OSError, serial.SerialException, NativeProtocolError) as error:
                if raw is not None:
                    raw.close()
                self.DEVICE = None
                self._native = None
                self.ERROR_ARGS['connection'] = str(error)
        return False

    def IsConnected(self):
        return self.DEVICE is not None and self.DEVICE.is_open

    def CheckActive(self):
        return self.IsConnected()

    def Close(self, cartPowerOff=False):
        if self.IsConnected():
            self.DEVICE.close()
        self.DEVICE = None

    def SetMode(self, mode, delay=0.1):
        if mode not in ('AGB', 'DMG'):
            raise ValueError('不支持的卡带模式')
        self.MODE = mode
        self.FW_VAR.update(CART_MODE=2 if mode == 'AGB' else 1, ADDRESS=0)

    def GetMode(self):
        return self.MODE

    def CartPowerOn(self):
        raise UnsupportedNativeFeature('盖中盖原生协议不支持供电切换')

    def CartPowerOff(self):
        raise UnsupportedNativeFeature('盖中盖原生协议不支持供电切换')

    def CartPowerCycle(self):
        raise UnsupportedNativeFeature('盖中盖原生协议不支持供电切换')

    def SetAutoPowerOff(self, value):
        self.SKIP_POWERCYCLE = True

    def _thread_worker_auto_poweroff_start(self):
        self._busy = True

    def _thread_worker_auto_poweroff_finish(self):
        self._busy = False

    def _set_fw_variable(self, key, value, log=True):
        if key not in self.DEVICE_VAR:
            raise KeyError(key)
        bits = self.DEVICE_VAR[key][0]
        self.FW_VAR[key] = int(value) & ((1 << bits) - 1)
        return 1

    def _get_fw_variable(self, key):
        return self.FW_VAR.get(key, 0)

    def ReadROM(self, address, length, skip_init=False, max_length=4096):
        with self._lock:
            if skip_init:
                address = self.FW_VAR['ADDRESS'] * (2 if self.MODE == 'AGB' else 1)
            raw = self._native.read(self.MODE, address, length)
            self.FW_VAR['ADDRESS'] = (address + length) // 2 if self.MODE == 'AGB' else address + length
            if self.INFO.get('action') in (1, 2, 4) and not self.NO_PROG_UPDATE:
                self.SetProgress({'action': 'READ', 'bytes_added': len(raw)})
            return bytearray(raw)

    def ReadHeader(self, checkRtc=True):
        # 普通原生读头不调用 LK 的供电、启动脉冲或 RTC 专用固件命令。
        self.SIGNAL = None
        header = self.ReadROMHeader()
        parser = RomFileAGB if self.MODE == 'AGB' else RomFileDMG
        data = parser.GetParser(header).GetHeader()
        if not data:
            return False
        if self.MODE == 'AGB':
            size = 0x10000
            if data.get('empty') or data.get('empty_nocart'):
                size = 0x2000000
            else:
                reference = header[0xa0:0xb0]
                while size < 0x2000000:
                    if self.ReadROM(size + 0xa0, 16) == reference:
                        break
                    size *= 2
            data['rom_size'] = size
        data.update(raw=header, has_rtc=False, rtc_dict={}, rtc_string='首版插件未读取 RTC',
                    no_rtc_reason='盖中盖 RTC 适配尚未实现')
        self.INFO.update(data)
        self.INFO.setdefault('dump_info', {})['header'] = data
        self.INFO['flash_type'] = data.get('flash_type', 0)
        self.INFO['last_action'] = 0
        return data

    def ReadRAM(self, address, length, command=None, max_length=4096):
        if command not in (None, self.DEVICE_CMD['AGB_CART_READ_SRAM'], self.DEVICE_CMD['DMG_CART_READ']):
            raise UnsupportedNativeFeature('该存档读取协议尚未适配盖中盖插件')
        with self._lock:
            target = address if self.MODE == 'AGB' else 0xa000 + address
            raw = self._native.read('SRAM' if self.MODE == 'AGB' else 'DMG', target, length)
            self.FW_VAR['ADDRESS'] = target + length
            if not self.NO_PROG_UPDATE:
                self.SetProgress({'action': 'READ', 'bytes_added': len(raw)})
            return bytearray(raw)

    def WriteRAM(self, address, buffer, command=None, max_length=256):
        if command not in (None, self.DEVICE_CMD['AGB_CART_WRITE_SRAM'], self.DEVICE_CMD['DMG_CART_WRITE_SRAM']):
            raise UnsupportedNativeFeature('首版插件仅支持普通 SRAM/FRAM 字节接口，不支持 EEPROM/Flash 存档编程')
        with self._lock:
            target = address if self.MODE == 'AGB' else 0xa000 + address
            self._native.write('SRAM' if self.MODE == 'AGB' else 'DMG', target, buffer)
            actual = self._native.read('SRAM' if self.MODE == 'AGB' else 'DMG', target, len(buffer))
            if actual != bytes(buffer):
                raise NativeProtocolError('存档写入后回读不一致')
            self.FW_VAR['ADDRESS'] = target + len(buffer)
            if self.INFO.get('action') == 3 and not self.NO_PROG_UPDATE:
                self.SetProgress({'action': 'WRITE', 'bytes_added': len(buffer)})
            return True

    def _cart_write(self, address, value, flashcart=False, sram=False):
        with self._lock:
            if self.MODE == 'AGB' and not sram:
                # 配置的命令地址以字节计；与主线相同，输出时忽略字地址的 A0。
                self._native.write('AGB', address & ~1, struct.pack('<H', value & 65535))
            else:
                self._native.write('SRAM' if self.MODE == 'AGB' else 'DMG', address, bytes([value & 255]))
            return True

    def _cart_write_flash(self, commands, flashcart=False):
        for address, value in commands:
            self._cart_write(address, value, flashcart=flashcart, sram=self.MODE == 'AGB' and not flashcart)
        return True

    def CompareCRC32(self, buffer, offset, length, address, flashcart=None, max_length=0x20000, reset=False, mbc=None, bank=0):
        expected = zlib.crc32(buffer[offset:offset + length])
        actual = 0
        done = 0
        while done < length:
            size = min(4096, length - done)
            actual = zlib.crc32(self.ReadROM(address + done, size), actual)
            done += size
        return True if expected == actual else (expected, actual)

    def WriteROM(self, address, buffer, flash_buffer_size=False, skip_init=False, rumble_stop=False, max_length=4096):
        if self.FW_VAR['FLASH_COMMAND_SET'] != 1 or self.FW_VAR['FLASH_METHOD'] not in (1, 2):
            raise UnsupportedNativeFeature('首版盖中盖插件尚未实现 Intel 或特殊卡带编程算法')
        if self.FW_VAR.get('FLASH_WE_PIN', 1) not in (0, 1):
            raise UnsupportedNativeFeature('盖中盖基础命令不提供 AUDIO/RESET 写脚控制')
        if self.MODE == 'DMG' and (self.FW_VAR.get('FLASH_COMMANDS_BANK_1') or self.FW_VAR.get('FLASH_PULSE_RESET')):
            raise UnsupportedNativeFeature('首版插件尚未实现编程命令期间的特殊换 Bank/复位时序')
        unit = 2 if self.MODE == 'AGB' else 1
        data = bytes(buffer)
        if len(data) % unit:
            raise ValueError('编程数据未按总线字宽对齐')
        page = int(flash_buffer_size or 0)
        prefixes = [(a * unit, v) for a, v in self._commands[:3]]
        normal = (prefixes[:2] == [(0xaaa, 0xaa), (0x554 if unit == 2 else 0x555, 0x55)]
                  and prefixes[2][1] == (0x25 if page else 0xa0)
                  and (bool(page) or prefixes[2][0] == 0xaaa)
                  and (not page or self._commands[5][1] == 0x29)
                  and not self.FW_VAR.get('FLASH_DOUBLE_DIE'))
        with self._lock:
            done = 0
            while done < len(data):
                if self.CANCEL:
                    raise NativeProtocolError('操作已取消')
                size = min(4096, len(data) - done)
                if normal and (not page or page <= 256 * unit) and (not page or (address + done) % page == 0) and (not page or size % page == 0):
                    part = data[done:done + size]
                    self._native.write(self.MODE, address + done, part, buffer_size=page)
                else:
                    size = unit if not page else min(size, page - ((address + done) % page))
                    part = data[done:done + size]
                    self._cart_write_flash(prefixes[:2], flashcart=True)
                    target = address + done
                    if page:
                        self._cart_write(target, self._commands[2][1], flashcart=True)
                        self._cart_write(target, len(part) // unit - 1, flashcart=True)
                        self._native.write(self.MODE, target, part)
                        self._cart_write(target, self._commands[5][1], flashcart=True)
                    else:
                        self._cart_write(prefixes[2][0], prefixes[2][1], flashcart=True)
                        self._native.write(self.MODE, target, part)
                    deadline = time.monotonic() + 0.5
                    while self._native.read(self.MODE, target + len(part) - unit, unit) != part[-unit:]:
                        if time.monotonic() >= deadline:
                            raise NativeProtocolError('Flash 编程等待超时')
                if self._native.read(self.MODE, address + done, len(part)) != part:
                    self.FW_VAR['STATUS_REGISTER'] = 0xffff
                    self.ERROR_ARGS = {'iteration': done // max(1, max_length)}
                    return False
                done += len(part)
                self.FW_VAR['ADDRESS'] = (address + done) // unit
                self.SetProgress({'action': 'WRITE', 'bytes_added': len(part), 'skipping': False})
            self.SKIPPING = False
            return None

    def _write(self, data, wait=False):
        # 这里只解析上位机共享 worker 的控制状态；实际 USB 线上不发送 LK 命令。
        self._input.extend(bytes([data]) if isinstance(data, int) else bytes(data))
        while self._input:
            command = self._input[0]
            length = 40 if command == 0xa7 else (2 + self._input[1] * 5 if command == 0xb8 and len(self._input) >= 2 else 2 if command == 0xb8 else 1)
            if len(self._input) < length:
                break
            packet = bytes(self._input[:length])
            del self._input[:length]
            if command == 0xa7:
                self.FW_VAR.update(FLASH_COMMAND_SET=packet[1], FLASH_METHOD=packet[2], FLASH_WE_PIN=packet[3])
                self._commands = [struct.unpack_from('>IH', packet, 4 + i * 6) for i in range(6)]
            elif command == 0xb8:
                pass  # 标准 AMD 路径由上位机 mapper 自行换 Bank。
            elif command == self.DEVICE_CMD['SET_MODE_AGB']:
                self.MODE = 'AGB'
                self.FW_VAR['CART_MODE'] = 2
            elif command == self.DEVICE_CMD['SET_MODE_DMG']:
                self.MODE = 'DMG'
                self.FW_VAR['CART_MODE'] = 1
            elif command in (self.DEVICE_CMD['SET_VOLTAGE_3_3V'], self.DEVICE_CMD['SET_VOLTAGE_5V']):
                if command == self.DEVICE_CMD['SET_VOLTAGE_5V']:
                    raise UnsupportedNativeFeature('当前设备不由插件切换电压，首版不支持要求切到 5V 的烧录流程')
                # 主线对固定电压设备也发送 3.3V 选择；不伪造实际电源切换。
            elif command == self.DEVICE_CMD['CART_PWR_ON']:
                self.CartPowerOn()
            elif command == self.DEVICE_CMD['CART_PWR_OFF']:
                self.CartPowerOff()
            elif command not in (self.DEVICE_CMD['ENABLE_PULLUPS'], self.DEVICE_CMD['DISABLE_PULLUPS'],
                                 self.DEVICE_CMD['SET_ADDR_AS_INPUTS'], self.DEVICE_CMD['DMG_MBC_RESET'], self.DEVICE_CMD['NULL']):
                raise UnsupportedNativeFeature(f'该功能尚未适配盖中盖插件：0x{command:02X}')
            self._reply.append(1)
        return self._read(1) if wait else None

    def _read(self, count):
        if len(self._reply) < count:
            return False
        raw = self._reply[:count]
        del self._reply[:count]
        return raw[0] if count == 1 else bytearray(raw)

    def _try_write(self, data, retries=5):
        return self._write(data, wait=True)

    def LoadFirmwareVersion(self): return self.IsConnected()
    def GetFirmwareVersion(self, more=False): return 'Native 插件 ' + PLUGIN_VERSION
    def GetFullName(self): return self.DEVICE_NAME
    def GetFullNameExtended(self, more=False): return f'{self.DEVICE_NAME} · {self.PORT} · 插件 {PLUGIN_VERSION}'
    def GetSupportedModes(self): return ['DMG', 'AGB']
    def GetSupprtedModes(self): return self.GetSupportedModes()
    def CanSetVoltageBySwitch(self): return False
    def CanSetVoltageByCode(self): return False
    def CanSetVoltageByAutoswitch(self): return False
    def CanPowerCycleCart(self): return False
    def IsSupported3dMemory(self): return False
    def IsClkConnected(self): return False
    def SupportsFirmwareUpdates(self): return False
    def FirmwareUpdateAvailable(self): return False
    def GetFirmwareUpdaterClass(self): return None
    def SupportsBootloaderReset(self): return False
    def SupportsAudioAsWe(self): return False
    def ChangeBaudRate(self, value): return None
    def GetDeviceSerial(self): return None
    def _clk_toggle(self, num=1):
        raise UnsupportedNativeFeature('盖中盖基础协议不支持逐次 CLK 脉冲')


def plugin_entry():
    for backend in (GbxDevice, CarbonateDevice):
        if not any(getattr(module.GbxDevice, 'DEVICE_NAME', None) == backend.DEVICE_NAME for module in HW_DEVICES):
            HW_DEVICES.insert(0, SimpleNamespace(GbxDevice=backend))
