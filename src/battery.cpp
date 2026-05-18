#include <Arduino.h>
#include <Wire.h>
#include "globals.h"

// =========================================================================
// BATTERY — BQ27441-G1A fuel gauge (Wio Terminal Battery Chassis 650mAh)
//
// Texas Instruments BQ27441-G1A at I2C address 0x55.
// Uses the chip's Standard Commands interface — no external library needed.
// =========================================================================

#define BQ27441_ADDR        0x55

#define BQ27441_CMD_FLAGS   0x06  // Bit 0: DSG (1=discharging), Bit 9: FC (full charge)
#define BQ27441_CMD_SOC     0x1C  // State of charge (0–100 %)
#define BQ27441_CMD_VOLTAGE 0x04  // Pack voltage (mV)

static bool g_batteryFound = false;

// Read a 16-bit standard command from the BQ27441 (little-endian).
// Returns 0xFFFF on failure.
static uint16_t bq27441Read(uint8_t cmd)
{
    Wire.beginTransmission(BQ27441_ADDR);
    Wire.write(cmd);
    if (Wire.endTransmission(false) != 0) return 0xFFFF;
    uint8_t n = Wire.requestFrom((uint8_t)BQ27441_ADDR, (uint8_t)2, (uint8_t)true);
    if (n < 2) return 0xFFFF;
    uint16_t lo = Wire.read();
    uint16_t hi = Wire.read();
    return (hi << 8) | lo;
}

// Try to reach the BQ27441 over I2C. Call once from setup().
// Returns true if the chip is present and readable.
bool batteryBegin()
{
    Wire.begin();
    Wire.beginTransmission(BQ27441_ADDR);
    if (Wire.endTransmission() != 0) { g_batteryFound = false; return false; }
    // A bare address probe can give a false ACK on first use of the SAMD51 Wire bus.
    // Confirm with an actual register read before declaring the chip present.
    g_batteryFound = (bq27441Read(BQ27441_CMD_SOC) != 0xFFFF);
    return g_batteryFound;
}

// Returns true when the battery is charging (DSG flag clear = not discharging).
bool batteryCharging()
{
    if (!g_batteryFound) return false;
    uint16_t flags = bq27441Read(BQ27441_CMD_FLAGS);
    if (flags == 0xFFFF) return false;
    return !(flags & 0x01);  // bit 0 DSG: 0 = charging, 1 = discharging
}

// Returns battery state of charge (0–100), or -1 if unavailable.
int batteryLevel()
{
    if (!g_batteryFound) return -1;
    uint16_t soc = bq27441Read(BQ27441_CMD_SOC);
    if (soc == 0xFFFF) return -1;
    return (int)soc;
}

// Draw a compact battery indicator in the top-right corner of the current screen.
// bgColor must match the screen background so the text box blends in.
void drawBatteryStatus(uint16_t bgColor)
{
    if (!g_batteryFound) return;
    int level = batteryLevel();
    if (level < 0) return;

    char buf[8];
    snprintf(buf, sizeof(buf), batteryCharging() ? "~%d%%" : "%d%%", level);

    uint16_t color = level <= 25 ? TFT_RED
                   : level <= 50 ? tft.color565(255, 165, 0)
                   :               tft.color565(100, 200, 100);

    tft.setTextSize(1);
    tft.setTextColor(color, bgColor);
    int w = strlen(buf) * 6;  // GLCD: 6px per character at textSize 1
    tft.drawString(buf, 316 - w, 4);
}
