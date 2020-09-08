#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"
using namespace std;

TFT_eSPI tft;
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

static LCDBackLight backLight;
int maxBrightness = backLight.getMaxBrightness(); // Max brightness (100)
int defaultBrightness = 25;
char optionTest = 'C';

//========================================================================= SETUP
void setup()
{
  tft.begin();
  tft.setRotation(3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer

  backLight.initialize();
  backLight.setBrightness(defaultBrightness);

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

//========================================================================= LOOP
void loop()
{ 
    if (digitalRead(WIO_KEY_A) == LOW)
    {
      optionTest = 'A';
    }
    else if (digitalRead(WIO_KEY_B) == LOW)
    {
      optionTest = 'B';
    }
    else if (digitalRead(WIO_KEY_C) == LOW)
    {
      optionTest = 'C';
    }
  
  
  switch (optionTest)
  {
    case 'A':
      setBrightness();
      break;
    case 'B':
      covidDataMA();
      break;
    case 'C':
      homeScreen();
      break;
  }
}
