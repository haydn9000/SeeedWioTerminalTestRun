
//========================================================================= BACKLIGHT
static int brightness = 25;  // Current brightness level (0–100), persists across calls.

// Renders the full brightness adjustment screen.
// Called once on entry and again after every brightness change.
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
  tft.drawRoundRect(20, 130, 280, 28, 4, TFT_DARKGREY);  // Track outline.
  if (brightness > 0)
    tft.fillRoundRect(20, 130, 280 * brightness / 100, 28, 4, TFT_WHITE);  // Fill.

  // Min/max labels beneath the bar.
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("0%", 20, 166);
  tft.drawString("100%", 278, 166);

  // Hint
  tft.drawString("LEFT / RIGHT: adjust      PRESS or C: back", 20, 224);
}

// Brightness adjustment sub-screen. Blocks until the user exits.
// Entered from the menu (PRESS) or directly via top button A.
void setBrightness()
{
  drawBrightness();

  // Spin until the joystick press that launched this screen is fully released,
  // otherwise the exit condition below would fire immediately.
  while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }
  delay(50);  // Extra debounce after release.

  while (true)
  {
    if (digitalRead(WIO_5S_RIGHT) == LOW && brightness < 100)
    {
      brightness += 5;
      backLight.setBrightness(brightness);  // Apply immediately so the screen dims/brightens in real time.
      drawBrightness();
      delay(150);  // Repeat rate — how fast brightness changes while held.
    }
    else if (digitalRead(WIO_5S_LEFT) == LOW && brightness > 0)
    {
      brightness -= 5;
      backLight.setBrightness(brightness);
      drawBrightness();
      delay(150);
    }
    else if (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW)
    {
      // Wait for the exit button to be fully released before returning.
      // This prevents loop() from catching the same press and triggering a double redraw.
      while (digitalRead(WIO_5S_PRESS) == LOW || digitalRead(WIO_KEY_C) == LOW) { delay(10); }
      delay(50);
      return;
    }
  }
}
