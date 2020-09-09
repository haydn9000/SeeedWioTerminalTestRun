
//========================================================================= COVID-19
void covidDataMA()
{
  while(true)
  {

  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
    
  spr.drawString("COVID-19", 30, 100);
  spr.drawString("WIP", 30, 120);
  
  spr.pushSprite(0, 0);
  delay(200);

  if (WIO_KEY_C != LOW) break;
  }
}
