#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "ui_manager.h"

namespace Cardputer {

UIManager uiManager;

// ---- Key input ----
// Called after M5Cardputer.update() has already been invoked this frame.

KeyInput pollKeys() {
    KeyInput ki;

    if (!M5Cardputer.Keyboard.isChange()) return ki;
    if (!M5Cardputer.Keyboard.isPressed()) return ki;

    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    ki.anyKey = true;
    ki.fn     = ks.fn;
    ki.enter  = ks.enter;
    ki.del    = ks.del;
    ki.tab    = ks.tab;

    // fn + tab or tab alone = ESC for UI navigation.
    // On the Cardputer ADV keyboard, the physical ESC key may map to ks.tab.
    if (ks.tab) {
        ki.esc = true;
        return ki;
    }

    for (uint8_t hk : ks.hid_keys) {
        switch (hk) {
            case 0x29: ki.esc       = true; break; // HID ESC
            case 0x52: ki.arrowUp   = true; break;
            case 0x51: ki.arrowDown = true; break;
            case 0x50: ki.arrowLeft = true; break;
            case 0x4F: ki.arrowRight= true; break;
        }
    }

    for (char c : ks.word) {
        if (c == 0x1B) { ki.esc = true; continue; }

        if (ks.fn) {
            switch (c) {
                case ';': ki.arrowUp    = true; continue;
                case '.': ki.arrowDown  = true; continue;
                case ',': ki.arrowLeft  = true; continue;
                case '/': ki.arrowRight = true; continue;
                case '`': ki.esc        = true; continue;
            }
        }

        // Navigation aliases (no fn required)
        switch (c) {
            case ',': ki.arrowLeft  = true; continue;
            case '.': ki.arrowDown  = true; continue;
            case ';': ki.arrowUp    = true; continue;
            case '/': ki.arrowRight = true; continue;
        }

        if (c >= 0x20 && c < 0x7F) ki.ch = c;
    }

    return ki;
}

// ---- UIManager ----

UIManager::UIManager() {}

void UIManager::addApp(AppBase* app) {
    _apps.push_back(app);
}

void UIManager::launchApp(int index) {
    if (index < 0 || index >= (int)_apps.size()) return;
    if (_currentApp < (int)_apps.size()) _apps[_currentApp]->onExit();
    _currentApp = index;
    wakeScreen();
    _apps[_currentApp]->onEnter();
}

void UIManager::returnToLauncher() {
    launchApp(0);
}

void UIManager::wakeScreen() {
    _lastInteraction = millis();
    if (!_screenOn) {
        M5Cardputer.Display.setBrightness(128);
        _screenOn        = true;
        _needsFullRedraw = true;
    }
}

void UIManager::notifyInteraction() {
    _lastInteraction = millis();
    if (!_screenOn) {
        M5Cardputer.Display.setBrightness(128);
        _screenOn        = true;
        _needsFullRedraw = true;
    }
}

void UIManager::update() {
    // Must be first - updates BtnA edge state and Keyboard state
    M5Cardputer.update();

    unsigned long now = millis();

    // Wake on BTNG0 or any keyboard key held down
    if (M5Cardputer.BtnA.isPressed() || M5Cardputer.Keyboard.isPressed()) {
        notifyInteraction();
    }

    // Screen timeout
    if (_screenOn && (now - _lastInteraction > SCREEN_TIMEOUT_MS)) {
        M5Cardputer.Display.setBrightness(0);
        _screenOn = false;
    }

    if (!_screenOn) return;

    if (_needsFullRedraw) {
        _needsFullRedraw = false;
        if (_currentApp < (int)_apps.size()) _apps[_currentApp]->onEnter();
    }

    if (_currentApp < (int)_apps.size()) _apps[_currentApp]->onUpdate();
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
