// bluetooth.ino — BLE GATT peripheral for WT-001 data reception.
//
// Pattern follows the official Seeed Wio Terminal TFT + BLE documentation example:
// https://wiki.seeedstudio.com/Wio-terminal-BLE-introduction/
//
// Device name:       WT-001
// Service UUID:      4e495554-494f-5500-0000-000000000001
// RX Characteristic: 4e495554-494f-5500-0000-000000000002  (host writes JSON here)
//
// Includes live in WTApp.ino (compiled first):
//   #include <rpcBLEDevice.h>
//   #include <BLEServer.h>

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
static volatile bool g_blePending = false;
static char          g_bleBuf[256];

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
// Call once from setup() after tft.begin() and Serial.begin().
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
        BLECharacteristic::PROPERTY_WRITE_NR  // accept write-without-response too
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
// Call every loop() iteration — processes pending writes and handles
// reconnection using the delay(500) + startAdvertising() pattern from docs.
void checkBLE()
{
    // Process any pending write on the main thread.
    if (g_blePending)
    {
        g_blePending = false;
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

// Call with true when entering BLE usage mode, false when leaving.
void bleSetActive(bool active)
{
    g_bleActive = active;
    if (active) {
        Serial.println("[ble] advertising as WT-001");
        BLEDevice::startAdvertising();
    } else {
        if (!g_bleConnected) {
            Serial.println("[ble] advertising stopped");
            BLEDevice::stopAdvertising();
        }
    }
}
