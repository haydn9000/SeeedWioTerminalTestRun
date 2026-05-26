#include <Arduino.h>
#include <Wire.h>
#include "globals.h"
#include "LIS3DHTR.h"

static LIS3DHTR<TwoWire> lis;
static bool lisReady = false;


//========================================================================= NAVIGATION
// Renders the full menu to the display. Called only when something changes
// to avoid unnecessary screen clears (which cause visible flicker).
static const int PAGE_SIZE = 10;

static void drawMenuCell(int i)
{
  // Only draw cells that belong to the currently visible page.
  int page      = menuIndex / PAGE_SIZE;
  int pageStart = page * PAGE_SIZE;
  int pageEnd   = (pageStart + PAGE_SIZE < MENU_COUNT) ? pageStart + PAGE_SIZE : MENU_COUNT;
  if (i < pageStart || i >= pageEnd) return;

  int pageIndex = i - pageStart;
  int row = pageIndex / 2;
  int col = pageIndex % 2;
  int bx  = (col == 0) ? 10 : 162;
  int by  = 32 + row * 37;
  int bw  = 148;
  int bh  = 35;
  if (i == menuIndex)
  {
    uint16_t selBg  = tft.color565(0, 35, 50);
    uint16_t selCol = tft.color565(0, 220, 245);
    tft.fillRect(bx, by, bw, bh, selBg);
    tft.drawRect(bx, by, bw, bh, selCol);
    int t = 5;
    tft.drawFastHLine(bx,      by,      t, selCol); tft.drawFastVLine(bx,      by,      t, selCol);
    tft.drawFastHLine(bx+bw-t, by,      t, selCol); tft.drawFastVLine(bx+bw-1, by,      t, selCol);
    tft.drawFastHLine(bx,      by+bh-1, t, selCol); tft.drawFastVLine(bx,      by+bh-t, t, selCol);
    tft.drawFastHLine(bx+bw-t, by+bh-1, t, selCol); tft.drawFastVLine(bx+bw-1, by+bh-t, t, selCol);
    tft.setTextSize(2);
    tft.setTextColor(selCol, selBg);
    tft.drawString(">",          bx + 5,  by + 9);
    tft.drawString(menuItems[i], bx + 18, by + 9);
  }
  else
  {
    tft.fillRect(bx, by, bw, bh, tft.color565(0, 8, 15));
    tft.drawRect(bx, by, bw, bh, tft.color565(15, 30, 42));
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(80, 110, 120), tft.color565(0, 8, 15));
    tft.drawString(menuItems[i], bx + 10, by + 9);
  }
}

void drawMenu()
{
  tft.fillScreen(TFT_BLACK);

  // --- Header strip ---
  tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
  tft.fillRect(0, 0, 3, 30, tft.color565(0, 220, 245));        // cyan accent bar
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
  tft.drawString("// MENU", 10, 7);
  tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 148, 170), tft.color565(0, 8, 20));
  tft.drawString("NAVIGATE", 257, 12);
  tft.drawFastHLine(0, 29, 320, tft.color565(0, 80, 100));
  for (int xi = 8; xi < 320; xi += 14)
      tft.drawFastVLine(xi, 27, 4, tft.color565(0, 140, 165));

  // Page indicator in header (top-right corner, replaces static "NAVIGATE" when paged)
  int totalPages = (MENU_COUNT + PAGE_SIZE - 1) / PAGE_SIZE;
  int curPage    = menuIndex / PAGE_SIZE;
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 148, 170), tft.color565(0, 8, 20));
  if (totalPages > 1)
  {
    char pgBuf[16];
    snprintf(pgBuf, sizeof(pgBuf), "PG %d/%d", curPage + 1, totalPages);
    tft.drawString(pgBuf, 272, 12);
  }
  else
  {
    tft.drawString("NAVIGATE", 257, 12);
  }

  // --- 2-column grid, current page only ---
  int pageStart = curPage * PAGE_SIZE;
  int pageEnd   = (pageStart + PAGE_SIZE < MENU_COUNT) ? pageStart + PAGE_SIZE : MENU_COUNT;
  for (int i = pageStart; i < pageEnd; i++) drawMenuCell(i);

  // --- Footer ---
  tft.fillRect(0, 219, 3, 21, tft.color565(0, 220, 245));
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
  if (totalPages > 1)
    tft.drawString("[UP/DN/L/R] NAV   [PRESS] SEL   [DN] NEXT PG", 8, 225);
  else
    tft.drawString("[UP/DN] ROW   [L/R] COL   [PRESS] SELECT", 8, 225);

  drawBatteryStatus(TFT_BLACK);
}

