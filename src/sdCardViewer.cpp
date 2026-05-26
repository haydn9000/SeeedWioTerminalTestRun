// sdCardViewer.cpp — Browse BMP images stored on the SD card.
//
// Controls:
//   [LEFT]    Previous image
//   [RIGHT]   Next image
//   [B]       Screenshot
//   [C]       Back to menu

#include <Arduino.h>
#include "globals.h"
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>

static const int MAX_BMPS = 50;

static char    bmpFiles[MAX_BMPS][64];  // up to 63 chars (LFN support) + null
static int     bmpCount = 0;
static int     bmpIndex = 0;

static uint8_t  rowBuf[1280];  // one BMP row: 320 px × 4 bytes max (32bpp)
static uint16_t rgbRow[320];   // one display row: RGB565

// -------------------------------------------------------------------------
static void scanBmps()
{
    bmpCount = 0;
    File dir = SD.open("/");
    if (!dir) return;

    while (bmpCount < MAX_BMPS)
    {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory())
        {
            // entry.name() may return a full path ("0:/FOO.BMP" or "/FOO.BMP").
            // Strip everything up to and including the last '/' to get bare filename.
            const char* full = entry.name();
            const char* slash = strrchr(full, '/');
            const char* n = slash ? slash + 1 : full;
            int len = (int)strlen(n);
            if (len >= 5 && len <= 63)
            {
                const char* ext = n + len - 4;
                if (ext[0] == '.' &&
                    (ext[1] == 'B' || ext[1] == 'b') &&
                    (ext[2] == 'M' || ext[2] == 'm') &&
                    (ext[3] == 'P' || ext[3] == 'p'))
                {
                    strncpy(bmpFiles[bmpCount], n, 63);
                    bmpFiles[bmpCount][63] = '\0';
                    bmpCount++;
                }
            }
        }
        entry.close();
    }
    dir.close();
}

// -------------------------------------------------------------------------
static void drawViewerHeader()
{
    tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
    tft.fillRect(0, 0, 3, 30, tft.color565(0, 220, 245));
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
    tft.drawString("// SD VIEWER", 10, 7);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 148, 170), tft.color565(0, 8, 20));
    tft.drawString("PHOTOS", 270, 12);
    tft.drawFastHLine(0, 29, 320, tft.color565(0, 80, 100));
}

// -------------------------------------------------------------------------
// 20 px status bar overlaid at the bottom of the screen.
static void drawViewerStatus()
{
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.fillRect(0, 220, 3, 20, tft.color565(0, 210, 235));

    char idx[10];
    snprintf(idx, sizeof(idx), "%d/%d", bmpIndex + 1, bmpCount);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 210, 235), TFT_BLACK);
    tft.drawString(idx, 8, 227);

    tft.setTextColor(tft.color565(180, 220, 230), TFT_BLACK);
    tft.drawString(bmpFiles[bmpIndex], 50, 227);

    tft.setTextColor(tft.color565(0, 80, 95), TFT_BLACK);
    tft.drawString("[</> NAV  [C] BACK", 176, 227);
}

// -------------------------------------------------------------------------
static void drawNoBmps()
{
    tft.fillScreen(TFT_BLACK);
    drawViewerHeader();

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
    tft.drawString("> NO_FILES", 50, 90);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 158, 178), nbg);
    tft.drawString("NO .BMP FILES ON SD CARD", 50, 118);
    tft.drawString("PRESS [C] TO GO BACK", 50, 134);
    drawBatteryStatus(TFT_BLACK);
}

// Last-detected BMP geometry — shown in the error screen.
static int32_t  g_lastW = 0, g_lastH = 0;
static uint16_t g_lastBpp = 0;

