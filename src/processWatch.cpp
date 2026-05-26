// processWatch.cpp — Top-5 CPU process monitor.
// Data arrives via USB serial or BLE as compact JSON:
//   {"p":[{"n":"chrome.exe","c":25.5,"m":1.2},...], "ok":1}
// Run tools/process_sender.py on the host.

#include <Arduino.h>
#include "globals.h"

struct ProcessEntry { char name[14]; float cpu; float mem; };   // mem = RSS in MB
struct ProcessData  { ProcessEntry procs[5]; int count; bool valid;
                      float total_cpu; float total_mem; };

static ProcessData procData        = {};
static uint16_t    procDataVersion = 0;

// -------------------------------------------------------------------------
bool parseProcessJson(const char* json)
{
    const char* p = strstr(json, "\"p\":[");
    if (!p) return false;
    p += 5;

    ProcessData tmp = {};

    while (*p && *p != ']' && tmp.count < 5)
    {
        while (*p && (*p == ',' || *p == ' ')) p++;
        if (*p != '{') { if (*p) p++; continue; }
        p++;

        ProcessEntry e = {};

        while (*p && *p != '}')
        {
            while (*p == ' ' || *p == ',') p++;
            if (*p != '"') { if (*p) p++; continue; }
            p++;
            char key = *p;
            while (*p && *p != ':') p++;
            if (!*p) break;
            p++;
            while (*p == ' ') p++;

            if (key == 'n' && *p == '"')
            {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < 13) e.name[i++] = *p++;
                e.name[i] = '\0';
                if (*p == '"') p++;
            }
            else if (key == 'c') { e.cpu = atof(p); while (*p && *p != ',' && *p != '}') p++; }
            else if (key == 'm') { e.mem = atof(p); while (*p && *p != ',' && *p != '}') p++; }
            else                 {                   while (*p && *p != ',' && *p != '}') p++; }
        }

        if (e.name[0] != '\0') tmp.procs[tmp.count++] = e;
        p = strchr(p, '}');
        if (p) p++;
    }

    tmp.valid = (tmp.count > 0);

    // Parse system totals from optional "tc" and "tm" fields
    const char* tcPtr = strstr(json, "\"tc\":");
    if (tcPtr) tmp.total_cpu = atof(tcPtr + 5);
    const char* tmPtr = strstr(json, "\"tm\":");
    if (tmPtr) tmp.total_mem = atof(tmPtr + 5);

    procData  = tmp;
    procDataVersion++;
    return tmp.valid;
}

// -------------------------------------------------------------------------
static uint16_t cpuBarColor(float pct)
{
    if (pct < 25.0f) return tft.color565(0, 210, 230);
    if (pct < 60.0f) return tft.color565(210, 175, 0);
    return tft.color565(210, 65, 55);
}

