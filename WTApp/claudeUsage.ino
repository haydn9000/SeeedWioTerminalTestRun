
//========================================================================= CLAUDE USAGE
// Mirrors the data model from the Clawdmeter project (Clawdmeter-main/firmware/src/data.h).
// Populate usageData from a serial/BLE source; the screen shows "No data" until valid = true.

struct UsageData {
    float session_pct;          // 5-hour window utilization (0–100).
    int   session_reset_mins;   // Minutes until the 5h session window resets.
    float weekly_pct;           // 7-day window utilization (0–100).
    int   weekly_reset_mins;    // Minutes until the weekly window resets.
    char  status[16];           // "allowed" or "limited".
    char  session_reset_str[56];// Formatted local time, e.g. "Resets 3:50pm (EDT)".
    char  weekly_reset_str[56]; // e.g. "Resets May 18, 8pm (EDT)".
    bool  valid;                // Stays false until the first successful data update.
};

// Global usage state — updated by checkSerial() whenever a new JSON line arrives.
UsageData usageData = { 0.0f, 0, 0.0f, 0, "unknown", "", "", false };

// Incremented every time a successful JSON parse completes.
// claudeUsageScreen() watches this to trigger a redraw on every new packet.
static uint16_t dataVersion = 0;

// -------------------------------------------------------------------------
// Parses one line of compact JSON from the sender script.
// Expected format:
//   {"s":45,"sr":142,"w":67,"wr":2580,"st":"allowed","srt":"Resets 3:50pm (America/New_York)","wrt":"Resets May 18, 8pm (America/New_York)","ok":true}
//
// Keys: s=session%, sr=session_reset_mins, w=weekly%, wr=weekly_reset_mins,
//       st=status, srt=session reset time string, wrt=weekly reset time string
// Returns true on success.
bool parseUsageJson(const char* json)
{
    const char* p;

    // "s": session percentage (integer 0-100).
    // Searches for the exact sequence "s": — keys like "sr" and "st" have extra
    // characters before their closing quote so they won't accidentally match.
    p = strstr(json, "\"s\":");
    if (!p) return false;
    usageData.session_pct = (float)atoi(p + 4);

    // "sr": session reset minutes.
    p = strstr(json, "\"sr\":");
    if (!p) return false;
    usageData.session_reset_mins = atoi(p + 5);

    // "w": weekly percentage.
    p = strstr(json, "\"w\":");
    if (!p) return false;
    usageData.weekly_pct = (float)atoi(p + 4);

    // "wr": weekly reset minutes.
    p = strstr(json, "\"wr\":");
    if (!p) return false;
    usageData.weekly_reset_mins = atoi(p + 5);

    // "st": status string — extract the value between the quotes.
    p = strstr(json, "\"st\":\"");
    if (!p) return false;
    p += 6;
    int i = 0;
    while (*p && *p != '"' && i < 15)
        usageData.status[i++] = *p++;
    usageData.status[i] = '\0';

    // "srt": session reset formatted time string (optional — sender may omit).
    p = strstr(json, "\"srt\":\"");
    if (p) {
        p += 7;
        int i = 0;
        while (*p && *p != '"' && i < 55) usageData.session_reset_str[i++] = *p++;
        usageData.session_reset_str[i] = '\0';
    }

    // "wrt": weekly reset formatted time string (optional).
    p = strstr(json, "\"wrt\":\"");
    if (p) {
        p += 7;
        int i = 0;
        while (*p && *p != '"' && i < 55) usageData.weekly_reset_str[i++] = *p++;
        usageData.weekly_reset_str[i] = '\0';
    }

    usageData.valid = true;
    dataVersion++;          // signal that the screen should redraw
    return true;
}

// -------------------------------------------------------------------------
// Called every loop() iteration. Reads a complete newline-terminated JSON
// string from Serial (non-blocking) and updates usageData if one is ready.
static char serialBuf[256];  // 256 to fit the longer JSON with formatted time strings
static int  serialPos = 0;

