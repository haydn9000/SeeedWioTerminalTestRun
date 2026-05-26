// wifiScanner.cpp — Scan for nearby Wi-Fi networks and display them.
//
// Controls:
//   [PRESS]   Rescan
//   [B]       Screenshot
//   [C]       Back to menu

#include <Arduino.h>
#include "globals.h"
#include "rpcWiFi.h"

static const int WIFI_MAX_SHOW = 8;

struct WifiResult { char ssid[33]; int rssi; bool open; };
static WifiResult wfResults[WIFI_MAX_SHOW];
static int        wfCount = 0;

// -------------------------------------------------------------------------
static uint16_t rssiColor(int rssi)
{
    if (rssi >= -60) return tft.color565(60, 210, 100);   // strong — green
    if (rssi >= -80) return tft.color565(210, 175, 0);    // fair   — amber
    return tft.color565(210, 65, 55);                      // weak   — red
}

// -------------------------------------------------------------------------
static void drawWifiHeader(int count = -1)
{
    tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
    tft.fillRect(0, 0, 3, 30, tft.color565(30, 210, 80));   // green accent
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(30, 210, 80), tft.color565(0, 8, 20));
    tft.drawString("// WIFI SCAN", 10, 7);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(20, 148, 60), tft.color565(0, 8, 20));
    if (count >= 0)
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d AP%s", count, count == 1 ? "" : "s");
        tft.drawString(buf, 265, 12);
    }
    else
    {
        tft.drawString("SCAN", 272, 12);
    }
    tft.drawFastHLine(0, 29, 320, tft.color565(0, 80, 40));
}

// -------------------------------------------------------------------------
static void drawWifiScanning()
{
    tft.fillScreen(TFT_BLACK);
    drawWifiHeader();

    uint16_t nc  = tft.color565(30, 210, 80);
    uint16_t nbg = tft.color565(0, 10, 5);
    tft.fillRect(10, 48, 300, 140, nbg);
    int t = 12;
    tft.drawFastHLine(10,  48,  t, nc); tft.drawFastVLine(10,  48,  t, nc);
    tft.drawFastHLine(298, 48,  t, nc); tft.drawFastVLine(309, 48,  t, nc);
    tft.drawFastHLine(10,  187, t, nc); tft.drawFastVLine(10,  175, t, nc);
    tft.drawFastHLine(298, 187, t, nc); tft.drawFastVLine(309, 175, t, nc);
    tft.drawFastHLine(11, 49, 298, tft.color565(0, 40, 20));
    tft.setTextSize(2);
    tft.setTextColor(nc, nbg);
    tft.drawString("> SCANNING...", 24, 80);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 148, 60), nbg);
    tft.drawString("SEARCHING 2.4GHz + 5GHz BANDS", 24, 110);
    tft.drawString("PRESS [C] TO CANCEL", 24, 125);
    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(30, 210, 80));
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 100, 40), TFT_BLACK);
    tft.drawString("[C] CANCEL", 8, 225);
}

static void drawWifiResults()
{
    tft.fillScreen(TFT_BLACK);
    drawWifiHeader(wfCount);

    if (wfCount <= 0)
    {
        uint16_t nc  = tft.color565(30, 210, 80);
        uint16_t nbg = tft.color565(0, 10, 5);
        tft.fillRect(10, 48, 300, 140, nbg);
        int t = 12;
        tft.drawFastHLine(10,  48,  t, nc); tft.drawFastVLine(10,  48,  t, nc);
        tft.drawFastHLine(298, 48,  t, nc); tft.drawFastVLine(309, 48,  t, nc);
        tft.drawFastHLine(10,  187, t, nc); tft.drawFastVLine(10,  175, t, nc);
        tft.drawFastHLine(298, 187, t, nc); tft.drawFastVLine(309, 175, t, nc);
        tft.setTextSize(2);
        tft.setTextColor(nc, nbg);
        tft.drawString("> NO_DATA", 50, 90);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 148, 60), nbg);
        tft.drawString("NO NETWORKS FOUND NEARBY", 50, 118);
        tft.drawString("PRESS [PRESS] TO RESCAN", 50, 134);
    }
    else
    {
        // Column headers
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(20, 148, 60), TFT_BLACK);
        tft.drawString("NETWORK", 20, 38);
        tft.drawString("AUTH", 170, 38);
        tft.drawString("dBm", 228, 38);
        tft.drawString("SIGNAL", 265, 38);

        for (int i = 0; i < wfCount; i++)
        {
            WifiResult& r  = wfResults[i];
            uint16_t    rc = rssiColor(r.rssi);
            int         rowY = 50 + i * 22;

            tft.setTextSize(1);
            tft.setTextColor(tft.color565(200, 235, 215), TFT_BLACK);
            tft.drawString(r.ssid, 20, rowY);

            tft.setTextColor(r.open ? tft.color565(80, 180, 80) : tft.color565(210, 175, 50), TFT_BLACK);
            tft.drawString(r.open ? "OPEN" : "LOCK", 170, rowY);

            char rssiBuf[8];
            snprintf(rssiBuf, sizeof(rssiBuf), "%d", r.rssi);
            tft.setTextColor(rc, TFT_BLACK);
            tft.drawString(rssiBuf, 228, rowY);

            int barFill = (int)((float)(r.rssi + 100) / 70.0f * 50.0f);
            if (barFill < 0)  barFill = 0;
            if (barFill > 50) barFill = 50;
            if (barFill > 0) tft.fillRect(265, rowY, barFill, 8, rc);
            tft.drawRect(265, rowY, 50, 8, tft.color565(20, 40, 25));
        }
    }

    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(30, 210, 80));
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 100, 40), TFT_BLACK);
    tft.drawString("[PRESS] RESCAN   [C] BACK", 8, 225);

    drawBatteryStatus(TFT_BLACK);
}

