#include "TFT_eSPI.h" //include TFT LCD library 
#include "Seeed_FS.h" //include file system library 
#include "RawImage.h" //include raw image library

TFT_eSPI tft; //initialize TFT LCD 


//========================================================================= SETUP
void setup()
{
    //check whether SD card is inserted and working
    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI))
    {
        while (1);
    }

    tft.begin();
    tft.setRotation(3);

    drawImage<uint8_t>("glicth.bmp", 0, 0);
}

//========================================================================= LOOP
void loop()
{ 
  
}
