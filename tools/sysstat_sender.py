#!/usr/bin/env python3
"""
sysstat_sender.py — Sends PC system stats to the Wio Terminal over USB serial or BLE.

Reports CPU/RAM usage, temperatures, NVIDIA GPU stats, and network bandwidth.
Sends a compact JSON line every POLL_INTERVAL seconds.

Usage:
    python sysstat_sender.py COM3           # USB serial — Windows
    python sysstat_sender.py /dev/ttyACM0  # USB serial — Linux/macOS
    python sysstat_sender.py --ble          # BLE (auto-discovers WT-001)
    python sysstat_sender.py --ble AA:BB:CC:DD:EE:FF  # BLE to specific address

Requirements:
    pip install pyserial psutil

Optional (install for the extras they unlock):
    pip install nvidia-ml-py   # NVIDIA GPU usage + temperature + VRAM
    pip install wmi            # Windows CPU temperature (also needs LibreHardwareMonitor
                               # running as a service — https://github.com/LibreHardwareMonitor)
    pip install bleak          # BLE transport (required for --ble mode only)
"""

import json
import sys
import time
import warnings
import psutil
import serial

# ---- optional: BLE via bleak (only needed for --ble mode) ----
_bleak_available = False
try:
    import asyncio
    from bleak import BleakClient, BleakScanner
    _bleak_available = True
except ImportError:
    pass

BLE_DEVICE_NAME = "WT-001"
BLE_RX_CHAR_UUID = "4e495554-494f-5500-0000-000000000002"

POLL_INTERVAL = 2      # seconds between samples
BAUD_RATE     = 115200

# Suppress noisy warnings from third-party packages we can't modify.
warnings.filterwarnings("ignore", category=SyntaxWarning, module="wmi")
warnings.filterwarnings("ignore", category=FutureWarning, message=".*pynvml.*")

# ---- optional: NVIDIA GPU via nvidia-ml-py (import name is still pynvml) ----
_gpu_handle = None
_gpu_name   = ""
try:
    import pynvml
    pynvml.nvmlInit()
    _gpu_handle = pynvml.nvmlDeviceGetHandleByIndex(0)
    name = pynvml.nvmlDeviceGetName(_gpu_handle)
    if isinstance(name, bytes):
        name = name.decode()
    _gpu_name = name
    print(f"[gpu] NVIDIA detected: {name}")
except Exception:
    print("[gpu] nvidia-ml-py not available — install with: pip install nvidia-ml-py")

# ---- optional: Windows CPU temperature via wmi + LibreHardwareMonitor --------
# Skip wmi in --ble mode: importing wmi initialises pythoncom in STA, which
# prevents bleak from using the MTA it requires for BLE callbacks on Windows.
_wmi_lhm = None
if sys.platform == "win32" and "--ble" not in sys.argv:
    try:
        import wmi  # SyntaxWarning already suppressed above
        for ns in (r"root\LibreHardwareMonitor", r"root\OpenHardwareMonitor"):
            try:
                candidate = wmi.WMI(namespace=ns)
                list(candidate.Hardware())   # test query
                _wmi_lhm = candidate
                print(f"[cpu] Hardware monitor WMI ready ({ns.split('\\')[-1]})")
                break
            except Exception:
                pass
        if _wmi_lhm is None:
            print("[cpu] CPU temperature unavailable on Windows without a hardware monitor.")
            print("      Install LibreHardwareMonitor, enable WMI support, run as administrator.")
    except ImportError:
        print("[cpu] wmi package not installed — run: pip install wmi")


# ---- helpers -----------------------------------------------------------------
def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def cpu_temp() -> int:
    """Return CPU package temperature in °C, or -1 if unavailable."""
    # Windows: query OHM / LHM WMI sensors
    if _wmi_lhm:
        try:
            for s in _wmi_lhm.Sensor():
                if s.SensorType == "Temperature" and "CPU" in s.Name:
                    return round(float(s.Value))
        except Exception:
            pass

    # Linux / macOS: psutil sensors
    try:
        temps = psutil.sensors_temperatures()
        for key in ("coretemp", "k10temp", "cpu_thermal", "acpitz", "zenpower"):
            if key in temps and temps[key]:
                return round(temps[key][0].current)
    except Exception:
        pass

    return -1


