#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_settings.h"
#include "../storage.h"
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
    disp.drawString("Settings", 4, 3);

    char pageStr[8];
    snprintf(pageStr, sizeof(pageStr), "%d/3", pageNum + 1);
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

// ---- Page 0: Connectivity toggles ----

void AppSettings::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(0);

    static const char* labels[3] = { "Bluetooth", "USB HID", "WiFi" };
    bool states[3] = { bluetoothEnabled, usbEnabled, wifiEnabled };

    for (int i = 0; i < 3; i++) {
        int y = CONTENT_Y + i * 22;
        bool sel = (i == _toggleSel);

        if (sel) {
            disp.fillRect(0, y - 2, disp.width(), 18, selBgColor());
        }

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), sel ? selBgColor() : SETTINGS_BG);
        char row[32];
        snprintf(row, sizeof(row), "%s%s", sel ? "> " : "  ", labels[i]);
        disp.drawString(row, 4, y);

        const char* stateStr = states[i] ? "ON " : "OFF";
        uint16_t stateColor = states[i]
            ? disp.color565(80, 220, 80)
            : disp.color565(200, 60, 60);
        uint16_t rowBg = sel ? selBgColor() : (uint16_t)SETTINGS_BG;

        disp.fillRect(disp.width() - 36, y - 1, 34, 13, rowBg);
        disp.fillRoundRect(disp.width() - 36, y - 1, 34, 12, 3,
                           states[i] ? disp.color565(30, 80, 30) : disp.color565(80, 30, 30));
        disp.setTextColor(stateColor, states[i] ? disp.color565(30, 80, 30) : disp.color565(80, 30, 30));
        disp.drawString(stateStr, disp.width() - 34, y);
    }

    if (_rebootNote) {
        int y = CONTENT_Y + 3 * 22 + 4;
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(200, 160, 0), SETTINGS_BG);
        disp.drawString("BT/USB: reboot to apply", 4, y);
    }

    _drawBottomBar("up/dn item  </> page  ENT toggle  ESC back");
}

void AppSettings::_handlePage0(KeyInput ki) {
    if (ki.arrowLeft) {
        // no previous page, wrap to last
        _page = 2;
        _idSel = 0; _editing = false; _idSaved = false;
        _needsRedraw = true;
    } else if (ki.arrowRight) {
        _page = 1;
        _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
        _needsRedraw = true;
    } else if (ki.arrowUp) {
        _toggleSel = (_toggleSel - 1 + 3) % 3;
        _needsRedraw = true;
    } else if (ki.arrowDown) {
        _toggleSel = (_toggleSel + 1) % 3;
        _needsRedraw = true;
    } else if (ki.enter) {
        switch (_toggleSel) {
            case 0:
                bluetoothEnabled = !bluetoothEnabled;
                saveBtSettings();
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
        _needsRedraw = true;
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

// ---- Page 2: Identifiers ----

void AppSettings::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SETTINGS_BG);
    _drawTopBar(2);

    static const char* fieldLabels[3] = { "API Key", "USB Manufacturer", "USB Product" };
    String* fieldValues[3] = { &apiKey, &usbManufacturer, &usbProduct };

    int fieldW = disp.width() - 8;

    for (int i = 0; i < 3; i++) {
        int y = CONTENT_Y + i * 32;
        bool sel  = (i == _idSel);
        bool edit = (sel && _editing);

        if (sel && !edit) {
            disp.fillRect(0, y - 1, disp.width(), 12, selBgColor());
        }

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : labelColor(), sel && !edit ? selBgColor() : (uint16_t)SETTINGS_BG);
        char row[32];
        snprintf(row, sizeof(row), "%s%s:", sel ? "> " : "  ", fieldLabels[i]);
        disp.drawString(row, 4, y);

        int fieldY = y + 13;
        if (edit) {
            _drawInputField(4, fieldY, fieldW, _editBuf, true,
                            i == 0 && _editBuf.length() > 4);
        } else {
            // Show masked value
            String val = *fieldValues[i];
            if (i == 0 && val.length() > 4) {
                val = val.substring(0, 2) + String(val.length() - 4, '*') + val.substring(val.length() - 2);
            }
            _drawInputField(4, fieldY, fieldW, val, false);
        }
    }

    if (_idSaved) {
        disp.setTextColor(TFT_GREEN, SETTINGS_BG);
        disp.drawString("Saved!", 4, CONTENT_Y + 3 * 32);
    }

    if (_editing) {
        _drawBottomBar("type  ENTER save  ESC cancel");
    } else {
        _drawBottomBar("up/dn item  </> page  ENT edit  ESC back");
    }
}

