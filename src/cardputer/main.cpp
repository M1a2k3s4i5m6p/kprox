#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../led.h"
#include "../hid.h"
#include "../storage.h"
#include "../registers.h"
#include "../connection.h"
#include "../token_parser.h"
#include "../api.h"
#include "../mtls.h"
#include "../keymap.h"
#include "ui_manager.h"
#include "app_launcher.h"
#include "app_kprox.h"
#include "app_keyboard_hid.h"
#include "app_clock.h"
#include "app_settings.h"
#include <M5Cardputer.h>
#include "nvs_flash.h"
#include "nvs.h"

// USB descriptor strings must reach TinyUSB before initArduino() calls USB.begin().
// Bluetooth name is handled in setup() via pointer construction — no timing issue there.
#ifdef BOARD_HAS_USB_HID
static void usbPreInit() __attribute__((constructor(110)));
static void usbPreInit() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    char mfg[64]  = DEFAULT_MANUFACTURER;
    char prod[64] = DEFAULT_PRODUCT_NAME;
    uint8_t usbEn = 1;
    nvs_handle_t h;
    if (nvs_open("kprox", NVS_READONLY, &h) == ESP_OK) {
        size_t len;
        nvs_get_u8(h, "usbEnabled", &usbEn);
        len = sizeof(mfg);  nvs_get_str(h, "usbMfg",    mfg,  &len);
        len = sizeof(prod); nvs_get_str(h, "usbProduct", prod, &len);
        nvs_close(h);
    }
    if (usbEn) {
        USB.manufacturerName(mfg);
        USB.productName(prod);
        USB.serialNumber(USB_SERIAL_NUMBER);
    }
}
#endif

// ---- mDNS version compatibility (same as kprox.ino) ----

class MDNSHelper {
public:
    static void safeUpdate() {
#ifdef ESP_ARDUINO_VERSION_MAJOR
#  if ESP_ARDUINO_VERSION_MAJOR < 2
        MDNS.update();
#  endif
#else
#  ifdef ARDUINO_ESP32_DEV
        MDNS.update();
#  endif
#endif
    }
};
#define MDNS_UPDATE() MDNSHelper::safeUpdate()

// ---- Global definitions ----

WebServer        server(80);
WebServer        serverHTTP(443);
WiFiUDP          udp;
Preferences      preferences;
CRGB             leds[NUM_LEDS];

// Constructed in setup() after settings are loaded so the BT device name
// and battery level reflect persisted values on every boot.
BleComboKeyboard* Keyboard = nullptr;
BleComboMouse*    Mouse    = nullptr;

USBHIDKeyboard USBKeyboard;
USBHIDMouse    USBMouse;
bool usbEnabled       = true;
bool usbInitialized   = false;
bool usbKeyboardReady = false;
bool usbMouseReady    = false;

String wifiSSID      = DEFAULT_WIFI_SSID;
String wifiPassword  = DEFAULT_WIFI_PASSWORD;
String apiKey        = DEFAULT_API_KEY;
String usbManufacturer = DEFAULT_MANUFACTURER;
String usbProduct    = DEFAULT_PRODUCT_NAME;

bool wifiEnabled     = true;

const char* hostname   = HOSTNAME;
const char* deviceName = DEFAULT_PRODUCT_NAME;

bool bluetoothEnabled     = true;
bool bluetoothInitialized = false;
bool mdnsEnabled          = false;
bool isLooping            = false;
bool isHalted             = false;
bool requestInProgress    = false;
bool udpEnabled           = true;
bool ledEnabled           = true;
bool registersLoaded      = false;

unsigned long loopDuration    = 0;
unsigned long loopStartTime   = 0;
unsigned long lastUdpBroadcast = 0;
unsigned long lastWifiCheck   = 0;
unsigned long lastStatusPrint = 0;

int loopingRegister = -1;
std::vector<String> pendingTokenStrings;
long                utcOffsetSeconds = 0;
int activeRegister  = 0;
int currentMouseX   = 0;
int currentMouseY   = 0;

uint8_t ledColorR = 0;
uint8_t ledColorG = 255;
uint8_t ledColorB = 0;

std::vector<String> registers;
std::vector<String> registerNames;

MouseBatch mouseBatch;

// ---- Watchdog ----

void initWatchdog() {
    esp_task_wdt_deinit();
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic  = true
    };
    esp_task_wdt_init(&cfg);
#else
    esp_task_wdt_init(WDT_TIMEOUT, true);
#endif
    esp_task_wdt_add(NULL);
}

void feedWatchdog() {
    esp_task_wdt_reset();
}

// ---- Splash screen ----

