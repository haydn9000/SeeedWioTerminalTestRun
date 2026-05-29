// bluetooth.cpp — BLE GATT peripheral for WT-001 data reception.
//
// Device name:       WT-001
// Service UUID:      4e495554-494f-5500-0000-000000000001
// RX Characteristic: 4e495554-494f-5500-0000-000000000002  (host writes JSON here)

#include <Arduino.h>
#include "globals.h"   // TFT_eSPI must be included before BLE headers
#include <rpcBLEDevice.h>
#include <BLEServer.h>

#define WIOT_SERVICE_UUID  "4e495554-494f-5500-0000-000000000001"
#define WIOT_RX_CHAR_UUID  "4e495554-494f-5500-0000-000000000002"
#define WIOT_DESC_UUID     "4e495554-494f-5500-0000-000000000003"
#define BLE_DEVICE_NAME    "WT-001"

static BLEServer*         g_bleServer       = nullptr;
static BLECharacteristic* g_rxChar          = nullptr;
static bool               g_bleConnected    = false;
static bool               g_bleWasConnected = false;
static bool               g_bleActive       = false;  // true only while BLE usage screen is open

// Buffer filled by the write callback; consumed by checkBLE() on the main thread.
// 512 bytes: worst-case process JSON is ~300 bytes (5 processes × 28-char names).
static volatile bool g_blePending = false;
static char          g_bleBuf[512];

// -------------------------------------------------------------------------
// Characteristic write callback — runs in BLE stack context.
class RxCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* pChar) override
    {
        std::string val = pChar->getValue();
        size_t len = val.length();
        if (len > 0 && len < sizeof(g_bleBuf))
        {
            memcpy(g_bleBuf, val.c_str(), len);
            g_bleBuf[len] = '\0';
            g_blePending  = true;
        }
    }
};

// -------------------------------------------------------------------------
// Server connection callbacks — only set flags; reconnect is handled in
// checkBLE() on the main thread to avoid stack-context issues.
class ServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer*)    override { g_bleConnected = true;  }
    void onDisconnect(BLEServer*) override { g_bleConnected = false; }
};

// -------------------------------------------------------------------------
// Call once from loop() after tft.begin() and Serial.begin().
void bleInit()
{
    BLEDevice::init(BLE_DEVICE_NAME);

    g_bleServer = BLEDevice::createServer();
    g_bleServer->setCallbacks(new ServerCallbacks());

    BLEService* svc = g_bleServer->createService(WIOT_SERVICE_UUID);

    g_rxChar = svc->createCharacteristic(
        WIOT_RX_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ  |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    g_rxChar->setAccessPermissions(GATT_PERM_READ | GATT_PERM_WRITE);
    g_rxChar->createDescriptor(
        WIOT_DESC_UUID,
        ATTRIB_FLAG_VOID | ATTRIB_FLAG_ASCII_Z,
        GATT_PERM_READ | GATT_PERM_WRITE,
        2
    );
    g_rxChar->setCallbacks(new RxCallbacks());

    svc->start();

    // Pre-configure advertising but do NOT start yet — startAdvertising is
    // called explicitly via bleSetActive(true) when the user picks BLE mode.
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(WIOT_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMinPreferred(0x12);
}

// -------------------------------------------------------------------------
// Call every loop() iteration — processes pending writes and handles reconnection.
void checkBLE()
{
    if (g_blePending)
    {
        g_blePending = false;
        if (strstr(g_bleBuf, "\"cpu\":"))
            parseSysStatsJson(g_bleBuf);
        else if (strstr(g_bleBuf, "\"p\":[" ))
            parseProcessJson(g_bleBuf);
        else
            parseUsageJson(g_bleBuf);
    }

    // Restart advertising after a disconnect — only if BLE mode is active.
    if (!g_bleConnected && g_bleWasConnected && g_bleActive)
    {
        delay(500);
        BLEDevice::startAdvertising();
    }
    g_bleWasConnected = g_bleConnected;
}

bool isBLEConnected()
{
    return g_bleConnected;
}

// Returns the BLE MAC address as "AA:BB:CC:DD:EE:FF" (big-endian, matching scanner output).
// rpcBLE toString() gives bytes LSB-first, so we reverse the 6 octets.
// Returns "--:--:--:--:--:--" before bleInit() has run.
const char* getBLEAddress()
{
    static char addrBuf[18] = "--:--:--:--:--:--";
    if (bleInitDone) {
        std::string s = BLEDevice::getAddress().toString();
        if (s.length() == 17) {
            uint8_t b[6];
            for (int i = 0; i < 6; i++)
                b[i] = (uint8_t)strtol(s.c_str() + i * 3, nullptr, 16);
            snprintf(addrBuf, sizeof(addrBuf),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     b[5], b[4], b[3], b[2], b[1], b[0]);
        }
    }
    return addrBuf;
}

// Call this after WiFi scan to restore BLE functionality.
// Called after exiting the WiFi scan screen — resets ble_start_flags so the
// next BLE operation re-issues ble_start() to re-activate the GAP state machine.
void bleReinit()
{
    BLEDevice::ble_start_flags = false;
    g_bleConnected    = false;
    g_bleWasConnected = false;
    g_bleActive       = false;
    g_blePending      = false;
    Serial.println("[ble] state reset after WiFi scan");
}

// bleReinitPending() kept for ABI compat — always returns false.
bool bleReinitPending() { return false; }

// Full deinit + reinit: tears down the BLE stack on the RTL8720DN and rebuilds
// it from scratch. Called on BLE scanner entry to guarantee a clean GAP state
// regardless of previous WiFi or advertising activity.
void bleHardReset()
{
    if (!bleInitDone) return;
    if (g_bleActive) BLEDevice::stopAdvertising();
    g_bleActive       = false;
    g_bleConnected    = false;
    g_bleWasConnected = false;
    g_blePending      = false;
    ble_deinit();
    delay(500);
    extern bool initialized;
    initialized = false;
    BLEDevice::ble_start_flags = false;
    g_bleServer = nullptr;
    g_rxChar    = nullptr;
    bleInit();   // re-registers GATT profile on RTL8720DN, leaves ble_start_flags=false
    Serial.println("[ble] hard reset complete");
}

// Call with true when entering BLE usage mode, false when leaving.
void bleSetActive(bool active)
{
    if (!bleInitDone) {
        Serial.println("[ble] not initialised yet — ignoring");
        return;
    }
    // Guard: if already inactive, don't send stopAdvertising() to the co-processor.
    // After bleReinit(), advertising was never started, so calling stopAdvertising()
    // would send an RPC to the RTL8720DN that can hang or be ignored.
    if (!active && !g_bleActive) return;
    g_bleActive = active;
    if (active) {
        // Ensure ble_start() has been called before advertising — needed after
        // WiFi scan resets the GAP state machine via wifi_off()/wifi_on().
        if (!BLEDevice::ble_start_flags) {
            BLEDevice::ble_start_flags = true;
            ble_start();
            delay(200);
        }
        Serial.println("[ble] advertising as WT-001");
        BLEDevice::startAdvertising();
    } else {
        if (g_bleConnected) {
            Serial.println("[ble] screen closed — connection persists");
        } else {
            BLEDevice::stopAdvertising();
            Serial.println("[ble] advertising stopped");
        }
    }
}
