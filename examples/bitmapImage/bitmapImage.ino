#include <TFT_eSPI.h>
#include "spongebobImage.h"


TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer


//========================================================================= SETUP
void setup()
{
  tft.begin();
  tft.setRotation(3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer
  
  spr.fillSprite(tft.color565(0, 0, 255));
  
  // Draw the image
  displayImage();
}


//========================================================================= Functions
void displayImage()
{
  spr.fillSprite(tft.color565(0, 0, 255));
  spr.pushImage(0, 0, spongebobWidth, spongebobHeight, spongebob);
  spr.pushSprite(0, 0);
}


//========================================================================= LOOP
void loop()
{ 
  
}
