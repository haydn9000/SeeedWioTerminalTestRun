
//========================================================================= HOMESCREEN
void homeScreen()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
  
  spr.drawString("Home", 30, 100);
  spr.drawString("WIP", 30, 120);

  spr.pushSprite(0, 0);
  delay(200);
}
