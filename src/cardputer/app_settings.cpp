#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_settings.h"
#include "../storage.h"
#include "../connection.h"
#include <WiFi.h>

namespace Cardputer {

static constexpr int  SETTINGS_BG      = 0x1863;
static constexpr int  BAR_TOP_H        = 18;
static constexpr int  BAR_BOT_H        = 14;
static constexpr int  CONTENT_Y        = BAR_TOP_H + 4;

static uint16_t barColor()  { return M5Cardputer.Display.color565(20, 110, 60); }
static uint16_t botColor()  { return M5Cardputer.Display.color565(16, 16, 16); }
static uint16_t labelColor(){ return M5Cardputer.Display.color565(170, 170, 170); }
static uint16_t selBgColor(){ return M5Cardputer.Display.color565(20, 60, 20); }

void AppSettings::_drawTopBar(int pageNum) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = barColor();
    disp.fillRect(0, 0, disp.width(), BAR_TOP_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);

    static const char* pageLabels[NUM_PAGES] = {
        "Connectivity", "WiFi Settings", "API Key", "Device Identity"
    };
    disp.drawString(pageLabels[pageNum], 4, 3);

    char pageStr[8];
    snprintf(pageStr, sizeof(pageStr), "%d/%d", pageNum + 1, NUM_PAGES);
    int pw = disp.textWidth(pageStr);
    disp.drawString(pageStr, disp.width() - pw - 4, 3);
}

void AppSettings::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = botColor();
    disp.fillRect(0, disp.height() - BAR_BOT_H, disp.width(), BAR_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - BAR_BOT_H + 2);
}

void AppSettings::_drawInputField(int x, int y, int w, const String& text, bool active, bool masked) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = disp.color565(50, 50, 50);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);

    int maxChars = (w - 4) / 6;
    String display = text;
    if (masked && display.length() > 0) {
        display = String(display.length(), '*');
    }
    if ((int)display.length() > maxChars) {
        display = display.substring(display.length() - maxChars);
    }
    if (active) display += "_";
    disp.drawString(display, x + 2, y + 3);
}

// ---- Page 0: Connectivity ----

static void _getConnStatus(int row,
                           const char** s1, uint16_t* c1,
                           const char** s2, uint16_t* c2) {
    auto& d = M5Cardputer.Display;
    *s2 = "";  *c2 = 0;
    switch (row) {
        case 0: // Bluetooth
            *s1 = bluetoothEnabled ? "ON" : "OFF";
            *c1 = bluetoothEnabled ? d.color565(80,220,80) : d.color565(200,60,60);
            if (bluetoothEnabled) {
                bool conn = bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
                *s2 = conn ? "connected" : "no peer";
                *c2 = conn ? d.color565(80,220,80) : d.color565(200,160,0);
            }
            break;
        case 1: // USB HID
            *s1 = usbEnabled ? "ON" : "OFF";
            *c1 = usbEnabled ? d.color565(80,220,80) : d.color565(200,60,60);
            break;
        case 2: // WiFi
            *s1 = wifiEnabled ? "ON" : "OFF";
            *c1 = wifiEnabled ? d.color565(80,220,80) : d.color565(200,60,60);
            if (wifiEnabled) {
                bool conn = (WiFi.status() == WL_CONNECTED);
                *s2 = conn ? "connected" : "disconnected";
                *c2 = conn ? d.color565(80,220,80) : d.color565(200,60,60);
            }
            break;
    }
}