static void showSplash() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(TFT_BLACK);

    // Large "KProx" text, vertically and horizontally centred
    disp.setTextDatum(MC_DATUM);
    disp.setTextColor(disp.color565(50, 140, 255), TFT_BLACK);
    disp.setTextSize(4);
    disp.drawString("KProx", disp.width() / 2, disp.height() / 2 - 8);

    disp.setTextSize(1);
    disp.setTextColor(disp.color565(80, 80, 80), TFT_BLACK);
    disp.drawString("HID Automation", disp.width() / 2, disp.height() / 2 + 22);

    disp.setTextDatum(TL_DATUM);
    delay(2000);
}

// ---- Setup ----

void setup() {
    delay(1000);

    M5Cardputer.begin(true);

    auto& disp = M5Cardputer.Display;
    disp.setBrightness(128);

    initWatchdog();
    randomSeed(esp_random());

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(80);
    FastLED.clear();
    FastLED.show();

    loadRegisters();
    loadBtSettings();
    loadWiFiSettings();
    loadWifiEnabledSettings();
    loadApiKeySettings();
    loadUtcOffsetSettings();
    loadMTLSSettings();
    loadKeymapSettings();
    loadUSBSettings();
    loadUSBIdentitySettings();

    if (ledEnabled) {
        for (int i = 0; i < 3; i++) { setLED(LED_COLOR_BOOT, 200); delay(200); feedWatchdog(); }
    }

    showSplash();

    if (!SPIFFS.begin(true)) { /* SPIFFS mount failed */ }
    feedWatchdog();
    keymapInit();

    // Construct BLE objects here — after settings are loaded — so the device
    // name and manufacturer are the persisted values, not compile-time defaults.
    // Battery level is read live on Cardputer; other boards report 100.
    uint8_t batLevel = (uint8_t)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100);
    Keyboard = new BleComboKeyboard(usbProduct.c_str(), usbManufacturer.c_str(), batLevel);
    Mouse    = new BleComboMouse(Keyboard);

    if (bluetoothEnabled) {
        Keyboard->begin();
        Mouse->begin();
        bluetoothInitialized = true;
    }

    if (usbEnabled) {
        USB.begin(); USBKeyboard.begin(); USBMouse.begin();
        usbInitialized   = true;
        usbKeyboardReady = true;
        usbMouseReady    = true;
    }

    delay(500);
    if (bluetoothEnabled) BLE_KEYBOARD.releaseAll();
    if (usbEnabled && usbInitialized) USBKeyboard.releaseAll();
    feedWatchdog();

    mouseBatch.accumulatedX = 0;
    mouseBatch.accumulatedY = 0;
    mouseBatch.lastUpdate   = 0;
    mouseBatch.hasMovement  = false;

    WiFi.setHostname(hostname);

    if (wifiEnabled) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    // Non-blocking WiFi wait with display feedback
    disp.fillScreen(disp.color565(30, 30, 30));
    disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
    disp.setTextSize(1);
    disp.drawString("Connecting to SSID: " + wifiSSID, 4, 4);
    if (wifiPassword == DEFAULT_WIFI_PASSWORD) {
        disp.setTextColor(disp.color565(255, 180, 0), disp.color565(30, 30, 30));
        disp.drawString("pw: " + wifiPassword, 4, 18);
        disp.drawString("Default Password: " + wifiPassword, 4, 18);
        disp.drawString("update in SetWiFi app", 4, 30);
        disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
    }
    for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < 30; attempts++) {
        delay(500);
        feedWatchdog();
        disp.drawString(".", 4 + attempts * 6, 46);
        if (attempts == 15) {
            WiFi.disconnect();
            delay(500);
            WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (ledEnabled) { setLED(LED_COLOR_WIFI_CONNECTED, 500); blinkLED(10, LED_COLOR_WIFI_CONNECTED, LED_COLOR_WIFI_CONNECTED_DUTY_CYCLE); }
        initNTP();
        if (udpEnabled) udp.begin(UDP_DISCOVERY_PORT);
        disp.setTextColor(TFT_GREEN, disp.color565(30, 30, 30));
        disp.drawString("Connected: " + WiFi.localIP().toString(), 4, 64);
    } else {
        if (ledEnabled) setLED(LED_COLOR_WIFI_ERROR, 500);
        disp.setTextColor(TFT_RED, disp.color565(30, 30, 30));
        disp.drawString("WiFi failed", 4, 64);
    }
    delay(800);
    feedWatchdog();
    } else {
        WiFi.mode(WIFI_OFF);
    }

    if (registers.empty()) {
        addRegister("{LEFT}{SLEEP 1000}{RIGHT}{SLEEP 1000}{UP}{SLEEP 1000}{DOWN}{SLEEP 1000}{ENTER}");
        addRegister("{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}");
        addRegister("Hello World{SLEEP 1000}{ENTER}Testing tokens: {RAND 1 100}{ENTER}{F1}{SLEEP 500}{ESC}");
        addRegister("{CHORD CTRL+A}{SLEEP 500}{CHORD CTRL+C}{SLEEP 500}{CHORD CTRL+V}");
    }

    if (wifiEnabled) {
        setupRoutes();
        server.begin();
    }

    feedWatchdog();

    if (ledEnabled && !registers.empty()) {
        delay(1000);
        blinkLED(activeRegister + 1, LED_COLOR_REG_CHANGE);
    }

    feedWatchdog();

    // Register apps: Launcher first, then real apps
    static Cardputer::AppLauncher    launcher;
    static Cardputer::AppKProx       appKProx;
    static Cardputer::AppKeyboardHID appKeyboard;
    static Cardputer::AppClock       appClock;
    static Cardputer::AppSettings    appSettings;

    Cardputer::uiManager.addApp(&launcher);
    Cardputer::uiManager.addApp(&appKProx);
    Cardputer::uiManager.addApp(&appKeyboard);
    Cardputer::uiManager.addApp(&appClock);
    Cardputer::uiManager.addApp(&appSettings);

    // Start directly in KProx app (index 1)
    Cardputer::uiManager.launchApp(1);
    Cardputer::uiManager.notifyInteraction();
}