void checkSerial()
{
    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r')
        {
            if (serialPos > 0)
            {
                serialBuf[serialPos] = '\0';
                parseUsageJson(serialBuf);
                serialPos = 0;
            }
        }
        else if (serialPos < (int)sizeof(serialBuf) - 1)
        {
            serialBuf[serialPos++] = c;
        }
        else
        {
            // Line too long — discard and reset.
            serialPos = 0;
        }
    }
}

// -------------------------------------------------------------------------
// Draws a small Claude-style 6-point asterisk (3 lines at 0°/60°/120°).
void drawClaudeStar(int cx, int cy, uint16_t color)
{
    tft.drawLine(cx - 5, cy,     cx + 5, cy,     color); // horizontal
    tft.drawLine(cx - 5, cy + 1, cx + 5, cy + 1, color); // horizontal (double-width)
    tft.drawLine(cx - 2, cy - 4, cx + 2, cy + 4, color); // 60°
    tft.drawLine(cx + 2, cy - 4, cx - 2, cy + 4, color); // 120°
}

// -------------------------------------------------------------------------
// Returns a colour for a given usage percentage using the Claude brand palette:
//   < 60%  → Claude coral-orange (primary brand colour — healthy headroom)
//   < 85%  → deep amber (approaching limit)
//   >= 85% → warning red (near or at cap)
uint16_t usageColor(float pct)
{
    if (pct < 60.0f) return tft.color565(217, 119, 87);   // Claude coral
    if (pct < 85.0f) return tft.color565(195, 135, 45);   // deep amber
    return tft.color565(210, 65, 55);                      // warning red
}

// -------------------------------------------------------------------------
// Draws one labelled usage row at vertical position y.
// Layout (fits 320px wide):
//   [label]                               (small text, y)
//   [████░░░░░░░░░░░░░░░░░░░] XX%        (bar 240px + pct at x=268, y+13)
//   [Resets in Xh Xm]                    (small text, y+39)
//   [Resets 3:50pm (America/New_York)]   (small text, y+49, if resetStr non-empty)
void drawUsageRow(const char* label, float pct, int resetMins, const char* resetStr, int y)
{
    // Clamp to valid range.
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;

    uint16_t col = usageColor(pct);

    // Section label — warm brownish grey to match Claude's warm palette.
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(190, 152, 135), TFT_BLACK);
    tft.drawString(label, 20, y);

    // Bar track (hollow) then filled portion.
    int barY = y + 13;
    int barW = 228;
    int barH = 22;    // tall enough to contain the percentage text (textSize 2 = 16px).
    tft.drawRoundRect(20, barY, barW, barH, 4, tft.color565(135, 105, 92));
    if (pct > 0.0f)
        tft.fillRoundRect(20, barY, (int)(barW * pct / 100.0f), barH, 4, col);

    // Percentage label to the right of the bar.
    char pctBuf[8];
    sprintf(pctBuf, "%.0f%%", pct);
    tft.setTextSize(2);
    tft.setTextColor(col, TFT_BLACK);
    tft.drawString(pctBuf, 256, barY + 3);

    // Reset countdown — brighter colour than the section label so it reads as secondary info.
    char resetBuf[40];
    if (resetMins >= 1440) {
        int days  = resetMins / 1440;
        int hours = (resetMins % 1440) / 60;
        int mins  = resetMins % 60;
        if (hours > 0)
            sprintf(resetBuf, "Resets in %dd %dh %dm", days, hours, mins);
        else
            sprintf(resetBuf, "Resets in %dd %dm",  days, mins);
    } else if (resetMins >= 60) {
        int hours = resetMins / 60;
        int mins  = resetMins % 60;
        sprintf(resetBuf, "Resets in %dh %dm", hours, mins);
    } else if (resetMins > 0) {
        sprintf(resetBuf, "Resets in %dm", resetMins);
    } else {
        sprintf(resetBuf, "Resetting now");
    }
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(215, 175, 148), TFT_BLACK);
    tft.drawString(resetBuf, 20, barY + barH + 4);

    // Absolute local-time line — same font, dimmer colour.
    if (resetStr && resetStr[0] != '\0') {
        tft.setTextColor(tft.color565(152, 118, 102), TFT_BLACK);
        tft.drawString(resetStr, 20, barY + barH + 14);
    }
}

