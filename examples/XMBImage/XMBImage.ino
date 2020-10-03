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

char optionTest = 'C';

//========================================================================= SETUP
void setup()
{
  tft.begin();
  tft.setRotation(3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer
  
  spr.fillSprite(tft.color565(255, 0, 0));
  
  // Draw the icons
  spr.pushImage(0, 0, 320, 240, spongebob);
//  displayImage();
    
  
  spr.pushSprite(0, 0);

  
  // Top 3 button inputs, far right button is A, middle B, left C.
  pinMode(WIO_KEY_A, INPUT);
  pinMode(WIO_KEY_B, INPUT);
  pinMode(WIO_KEY_C, INPUT);
  // 5 Way switch
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
}

//========================================================================= Functions
void displayImage()
{
  
}

//========================================================================= LOOP
void loop()
{ 
  
}
