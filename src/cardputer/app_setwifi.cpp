#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_setwifi.h"
#include "../storage.h"
#include <WiFi.h>

namespace Cardputer {

void AppSetWifi::onEnter() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(disp.color565(30, 30, 30));
    disp.setTextScroll(true);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
    disp.setCursor(0, 0);

    _state    = S_SSID;
    _ssid     = "";
    _pass     = "";
    _inputBuf = "";

    disp.setTextColor(TFT_ORANGE, disp.color565(30, 30, 30));
    disp.println("WiFi SSID:");
    disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
    disp.print(">>> ");
}

void AppSetWifi::onExit() {
    M5Cardputer.Display.setTextScroll(false);
}

void AppSetWifi::_appendLine(const char* text, uint16_t color) {
    auto& disp = M5Cardputer.Display;
    disp.setTextColor(color, disp.color565(30, 30, 30));
    disp.println(text);
}

void AppSetWifi::_connect() {
    auto& disp = M5Cardputer.Display;

    disp.setTextColor(TFT_ORANGE, disp.color565(30, 30, 30));
    disp.printf("Connecting to %s...\n", _ssid.c_str());

    WiFi.disconnect(true);
    delay(200);
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiSSID     = _ssid;
        wifiPassword = _pass;
        saveWiFiSettings();

        disp.setTextColor(TFT_GREEN, disp.color565(30, 30, 30));
        disp.println("Connected!");
        disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
        disp.println(WiFi.localIP().toString().c_str());
    } else {
        disp.setTextColor(TFT_RED, disp.color565(30, 30, 30));
        disp.println("Failed.");
    }

    disp.setTextColor(disp.color565(120, 120, 120), disp.color565(30, 30, 30));
    disp.println("ESC to return");
    _state = S_DONE;
}

void AppSetWifi::onUpdate() {
    if (_state == S_DONE) {
        KeyInput ki = pollKeys();
        if (ki.esc) uiManager.returnToLauncher();
        return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;

    uiManager.notifyInteraction();

    if (ki.esc) {
        uiManager.returnToLauncher();
        return;
    }

    auto& disp = M5Cardputer.Display;

    if (ki.enter) {
        disp.println("");
        if (_state == S_SSID) {
            _ssid  = _inputBuf;
            _inputBuf = "";
            _state = S_PASS;
            disp.setTextColor(TFT_ORANGE, disp.color565(30, 30, 30));
            disp.println("WiFi Password:");
            disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
            disp.print(">>> ");
        } else if (_state == S_PASS) {
            _pass  = _inputBuf;
            _inputBuf = "";
            _state = S_CONNECTING;
            _connect();
        }
        return;
    }

    if (ki.del && _inputBuf.length() > 0) {
        _inputBuf.remove(_inputBuf.length() - 1);
        // Erase last char on display
        int cx = disp.getCursorX() - 8;
        int cy = disp.getCursorY();
        if (cx < 0) cx = 0;
        disp.setCursor(cx, cy);
        disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
        disp.print(' ');
        disp.setCursor(cx, cy);
        return;
    }

    if (ki.ch) {
        _inputBuf += ki.ch;
        disp.setTextColor(TFT_WHITE, disp.color565(30, 30, 30));
        disp.print(ki.ch);
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
