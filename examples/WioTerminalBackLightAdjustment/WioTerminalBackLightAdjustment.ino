#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"
//#include <cstdint>

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

static LCDBackLight backLight;
std::uint8_t maxBrightness = backLight.getMaxBrightness(); // Max brightness

void setup() 
{
    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(tft.color565(255, 0, 0));

    backLight.initialize();
    backLight.setBrightness(50); // Max brightness is 100.
    
    spr.createSprite(TFT_HEIGHT, TFT_WIDTH);

    // Top 3 button inputs, far right button is A, middle B, left C.
    pinMode(WIO_KEY_A, INPUT);
    pinMode(WIO_KEY_B, INPUT);
    pinMode(WIO_KEY_C, INPUT);
}

static std::uint8_t brightness = 50;
void setBrightness()
{
  tft.setTextSize(5);
  tft.setTextColor(TFT_BLACK);

  if (digitalRead(WIO_KEY_A) == LOW)
  {
    brightness += 10;
    backLight.setBrightness(brightness);
    tft.drawString("Brightness set to:" + 10);
    delay(50);
  }
  else if (digitalRead(WIO_KEY_B) == LOW) 
  {
    brightness -= 10;
    backLight.setBrightness(brightness);
    tft.drawString("Brightness set to:" + 10);
    delay(50);
  }
  
}

void loop() {

}
