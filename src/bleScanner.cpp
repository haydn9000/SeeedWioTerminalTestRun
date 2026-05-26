// bleScanner.cpp — Scan for nearby BLE devices and display them with RSSI.
//
// Controls:
//   [PRESS]   Rescan (3-second scan, advertising paused during scan)
//   [C]       Back to menu

#include <Arduino.h>
#include "globals.h"
#include <rpcBLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

static const int BLE_MAX_RESULTS = 8;

struct BLEResult { char name[20]; char addr[18]; int rssi; };

static BLEResult bleResults[BLE_MAX_RESULTS];
static int       bleResultCount = 0;

// Async scan completion — set from BLE stack callback, polled on main thread.
static volatile bool g_bleScanDone = false;
static void bleScanCallback(BLEScanResults) { g_bleScanDone = true; }

// -------------------------------------------------------------------------
static uint16_t rssiColor(int rssi)
{
    if (rssi >= -60) return tft.color565(60, 210, 100);   // strong — green
    if (rssi >= -80) return tft.color565(210, 175, 0);    // fair   — amber
    return tft.color565(210, 65, 55);                      // weak   — red
}

// -------------------------------------------------------------------------
static void drawBleHeader()
{
    tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
    tft.fillRect(0, 0, 3, 30, tft.color565(0, 220, 245));
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
    tft.drawString("// BLE SCAN", 10, 7);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 148, 170), tft.color565(0, 8, 20));
    tft.drawString("NEARBY", 270, 12);
    tft.drawFastHLine(0, 29, 320, tft.color565(0, 80, 100));
    for (int xi = 8; xi < 320; xi += 14)
        tft.drawFastVLine(xi, 27, 4, tft.color565(0, 140, 165));
}

// -------------------------------------------------------------------------
static void drawBleScanning()
{
    tft.fillScreen(TFT_BLACK);
    drawBleHeader();

    uint16_t nc  = tft.color565(0, 220, 245);
    uint16_t nbg = tft.color565(0, 8, 18);
    tft.fillRect(10, 48, 300, 140, nbg);
    int t = 12;
    tft.drawFastHLine(10,  48,  t, nc); tft.drawFastVLine(10,  48,  t, nc);
    tft.drawFastHLine(298, 48,  t, nc); tft.drawFastVLine(309, 48,  t, nc);
    tft.drawFastHLine(10,  187, t, nc); tft.drawFastVLine(10,  175, t, nc);
    tft.drawFastHLine(298, 187, t, nc); tft.drawFastVLine(309, 175, t, nc);
    tft.drawFastHLine(11, 49, 298, tft.color565(0, 40, 60));

    tft.setTextSize(2);
    tft.setTextColor(nc, nbg);
    tft.drawString("> SCANNING...", 24, 80);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 158, 178), nbg);
    tft.drawString("SCANNING FOR 3 SECONDS", 24, 110);
    tft.drawString("ADVERTISING PAUSED DURING SCAN", 24, 125);
    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(0, 220, 245));
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
    tft.drawString("[C] CANCEL", 8, 225);
}

// -------------------------------------------------------------------------
static void drawBleScanResults()
{
    tft.fillScreen(TFT_BLACK);
    drawBleHeader();

    if (bleResultCount == 0)
    {
        uint16_t nc  = tft.color565(0, 220, 245);
        uint16_t nbg = tft.color565(0, 8, 18);
        tft.fillRect(10, 48, 300, 140, nbg);
        int t = 12;
        tft.drawFastHLine(10,  48,  t, nc); tft.drawFastVLine(10,  48,  t, nc);
        tft.drawFastHLine(298, 48,  t, nc); tft.drawFastVLine(309, 48,  t, nc);
        tft.drawFastHLine(10,  187, t, nc); tft.drawFastVLine(10,  175, t, nc);
        tft.drawFastHLine(298, 187, t, nc); tft.drawFastVLine(309, 175, t, nc);
        tft.drawFastHLine(11, 49, 298, tft.color565(0, 40, 60));
        tft.setTextSize(2);
        tft.setTextColor(nc, nbg);
        tft.drawString("> NO_DATA", 50, 90);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 158, 178), nbg);
        tft.drawString("NO DEVICES FOUND NEARBY", 50, 118);
        tft.drawString("PRESS [PRESS] TO RESCAN", 50, 134);
    }
    else
    {
        // Column headers
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 148, 165), TFT_BLACK);
        tft.drawString("DEVICE", 20, 38);
        tft.drawString("dBm", 222, 38);
        tft.drawString("SIGNAL", 264, 38);

        int show = bleResultCount > 5 ? 5 : bleResultCount;
        for (int i = 0; i < show; i++)
        {
            BLEResult& r  = bleResults[i];
            uint16_t   rc = rssiColor(r.rssi);
            int rowY = 50 + i * 32;

            bool hasName = (r.name[0] != '\0' && r.name[0] != ' ');
            tft.setTextSize(1);
            tft.setTextColor(tft.color565(200, 235, 245), TFT_BLACK);
            tft.drawString(hasName ? r.name : r.addr, 20, rowY);
            if (hasName)
            {
                tft.setTextColor(tft.color565(155, 185, 200), TFT_BLACK);
                tft.drawString(r.addr, 20, rowY + 11);
            }

            // dBm value — same line as device name
            char rssiBuf[8]; sprintf(rssiBuf, "%d", r.rssi);
            tft.setTextColor(rc, TFT_BLACK);
            tft.drawString(rssiBuf, 222, rowY);

            // Signal bar — same line as dBm and name
            int barFill = (int)((float)(r.rssi - (-100)) / 70.0f * 56.0f);
            if (barFill < 0)  barFill = 0;
            if (barFill > 56) barFill = 56;
            if (barFill > 0)
                tft.fillRect(264, rowY, barFill, 8, rc);
            tft.drawRect(264, rowY, 56, 8, tft.color565(20, 40, 50));
        }

        char countBuf[32];
        sprintf(countBuf, "%d DEVICE%s FOUND", bleResultCount, bleResultCount == 1 ? "" : "S");
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 148, 170), TFT_BLACK);
        tft.drawString(countBuf, 20, 214);
    }

    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(0, 220, 245));
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
    tft.drawString("[PRESS] RESCAN   [C] BACK", 8, 225);

    drawBatteryStatus(TFT_BLACK);
}

