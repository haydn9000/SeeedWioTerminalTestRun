
//========================================================================= BACHLIGHT
static int brightness = 25;
void setBrightness()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);

  if (digitalRead(WIO_KEY_C) == LOW)
  {
    drawImage<uint16_t>("background.bmp", 0, 0);  // Display image on LCD.
    optionTest = 'C';
  }
  else if (digitalRead(WIO_5S_RIGHT) == LOW && brightness < 100) // When A is pressed backlight gets brighter
  {
    brightness += 5;
    backLight.setBrightness(brightness);
    spr.drawString("Brightness: ", 30, 100);
    spr.drawNumber(brightness, 170, 100);
  }
  else if (digitalRead(WIO_5S_LEFT) == LOW && brightness > 0) // When B is pressed backlight gets dimmer
  {
    brightness -= 5;
    backLight.setBrightness(brightness);
    spr.drawString("Brightness: ", 30, 100);
    spr.drawNumber(brightness, 170, 100);;
  }
  else if ((brightness < 1 || brightness > 99) && 
           (digitalRead(WIO_KEY_A) == LOW || digitalRead(WIO_KEY_B) == LOW))
  {
    spr.drawString("Limit reached!", 30, 100);
    spr.drawNumber(brightness, 200, 100);
  }
  else
  {
    spr.drawString("Brightness: ", 30, 100);
    spr.drawNumber(brightness, 170, 100);
  }

  spr.drawRect(30, 120, 175, 20, TFT_WHITE);
  spr.fillRect(30, 120, (175*brightness/100), 20, TFT_WHITE);

  spr.pushSprite(0, 0);
  delay(200);
}