// ---- Loop ----

void loop() {
    feedWatchdog();
    if (wifiEnabled) {
        server.handleClient();
        if (mtlsEnabled) serverHTTP.handleClient();
        MDNS_UPDATE();
    }

    if (wifiEnabled && udpEnabled && WiFi.status() == WL_CONNECTED && millis() - lastUdpBroadcast > UDP_BROADCAST_INTERVAL) {
        broadcastDiscovery();
        lastUdpBroadcast = millis();
    }

    static bool          mdnsSetupAttempted = false;
    static unsigned long wifiConnectedTime  = 0;

    if (WiFi.status() == WL_CONNECTED && !mdnsEnabled && !mdnsSetupAttempted) {
        if (!wifiConnectedTime) wifiConnectedTime = millis();
        else if (millis() - wifiConnectedTime > 10000) { mdnsSetupAttempted = true; setupMDNS(); }
    } else if (WiFi.status() != WL_CONNECTED) {
        wifiConnectedTime = 0;
    }

    if (!isHalted) {
        // Loop playback
        if (isLooping && loopingRegister >= 0 && (size_t)loopingRegister < registers.size()) {
            unsigned long now = millis();
            if (loopDuration == 0 || (now - loopStartTime < loopDuration)) {
                if (!requestInProgress) {
                    playRegister(loopingRegister);
                    delay(100);
                    feedWatchdog();
                }
            } else {
                isLooping = false;
                loopingRegister = -1;
            }
        }
    }

    if (!isHalted && !pendingTokenStrings.empty()) {
        for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
            if (!pendingTokenStrings[i].startsWith("SCHED|")) {
                String tok = pendingTokenStrings[i];
                pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                putTokenString(tok);
            }
        }

        time_t now = time(nullptr);
        if (now > 100000) {
            struct tm* t = localtime(&now);
            for (int i = (int)pendingTokenStrings.size() - 1; i >= 0; i--) {
                String& tok = pendingTokenStrings[i];
                if (!tok.startsWith("SCHED|")) continue;
                int p1 = tok.indexOf('|', 6);
                int p2 = tok.indexOf('|', p1 + 1);
                int p3 = tok.indexOf('|', p2 + 1);
                int targetH = tok.substring(6,      p1).toInt();
                int targetM = tok.substring(p1 + 1, p2).toInt();
                int targetS = tok.substring(p2 + 1, p3).toInt();
                int nowMins = t->tm_hour * 60 + t->tm_min;
                int tgtMins = targetH    * 60 + targetM;
                if (nowMins == tgtMins && t->tm_sec >= targetS) {
                    String remainder = tok.substring(p3 + 1);
                    pendingTokenStrings.erase(pendingTokenStrings.begin() + i);
                    if (remainder.length() > 0) putTokenString(remainder);
                }
            }
        }
    }

    cleanupConnections();

    if (!isHalted && mouseBatch.hasMovement && millis() - mouseBatch.lastUpdate > MOUSE_BATCH_TIMEOUT) {
        sendBatchedMouseMovement();
    }

    static unsigned long lastPeriodicCleanup = 0;
    if (millis() - lastPeriodicCleanup > 60000) {
        if (!requestInProgress && !isHalted) { hidReleaseAll(); delay(KEY_RELEASE_DELAY); }
        lastPeriodicCleanup = millis();
        feedWatchdog();
    }

    // Update BLE battery level every 30 seconds
    static unsigned long lastBatUpdate = 0;
    if (Keyboard && bluetoothInitialized && millis() - lastBatUpdate > 30000) {
        uint8_t bat = (uint8_t)constrain(M5Cardputer.Power.getBatteryLevel(), 0, 100);
        BLE_KEYBOARD.setBatteryLevel(bat);
        lastBatUpdate = millis();
    }

    if (ESP.getFreeHeap() < 8000) {
        delay(1000);
        ESP.restart();
    }

    // Update UI
    Cardputer::uiManager.update();

    delay(10);
}

#endif // BOARD_M5STACK_CARDPUTER