void AppSettings::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(0);

    static const char* rowLabels[3] = { "Bluetooth", "USB HID", "WiFi" };

    for (int i = 0; i < 3; i++) {
        int  y   = CONTENT_Y + i * 28;
        bool sel = (i == _toggleSel);
        uint16_t rowBg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;
        if (sel) disp.fillRect(0, y - 2, disp.width(), 24, rowBg);

        // Row label
        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), rowBg);
        char lbl[20];
        snprintf(lbl, sizeof(lbl), "%s%s", sel ? "> " : "  ", rowLabels[i]);
        disp.drawString(lbl, 4, y);

        // Status badges right-aligned on top line
        const char* s1; uint16_t c1;
        const char* s2; uint16_t c2;
        _getConnStatus(i, &s1, &c1, &s2, &c2);

        int rx = disp.width() - 4;
        if (s2 && *s2) {
            int bw = disp.textWidth(s2) + 6;
            rx -= bw;
            disp.fillRoundRect(rx, y, bw, 11, 2, disp.color565(25,25,25));
            disp.setTextColor(c2, disp.color565(25,25,25));
            disp.drawString(s2, rx + 3, y + 1);
            rx -= 4;
        }
        {
            int bw = disp.textWidth(s1) + 8;
            rx -= bw;
            uint16_t bb = (strcmp(s1,"ON")==0) ? disp.color565(20,70,20) : disp.color565(70,20,20);
            disp.fillRoundRect(rx, y - 1, bw, 12, 3, bb);
            disp.setTextColor(c1, bb);
            disp.drawString(s1, rx + 4, y);
        }

        // Action hint on second line of selected row
        if (sel) {
            disp.setTextSize(1);
            disp.setTextColor(disp.color565(120,180,120), rowBg);
            const char* hint = "";
            if (i == 0) {
                bool conn = bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected();
                if (!bluetoothEnabled)  hint = "ENTER enable  C connect";
                else if (conn)          hint = "ENTER disable  C disconnect";
                else                    hint = "ENTER disable  C reconnect";
            } else if (i == 1) {
                hint = "ENTER toggle  (reboot to apply)";
            } else {
                bool conn = (WiFi.status() == WL_CONNECTED);
                if (!wifiEnabled)  hint = "ENTER enable+connect";
                else if (conn)     hint = "ENTER disable  C disconnect";
                else               hint = "ENTER disable  C reconnect";
            }
            disp.drawString(hint, 6, y + 12);
        }
    }

    if (_rebootNote) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(200,160,0), SETTINGS_BG);
        disp.drawString("USB/BT: reboot to apply", 4, CONTENT_Y + 3*28 + 2);
    }

    _drawBottomBar("up/dn  ENTER toggle  C connect/disc  </> page  ESC");
}

void AppSettings::_handlePage0(KeyInput ki) {
    if (ki.arrowLeft) {
        _page = NUM_PAGES - 1; _idSel = 0; _editing = false; _idSaved = false;
        _needsRedraw = true; return;
    }
    if (ki.arrowRight) {
        _page = 1;
        _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
        _needsRedraw = true; return;
    }
    if (ki.arrowUp)   { _toggleSel = (_toggleSel - 1 + 3) % 3; _needsRedraw = true; return; }
    if (ki.arrowDown) { _toggleSel = (_toggleSel + 1) % 3;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_toggleSel) {
            case 0:
                if (bluetoothEnabled) disableBluetooth();
                else                  enableBluetooth();
                _rebootNote = true;
                break;
            case 1:
                usbEnabled = !usbEnabled;
                saveUSBSettings();
                _rebootNote = true;
                break;
            case 2:
                wifiEnabled = !wifiEnabled;
                saveWifiEnabledSettings();
                if (wifiEnabled) {
                    WiFi.mode(WIFI_STA);
                    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                } else {
                    WiFi.disconnect(true);
                    WiFi.mode(WIFI_OFF);
                }
                break;
        }
        _needsRedraw = true; return;
    }

    // C = connect / disconnect
    if (ki.ch == 'c' || ki.ch == 'C') {
        switch (_toggleSel) {
            case 0: // Bluetooth
                if (!bluetoothEnabled) {
                    enableBluetooth();
                } else if (bluetoothInitialized && BLE_KEYBOARD_VALID && BLE_KEYBOARD.isConnected()) {
                    BLE_KEYBOARD.end();
                    delay(300);
                    BLE_KEYBOARD.begin();
                } else if (bluetoothInitialized && BLE_KEYBOARD_VALID) {
                    // re-advertise
                    BLE_KEYBOARD.end();
                    delay(300);
                    BLE_KEYBOARD.begin();
                }
                break;
            case 2: // WiFi
                if (WiFi.status() == WL_CONNECTED) {
                    WiFi.disconnect();
                } else {
                    if (!wifiEnabled) { wifiEnabled = true; saveWifiEnabledSettings(); }
                    WiFi.mode(WIFI_STA);
                    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                }
                break;
        }
        _needsRedraw = true; return;
    }
}


// ---- Page 1: Set WiFi ----

