#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"
using namespace std;

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

static LCDBackLight backLight;
int maxBrightness = backLight.getMaxBrightness(); // Max brightness

//========================================================================= SETUP
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

//========================================================================= LOOP
void loop()
{
  setBrightness();
}