// -------------------------------------------------------------------------
static void drawProcessWatch()
{
    tft.fillScreen(TFT_BLACK);

    // Header
    tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
    tft.fillRect(0, 0, 3, 30, tft.color565(0, 220, 245));
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
    tft.drawString("// PROCESSES", 10, 7);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 148, 170), tft.color565(0, 8, 20));
    tft.drawString("WATCH", 281, 12);
    tft.drawFastHLine(0, 29, 320, tft.color565(0, 80, 100));
    for (int xi = 8; xi < 320; xi += 14)
        tft.drawFastVLine(xi, 27, 4, tft.color565(0, 140, 165));

    if (!procData.valid)
    {
        // No-data panel — full height below header, matching sysStats layout
        uint16_t nc   = tft.color565(0, 220, 245);
        uint16_t nbg  = tft.color565(0, 8, 18);
        uint16_t ndim = tft.color565(0, 130, 155);
        tft.fillRect(10, 48, 300, 160, nbg);
        int t = 12;
        tft.drawFastHLine(10,  48,  t, nc); tft.drawFastVLine(10,  48,  t, nc);
        tft.drawFastHLine(298, 48,  t, nc); tft.drawFastVLine(309, 48,  t, nc);
        tft.drawFastHLine(10,  207, t, nc); tft.drawFastVLine(10,  195, t, nc);
        tft.drawFastHLine(298, 207, t, nc); tft.drawFastVLine(309, 195, t, nc);
        tft.drawFastHLine(11, 49, 298, tft.color565(0, 40, 60));
        tft.setTextSize(2);
        tft.setTextColor(nc, nbg);
        tft.drawString("> NO_DATA", 50, 90);
        tft.setTextSize(1);
        tft.setTextColor(ndim, nbg);
        tft.drawString("AWAITING DATA STREAM...", 50, 118);
        tft.drawString("USB:  process_sender.py <port>", 50, 134);
        tft.drawString("BLE:  process_sender.py --ble", 50, 150);
        tft.setTextColor(tft.color565(0, 80, 100), nbg);
        tft.drawString(Serial ? "STATUS: USB ACTIVE" : "STATUS: WAITING...", 50, 166);
        char addrLine[40];
        snprintf(addrLine, sizeof(addrLine), "ADDR:  %s",
                 bleInitDone ? getBLEAddress() : "INIT...");
        tft.drawString(addrLine, 50, 182);
    }
    else
    {
        // --- System totals strip (y=30-43) ---
        uint16_t stBg = tft.color565(0, 5, 12);
        tft.fillRect(0, 30, 320, 14, stBg);
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 175, 200), stBg);
        tft.drawString("CPU", 10, 33);
        tft.drawString("RAM", 162, 33);
        uint16_t cc = cpuBarColor(procData.total_cpu);
        uint16_t rc = procData.total_mem < 50.0f ? tft.color565(60, 210, 100) :
                      procData.total_mem < 80.0f ? tft.color565(210, 175, 0)  :
                                                    tft.color565(210, 65, 55);
        int bfC = (int)(55 * procData.total_cpu / 100.0f); if (bfC > 55) bfC = 55;
        int bfR = (int)(55 * procData.total_mem / 100.0f); if (bfR > 55) bfR = 55;
        if (bfC > 0) tft.fillRect(28, 34, bfC, 6, cc);
        tft.drawRect(28, 34, 55, 6, tft.color565(0, 30, 45));
        if (bfR > 0) tft.fillRect(180, 34, bfR, 6, rc);
        tft.drawRect(180, 34, 55, 6, tft.color565(0, 30, 45));
        char cbuf[10]; sprintf(cbuf, "%.1f%%", procData.total_cpu);
        tft.setTextColor(cc, stBg); tft.drawString(cbuf, 87, 33);
        char rbuf[10]; sprintf(rbuf, "%.1f%%", procData.total_mem);
        tft.setTextColor(rc, stBg); tft.drawString(rbuf, 239, 33);

        // Column headers
        tft.setTextSize(1);
        tft.setTextColor(tft.color565(0, 175, 190), TFT_BLACK);
        tft.drawString("PROCESS", 20, 46);
        tft.drawString("CPU%", 202, 46);
        tft.drawString("MB", 272, 46);
        tft.drawFastHLine(0, 54, 320, tft.color565(0, 30, 42));

        for (int i = 0; i < procData.count && i < 5; i++)
        {
            int rowY = 57 + i * 32;
            ProcessEntry& e = procData.procs[i];
            uint16_t col = cpuBarColor(e.cpu);

            tft.setTextSize(1);
            tft.setTextColor(tft.color565(210, 240, 255), TFT_BLACK);
            tft.drawString(e.name, 20, rowY);

            int barX = 20, barY = rowY + 11, barW = 175, barH = 11;
            float fill = e.cpu > 100.0f ? 100.0f : e.cpu;
            if (fill > 0.0f)
                tft.fillRect(barX, barY, (int)(barW * fill / 100.0f), barH, col);
            tft.drawRect(barX, barY, barW, barH, tft.color565(20, 40, 50));

            char cpuBuf[8]; sprintf(cpuBuf, "%.1f", e.cpu);
            tft.setTextColor(col, TFT_BLACK);
            tft.drawString(cpuBuf, 202, barY + 1);

            char memBuf[8];
            if (e.mem < 1000.0f) sprintf(memBuf, "%.0fM", e.mem);
            else                 sprintf(memBuf, "%.1fG", e.mem / 1024.0f);
            tft.setTextColor(tft.color565(200, 225, 245), TFT_BLACK);
            tft.drawString(memBuf, 272, barY + 1);
        }
    }

    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(0, 220, 245));
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
    tft.drawString("[C] BACK", 8, 225);

    drawBatteryStatus(TFT_BLACK);
}

// -------------------------------------------------------------------------
void processWatchScreen()
{
    while (digitalRead(WIO_5S_PRESS) == LOW) delay(10);

    procData.valid = false;
    procDataVersion++;
    uint16_t drawnVersion = 0;
    drawProcessWatch();
    drawnVersion = procDataVersion;

    while (true)
    {
        checkSerial();
        checkBLE();

        if (procDataVersion != drawnVersion)
        {
            drawnVersion = procDataVersion;
            drawProcessWatch();
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