// Called every loop iteration while optionTest == 'C'.
// Navigation moves only repaint the two affected cells — no fillScreen flash.
// Full drawMenu() is used on first draw and on return from a sub-screen.
void navigation()
{
  if (menuNeedsRedraw) { menuNeedsRedraw = false; drawMenu(); return; }

  int prev = menuIndex;

  if (digitalRead(WIO_5S_UP) == LOW)
  {
    if (menuIndex >= 2) menuIndex -= 2;
    delay(200);
  }
  else if (digitalRead(WIO_5S_DOWN) == LOW)
  {
    if (menuIndex + 2 < MENU_COUNT) menuIndex += 2;
    delay(200);
  }
  else if (digitalRead(WIO_5S_LEFT) == LOW)
  {
    if (menuIndex % 2 == 1) menuIndex--;
    delay(200);
  }
  else if (digitalRead(WIO_5S_RIGHT) == LOW)
  {
    if (menuIndex % 2 == 0 && menuIndex + 1 < MENU_COUNT) menuIndex++;
    delay(200);
  }
  else if (digitalRead(WIO_5S_PRESS) == LOW)
  {
    switch (menuIndex)
    {
      case 0:  homeScreen();              break;
      case 1:  pomodoroScreen();          break;
      case 2:  stopwatchScreen();         break;
      case 3:  countdownTimerScreen();    break;
      case 4:  claudeUsageScreen();       break;
      case 5:  sysStatsScreen();          break;
      case 6:  processWatchScreen();      break;
      case 7:  wifiScannerScreen();       break;
      case 8:  bleScannerScreen();        break;
      case 9:  sdCardViewerScreen();      break;
      case 10: matrixRainScreen();        break;
      case 11: setBrightness();           break;
    }
    delay(200);
    menuNeedsRedraw = true;
    return;
  }

  if (menuIndex != prev)
  {
    if (menuIndex / PAGE_SIZE != prev / PAGE_SIZE)
      drawMenu();                          // page changed — full redraw
    else
    { drawMenuCell(prev); drawMenuCell(menuIndex); }
  }
}


//========================================================================= HOMESCREEN — SENSOR DASHBOARD

// --- geometry
static const int BAR_X  = 20;
static const int BAR_W  = 194;
static const int BAR_H  = 18;
static const int RVAL_X = 222;   // x start of right-side value text

static const int Y_ACCEL_LBL = 38;
static const int Y_XBAR      = 48;
static const int Y_YBAR      = 70;
static const int Y_ZBAR      = 92;
static const int Y_LIGHT_LBL = 120;
static const int Y_LBAR      = 130;
static const int Y_MIC_LBL   = 162;
static const int Y_MBAR      = 172;

// --- Sample the mic for ~30ms, return peak-to-peak amplitude.
static int sampleMic()
{
    int lo = 4095, hi = 0;
    for (int i = 0; i < 30; i++) {
        int v = analogRead(WIO_MIC);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        delay(1);
    }
    return hi - lo;
}

