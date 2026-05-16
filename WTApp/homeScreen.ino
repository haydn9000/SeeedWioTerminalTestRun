
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
    int y = 65 + i * 55;  // Vertical spacing: 55px between items.
    if (i == menuIndex)
    {
      // Selected item: filled white box, black text.
      tft.fillRoundRect(20, y - 5, 280, 40, 4, TFT_WHITE);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
    }
    else
    {
      // Unselected item: dark outline only, white text.
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
    // Wrap around to the last item when moving past the top.
    menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
    delay(200);  // Debounce — prevents skipping multiple items per press.
    changed = true;
  }
  else if (digitalRead(WIO_5S_DOWN) == LOW)
  {
    // Wrap around to the first item when moving past the bottom.
    menuIndex = (menuIndex + 1) % MENU_COUNT;
    delay(200);
    changed = true;
  }
  else if (digitalRead(WIO_5S_PRESS) == LOW)
  {
    // Launch the sub-screen for the selected item.
    // Each sub-screen blocks until the user exits, then returns here.
    switch (menuIndex)
    {
      case 0: homeScreen(); break;
      case 1: claudeUsageScreen(); break;
      case 2: setBrightness(); break;
    }
    delay(200);  // Short pause after returning so the press isn't re-detected.
    changed = true;  // Redraw the menu once after returning from a sub-screen.
  }

  // Only call drawMenu when something actually changed to prevent flicker.
  if (changed) drawMenu();
}

//========================================================================= HOMESCREEN
// Placeholder home screen drawn using the sprite buffer for flicker-free updates.
void homeScreen()
{
  spr.fillSprite(tft.color565(255, 0, 0));
  spr.setTextSize(2);
  spr.setTextColor(TFT_WHITE);
  spr.drawString("Home", 30, 100);
  spr.drawString("WIP", 30, 120);
  spr.setTextSize(1);
  spr.setTextColor(tft.color565(148, 112, 98));
  spr.drawString("C: back", 20, 224);
  spr.pushSprite(0, 0);

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
