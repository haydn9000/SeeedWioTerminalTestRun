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
MAX_NAME_LEN  = 13      # truncated to fit the display

# Number of logical CPUs — used to normalise per-core % to total-CPU %.
# psutil.Process.cpu_percent() returns per-core %, not % of total system capacity.
# Dividing by cpu_count converts it to the same 0-100 % scale as Task Manager.
_CPU_COUNT = psutil.cpu_count(logical=True) or 1

# Windows pseudo-processes that represent idle/system time, not real work.
# psutil reports these with absurdly high per-core % on multi-core machines.
_SKIP_NAMES = {"system idle process", "system idle", "idle"}


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def collect() -> dict:
    """
    Return top MAX_PROCS processes by CPU %, normalised to total-CPU scale.

    Two-pass approach: prime all processes first, wait 0.5 s,
    then read the actual percentages so they're meaningful.
    Skips Windows idle pseudo-processes.
    psutil.Process.cpu_percent() already returns % of total system CPU
    capacity (it divides by the sum of all-core CPU time), so no further
    normalisation is needed — values match Task Manager's CPU% column.
    Memory is reported as RSS in MB.
    Also returns system-wide CPU% and RAM% as "tc" and "tm".
    """
    # Prime per-process CPU counters AND the system-wide counter
    psutil.cpu_percent(interval=None)
    procs = []
    for p in psutil.process_iter():
        try:
            if p.name().lower().strip() in _SKIP_NAMES:
                continue
            p.cpu_percent(interval=None)   # prime (returns 0.0 on first call)
            procs.append(p)
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    time.sleep(0.5)   # measurement window

    total_cpu = round(psutil.cpu_percent(interval=None), 1)
    total_mem = round(psutil.virtual_memory().percent, 1)

    data = []
    for p in procs:
        try:
            cpu_raw = p.cpu_percent(interval=None)
            cpu     = round(cpu_raw / _CPU_COUNT, 1)   # per-core % → % of total CPU
            mem_mb  = round(p.memory_info().rss / 1_048_576, 1)   # bytes → MB
            name    = p.name()[:MAX_NAME_LEN]
            if cpu > 0.0:
                data.append({"n": name, "c": cpu, "m": mem_mb})
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    data.sort(key=lambda x: x["c"], reverse=True)
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
        while True:
            data = collect()
            line = json.dumps(data, separators=(",", ":")) + "\n"
            ser.write(line.encode())
            log(f"Sent: {line.strip()}")
    except KeyboardInterrupt:
        log("Stopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
