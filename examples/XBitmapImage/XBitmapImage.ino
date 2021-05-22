#include <TFT_eSPI.h>  // Include TFT LCD library.
#include "homeIcon.h"


TFT_eSPI tft;  // Initialize TFT LCD.
TFT_eSprite spr = TFT_eSprite(&tft);  // Buffer


//========================================================================= SETUP
void setup()
{
  tft.begin();  // Start TFT LCD.
  tft.setRotation(3);  // Set screen rotation.
  tft.fillScreen(TFT_BLACK); // Black screen fill.
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
