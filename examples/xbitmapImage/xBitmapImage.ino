#include <TFT_eSPI.h>
#include "homeIcon.h"


TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer


//========================================================================= SETUP
void setup()
{
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK); // Black screen fill
//  spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer
  
//  spr.fillSprite(tft.color565(255, 0, 0));
  
  // Draw the image
  displayImage();
}


//========================================================================= Functions
void displayImage()
{
//  spr.fillSprite(tft.color565(0, 0, 255));
  tft.drawXBitmap(0, 0, homeIcon, homeIconWidth, homeIconHeight, TFT_WHITE, TFT_GREEN);
//  spr.pushSprite(0, 0);
}


//========================================================================= LOOP
void loop()
{ 
  
}