// -------------------------------------------------------------------------
// Renders the full Claude Usage screen.
void drawClaudeUsage(int mode)
{
    tft.fillScreen(TFT_BLACK);

    // Title — Claude coral-orange.
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(217, 119, 87), TFT_BLACK);
    tft.drawString("CLAUDE USAGE", 72, 8);

    if (!usageData.valid)
    {
        // Bordered card — coral outer glow + warm fill, themed after Claude.
        tft.fillRoundRect(8, 75, 304, 110, 8, tft.color565(28, 14, 10));          // dark coral-tinted fill
        tft.drawRoundRect(8, 75, 304, 110, 8, tft.color565(217, 119, 87));        // coral border (thick layer 1)
        tft.drawRoundRect(9, 76, 302, 108, 7, tft.color565(180, 90, 62));         // coral border (thick layer 2)
        tft.drawRoundRect(10, 77, 300, 106, 6, tft.color565(130, 58, 38));        // coral border (thick layer 3)

        // Claude star centred above the text.
        drawClaudeStar(160, 103, tft.color565(217, 119, 87));

        // Placeholder text.
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(217, 119, 87), tft.color565(28, 14, 10));
        tft.drawString("No data received", 36, 118);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(190, 152, 135), tft.color565(28, 14, 10));
        if (mode == 1) {
            tft.drawString("Run ble_sender.py on your PC.", 20, 145);
            tft.drawString("Device: WT-001", 20, 157);
        } else {
            tft.drawString("Run serial_sender.py <port>.", 20, 145);
            tft.drawString("e.g. serial_sender.py COM7", 20, 157);
        }
    }
    else
    {
        // Session row — 5-hour rolling window.
        // y=35: label at 35, bar 48–70 (h=22), reset countdown at 74, absolute time at 84.
        drawUsageRow("SESSION  (5h window)", usageData.session_pct, usageData.session_reset_mins, usageData.session_reset_str, 35);

        // Weekly row — 7-day rolling window.
        // y=112: leaves ~16px gap after the session row ends at ~96.
        drawUsageRow("WEEKLY   (7d window)", usageData.weekly_pct, usageData.weekly_reset_mins, usageData.weekly_reset_str, 112);

        // Status row at y=187: flanking session% + weekly% stats beside the pill badge.
        // "LIMITED" / "ALLOWED" = 7 chars × 6px + 16px padding = 58px wide badge.
        bool limited = (strncmp(usageData.status, "limited", 7) == 0);
        const char* statusText  = limited ? "LIMITED" : "ALLOWED";
        uint16_t    badgeColor  = limited ? tft.color565(210, 65, 55)  : tft.color565(175, 140, 60);   // red or muted gold
        uint16_t    badgeBg     = limited ? tft.color565(55, 18, 15)   : tft.color565(40, 32, 8);      // dark red or dark gold
        int badgeW = 58;
        int badgeH = 16;
        int badgeX = (320 - badgeW) / 2;  // centred: (320-58)/2 = 131
        int badgeY = 187;

        // Left flanking stat: session percentage in its usage colour.
        char s5h[8]; sprintf(s5h, "%d%%", (int)usageData.session_pct);
        tft.setTextSize(2);
        tft.setTextColor(usageColor(usageData.session_pct), TFT_BLACK);
        tft.drawString(s5h, 28, badgeY);

        // Right flanking stat: weekly percentage — right-aligned to x=300.
        char s7d[8]; sprintf(s7d, "%d%%", (int)usageData.weekly_pct);
        tft.setTextColor(usageColor(usageData.weekly_pct), TFT_BLACK);
        tft.drawString(s7d, 300 - (int)strlen(s7d) * 12, badgeY);

        // Pill badge + Claude star to its left.
        tft.setTextSize(1);
        badgeW = 58;
        badgeH = 16;
        badgeX = (320 - badgeW) / 2;
        tft.fillRoundRect(badgeX, badgeY, badgeW, badgeH, 5, badgeBg);
        tft.drawRoundRect(badgeX, badgeY, badgeW, badgeH, 5, badgeColor);
        tft.setTextColor(badgeColor, badgeBg);
        tft.drawString(statusText, badgeX + 8, badgeY + 4);
        drawClaudeStar(badgeX - 11, badgeY + 8, badgeColor);
    }

    // Back hint — warm muted to match the palette.
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(148, 112, 98), TFT_BLACK);
    tft.drawString("C: back", 20, 224);

    drawBatteryStatus(TFT_BLACK);
}

