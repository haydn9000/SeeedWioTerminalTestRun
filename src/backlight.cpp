#include <Arduino.h>
#include "globals.h"


//========================================================================= BACKLIGHT
static int brightness = 25;
static const int MIN_BRIGHTNESS = 5;

// Renders the full brightness adjustment screen.
void drawBrightness()
{
  tft.fillScreen(TFT_BLACK);

  // --- Header strip (matches sysStats cyberpunk style) ---
  tft.fillRect(0, 0, 320, 30, tft.color565(0, 8, 20));
  tft.fillRect(0, 0, 3, 30, tft.color565(0, 200, 230));          // left accent bar
  tft.setTextSize(2);
  tft.setTextColor(tft.color565(0, 220, 245), tft.color565(0, 8, 20));
  tft.drawString("// SETTINGS", 10, 7);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 158, 188), tft.color565(0, 8, 20));
  tft.drawString("BACKLIGHT", 256, 12);
  tft.drawFastHLine(0, 29, 320, tft.color565(0, 100, 130));
  for (int xi = 8; xi < 320; xi += 14)
    tft.drawFastVLine(xi, 27, 4, tft.color565(0, 155, 185));

  // Sub-label
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 130, 155), TFT_BLACK);
  tft.drawString("DISPLAY BRIGHTNESS", 106, 36);

  // Large centred percentage — colour shifts with level
  uint16_t valCol = (brightness < 40) ? tft.color565(0, 170, 200) :
                    (brightness < 75) ? tft.color565(0, 220, 245) :
                                        tft.color565(200, 240, 255);
  tft.setTextSize(6);
  tft.setTextColor(valCol, TFT_BLACK);
  char buf[8];
  sprintf(buf, "%d%%", brightness);
  int textW = tft.textWidth(buf);
  tft.drawString(buf, (320 - textW) / 2, 52);

  // Separator line above bar
  tft.drawFastHLine(60, 116, 200, tft.color565(0, 45, 65));

  // Segmented bar — 20 segments (one per 5% step)
  const int BAR_X = 20, BAR_Y = 140, BAR_W = 280, BAR_H = 26;
  const int N_SEGS = 20, SEG_GAP = 2;
  const int SEG_W  = BAR_W / N_SEGS;   // 14 px per segment
  int litSegs = brightness / 5;
  for (int s = 0; s < N_SEGS; s++) {
    bool lit = (s < litSegs);
    uint16_t col = lit ? tft.color565(0, 210, 240) : tft.color565(12, 20, 38);
    tft.fillRect(BAR_X + s * SEG_W, BAR_Y, SEG_W - SEG_GAP, BAR_H, col);
  }

  // Corner brackets around the bar
  const int bx = 16, by = 136, bw = 288, bh = 34, bt = 12;
  const uint16_t bc = tft.color565(0, 160, 190);
  tft.drawFastHLine(bx,          by,       bt, bc);
  tft.drawFastVLine(bx,          by,       bt, bc);
  tft.drawFastHLine(bx + bw - bt, by,      bt, bc);
  tft.drawFastVLine(bx + bw - 1,  by,      bt, bc);
  tft.drawFastHLine(bx,          by+bh-1,  bt, bc);
  tft.drawFastVLine(bx,          by+bh-bt, bt, bc);
  tft.drawFastHLine(bx + bw - bt, by+bh-1, bt, bc);
  tft.drawFastVLine(bx + bw - 1,  by+bh-bt,bt, bc);

  // MIN / MAX labels
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 148, 170), TFT_BLACK);
  tft.drawString("MIN", BAR_X,               BAR_Y + BAR_H + 8);
  tft.drawString("MAX", BAR_X + BAR_W - 18,  BAR_Y + BAR_H + 8);

  // Footer
  tft.fillRect(0, 219, 3, 21, tft.color565(0, 200, 230));
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(0, 100, 118), TFT_BLACK);
  tft.drawString("[<][>] ADJUST     [PRESS] / [C] BACK", 8, 225);

  drawBatteryStatus(TFT_BLACK);
}

// Redraws only the value text and bar — no fillScreen, so adjustments are flicker-free.
static void refreshBrightnessBar()
{
  // Erase the old value text (size-6 text is ~48px tall starting at y=52).
  tft.fillRect(0, 52, 320, 64, TFT_BLACK);

  uint16_t valCol = (brightness < 40) ? tft.color565(0, 170, 200) :
                    (brightness < 75) ? tft.color565(0, 220, 245) :
                                        tft.color565(200, 240, 255);
  tft.setTextSize(6);
  tft.setTextColor(valCol, TFT_BLACK);
  char buf[8];
  sprintf(buf, "%d%%", brightness);
  int textW = tft.textWidth(buf);
  tft.drawString(buf, (320 - textW) / 2, 52);

  // Redraw segmented bar only.
  const int BAR_X = 20, BAR_Y = 140, BAR_W = 280, BAR_H = 26;
  const int N_SEGS = 20, SEG_GAP = 2, SEG_W = BAR_W / N_SEGS;
  int litSegs = brightness / 5;
  for (int s = 0; s < N_SEGS; s++) {
    bool lit = (s < litSegs);
    uint16_t col = lit ? tft.color565(0, 210, 240) : tft.color565(12, 20, 38);
    tft.fillRect(BAR_X + s * SEG_W, BAR_Y, SEG_W - SEG_GAP, BAR_H, col);
  }
}

// Brightness adjustment sub-screen. Blocks until the user exits.
void setBrightness()
{
  drawBrightness();

  // Spin until the joystick press that launched this screen is fully released.
  while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }
  delay(50);

  while (true)
  {
    if (digitalRead(WIO_5S_RIGHT) == LOW && brightness < 100)
    {
      brightness += 5;
      backLight.setBrightness(brightness);
      refreshBrightnessBar();
      delay(150);
    }
    else if (digitalRead(WIO_5S_LEFT) == LOW && brightness > MIN_BRIGHTNESS)
    {
      brightness -= 5;
      if (brightness < MIN_BRIGHTNESS) brightness = MIN_BRIGHTNESS;
      backLight.setBrightness(brightness);
      refreshBrightnessBar();
      delay(150);
    }
    else if (digitalRead(WIO_KEY_B) == LOW)
    {
      while (digitalRead(WIO_KEY_B) == LOW) { delay(10); }
      takeScreenshot();
    }
    else if (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW)
    {
      while (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW) { delay(10); }
      delay(50);
      return;
    }
  }
}
