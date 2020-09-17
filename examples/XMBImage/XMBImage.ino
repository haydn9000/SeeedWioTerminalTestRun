#include <TFT_eSPI.h>
#include "bitmapImages.h"

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define GREY     0xD6BA

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

//static LCDBackLight backLight;
//int maxBrightness = backLight.getMaxBrightness(); // Max brightness

void setup() 
{
    tft.begin();
    tft.setRotation(3);
    spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer

//    backLight.initialize();
//    backLight.setBrightness(50); // Max brightness is 100.


    // Draw the icons
    tft.pushImage(100, 100, 100, 95, homeIcon);

    // Top 3 button inputs, far right button is A, middle B, left C.
    pinMode(WIO_KEY_A, INPUT);
    pinMode(WIO_KEY_B, INPUT);
    pinMode(WIO_KEY_C, INPUT);
}

void drawHome()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);

//  spr.drawXbm(110, 0, 100, 95, houseIcon);

  spr.pushSprite(0, 0);
  delay(200);
}

void loop() 
{
  
}