// -------------------------------------------------------------------------
// Claude Usage sub-screen. Blocks until KEY_C is pressed.
void claudeUsageScreen(int mode)
{
    // Entry debounce — wait for joystick press from the picker to be released.
    while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }

    // Reset stale data from a previous session so we don't show the wrong source's data.
    usageData.valid = false;
    dataVersion++;

    // Enable BLE advertising only while we're in BLE mode.
    if (mode == 1) bleSetActive(true);

    drawClaudeUsage(mode);

    uint16_t drawnVersion = dataVersion;

    while (true)
    {
        checkSerial();
        checkBLE();

        if (dataVersion != drawnVersion)
        {
            drawnVersion = dataVersion;
            drawClaudeUsage(mode);
        }

        if (digitalRead(WIO_KEY_C) == LOW)
        {
            while (digitalRead(WIO_KEY_C) == LOW) { delay(10); }
            delay(50);
            if (mode == 1) bleSetActive(false);
            return;
        }
    }
}

// -------------------------------------------------------------------------
// Data source picker shown before the usage screen.
// Returns 0 = Serial, 1 = Bluetooth, -1 = back (KEY_C pressed).
int claudeUsagePicker()
{
    // Wait for the joystick press that launched us to be released.
    while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }

    const char* items[] = { "USB Serial", "Bluetooth" };
    static int sel = 0;  // Persists across calls so the last-used source is pre-selected.

    auto drawPicker = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(217, 119, 87), TFT_BLACK);
        tft.drawString("CLAUDE USAGE", 72, 8);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(190, 152, 135), TFT_BLACK);
        tft.drawString("Select data source:", 20, 38);

        for (int i = 0; i < 2; i++) {
            int y = 65 + i * 65;
            if (i == sel) {
                tft.fillRoundRect(20, y, 280, 48, 6, TFT_WHITE);
                tft.setTextSize(2);
                tft.setTextColor(TFT_BLACK, TFT_WHITE);
            } else {
                tft.fillRoundRect(20, y, 280, 48, 6, TFT_BLACK);
                tft.drawRoundRect(20, y, 280, 48, 6, tft.color565(135, 105, 92));
                tft.setTextSize(2);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
            }
            tft.drawString(items[i], 35, y + 14);
        }

        tft.setTextSize(1);
        tft.setTextColor(tft.color565(148, 112, 98), TFT_BLACK);
        tft.drawString("UP/DOWN: choose   PRESS: confirm   C: back", 8, 224);
    };

    drawPicker();

    while (true)
    {
        if (digitalRead(WIO_5S_UP) == LOW) {
            sel = (sel - 1 + 2) % 2;
            drawPicker();
            delay(200);
        } else if (digitalRead(WIO_5S_DOWN) == LOW) {
            sel = (sel + 1) % 2;
            drawPicker();
            delay(200);
        } else if (digitalRead(WIO_5S_PRESS) == LOW) {
            return sel;  // debounce handled in claudeUsageScreen entry
        } else if (digitalRead(WIO_KEY_C) == LOW) {
            while (digitalRead(WIO_KEY_C) == LOW) { delay(10); }
            delay(50);
            return -1;
        }
        delay(20);
    }
}
