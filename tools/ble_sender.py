#!/usr/bin/env python3
"""
ble_sender.py — Sends Claude API usage data to the Wio Terminal over BLE.

Scans for a device named "WT-001", connects, then polls the Anthropic API
every POLL_INTERVAL seconds and writes a compact JSON line to the RX
characteristic. The Wio Terminal feeds this into parseUsageJson() and
updates the Claude Usage screen.

Usage:
    python ble_sender.py                      # auto-discover WT-001
    python ble_sender.py <address>            # connect to specific BLE address
      Windows:  python ble_sender.py "AA:BB:CC:DD:EE:FF"
      macOS:    python ble_sender.py "12345678-XXXX-XXXX-XXXX-XXXXXXXXXXXX"

Requirements:
    pip install bleak httpx

Credentials:
    Reads the same OAuth token as serial_sender.py:
    ~/.claude/.credentials.json  (field: "accessToken")
"""

import asyncio
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import httpx
from bleak import BleakClient, BleakScanner

# ---- Configuration -------------------------------------------------------
DEVICE_NAME   = "WT-001"
SERVICE_UUID  = "4e495554-494f-5500-0000-000000000001"
RX_CHAR_UUID  = "4e495554-494f-5500-0000-000000000002"
POLL_INTERVAL = 60   # Seconds between API polls.

# ---- Anthropic API -------------------------------------------------------
API_URL  = "https://api.anthropic.com/v1/messages"
API_BODY = {
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [{"role": "user", "content": "hi"}],
}
API_HEADERS_TEMPLATE = {
    "anthropic-version": "2023-06-01",
    "anthropic-beta":    "oauth-2025-04-20",
    "Content-Type":      "application/json",
    "User-Agent":        "claude-code/2.1.5",
}


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


def format_reset_time(reset_ts: str) -> str:
    """Convert a Unix timestamp to a local-time display string."""
    try:
        ts = float(reset_ts)
    except (ValueError, TypeError):
        return ""
    dt      = datetime.fromtimestamp(ts)
    now     = datetime.now()
    tz_abbr = datetime.now().astimezone().strftime('%Z')
    hour    = dt.hour % 12 or 12
    ampm    = "am" if dt.hour < 12 else "pm"
    mins    = f":{dt.minute:02d}" if dt.minute != 0 else ""
    time_s  = f"{hour}{mins}{ampm}"
    if dt.date() == now.date():
        return f"Resets {time_s} ({tz_abbr})"
    months = ["Jan","Feb","Mar","Apr","May","Jun",
              "Jul","Aug","Sep","Oct","Nov","Dec"]
    return f"Resets {months[dt.month - 1]} {dt.day}, {time_s} ({tz_abbr})"


def load_token() -> str | None:
    """Load the OAuth access token from ~/.claude/.credentials.json."""
    creds_path = Path.home() / ".claude" / ".credentials.json"
    if not creds_path.exists():
        log(f"Credentials file not found: {creds_path}")
        return None
    blob = creds_path.read_text().strip()
    try:
        data = json.loads(blob)
        if isinstance(data.get("accessToken"), str):
            return data["accessToken"]
        for v in data.values():
            if isinstance(v, dict) and isinstance(v.get("accessToken"), str):
                return v["accessToken"]
    except (json.JSONDecodeError, AttributeError):
        pass
    m = re.search(r'"accessToken"\s*:\s*"([^"]+)"', blob)
    return m.group(1) if m else None


def poll_api(token: str) -> dict | None:
    """Poll the Anthropic API and return a compact usage payload dict."""
    headers = {**API_HEADERS_TEMPLATE, "Authorization": f"Bearer {token}"}
    try:
        resp = httpx.post(API_URL, json=API_BODY, headers=headers, timeout=15)
    except httpx.RequestError as e:
        log(f"API request failed: {e}")
        return None

    if resp.status_code not in (200, 429):
        log(f"Unexpected status {resp.status_code}")
        return None

    def hdr(name: str, default: str = "0") -> str:
        return resp.headers.get(name, default)

    now = time.time()

    def reset_minutes(ts: str) -> int:
        try:
            r = float(ts)
        except ValueError:
            return 0
        return max(0, int(round((r - now) / 60.0)))

    def pct(util: str) -> int:
        try:
            return int(round(float(util) * 100))
        except ValueError:
            return 0

    sr_ts = hdr("anthropic-ratelimit-unified-5h-reset", "0")
    wr_ts = hdr("anthropic-ratelimit-unified-7d-reset", "0")

    return {
        "s":   pct(hdr("anthropic-ratelimit-unified-5h-utilization")),
        "sr":  reset_minutes(sr_ts),
        "w":   pct(hdr("anthropic-ratelimit-unified-7d-utilization")),
        "wr":  reset_minutes(wr_ts),
        "st":  hdr("anthropic-ratelimit-unified-5h-status", "unknown"),
        "srt": format_reset_time(sr_ts),
        "wrt": format_reset_time(wr_ts),
        "ok":  True,
    }


async def find_device(address: str | None) -> str | None:
    """Scan for WT-001 and return its address, or verify a given address."""
    if address:
        log(f"Using provided address: {address}")
        return address

    log(f'Scanning for "{DEVICE_NAME}"...')
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if device:
        log(f"Found {device.name} at {device.address}")
        return device.address
    log(f'Device "{DEVICE_NAME}" not found in scan. Is the Wio Terminal on and advertising?')
    return None


async def run(token: str, address: str | None) -> None:
    """Main async loop: discover → connect → poll → write indefinitely."""
    while True:
        addr = await find_device(address)
        if not addr:
            log("Retrying scan in 10s...")
            await asyncio.sleep(10)
            continue

        try:
            async with BleakClient(addr) as client:
                log(f"Connected to {addr}")
                last_poll = 0.0

                while client.is_connected:
                    now = time.time()
                    if now - last_poll >= POLL_INTERVAL:
                        log("Polling Anthropic API...")
                        payload = poll_api(token)
                        if payload:
                            line = json.dumps(payload, separators=(",", ":")) + "\n"
                            data = line.encode()
                            # BLE characteristic max write is typically 512 bytes;
                            # our JSON is well under that limit.
                            await client.write_gatt_char(RX_CHAR_UUID, data, response=True)
                            log(f"Sent ({len(data)}B): {line.strip()}")
                        else:
                            log("Poll failed — will retry next interval.")
                        last_poll = time.time()
                    await asyncio.sleep(1)

                log("Disconnected.")
        except Exception as e:
            log(f"BLE error: {e}")

        log("Reconnecting in 5s...")
        await asyncio.sleep(5)


def main() -> None:
    address = sys.argv[1] if len(sys.argv) > 1 else None

    token = load_token()
    if not token:
        log("Could not load API token — aborting.")
        sys.exit(1)
    log("Token loaded.")

    try:
        asyncio.run(run(token, address))
    except KeyboardInterrupt:
        log("Stopped.")


if __name__ == "__main__":
    main()
