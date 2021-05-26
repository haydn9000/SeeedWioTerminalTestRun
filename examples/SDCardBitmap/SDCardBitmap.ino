#include "TFT_eSPI.h"  // Include TFT LCD library 
#include "Seeed_FS.h"  // Include file system library 
#include "RawImage.h"  // Include raw image library

TFT_eSPI tft;  // Initialize TFT LCD 


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

    drawImage<uint16_t>("glitch.bmp", 0, 0);  // Display image on LCD.
}

//========================================================================= LOOP
void loop()
{ 
  
}
