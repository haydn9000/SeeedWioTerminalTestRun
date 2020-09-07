
//========================================================================= HOMESCREEN
void homeScreen()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
  
  spr.drawString("Home", 30, 100);
  spr.drawNumber(brightness, 170, 100);

    char optionTest;

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
  spr.pushSprite(0, 0);
  delay(200);
}
