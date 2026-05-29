#!/usr/bin/env python3
"""
process_sender.py — Sends top CPU-consuming processes to the Wio Terminal.

Sends a compact JSON line every ~2 seconds:
    {"p":[{"n":"chrome.exe","c":25.5,"m":1.2}, ...], "ok":1}

Usage:
    python process_sender.py COM3               # USB serial — Windows
    python process_sender.py /dev/ttyACM0      # USB serial — Linux/macOS
    python process_sender.py --ble              # BLE (auto-discovers WT-001)
    python process_sender.py --ble AA:BB:CC:DD:EE:FF  # BLE to specific address

Requirements:
    pip install pyserial psutil

Optional:
    pip install bleak   # BLE transport (required for --ble mode only)
"""

import ctypes
import json
import sys
import time
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

BLE_DEVICE_NAME  = "WT-001"
BLE_RX_CHAR_UUID = "4e495554-494f-5500-0000-000000000002"

POLL_INTERVAL = 2       # seconds between samples
BAUD_RATE     = 115200
MAX_PROCS     = 5
MAX_NAME_LEN  = 28      # truncated to fit the display

# Number of logical CPUs — used on Windows only to normalise per-core % to
# total-CPU % (matching Task Manager).
# On macOS/Linux, Activity Monitor and top/htop already show per-core %,
# which is the same scale psutil returns, so no division is needed there.
_CPU_COUNT = psutil.cpu_count(logical=True) or 1
_NORMALISE_CPU = sys.platform == "win32"

# Windows pseudo-processes that represent idle/system time, not real work.
# psutil reports these with absurdly high per-core % on multi-core machines.
_SKIP_NAMES = {"system idle process", "system idle", "idle"}

# Cross-poll CPU measurement state.
# Prime at the END of each collect() call, read at the START of the next.
# The measurement window is the full inter-call interval (~2.5 s BLE, ~2 s serial)
# which is much closer to Activity Monitor's ~5 s averaging window than a 1 s sleep.
_primed_procs: list = []

# ---- macOS phys_footprint via proc_pid_rusage (matches Activity Monitor exactly) ----
# proc_pid_rusage() is a read-only accounting call that does NOT require task_for_pid,
# so it works for protected system processes (WindowServer etc.) that block task_for_pid
# even as root.  ri_phys_footprint = resident + compressed = Activity Monitor "Memory".
# Falls back to RSS on non-macOS or any failure.
if sys.platform == "darwin":
    _RUSAGE_INFO_V0 = 0

    class _RusageInfoV0(ctypes.Structure):
        # Mirrors rusage_info_v0 from sys/resource.h (macOS SDK).
        _fields_ = [
            ("ri_uuid",               ctypes.c_uint8 * 16),
            ("ri_user_time",          ctypes.c_uint64),
            ("ri_system_time",        ctypes.c_uint64),
            ("ri_pkg_idle_wkups",     ctypes.c_uint64),
            ("ri_interrupt_wkups",    ctypes.c_uint64),
            ("ri_pageins",            ctypes.c_uint64),
            ("ri_wired_size",         ctypes.c_uint64),
            ("ri_resident_size",      ctypes.c_uint64),
            ("ri_phys_footprint",     ctypes.c_uint64),  # offset 72
            ("ri_proc_start_abstime", ctypes.c_uint64),
            ("ri_proc_exit_abstime",  ctypes.c_uint64),
        ]  # sizeof == 88 bytes

    _libsys = ctypes.CDLL("/usr/lib/libSystem.B.dylib", use_errno=True)
    _libsys.proc_pid_rusage.restype  = ctypes.c_int
    _libsys.proc_pid_rusage.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]

    def _proc_mem_bytes(pid: int) -> int | None:
        """Return phys_footprint in bytes — Activity Monitor 'Memory' column.
        Returns None on failure so caller falls back to RSS."""
        try:
            info = _RusageInfoV0()
            if _libsys.proc_pid_rusage(pid, _RUSAGE_INFO_V0, ctypes.byref(info)) == 0:
                return int(info.ri_phys_footprint)
            return None
        except Exception:
            return None