// -------------------------------------------------------------------------
// Returns false if the user pressed KEY_C to cancel during the scan.
static bool doBleScan()
{
    bleResultCount = 0;
    if (!bleInitDone) return true;

    bleSetActive(false);   // pause advertising — scan + advertising can't run simultaneously
    delay(200);

    g_bleScanDone = false;
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    // The async start() overload skips the ble_start() initialisation that the
    // blocking overload performs lazily. Mirror that check here so the BLE radio
    // is actually activated before we ask the RTL8720DN to scan.
    if (!BLEDevice::ble_start_flags) {
        BLEDevice::ble_start_flags = true;
        ble_start();
    }
    scan->start(3, bleScanCallback, false);   // async — returns immediately

    // Poll until done or KEY_C pressed (5-second safety timeout).
    unsigned long scanStart = millis();
    while (!g_bleScanDone && millis() - scanStart < 5000)
    {
        if (digitalRead(WIO_KEY_C) == LOW)
        {
            scan->stop();   // signals RTL8720DN to stop + releases internal semaphore
            while (digitalRead(WIO_KEY_C) == LOW) delay(10);
            return false;   // cancelled
        }
        delay(20);
    }

    // If the scan never completed (timed out), stop it and bail — calling
    // getResults() on an incomplete scan blocks on an unreleased semaphore.
    if (!g_bleScanDone)
    {
        scan->stop();
        return true;   // no results but not cancelled
    }

    BLEScanResults raw = scan->getResults();
    int total = raw.getCount();
    for (int i = 0; i < total && bleResultCount < BLE_MAX_RESULTS; i++)
    {
        BLEAdvertisedDevice dev = raw.getDevice(i);
        BLEResult& r = bleResults[bleResultCount];

        r.rssi = dev.getRSSI();

        std::string addr = dev.getAddress().toString();
        strncpy(r.addr, addr.c_str(), 17);
        r.addr[17] = '\0';

        std::string nm = dev.getName();
        if (!nm.empty())
        {
            strncpy(r.name, nm.c_str(), 19);
            r.name[19] = '\0';
        }
        else
        {
            r.name[0] = '\0';
        }

        bleResultCount++;
    }

    // Sort by RSSI descending (strongest first)
    for (int i = 0; i < bleResultCount - 1; i++)
        for (int j = i + 1; j < bleResultCount; j++)
            if (bleResults[j].rssi > bleResults[i].rssi)
            {
                BLEResult tmp  = bleResults[i];
                bleResults[i]  = bleResults[j];
                bleResults[j]  = tmp;
            }
    return true;
}

// -------------------------------------------------------------------------
void bleScannerScreen()
{
    while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);

    // Full BLE deinit + reinit on every entry: guarantees a clean GAP state
    // regardless of prior WiFi activity, advertising, or previous scans.
    bleHardReset();

    drawBleScanning();
    if (!doBleScan()) { delay(50); return; }   // user cancelled during initial scan
    drawBleScanResults();

    while (true)
    {
        if (digitalRead(WIO_5S_PRESS) == LOW)
        {
            while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);
            drawBleScanning();
            if (!doBleScan()) { delay(50); return; }   // user cancelled during rescan
            drawBleScanResults();
        }

        if (digitalRead(WIO_KEY_B) == LOW)
        {
            while (digitalRead(WIO_KEY_B) == LOW) delay(10);
            takeScreenshot();
        }

        if (digitalRead(WIO_KEY_C) == LOW)
        {
            while (digitalRead(WIO_KEY_C) == LOW) delay(10);
            delay(50);
            return;
        }

        delay(50);
    }
}