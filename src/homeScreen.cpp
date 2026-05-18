#include <Arduino.h>
#include "globals.h"


//========================================================================= NAVIGATION
// Renders the full menu to the display. Called only when something changes
// to avoid unnecessary screen clears (which cause visible flicker).
void drawMenu()
{
  tft.fillScreen(TFT_BLACK);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("MENU", 130, 12);

  // Draw each menu item; highlight the selected one with a filled white rectangle.
  for (int i = 0; i < MENU_COUNT; i++)
  {
    int y = 65 + i * 55;
    if (i == menuIndex)
    {
      tft.fillRoundRect(20, y - 5, 280, 40, 4, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
    }
    else
    {
      tft.fillRoundRect(20, y - 5, 280, 40, 4, TFT_BLACK);
      tft.drawRoundRect(20, y - 5, 280, 40, 4, TFT_DARKGREY);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.drawString(menuItems[i], 35, y + 6);
  }

  // Hint
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("UP/DOWN: navigate   PRESS: select", 20, 224);

  drawBatteryStatus(TFT_BLACK);
}

// Called every loop iteration while optionTest == 'C'.
// Reads joystick input and redraws the menu only when the selection changes.
void navigation()
{
  // Consume the redraw flag set by external callers (e.g. returning from a sub-screen).
  bool changed = menuNeedsRedraw;
  menuNeedsRedraw = false;

  if (digitalRead(WIO_5S_UP) == LOW)
  {
    menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
    delay(200);
    changed = true;
  }
  else if (digitalRead(WIO_5S_DOWN) == LOW)
  {
    menuIndex = (menuIndex + 1) % MENU_COUNT;
    delay(200);
    changed = true;
  }
  else if (digitalRead(WIO_5S_PRESS) == LOW)
  {
    switch (menuIndex)
    {
      case 0: homeScreen(); break;
      case 1: {
        // Loop: picker → usage screen → back to picker. KEY_C on the picker exits to menu.
        while (true) {
          int mode = claudeUsagePicker();
          if (mode < 0) break;
          claudeUsageScreen(mode);
        }
        break;
      }
      case 2: setBrightness(); break;
    }
    delay(200);
    changed = true;
  }

  if (changed) drawMenu();
}


//========================================================================= HOMESCREEN
// Placeholder home screen — displays a WIP message on a red background.
void homeScreen()
{
  // Wait for the joystick press that launched us to be fully released.
  while (digitalRead(WIO_5S_PRESS) == LOW) { delay(10); }

  tft.fillScreen(tft.color565(180, 30, 30));
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, tft.color565(180, 30, 30));
  tft.drawString("Home", 30, 100);
  tft.drawString("WIP", 30, 125);
  tft.setTextSize(1);
  tft.setTextColor(tft.color565(220, 160, 140), tft.color565(180, 30, 30));
  tft.drawString("C: back", 20, 224);

  drawBatteryStatus(tft.color565(180, 30, 30));

  while (true)
  {
    if (digitalRead(WIO_KEY_C) == LOW)
    {
      while (digitalRead(WIO_KEY_C) == LOW) { delay(10); }
      delay(50);
      return;
    }
    delay(20);
  }
}
