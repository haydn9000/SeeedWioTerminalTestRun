#include <TFT_eSPI.h>          //  Include TFT LCD library.
#include "lcd_backlight.hpp"   //  Include TFT LCD backlight library.
#include <rpcBLEDevice.h>      //  Wio Terminal RTL8720DN BLE stack (Seeed_Arduino_rpcBLE).
#include <BLEServer.h>

using namespace std;

TFT_eSPI tft;                    // TFT display driver instance.
TFT_eSprite spr = TFT_eSprite(&tft); // Sprite buffer (used for flicker-free drawing on sub-screens).

static LCDBackLight backLight;
int maxBrightness = backLight.getMaxBrightness(); // Max brightness value (100).
int defaultBrightness = 25;      // Brightness on startup (0–100).
char optionTest = 'C';           // Tracks the active top-level screen: 'A'=settings, 'B'=unused, 'C'=menu.

// Menu state shared across WioTApp.ino and homeScreen.ino.
int menuIndex = 0;               // Currently highlighted menu item.
const int MENU_COUNT = 3;        // Total number of menu items.
const char* menuItems[] = { "Home", "Claude Usage", "Settings" };
bool menuNeedsRedraw = true;     // Set true to force the menu to redraw on the next navigation() call.
bool bleInitDone = false;        // BLE init is deferred to loop() so the display renders first.


//========================================================================= SETUP
void setup()
{

  tft.begin();  // Start TFT LCD.
  tft.setRotation(3);  // Set screen rotation (3 = landscape).
  spr.createSprite(TFT_HEIGHT, TFT_WIDTH);  // Allocate sprite buffer once at startup.

  backLight.initialize();                  // Configure SAMD51 timer/PWM for backlight control.
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

  // Serial for receiving JSON usage data over USB.
  Serial.begin(115200);
  Serial.println("[boot] setup complete");

  // Draw the menu immediately so the screen is live before BLE init can hang.
  drawMenu();
}


//========================================================================= LOOP
void loop()
{ 
  // Defer BLE init until after the first frame renders (drawMenu runs in setup).
  // BLEDevice::init() talks to the RTL8720DN chip — requires BLE firmware.
  if (!bleInitDone && millis() > 3000)
  {
    bleInitDone = true;
    Serial.println("[ble] calling bleInit()...");
    bleInit();
    Serial.println("[ble] bleInit() done — ready (advertising starts when BLE mode selected)");
  }

  // Check for incoming JSON usage data — serial or BLE (both non-blocking).
  checkSerial();
  checkBLE();

  // Top button C → navigate to menu screen.
  if (digitalRead(WIO_KEY_C) == LOW)
  {
    menuNeedsRedraw = true;  // Ensure the menu redraws when switching back to it.
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
    case 'A':          // Direct shortcut to brightness from top button A.
      setBrightness();
      break;
    case 'B':          // Unused — reserved for a future screen.
      break;
    case 'C':          // Main menu, joystick-navigated.
      navigation();
      break;
  }
}
