#pragma once
#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"

// ---- Global objects (defined in main.cpp) ----
extern TFT_eSPI tft;
extern TFT_eSprite spr;
extern LCDBackLight backLight;
extern int maxBrightness;
extern char optionTest;
extern bool bleInitDone;

// ---- Menu state (defined in main.cpp) ----
extern int menuIndex;
constexpr int MENU_COUNT = 3;
extern const char* menuItems[];
extern bool menuNeedsRedraw;

// ---- homeScreen.cpp ----
void drawMenu();
void navigation();
void homeScreen();

// ---- backlight.cpp ----
void drawBrightness();
void setBrightness();

// ---- battery.cpp ----
bool batteryBegin();
bool batteryCharging();
int batteryLevel();
void drawBatteryStatus(uint16_t bgColor);

// ---- claudeUsage.cpp ----
bool parseUsageJson(const char* json);
void checkSerial();
void claudeUsageScreen(int mode);
int claudeUsagePicker();

// ---- bluetooth.cpp ----
void bleInit();
void checkBLE();
bool isBLEConnected();
void bleSetActive(bool active);
