#include <Arduino.h>
#include "globals.h"

TFT_eSPI tft;
TFT_eSprite spr(&tft);
LCDBackLight backLight;

int maxBrightness;
static int defaultBrightness = 25;
char optionTest = 'C';

int menuIndex = 0;
const char* menuItems[] = { "Home", "Claude Usage", "Sys Stats", "Settings" };
bool menuNeedsRedraw = true;
bool bleInitDone = false;


//========================================================================= SETUP
void setup()
{
  tft.begin();
  tft.setRotation(3);
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH);

  backLight.initialize();
  maxBrightness = backLight.getMaxBrightness();
  backLight.setBrightness(defaultBrightness);

  // Top 3 button inputs — far right = A, middle = B, left = C.
  // Using INPUT (not PULLUP) as these buttons have external pull-downs on the Wio Terminal.
  pinMode(WIO_KEY_A, INPUT);
  pinMode(WIO_KEY_B, INPUT);
  pinMode(WIO_KEY_C, INPUT);
  // 5-way joystick — active LOW with internal pull-ups.
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  Serial.begin(115200);

  if (batteryBegin()) {
    Serial.println("[battery] BQ27441 found — battery status enabled");
  } else {
    Serial.println("[battery] BQ27441 not found — battery overlay disabled");
  }

  Serial.println("[boot] setup complete");

  // Draw the menu immediately so the screen is live before BLE init can hang.
  drawMenu();
  menuNeedsRedraw = false;
}


//========================================================================= LOOP
void loop()
{
  // Defer BLE init until after the first frame renders (drawMenu runs in setup).
  // BLEDevice::init() talks to the RTL8720DN chip — requires BLE firmware.
  if (!bleInitDone && millis() > 3000)
  {
    bleInitDone = true;
    Serial.println("[ble] initialising...");
    bleInit();
    Serial.println("[ble] ready");
  }

  // Check for incoming JSON usage data — serial or BLE (both non-blocking).
  checkSerial();
  checkBLE();

  // Top button C → navigate to menu screen.
  if (digitalRead(WIO_KEY_C) == LOW)
  {
    menuNeedsRedraw = true;
    optionTest = 'C';
  }
  // Top button B → placeholder for a future screen.
  else if (digitalRead(WIO_KEY_B) == LOW)
  {
    optionTest = 'B';
  }
  // Top button A → go directly to brightness settings.
  else if (digitalRead(WIO_KEY_A) == LOW)
  {
    optionTest = 'A';
  }

  // Dispatch to the active screen handler each loop iteration.
  switch (optionTest)
  {
    case 'A':
      setBrightness();
      optionTest = 'C';
      menuNeedsRedraw = true;
      break;
    case 'B':
      break;
    case 'C':
      navigation();
      break;
  }
}