def gpu_stats() -> tuple:
    """Return (usage_pct, temp_c, vram_pct) or (-1, -1, -1) if unavailable."""
    if _gpu_handle is None:
        return -1, -1, -1
    try:
        util = pynvml.nvmlDeviceGetUtilizationRates(_gpu_handle)
        temp = pynvml.nvmlDeviceGetTemperature(_gpu_handle, pynvml.NVML_TEMPERATURE_GPU)
        mem  = pynvml.nvmlDeviceGetMemoryInfo(_gpu_handle)
        return util.gpu, temp, round(mem.used / mem.total * 100)
    except Exception:
        return -1, -1, -1


_net_prev = None

def net_stats() -> tuple:
    """Return (download_MB/s, upload_MB/s) averaged since last call."""
    global _net_prev
    c   = psutil.net_io_counters()
    now = time.monotonic()
    if _net_prev is None:
        _net_prev = (c.bytes_recv, c.bytes_sent, now)
        return 0.0, 0.0
    rx0, tx0, t0 = _net_prev
    dt  = max(now - t0, 0.001)
    dn  = (c.bytes_recv - rx0) / dt / 1_048_576
    up  = (c.bytes_sent - tx0) / dt / 1_048_576
    _net_prev = (c.bytes_recv, c.bytes_sent, now)
    return max(0.0, dn), max(0.0, up)


def collect() -> dict:
    cpu_pct       = round(psutil.cpu_percent(interval=0.5))
    ct            = cpu_temp()
    ram           = psutil.virtual_memory()
    gpu_pct, gt, gm = gpu_stats()
    dn, up        = net_stats()

    return {
        "cpu": cpu_pct,
        "ct":  ct,
        "ram": round(ram.percent),
        "rb":  f"{ram.used / 1_073_741_824:.1f}/{ram.total / 1_073_741_824:.1f}GB",
        "gpu": gpu_pct,
        "gt":  gt,
        "gm":  gm,
        "gn":  _gpu_name,
        "nd":  round(dn, 2),
        "nu":  round(up, 2),
        "ok":  True,
    }


async def _ble_find(address: str | None) -> str | None:
    if address:
        log(f"Using address: {address}")
        return address
    log(f'Scanning for "{BLE_DEVICE_NAME}"...')
    device = await BleakScanner.find_device_by_name(BLE_DEVICE_NAME, timeout=10.0)
    if device:
        log(f"Found {device.name} at {device.address}")
        return device.address
    log(f'"{BLE_DEVICE_NAME}" not found — is the Wio Terminal on the Sys Stats screen?')
    return None


async def _ble_run(address: str | None) -> None:
    net_stats()   # prime the network baseline
    while True:
        addr = await _ble_find(address)
        if not addr:
            log("Retrying in 10s...")
            await asyncio.sleep(10)
            continue
        try:
            async with BleakClient(addr) as client:
                log(f"Connected to {addr}")
                while client.is_connected:
                    data = collect()
                    line = json.dumps(data, separators=(",", ":")) + "\n"
                    await client.write_gatt_char(BLE_RX_CHAR_UUID, line.encode(), response=True)
                    log(f"Sent: {line.strip()}")
                    await asyncio.sleep(POLL_INTERVAL)
                log("Disconnected.")
        except Exception as e:
            log(f"BLE error: {e}")
        log("Reconnecting in 5s...")
        await asyncio.sleep(5)


def main() -> None:
    args = sys.argv[1:]

    # --ble [address]  →  BLE mode
    if args and args[0] == "--ble":
        if not _bleak_available:
            print("BLE mode requires bleak:  pip install bleak")
            sys.exit(1)
        address = args[1] if len(args) > 1 else None
        try:
            asyncio.run(_ble_run(address))
        except KeyboardInterrupt:
            log("Stopped.")
        return

    # <port>  →  USB serial mode
    if not args:
        print(f"Usage: python {sys.argv[0]} <port>")
        print(f"       python {sys.argv[0]} --ble [address]")
        print( "  e.g. python sysstat_sender.py COM3")
        print( "  e.g. python sysstat_sender.py --ble")
        sys.exit(1)

    port = args[0]
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        log(f"Opened {port} at {BAUD_RATE} baud.")
    except serial.SerialException as e:
        log(f"Could not open {port}: {e}")
        sys.exit(1)

    net_stats()   # prime the network baseline

    try:
        while True:
            data = collect()
            line = json.dumps(data, separators=(",", ":")) + "\n"
            ser.write(line.encode())
            log(f"Sent: {line.strip()}")
            time.sleep(POLL_INTERVAL)
    except KeyboardInterrupt:
        log("Stopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
