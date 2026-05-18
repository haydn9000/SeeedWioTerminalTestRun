#include <Arduino.h>
#include "globals.h"


//========================================================================= BACKLIGHT
static int brightness = 25;
static const int MIN_BRIGHTNESS = 5;

// Renders the full brightness adjustment screen.
void drawBrightness()
{
  tft.fillScreen(TFT_BLACK);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("BRIGHTNESS", 95, 12);

  // Large centred percentage value.
  tft.setTextSize(6);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf[8];
  sprintf(buf, "%d%%", brightness);
  int textW = tft.textWidth(buf);
  tft.drawString(buf, (320 - textW) / 2, 55);

  // Progress bar — hollow track with a filled portion proportional to brightness.
  tft.drawRoundRect(20, 130, 280, 28, 4, TFT_DARKGREY);
  if (brightness > 0)
    tft.fillRoundRect(20, 130, 280 * brightness / 100, 28, 4, TFT_WHITE);

  // Min/max labels beneath the bar.
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("5%", 20, 166);
  tft.drawString("100%", 278, 166);

  // Hint
  tft.drawString("LEFT / RIGHT: adjust      PRESS or C: back", 20, 224);

  drawBatteryStatus(TFT_BLACK);
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
      drawBrightness();
      delay(150);
    }
    else if (digitalRead(WIO_5S_LEFT) == LOW && brightness > MIN_BRIGHTNESS)
    {
      brightness -= 5;
      if (brightness < MIN_BRIGHTNESS) brightness = MIN_BRIGHTNESS;
      backLight.setBrightness(brightness);
      drawBrightness();
      delay(150);
    }
    else if (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW)
    {
      while (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW) { delay(10); }
      delay(50);
      return;
    }
  }
}