void AppSettings::_handlePage2(KeyInput ki) {
    if (_editing) {
        if (ki.esc) {
            _editing     = false;
            _editBuf     = "";
            _needsRedraw = true;
            return;
        }
        if (ki.enter) {
            if (_editBuf.length() > 0) {
                switch (_idSel) {
                    case 0: apiKey          = _editBuf; saveApiKeySettings();         break;
                    case 1: usbManufacturer = _editBuf; saveUSBIdentitySettings();    break;
                    case 2: usbProduct      = _editBuf; saveUSBIdentitySettings();    break;
                }
                _idSaved = true;
            }
            _editing     = false;
            _editBuf     = "";
            _needsRedraw = true;
            return;
        }
        if (ki.del && _editBuf.length() > 0) {
            _editBuf.remove(_editBuf.length() - 1);
            _needsRedraw = true;
            return;
        }
        if (ki.ch) {
            _editBuf    += ki.ch;
            _idSaved     = false;
            _needsRedraw = true;
        }
        return;
    }

    if (ki.arrowLeft) {
        _page = 1;
        _wifiState = WS_SSID; _wifiInputBuf = ""; _newSSID = ""; _wifiStatusMsg = "";
        _needsRedraw = true;
    } else if (ki.arrowRight) {
        _page = 0;  // wrap to first page
        _needsRedraw = true;
    } else if (ki.arrowUp) {
        _idSel       = (_idSel - 1 + 3) % 3;
        _idSaved     = false;
        _needsRedraw = true;
    } else if (ki.arrowDown) {
        _idSel       = (_idSel + 1) % 3;
        _idSaved     = false;
        _needsRedraw = true;
    } else if (ki.enter) {
        String* vals[3] = { &apiKey, &usbManufacturer, &usbProduct };
        _editBuf     = *vals[_idSel];
        _editing     = true;
        _idSaved     = false;
        _needsRedraw = true;
    }
}

// ---- AppBase overrides ----

void AppSettings::onEnter() {
    _page        = 0;
    _toggleSel   = 0;
    _rebootNote  = false;
    _idSel       = 0;
    _editing     = false;
    _idSaved     = false;
    _wifiState   = WS_SSID;
    _wifiInputBuf = "";
    _newSSID      = "";
    _wifiStatusMsg = "";
    _needsRedraw = true;
}

void AppSettings::onExit() {
    _editing = false;
    _editBuf = "";
}

void AppSettings::onUpdate() {
    // BtnA (physical side button) advances to the next page — checked before
    // pollKeys() so it's completely independent of keyboard alias conflicts.
    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (_page < 2) {
            _page++;
            // Reset sub-state when entering a page fresh
            if (_page == 1) {
                _wifiState     = WS_SSID;
                _wifiInputBuf  = "";
                _newSSID       = "";
                _wifiStatusMsg = "";
            } else if (_page == 2) {
                _idSel   = 0;
                _editing = false;
                _idSaved = false;
            }
        } else {
            _page = 0;  // wrap back to first page
        }
        _needsRedraw = true;
        return;
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); break;
            case 1: _drawPage1(); break;
            case 2: _drawPage2(); break;
        }
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc) {
        if (_page == 0) {
            uiManager.returnToLauncher();
        } else {
            _page--;
            _needsRedraw = true;
        }
        return;
    }

    switch (_page) {
        case 0: _handlePage0(ki); break;
        case 1: _handlePage1(ki); break;
        case 2: _handlePage2(ki); break;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
