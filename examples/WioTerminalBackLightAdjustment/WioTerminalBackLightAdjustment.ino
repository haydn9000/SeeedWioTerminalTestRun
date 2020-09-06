#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"
using namespace std;

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

static LCDBackLight backLight;
int maxBrightness = backLight.getMaxBrightness(); // Max brightness

void setup() 
{
    tft.begin();
    tft.setRotation(3);
    spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer

    backLight.initialize();
    backLight.setBrightness(50); // Max brightness is 100.

    // Top 3 button inputs, far right button is A, middle B, left C.
    pinMode(WIO_KEY_A, INPUT);
    pinMode(WIO_KEY_B, INPUT);
    pinMode(WIO_KEY_C, INPUT);
}

static int brightness = 50;
void setBrightness()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);


  if (digitalRead(WIO_KEY_A) == LOW && brightness < 100) // When A is pressed backlight gets brighter
  {
    brightness += 5;
    backLight.setBrightness(brightness);
    spr.drawString("Brightness set to: ", 15, 110);
    spr.drawNumber(brightness, 232, 110);
  }
  else if (digitalRead(WIO_KEY_B) == LOW && brightness > 0 ) // When B is pressed backlight gets dimmer
  {
    brightness -= 5;
    backLight.setBrightness(brightness);
    spr.drawString("Brightness set to: ", 15, 110);
    spr.drawNumber(brightness, 232, 110);
  }
  else if (brightness < 1 || brightness > 99)
  {
    spr.drawString("Limit reached!", 15, 110);
    spr.drawNumber(brightness, 190, 110);
  }
  else
  {
    spr.drawString("Brightness = ", 15, 110);
    spr.drawNumber(brightness, 170, 110);
  }

  spr.pushSprite(0, 0);
  delay(200);
}

void loop() 
{
  setBrightness();
}
