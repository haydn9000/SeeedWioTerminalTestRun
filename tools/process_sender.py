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
import struct
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

# Number of logical CPUs — used to normalise per-core CPU time to total-CPU
# capacity % when computing process CPU% on Windows (matching Task Manager).
# Not used on macOS/Linux, where psutil already returns per-core %.
_CPU_COUNT = psutil.cpu_count(logical=True) or 1

# Windows pseudo-processes that represent idle/system time, not real work.
# psutil reports these with absurdly high per-core % on multi-core machines.
_SKIP_NAMES = {"system idle process", "system idle", "idle"}

# Cross-poll CPU measurement state.
# Prime at the END of each collect() call, read at the START of the next.
# The measurement window is the full inter-call interval (~2.5 s BLE, ~2 s serial)
# which is much closer to Activity Monitor's ~5 s averaging window than a 1 s sleep.
# Each entry is a (Process, cached_name) tuple — the name is captured during priming
# so the read step never calls p.name() again (avoids a second OpenProcess per proc
# on Windows, which is the main source of >6 s cycle times on Windows).
_primed_procs: list = []  # list[tuple[psutil.Process, str]]

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


# ---- Windows fast path: NtQuerySystemInformation -------------------------
# psutil's cpu_percent() on Windows opens an individual process handle
# (OpenProcess + GetProcessTimes + CloseHandle) for every process in the
# list — ~10 ms × 500 processes = 5+ seconds per cycle.  Instead, we call
# NtQuerySystemInformation(SystemProcessInformation) once, which returns
# CPU times for ALL processes in a single kernel call with no per-process
# handle opens.  This brings the full collect() cycle to <50 ms on Windows.
#
# SYSTEM_PROCESS_INFORMATION field offsets (64-bit Windows 7+):
#   +0   NextEntryOffset         ULONG
#   +8   WorkingSetPrivateSize   SIZE_T (bytes) — Task Manager "Memory" column
#   +40  UserTime                LARGE_INTEGER (100 ns units)
#   +48  KernelTime              LARGE_INTEGER (100 ns units)
#   +56  ImageName.Length        USHORT (byte count, divide by 2 for chars)
#   +64  ImageName.Buffer        ULONGLONG pointer to wchar_t string
#   +80  UniqueProcessId         ULONGLONG
if sys.platform == 'win32':
    import ctypes.wintypes as _wt
    _ntdll = ctypes.WinDLL('ntdll')
    _STATUS_SUCCESS               = 0
    _STATUS_INFO_LENGTH_MISMATCH  = 0xC0000004
    _SystemProcessInformation     = 5
    _SPI_NEXT       = 0
    _SPI_WS_PRIVATE = 8    # WorkingSetPrivateSize — Task Manager "Memory" column
    _SPI_UTIME      = 40
    _SPI_KTIME      = 48
    _SPI_NAMELEN    = 56
    _SPI_NAMEBUF    = 64
    _SPI_PID        = 80

    def _nt_query_processes() -> dict:
        """Single NtQuerySystemInformation call → {pid: (name, cpu_100ns, ws_bytes)}."""
        buf_size = 0x200000  # 2 MB initial; doubled on STATUS_INFO_LENGTH_MISMATCH
        for _ in range(5):
            buf = (ctypes.c_ubyte * buf_size)()
            ret_len = _wt.ULONG(0)
            status = _ntdll.NtQuerySystemInformation(
                _SystemProcessInformation, buf, buf_size, ctypes.byref(ret_len))
            if status == _STATUS_SUCCESS:
                break
            if status == _STATUS_INFO_LENGTH_MISMATCH:
                buf_size = ret_len.value + 0x10000
                continue
            return {}  # unexpected NTSTATUS
        raw = bytes(buf[:ret_len.value])
        result = {}
        pos = 0
        while pos < len(raw):
            try:
                next_off, = struct.unpack_from('<I', raw, pos + _SPI_NEXT)
                utime,    = struct.unpack_from('<q', raw, pos + _SPI_UTIME)
                ktime,    = struct.unpack_from('<q', raw, pos + _SPI_KTIME)
                nlen,     = struct.unpack_from('<H', raw, pos + _SPI_NAMELEN)
                nbuf,     = struct.unpack_from('<Q', raw, pos + _SPI_NAMEBUF)
                pid,      = struct.unpack_from('<Q', raw, pos + _SPI_PID)
                ws,       = struct.unpack_from('<q', raw, pos + _SPI_WS_PRIVATE)
                # Buffer pointer is absolute VA inside our allocated `buf`
                name = ctypes.wstring_at(nbuf, nlen // 2) if nlen and nbuf else ''
                result[pid] = (name, utime + ktime, ws)
            except Exception:
                pass
            if not next_off:
                break
            pos += next_off
        return result

    _win_snapshot: dict = {}   # pid → (name, total_100ns, ws_bytes)
    _win_snap_ts: float = 0.0

    def _win_collect() -> list:
        """Two NtQuerySystemInformation snapshots → top processes by CPU%.
        Aggregates all instances of the same exe name into one row, matching
        Task Manager's grouping (e.g. 35 firefox.exe processes become one entry)."""
        global _win_snapshot, _win_snap_ts
        now     = time.perf_counter()
        curr    = _nt_query_processes()
        elapsed = now - _win_snap_ts
        agg: dict = {}   # name → [cpu_sum, ws_sum]
        if _win_snapshot and elapsed > 0:
            wall_100ns = elapsed * _CPU_COUNT * 1e7
            for pid, (name, ct, ws) in curr.items():
                if pid in _win_snapshot and name and name.lower().strip() not in _SKIP_NAMES:
                    delta = ct - _win_snapshot[pid][1]
                    if delta >= 0:
                        cpu   = delta / wall_100ns * 100
                        entry = agg.setdefault(name, [0.0, 0])
                        entry[0] += cpu
                        entry[1] += ws
        _win_snapshot = curr
        _win_snap_ts  = now
        result = []
        for name, (cpu_sum, ws_sum) in agg.items():
            cpu = round(cpu_sum, 1)
            if cpu > 0:
                result.append({'n': name[:MAX_NAME_LEN], 'c': cpu, 'm': ws_sum / 1_048_576})
        result.sort(key=lambda x: x['c'], reverse=True)
        return result


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def collect() -> dict:
    """
    Return top MAX_PROCS processes by CPU %, with system-wide CPU% and RAM%.

    On Windows: uses NtQuerySystemInformation(SystemProcessInformation) to
    snapshot CPU times for all processes in a single kernel call with no
    per-process handle opens.  Two snapshots bracketing the inter-call sleep
    give the CPU delta.  Aggregates all instances of the same exe by name,
    matching Task Manager's grouping.  Memory is WorkingSetPrivateSize
    (offset +8), which matches the Task Manager "Memory" column.

    On macOS/Linux: cross-poll via psutil — counters are primed at the END of
    each call and read at the START of the next.  The measurement window is
    the full inter-call interval.  Memory is phys_footprint (macOS) or RSS.

    The first call always returns empty process data (baseline not yet set).
    Also returns system-wide CPU% and RAM% as "tc" and "tm".
    """
    global _primed_procs

    # ---- system-wide stats (fast single call on all platforms) ----
    total_cpu = round(psutil.cpu_percent(interval=None), 1)
    ram = psutil.virtual_memory()
    total_mem = round((ram.total - ram.available) / ram.total * 100, 1)

    if sys.platform == 'win32':
        # Single NtQuerySystemInformation call — no per-process OpenProcess.
        data = _win_collect()
        psutil.cpu_percent(interval=None)   # prime system-wide for next call
        data.sort(key=lambda x: x["c"], reverse=True)
        return {"p": data[:MAX_PROCS], "tc": total_cpu, "tm": total_mem, "ok": 1}

    # ---- macOS/Linux: read per-process CPU since last prime ----------------
    data = []
    for p, name in _primed_procs:
        try:
            cpu_raw = p.cpu_percent(interval=None)
            cpu    = round(cpu_raw, 1)
            fp = _proc_mem_bytes(p.pid)
            mem_mb = fp / 1_048_576 if fp is not None else p.memory_info().rss / 1_048_576
            if cpu > 0.0:
                data.append({"n": name[:MAX_NAME_LEN], "c": cpu, "m": mem_mb})
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    # ---- prime for next call ----
    psutil.cpu_percent(interval=None)   # system-wide prime
    new_procs = []
    for p in psutil.process_iter():
        try:
            with p.oneshot():
                name = p.name()
                if name.lower().strip() in _SKIP_NAMES:
                    continue
                p.cpu_percent(interval=None)   # per-process prime
            new_procs.append((p, name))
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            # Privileged processes (e.g. kernel_task on macOS) are silently
            # skipped — reading their CPU stats requires root.
            pass
    _primed_procs = new_procs

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