// -------------------------------------------------------------------------
// Load a 320×240 24-bit or 32-bit BMP and draw it full-screen.
// Reads rows sequentially (fast) — image fills bottom-to-top for standard BMPs.
// Returns true on success.
static bool loadAndDrawBmp(const char* name)
{
    // Do NOT prepend '/' — Seeed_Arduino_FS prepends "0:/" internally.
    // "/SCRN0001.BMP" would become "0://SCRN0001.BMP" and FatFS would reject it.
    File f = SD.open(name, FILE_READ);
    if (!f) return false;

    uint8_t hdr[54];
    if (f.read(hdr, 54) != 54) { f.close(); return false; }
    if (hdr[0] != 'B' || hdr[1] != 'M') { f.close(); return false; }

    uint32_t dataOff = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8)
                     | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
    int32_t  w       = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8)
                     | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
    int32_t  h       = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8)
                     | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
    uint16_t bpp     = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
    // compression: 0=BI_RGB, 3=BI_BITFIELDS (Windows 32bpp screenshots use this)
    uint32_t comp    = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8)
                     | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);

    // Store for error display.
    g_lastW = w; g_lastH = h; g_lastBpp = bpp;

    // Accept 24bpp BI_RGB and 32bpp BI_RGB / BI_BITFIELDS (e.g. Windows screenshots).
    bool is24 = (bpp == 24 && comp == 0);
    bool is32 = (bpp == 32 && (comp == 0 || comp == 3));
    int32_t absH = h < 0 ? -h : h;

    if ((!is24 && !is32) || w != 320 || absH != 240)
    {
        f.close();
        return false;
    }

    // Positive height = standard bottom-to-top BMP storage.
    bool bottomToTop = (h > 0);
    int  stride      = is32 ? 1280 : 960;   // bytes per row

    f.seek(dataOff);

    // Read rows sequentially and push each to its correct display y position.
    for (int fileRow = 0; fileRow < 240; fileRow++)
    {
        f.read(rowBuf, stride);
        int dispY = bottomToTop ? (239 - fileRow) : fileRow;
        if (is32)
        {
            for (int x = 0; x < 320; x++)  // BGRA → RGB565 big-endian (ignore alpha)
            {
                uint16_t c = tft.color565(rowBuf[x * 4 + 2], rowBuf[x * 4 + 1], rowBuf[x * 4 + 0]);
                rgbRow[x] = (c >> 8) | (c << 8);   // swap to big-endian for SAMD51 DMA
            }
        }
        else
        {
            for (int x = 0; x < 320; x++)  // BGR → RGB565 big-endian
            {
                uint16_t c = tft.color565(rowBuf[x * 3 + 2], rowBuf[x * 3 + 1], rowBuf[x * 3 + 0]);
                rgbRow[x] = (c >> 8) | (c << 8);   // swap to big-endian for SAMD51 DMA
            }
        }
        tft.pushImage(0, dispY, 320, 1, rgbRow);
    }

    f.close();
    return true;
}

// -------------------------------------------------------------------------
static void showCurrentBmp()
{
    // Loading hint — drawn before the image pixels arrive.
    tft.fillRect(0, 0, 320, 20, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 140, 165), TFT_BLACK);
    char msg[32];
    snprintf(msg, sizeof(msg), "Loading %s...", bmpFiles[bmpIndex]);
    tft.drawString(msg, 5, 5);

    bool ok = loadAndDrawBmp(bmpFiles[bmpIndex]);

    if (!ok)
    {
        tft.fillScreen(TFT_BLACK);
        drawViewerHeader();
        tft.setTextSize(2);
        tft.setTextColor(tft.color565(210, 80, 60), TFT_BLACK);
        tft.drawString("UNSUPPORTED", 30, 70);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(160, 100, 80), TFT_BLACK);
        tft.drawString("Requires 320x240, 24 or 32-bit BMP", 30, 100);
        char info[48];
        snprintf(info, sizeof(info), "Detected: %ldx%ld  %u-bit",
                 g_lastW, (g_lastH < 0 ? -g_lastH : g_lastH), g_lastBpp);
        tft.drawString(info, 30, 116);
        tft.drawString(bmpFiles[bmpIndex], 30, 132);
    }

    drawViewerStatus();
    drawBatteryStatus(TFT_BLACK);
}

// -------------------------------------------------------------------------
void sdCardViewerScreen()
{
    while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);

    // Show scanning state while we enumerate the SD card.
    tft.fillScreen(TFT_BLACK);
    drawViewerHeader();
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 160, 185), TFT_BLACK);
    tft.drawString("Scanning SD card for .BMP files...", 10, 50);

    // Re-initialise SD in case BLE/WiFi operations disturbed the SPI state.
    SD.end();
    SD.begin(SDCARD_SS_PIN, SDCARD_SPI);

    scanBmps();

    if (bmpCount == 0)
    {
        drawNoBmps();
        while (true)
        {
            if (digitalRead(WIO_KEY_C) == LOW)
            {
                while (digitalRead(WIO_KEY_C) == LOW) delay(10);
                delay(50);
                return;
            }
            delay(20);
        }
    }

    bmpIndex = 0;
    showCurrentBmp();

    while (true)
    {
        if (digitalRead(WIO_5S_LEFT) == LOW)
        {
            while (digitalRead(WIO_5S_LEFT) == LOW) delay(10);
            if (bmpIndex > 0) { bmpIndex--; showCurrentBmp(); }
        }

        if (digitalRead(WIO_5S_RIGHT) == LOW)
        {
            while (digitalRead(WIO_5S_RIGHT) == LOW) delay(10);
            if (bmpIndex < bmpCount - 1) { bmpIndex++; showCurrentBmp(); }
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

        delay(20);
    }
}