void AppSettings::_drawPage1() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(1);

    int y = CONTENT_Y;
    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Current SSID:", 4, y);

    String maskedSSID = wifiSSID.isEmpty() ? "(none)" : wifiSSID;
    disp.setTextColor(TFT_YELLOW, SETTINGS_BG);
    disp.drawString(maskedSSID, 4 + disp.textWidth("Current SSID: "), y);

    y += 18;

    if (_wifiState == WS_SSID) {
        disp.setTextColor(labelColor(), SETTINGS_BG);
        disp.drawString("New SSID:", 4, y);
        y += 12;
        _drawInputField(4, y, disp.width() - 8, _wifiInputBuf, true);

        _drawBottomBar("type SSID  ENTER confirm  </> page  ESC back");
    } else if (_wifiState == WS_PASS) {
        disp.setTextColor(disp.color565(100, 200, 100), SETTINGS_BG);
        disp.drawString("SSID: " + _newSSID, 4, y);
        y += 18;
        disp.setTextColor(labelColor(), SETTINGS_BG);
        disp.drawString("Password:", 4, y);
        y += 12;
        _drawInputField(4, y, disp.width() - 8, _wifiInputBuf, true, true);

        _drawBottomBar("type pass  ENTER connect  ESC back");
    } else if (_wifiState == WS_CONNECTING) {
        disp.setTextColor(disp.color565(200, 160, 0), SETTINGS_BG);
        disp.drawString("Connecting...", 4, y + 20);
        _drawBottomBar("");
    } else {
        uint16_t statusColor = _wifiSuccess
            ? disp.color565(80, 220, 80)
            : disp.color565(220, 80, 80);
        disp.setTextColor(statusColor, SETTINGS_BG);
        disp.drawString(_wifiStatusMsg, 4, y + 10);
        if (_wifiSuccess) {
            disp.setTextColor(labelColor(), SETTINGS_BG);
            disp.drawString(WiFi.localIP().toString().c_str(), 4, y + 26);
        }
        _drawBottomBar("any key to continue");
    }
}

void AppSettings::_connectWifi() {
    WiFi.disconnect(true);
    delay(200);
    WiFi.setHostname(hostname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_newSSID.c_str(), _wifiInputBuf.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        feedWatchdog();
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiSSID     = _newSSID;
        wifiPassword = _wifiInputBuf;
        wifiEnabled  = true;
        saveWiFiSettings();
        saveWifiEnabledSettings();
        _wifiStatusMsg = "Connected!";
        _wifiSuccess   = true;
    } else {
        _wifiStatusMsg = "Connection failed.";
        _wifiSuccess   = false;
    }
    _wifiState = WS_DONE;
}

void AppSettings::_handlePage1(KeyInput ki) {
    if (_wifiState == WS_DONE) {
        if (ki.anyKey) {
            _wifiState     = WS_SSID;
            _wifiInputBuf  = "";
            _newSSID       = "";
            _wifiStatusMsg = "";
        }
        _needsRedraw = true;
        return;
    }
    if (_wifiState == WS_CONNECTING) return;

    // Left/right navigate pages only when not mid-entry
    if (ki.arrowLeft && _wifiState == WS_SSID && _wifiInputBuf.length() == 0) {
        _page = 0; _needsRedraw = true; return;
    }
    if (ki.arrowRight && _wifiState == WS_SSID && _wifiInputBuf.length() == 0) {
        _page = 2; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; return;
    }

    if (ki.enter) {
        if (_wifiState == WS_SSID) {
            if (_wifiInputBuf.length() > 0) {
                _newSSID      = _wifiInputBuf;
                _wifiInputBuf = "";
                _wifiState    = WS_PASS;
            }
        } else if (_wifiState == WS_PASS) {
            _wifiState = WS_CONNECTING;
            _needsRedraw = true;
            _drawPage1();
            _connectWifi();
        }
        _needsRedraw = true;
        return;
    }

    if (ki.del && _wifiInputBuf.length() > 0) {
        _wifiInputBuf.remove(_wifiInputBuf.length() - 1);
        _needsRedraw = true;
        return;
    }

    if (ki.ch) {
        _wifiInputBuf += ki.ch;
        _needsRedraw = true;
    }
}

// ---- Page 2: API Key ----

void AppSettings::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(2);

    int y = CONTENT_Y;
    int fieldW = disp.width() - 8;

    disp.setTextSize(1);
    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("Current key:", 4, y);
    y += 12;

    String masked;
    if (apiKey.length() <= 4) {
        masked = apiKey;
    } else {
        masked = apiKey.substring(0, 2);
        for (size_t i = 2; i < apiKey.length() - 2; i++) masked += '*';
        masked += apiKey.substring(apiKey.length() - 2);
    }
    _drawInputField(4, y, fieldW, masked, false);
    y += 20;

    disp.setTextColor(labelColor(), SETTINGS_BG);
    disp.drawString("New API Key:", 4, y);
    y += 12;
    _drawInputField(4, y, fieldW, _editBuf, _editing);

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, y + 22);
    }

    if (_editing) {
        _drawBottomBar("type  ENTER save  ESC cancel");
    } else {
        _drawBottomBar("ENTER edit  </> page  ESC back");
    }
}