else:
    def _proc_mem_bytes(pid: int) -> int | None:  # type: ignore[misc]
        return None


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def collect() -> dict:
    """
    Return top MAX_PROCS processes by CPU %, cross-poll measured.

    Read per-process CPU counters primed in the PREVIOUS call, then prime
    fresh counters for the NEXT call.  The measurement window is the full
    inter-call interval (~2.5 s BLE, ~2 s serial), giving a stable average
    comparable to Activity Monitor's display.

    psutil.Process.cpu_percent() returns per-core % (0-100% per core; can
    exceed 100% for multi-threaded processes).  On Windows, dividing by
    cpu_count converts this to % of total CPU capacity, matching Task Manager.
    On macOS and Linux, Activity Monitor / top already use the same per-core
    scale psutil returns, so no division is applied there.

    The first call returns empty process data (nothing was primed yet).
    Memory is reported in MB as a float: phys_footprint (macOS, resident + compressed,
    matches Activity Monitor) when available, otherwise RSS.  The display formats
    values < 1000 as "###.#M" and ≥ 1000 as "#.#G".
    Also returns system-wide CPU% and RAM% as "tc" and "tm".
    """
    global _primed_procs

    # ---- 1. Read: system + per-process CPU since last call ------------------
    total_cpu = round(psutil.cpu_percent(interval=None), 1)
    ram = psutil.virtual_memory()
    total_mem = round((ram.total - ram.available) / ram.total * 100, 1)

    data = []
    for p in _primed_procs:
        try:
            cpu_raw = p.cpu_percent(interval=None)
            # Windows: divide by cpu_count to match Task Manager's total-capacity scale.
            # macOS/Linux: psutil and Activity Monitor/top already use per-core scale.
            cpu    = round(cpu_raw / _CPU_COUNT if _NORMALISE_CPU else cpu_raw, 1)
            fp = _proc_mem_bytes(p.pid)
            mem_mb = fp / 1_048_576 if fp is not None else p.memory_info().rss / 1_048_576
            name   = p.name()[:MAX_NAME_LEN]
            if cpu > 0.0:
                data.append({"n": name, "c": cpu, "m": mem_mb})
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    data.sort(key=lambda x: x["c"], reverse=True)

    # ---- 2. Prime: snapshot all processes for next call ---------------------
    psutil.cpu_percent(interval=None)   # system-wide prime
    new_procs = []
    for p in psutil.process_iter():
        try:
            if p.name().lower().strip() in _SKIP_NAMES:
                continue
            p.cpu_percent(interval=None)   # per-process prime
            new_procs.append(p)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            # Privileged processes (e.g. kernel_task on macOS) are silently
            # skipped — reading their CPU stats requires root.
            pass
    _primed_procs = new_procs

    return {"p": data[:MAX_PROCS], "tc": total_cpu, "tm": total_mem, "ok": 1}


# ---- BLE transport -----------------------------------------------------------

async def _ble_find(address: str | None) -> str | None:
    if address:
        log(f"Using address: {address}")
        return address
    log(f'Scanning for "{BLE_DEVICE_NAME}"...')
    device = await BleakScanner.find_device_by_name(BLE_DEVICE_NAME, timeout=10.0)
    if device:
        log(f"Found {device.name} at {device.address}")
        return device.address
    log(f'"{BLE_DEVICE_NAME}" not found — is the Wio Terminal on the Processes screen?')
    return None


async def _ble_run(address: str | None) -> None:
    collect()   # prime CPU counters; first real readings arrive on next call
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
                    await asyncio.sleep(1.5)
                log("Disconnected.")
        except Exception as e:
            log(f"BLE error: {e}")
        log("Reconnecting in 5s...")
        await asyncio.sleep(5)


# ---- Entry point -------------------------------------------------------------

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
        print( "  e.g. python process_sender.py COM3")
        print( "  e.g. python process_sender.py --ble")
        sys.exit(1)

    port = args[0]
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        log(f"Opened {port} at {BAUD_RATE} baud.")
    except serial.SerialException as e:
        log(f"Could not open {port}: {e}")
        sys.exit(1)

    try:
        collect()   # prime CPU counters; first real readings arrive on next call
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
