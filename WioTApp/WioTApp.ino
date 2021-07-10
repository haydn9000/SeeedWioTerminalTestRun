#include <TFT_eSPI.h>          //  Include TFT LCD library.
#include "lcd_backlight.hpp"   //  Include TFT LCD backlight library.
#include "Seeed_FS.h"          //  Include file system library.
#include "RawImage.h"          //  Include raw image library.

using namespace std;

TFT_eSPI tft;  // Initialize TFT LCD.
TFT_eSprite spr = TFT_eSprite(&tft); // Buffer

static LCDBackLight backLight;
int maxBrightness = backLight.getMaxBrightness(); // Max brightness (100)
int defaultBrightness = 25;
char optionTest = 'C';


typedef void (*FunctionPointer) ();

const FunctionPointer PROGMEM  mainGameLoop[] = {
  stateMenuIntro,
  stateMenuMain,
  stateMenuHelp,
  stateMenuPlay,
  stateMenuInfo,
  stateMenuSoundfx,
  stateGameNextStage,
  stateGamePlaying,
  stateGamePause,
  stateGameOver,
  stateGameEnded,
};


//========================================================================= SETUP
void setup()
{

  // Check whether SD card is inserted and working
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI))
  {
    while (1);
  }

  tft.begin();  // Start TFT LCD.
  tft.setRotation(3);  // Set screen rotation.
  // spr.createSprite(TFT_HEIGHT, TFT_WIDTH);  // Create buffer.
  
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

  drawImage<uint16_t>("background.bmp", 0, 0);  // Display image on LCD.
}


//========================================================================= LOOP
void loop()
{ 
  if (digitalRead(WIO_KEY_C) == LOW)
  {
    drawImage<uint16_t>("background.bmp", 0, 0); // Display image on LCD.
    optionTest = 'C';
  }

  else if (digitalRead(WIO_KEY_B) == LOW)
  {
    drawImage<uint16_t>("second_icon.bmp", 0, 0); // Display image on LCD.
    optionTest = 'B';
  }
  else if (digitalRead(WIO_KEY_A) == LOW)
  {
    drawImage<uint16_t>("setting_icon.bmp", 0, 0); // Display image on LCD.
    optionTest = 'A';
  }

  switch (optionTest)
  {
    case 'A':
      if (digitalRead(WIO_5S_PRESS) == LOW)
        spr.createSprite(TFT_HEIGHT, TFT_WIDTH); // Create buffer.
        setBrightness();
      break;
    case 'B':
      break;
    case 'C':
      navigation();
      break;
  }
}
