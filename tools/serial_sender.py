#!/usr/bin/env python3
"""
serial_sender.py — Sends Claude API usage data to the Wio Terminal over USB serial.

Polls the Anthropic API every POLL_INTERVAL seconds and writes a compact JSON
line to the serial port. The Wio Terminal reads this in checkSerial() and
updates the Claude Usage screen.

Usage:
    python serial_sender.py COM3          # Windows
    python serial_sender.py /dev/ttyACM0  # Linux
    python serial_sender.py /dev/cu.usbmodem... # macOS

Requirements:
    pip install pyserial httpx

Credentials:
    Reads the same OAuth token as the Clawdmeter daemon:
    - Windows/macOS/Linux: ~/.claude/.credentials.json
    The token is extracted from the "accessToken" field (same logic as
    claude_usage_daemon.py).
"""

import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import httpx
import serial

# ---- Configuration -------------------------------------------------------
POLL_INTERVAL = 60       # Seconds between API polls.
BAUD_RATE     = 115200   # Must match Serial.begin() in WioTApp.ino.

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

def format_reset_time(reset_ts: str) -> str:
    """
    Convert a Unix timestamp to a local-time display string using the system timezone.
    Same-day:    "Resets 3:50pm (EDT)"
    Future day:  "Resets May 18, 8pm (EDT)"
    """
    try:
        ts = float(reset_ts)
    except (ValueError, TypeError):
        return ""
    dt      = datetime.fromtimestamp(ts)          # converts to local time automatically
    now     = datetime.now()
    tz_abbr = datetime.now().astimezone().strftime('%Z')  # e.g. "EDT", "BST"
    hour    = dt.hour % 12 or 12
    ampm    = "am" if dt.hour < 12 else "pm"
    mins    = f":{dt.minute:02d}" if dt.minute != 0 else ""
    time_s  = f"{hour}{mins}{ampm}"
    if dt.date() == now.date():
        return f"Resets {time_s} ({tz_abbr})"
    months = ["Jan","Feb","Mar","Apr","May","Jun",
              "Jul","Aug","Sep","Oct","Nov","Dec"]
    return f"Resets {months[dt.month - 1]} {dt.day}, {time_s} ({tz_abbr})"


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)





def load_token() -> str | None:
    """Load the OAuth access token from ~/.claude/.credentials.json."""
    creds_path = Path.home() / ".claude" / ".credentials.json"
    if not creds_path.exists():
        log(f"Credentials file not found: {creds_path}")
        return None
    blob = creds_path.read_text().strip()
    # Try JSON parse first, then regex fallback.
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
    """
    Make a minimal API call and read usage headers from the response.
    Returns a compact payload dict or None on failure.
    """
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
        mins = (r - now) / 60.0
        return max(0, int(round(mins)))

    def pct(util: str) -> int:
        try:
            return int(round(float(util) * 100))
        except ValueError:
            return 0

    sr_ts = hdr("anthropic-ratelimit-unified-5h-reset", "0")
    wr_ts = hdr("anthropic-ratelimit-unified-7d-reset", "0")

    # Compact keys match what parseUsageJson() in claudeUsage.ino expects.
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


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <serial_port>")
        print("  e.g. python serial_sender.py COM3")
        sys.exit(1)

    port = sys.argv[1]

    token = load_token()
    if not token:
        log("Could not load API token — aborting.")
        sys.exit(1)
    log("Token loaded.")

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=1)
        log(f"Opened {port} at {BAUD_RATE} baud.")
    except serial.SerialException as e:
        log(f"Could not open {port}: {e}")
        sys.exit(1)

    last_poll = 0.0
    try:
        while True:
            now = time.time()
            if now - last_poll >= POLL_INTERVAL:
                log("Polling Anthropic API...")
                payload = poll_api(token)
                if payload:
                    line = json.dumps(payload, separators=(",", ":")) + "\n"
                    ser.write(line.encode())
                    log(f"Sent: {line.strip()}")
                    last_poll = now
                else:
                    log("Poll failed — will retry next interval.")
                    last_poll = now  # avoid hammering on repeated failures
            time.sleep(1)
    except KeyboardInterrupt:
        log("Stopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