// -------------------------------------------------------------------------
// Scan, deduplicate by SSID (keep strongest RSSI), sort by signal strength.
// Returns false if the user pressed KEY_C to cancel during the scan.
static bool doWifiScan()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.scanNetworks(true);   // async — returns immediately

    // Poll until scan complete or KEY_C pressed (15-second safety timeout).
    unsigned long scanStart = millis();
    int total = 0;
    while (true)
    {
        int n = WiFi.scanComplete();
        if (n >= 0)              { total = n; break; }   // scan finished
        if (n == WIFI_SCAN_FAILED) break;                // failed — 0 results
        if (millis() - scanStart > 15000) break;         // safety timeout
        if (digitalRead(WIO_KEY_C) == LOW)
        {
            WiFi.scanDelete();   // free partial results
            while (digitalRead(WIO_KEY_C) == LOW) delay(10);
            return false;        // cancelled
        }
        delay(20);
    }
    WiFi.disconnect();   // release AP association; WIFI_OFF hangs (library bug)

    wfCount = 0;
    for (int i = 0; i < total; i++)
    {
        String ssid = WiFi.SSID(i);
        int    rssi = WiFi.RSSI(i);
        bool   open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

        // Merge duplicate SSIDs — keep the strongest RSSI entry.
        bool found = false;
        for (int j = 0; j < wfCount; j++)
        {
            if (ssid == wfResults[j].ssid)
            {
                if (rssi > wfResults[j].rssi) wfResults[j].rssi = rssi;
                found = true;
                break;
            }
        }
        if (!found && wfCount < WIFI_MAX_SHOW)
        {
            const char* src = ssid.length() ? ssid.c_str() : "(hidden)";
            strncpy(wfResults[wfCount].ssid, src, 32);
            wfResults[wfCount].ssid[32] = '\0';
            wfResults[wfCount].rssi = rssi;
            wfResults[wfCount].open = open;
            wfCount++;
        }
    }
    WiFi.scanDelete();   // free scan result memory after we've copied all SSIDs

    // Sort by RSSI descending (strongest first).
    for (int i = 0; i < wfCount - 1; i++)
        for (int j = i + 1; j < wfCount; j++)
            if (wfResults[j].rssi > wfResults[i].rssi)
            {
                WifiResult tmp = wfResults[i];
                wfResults[i]  = wfResults[j];
                wfResults[j]  = tmp;
            }
    return true;
}

// -------------------------------------------------------------------------
void wifiScannerScreen()
{
    while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);

    drawWifiScanning();
    if (!doWifiScan())
    {
        bleReinit();   // restore BLE — WiFi.mode(WIFI_STA) resets the RTL8720DN
        delay(50);
        return;        // user cancelled during initial scan
    }
    drawWifiResults();

    while (true)
    {
        if (digitalRead(WIO_5S_PRESS) == LOW)
        {
            while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);
            drawWifiScanning();
            if (!doWifiScan())
            {
                bleReinit();
                delay(50);
                return;   // user cancelled during rescan
            }
            drawWifiResults();
        }

        if (digitalRead(WIO_KEY_B) == LOW)
        {
            while (digitalRead(WIO_KEY_B) == LOW) delay(10);
            takeScreenshot();
        }

        if (digitalRead(WIO_KEY_C) == LOW)
        {
            while (digitalRead(WIO_KEY_C) == LOW) delay(10);
            bleReinit();   // restore BLE — WiFi.mode(WIFI_STA) resets the RTL8720DN
            delay(50);
            return;
        }

        delay(20);
    }
}