// --- Bipolar bar: val in [-range, +range], filled from centre.
//     Green rightward for positive, blue leftward for negative.
static void drawBiBar(int x, int y, int w, int h,
                      float val, float range, uint16_t posCol, uint16_t negCol)
{
    int cx   = x + w / 2;
    int half = w / 2 - 1;
    int fill = (int)(val / range * half);
    if (fill >  half) fill =  half;
    if (fill < -half) fill = -half;

    tft.fillRect(x + 1, y + 1, w - 2, h - 2, TFT_BLACK);
    if (fill > 0)
        tft.fillRect(cx,        y + 1, fill,  h - 2, posCol);
    else if (fill < 0)
        tft.fillRect(cx + fill, y + 1, -fill, h - 2, negCol);

    // Restore centre tick (cleared by fillRect above)
    tft.drawFastVLine(cx, y + 1, h - 2, tft.color565(0, 60, 80));
}

// --- Unipolar bar: val in [0, maxVal], filled left to right.
static void drawUniBar(int x, int y, int w, int h, int val, int maxVal, uint16_t col)
{
    int fill = (val >= maxVal) ? w - 2 : (int)((long)val * (w - 2) / maxVal);
    tft.fillRect(x + 1,        y + 1, fill,             h - 2, col);
    tft.fillRect(x + 1 + fill, y + 1, w - 2 - fill, h - 2, TFT_BLACK);
}

// --- Draw the static frame (borders, labels) — called once on entry.
static void drawHomeFrame()
{
    tft.fillScreen(TFT_BLACK);

    // --- Header strip ---
    tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
    tft.fillRect(0, 0, 3, 30, tft.color565(0, 200, 230));          // left accent bar
    tft.setTextSize(2);
    tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
    tft.drawString("// HOME", 10, 7);
    tft.setTextSize(1);
    tft.setTextColor(tft.color565(0, 158, 188), tft.color565(0, 8, 20));
    tft.drawString("SENSORS", 272, 12);
    tft.drawFastHLine(0, 29, 320, tft.color565(0, 100, 130));
    for (int xi = 8; xi < 320; xi += 14)
        tft.drawFastVLine(xi, 27, 4, tft.color565(0, 155, 185));

    const uint16_t colBorder = tft.color565(20, 32, 52);   // dark-blue bar border
    const uint16_t colDiv    = tft.color565(0, 60, 80);    // dim-neon centre divider
    tft.setTextSize(1);

    // Accel section — label in dim cyan
    tft.setTextColor(tft.color565(0, 150, 175), TFT_BLACK);
    tft.drawString("ACCELEROMETER", BAR_X, Y_ACCEL_LBL);
    const int accelYs[] = { Y_XBAR, Y_YBAR, Y_ZBAR };
    for (int i = 0; i < 3; i++) {
        tft.drawRect(BAR_X, accelYs[i], BAR_W, BAR_H, colBorder);
        tft.drawFastVLine(BAR_X + BAR_W / 2, accelYs[i] + 1, BAR_H - 2, colDiv);
    }

    // Light section — label in dim amber
    tft.setTextColor(tft.color565(180, 140, 0), TFT_BLACK);
    tft.drawString("LIGHT SENSOR", BAR_X, Y_LIGHT_LBL);
    tft.drawRect(BAR_X, Y_LBAR, BAR_W, BAR_H, colBorder);

    // Mic section — label in dim green
    tft.setTextColor(tft.color565(50, 180, 50), TFT_BLACK);
    tft.drawString("MICROPHONE", BAR_X, Y_MIC_LBL);
    tft.drawRect(BAR_X, Y_MBAR, BAR_W, BAR_H, colBorder);

    // Footer
    tft.fillRect(0, 219, 3, 21, tft.color565(0, 200, 230));
    tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
    tft.drawString("[C] EXIT", 8, 225);

    drawBatteryStatus(TFT_BLACK);
}