void AppSettings::_handlePage2(KeyInput ki) {
    if (_editing) {
        if (ki.esc) {
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.enter) {
            if (_editBuf.length() > 0) { apiKey = _editBuf; saveApiKeySettings(); _idSaved = true; }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft)  { _page = 1; _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = ""; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 3; _idSel = 0; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) { _editBuf = ""; _editing = true; _idSaved = false; _needsRedraw = true; }
}

// ---- Page 3: Device Identity ----

void AppSettings::_drawPage3() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(3);

    static const char* labels[2] = { "USB Manufacturer", "USB/BT Product Name" };
    String* vals[2] = { &usbManufacturer, &usbProduct };

    int fieldW = disp.width() - 8;

    for (int i = 0; i < 2; i++) {
        int y    = CONTENT_Y + i * 36;
        bool sel  = (i == _idSel);
        bool edit = (sel && _editing);

        if (sel && !edit) disp.fillRect(0, y - 1, disp.width(), 12, selBgColor());

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(),
                          sel && !edit ? selBgColor() : (uint16_t)SETTINGS_BG);
        char row[36];
        snprintf(row, sizeof(row), "%s%s:", sel ? "> " : "  ", labels[i]);
        disp.drawString(row, 4, y);

        int fieldY = y + 13;
        if (edit) {
            _drawInputField(4, fieldY, fieldW, _editBuf, true);
        } else {
            _drawInputField(4, fieldY, fieldW, *vals[i], false);
        }
    }

    // BT name note
    int noteY = CONTENT_Y + 2 * 36 + 2;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(160, 130, 40), SETTINGS_BG);
    disp.drawString("* BT name = USB Product; reboot to apply", 4, noteY);

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved! Reboot to apply.", 4, noteY + 12);
    }

    if (_editing) {
        _drawBottomBar("type  ENTER save  ESC cancel");
    } else {
        _drawBottomBar("up/dn item  </> page  ENT edit  ESC back");
    }
}

void AppSettings::_handlePage3(KeyInput ki) {
    if (_editing) {
        if (ki.esc) {
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.enter) {
            if (_editBuf.length() > 0) {
                if (_idSel == 0) { usbManufacturer = _editBuf; }
                else             { usbProduct      = _editBuf; }
                saveUSBIdentitySettings();
                _idSaved = true;
            }
            _editing = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.ch) { _editBuf += ki.ch; _idSaved = false; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft)  { _page = 2; _editing = false; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowRight) { _page = 0; _needsRedraw = true; }  // wrap to first page
    else if (ki.arrowUp)   { _idSel = (_idSel - 1 + 2) % 2; _idSaved = false; _needsRedraw = true; }
    else if (ki.arrowDown) { _idSel = (_idSel + 1) % 2;      _idSaved = false; _needsRedraw = true; }
    else if (ki.enter) {
        String* vals[2] = { &usbManufacturer, &usbProduct };
        _editBuf = *vals[_idSel]; _editing = true; _idSaved = false; _needsRedraw = true;
    }
}

// ---- AppBase overrides ----

void AppSettings::onEnter() {
    _page         = 0;
    _toggleSel    = 0;
    _rebootNote   = false;
    _idSel        = 0;
    _editing      = false;
    _idSaved      = false;
    _editBuf      = "";
    _wifiState    = WS_SSID;
    _wifiInputBuf = "";
    _newSSID      = "";
    _wifiStatusMsg = "";
    _needsRedraw  = true;
}

void AppSettings::onExit() {
    _editing = false;
    _editBuf = "";
}

void AppSettings::onUpdate() {
    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _page = (_page + 1) % NUM_PAGES;
        _editing = false; _editBuf = ""; _idSaved = false;
        if (_page == 1) { _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = ""; }
        _needsRedraw = true;
        return;
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); break;
            case 1: _drawPage1(); break;
            case 2: _drawPage2(); break;
            case 3: _drawPage3(); break;
        }
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) {
        if (_editing) {
            _editing = false; _editBuf = ""; _needsRedraw = true;
        } else if (_page == 0) {
            uiManager.returnToLauncher();
        } else {
            _page--; _needsRedraw = true;
        }
        return;
    }

    switch (_page) {
        case 0: _handlePage0(ki); break;
        case 1: _handlePage1(ki); break;
        case 2: _handlePage2(ki); break;
        case 3: _handlePage3(ki); break;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
