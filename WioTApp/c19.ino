
//========================================================================= COVID-19
void covidDataMA()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
  
  spr.drawString("COVID-19 WIP", 30, 100);
  spr.drawString("WIP", 30, 110);
  spr.drawNumber(brightness, 170, 100);

  spr.pushSprite(0, 0);
  delay(200);
}