// --- Read sensors and repaint only the bar interiors and value text.
static void updateHomeSensors()
{
    const uint16_t colAccelP = tft.color565(0,   220, 245);   // neon cyan — +
    const uint16_t colAccelN = tft.color565(220,  40, 190);   // neon magenta — -
    const uint16_t colLight  = tft.color565(255, 200,   0);   // neon yellow
    const uint16_t colMic    = tft.color565(80,  255,  80);   // neon green
    const uint16_t colVal    = tft.color565(170, 225, 235);   // bright neon readout
    const uint16_t axCols[]  = {
        tft.color565(0,   200, 220),   // X — cyan
        tft.color565(200,  40, 180),   // Y — magenta
        tft.color565(210, 175,   0),   // Z — yellow
    };

    // Accelerometer
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    if (lisReady) {
        ax = lis.getAccelerationX();
        ay = lis.getAccelerationY();
        az = lis.getAccelerationZ();
    }
    const float accelVals[] = { ax, ay, az };
    const char* axNames[]   = { "X", "Y", "Z" };
    const int   accelYs[]   = { Y_XBAR, Y_YBAR, Y_ZBAR };

    for (int i = 0; i < 3; i++) {
        drawBiBar(BAR_X, accelYs[i], BAR_W, BAR_H,
                  accelVals[i], 2.0f, colAccelP, colAccelN);
        tft.fillRect(RVAL_X, accelYs[i], 320 - RVAL_X, BAR_H, TFT_BLACK);
        tft.setTextSize(1);
        tft.setTextColor(axCols[i], TFT_BLACK);
        tft.drawString(axNames[i], RVAL_X, accelYs[i] + 5);
        char buf[10];
        snprintf(buf, sizeof(buf), "%+.2fg", accelVals[i]);
        tft.setTextColor(colVal, TFT_BLACK);
        tft.drawString(buf, RVAL_X + 12, accelYs[i] + 5);
    }

    // Light sensor
    int lightRaw = analogRead(WIO_LIGHT);
    drawUniBar(BAR_X, Y_LBAR, BAR_W, BAR_H, lightRaw, 1023, colLight);
    tft.fillRect(RVAL_X, Y_LBAR, 320 - RVAL_X, BAR_H, TFT_BLACK);
    {
        char buf[12]; snprintf(buf, sizeof(buf), "%4d", lightRaw);
        tft.setTextSize(1);
        tft.setTextColor(colVal, TFT_BLACK);
        tft.drawString(buf, RVAL_X, Y_LBAR + 5);
    }

    // Microphone (blocks ~30ms to sample)
    int micPP = sampleMic();
    drawUniBar(BAR_X, Y_MBAR, BAR_W, BAR_H, micPP, 512, colMic);
    tft.fillRect(RVAL_X, Y_MBAR, 320 - RVAL_X, BAR_H, TFT_BLACK);
    {
        char buf[12]; snprintf(buf, sizeof(buf), "%4d", micPP);
        tft.setTextSize(1);
        tft.setTextColor(colVal, TFT_BLACK);
        tft.drawString(buf, RVAL_X, Y_MBAR + 5);
    }
}

void homeScreen()
{
    while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }

    // Lazy-init accelerometer on first entry (Wire1 = internal I2C bus).
    if (!lisReady) {
        Wire1.begin();
        lis.begin(Wire1);
        lis.setOutputDataRate(LIS3DHTR_DATARATE_25HZ);
        lis.setFullScaleRange(LIS3DHTR_RANGE_2G);
        lisReady = lis.isConnection();
    }

    drawHomeFrame();
    updateHomeSensors();

    uint32_t lastUpdate = millis();

    while (true)
    {
        if (millis() - lastUpdate >= 200)
        {
            lastUpdate = millis();
            updateHomeSensors();
        }

        if (digitalRead(WIO_KEY_B) == LOW)
        {
            while (digitalRead(WIO_KEY_B) == LOW) { delay(10); }
            takeScreenshot();
        }

        if (digitalRead(WIO_KEY_C) == LOW)
        {
            while (digitalRead(WIO_KEY_C) == LOW) { delay(10); }
            delay(50);
            return;
        }
        delay(10);
    }
}