#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "ui_manager.h"
#include <vector>

namespace Cardputer {

UIManager uiManager;

// ---- Key input ----
// Called after M5Cardputer.update() has already been invoked this frame.

// Repeat state
static KeyInput  s_lastKey;
static uint32_t  s_repeatDeadline = 0;

KeyInput pollKeys(bool editMode) {
    KeyInput ki;
    bool pressed = M5Cardputer.Keyboard.isPressed();
    bool changed = M5Cardputer.Keyboard.isChange();

    if (!pressed) {
        s_lastKey = KeyInput{};
        return ki;
    }

    // New keypress event — always parse fresh, reset repeat timer
    if (changed) {
        Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
        ki.anyKey = true;
        ki.fn     = ks.fn;
        ki.enter  = ks.enter;
        ki.del    = ks.del;
        ki.tab    = ks.tab;

        if (ks.fn && ks.enter) {
            ki.nextPage = true;
            s_lastKey = ki;
            s_repeatDeadline = millis() + KEY_REPEAT_INITIAL_MS;
            return ki;
        }

        for (uint8_t hk : ks.hid_keys) {
            switch (hk) {
                case 0x29: ki.esc        = true; break;
                case 0x52: ki.arrowUp    = true; break;
                case 0x51: ki.arrowDown  = true; break;
                case 0x50: ki.arrowLeft  = true; break;
                case 0x4F: ki.arrowRight = true; break;
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

            if (!editMode) {
                switch (c) {
                    case ',': ki.arrowLeft  = true; continue;
                    case '.': ki.arrowDown  = true; continue;
                    case ';': ki.arrowUp    = true; continue;
                    case '/': ki.arrowRight = true; continue;
                    case '`': ki.esc        = true; continue;
                }
            }

            if (c >= 0x20 && c < 0x7F) ki.ch = c;
        }

        s_lastKey = ki;
        s_repeatDeadline = millis() + KEY_REPEAT_INITIAL_MS;
        return ki;
    }

    // Key held, no new event — fire repeat if deadline reached
    if (s_lastKey.anyKey) {
        uint32_t now = millis();
        if (now >= s_repeatDeadline) {
            ki = s_lastKey;
            ki.isRepeat = true;
            s_repeatDeadline = now + KEY_REPEAT_RATE_MS;
            return ki;
        }
    }

    return ki;
}

void drawTabHint(int afterX) {
    auto& disp = M5Cardputer.Display;
    uint16_t bg = disp.color565(25, 40, 80);
    uint16_t tc = disp.color565(100, 150, 230);
    disp.setTextSize(1);
    const char* label = "Tab help";
    int bw = disp.textWidth(label) + 6;
    int bx = afterX + 4;
    disp.fillRoundRect(bx, 3, bw, 11, 2, bg);
    disp.setTextColor(tc, bg);
    disp.drawString(label, bx + 3, 4);
}


// ---- UIManager ----

UIManager::UIManager() {}

void UIManager::addApp(AppBase* app) {
    _apps.push_back(app);
}

std::vector<int> UIManager::visibleApps() const {
    std::vector<int> result;
    int numApps = (int)_apps.size() - 1; // exclude launcher at 0

    if (appOrder.empty()) {
        for (int i = 1; i <= numApps; i++) result.push_back(i);
        return result;
    }

    // Use the saved order. Any app index not present in appOrder
    // (e.g. newly added apps) is appended at the end, visible by default.
    for (size_t i = 0; i < appOrder.size(); i++) {
        int idx = appOrder[i];
        if (idx >= 1 && idx < (int)_apps.size()) {
            bool hidden = (i < appHidden.size()) && appHidden[i];
            if (!hidden) result.push_back(idx);
        }
    }
    // Append any app indices not present in appOrder
    for (int i = 1; i <= numApps; i++) {
        bool found = false;
        for (int v : appOrder) { if (v == i) { found = true; break; } }
        if (!found) result.push_back(i);
    }
    return result;
}

void UIManager::launchApp(int index) {
    if (index < 0 || index >= (int)_apps.size()) return;
    if (_currentApp < (int)_apps.size()) _apps[_currentApp]->onExit();
    _currentApp = index;
    // Clear repeat state so a held key from the previous app doesn't fire in the new one
    extern KeyInput s_lastKey;
    extern uint32_t s_repeatDeadline;
    s_lastKey        = KeyInput{};
    s_repeatDeadline = 0;
    wakeScreen();
    _apps[_currentApp]->onEnter();
}

void UIManager::returnToLauncher() {
    launchApp(0);
}

void UIManager::wakeScreen() {
    _lastInteraction = millis();
    if (!_screenOn) {
        M5Cardputer.Display.setBrightness((uint8_t)g_displayBrightness);
        _screenOn        = true;
        _needsFullRedraw = true;
    }
}

void UIManager::notifyInteraction() {
    _lastInteraction = millis();
    if (!_screenOn) {
        M5Cardputer.Display.setBrightness((uint8_t)g_displayBrightness);
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
    if (_screenOn && g_screenTimeoutMs > 0 && (now - _lastInteraction > g_screenTimeoutMs)) {
        M5Cardputer.Display.setBrightness(0);
        _screenOn = false;
    }

    if (!_screenOn) return;

    if (_needsFullRedraw) {
        _needsFullRedraw = false;
        // Re-render the current app without reinitialising it — calling onEnter()
        // would wipe all in-progress input state (text buffers, cursor, etc.)
        if (_currentApp < (int)_apps.size()) {
            _apps[_currentApp]->requestRedraw();
        }
    }

    if (g_needsDisplayRedraw) {
        g_needsDisplayRedraw = false;
        if (_currentApp < (int)_apps.size())
            _apps[_currentApp]->requestRedraw();
    }

    // Global BtnA: play current register from any app that doesn't handle it itself
    if (M5Cardputer.BtnA.wasPressed()) {
        AppBase* app = _apps[_currentApp];
        if (!app->handlesGlobalBtnA() && !registers.empty()) {
            pendingTokenStrings.push_back(registers[activeRegister]);
            notifyInteraction();
        }
    }

    // TAB: toggle help overlay
    if (M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();
        if (ks.tab) {
            _showHelp = !_showHelp;
            _helpScroll = 0;
            _helpNeedsRedraw = _showHelp;
            notifyInteraction();
            if (!_showHelp) _apps[_currentApp]->requestRedraw();
            while (M5Cardputer.Keyboard.isPressed()) { delay(20); M5Cardputer.update(); }
            return;
        }
    }

    if (_showHelp) {
        bool helpRedraw = _helpNeedsRedraw;
        if (M5Cardputer.Keyboard.isPressed()) {
            auto ks = M5Cardputer.Keyboard.keysState();
            bool consumed = false;
            for (char ch : ks.word) {
                if (ch == ';') { if (_helpScroll > 0) { _helpScroll--; helpRedraw = true; } consumed = true; }
                if (ch == '.') { _helpScroll++; helpRedraw = true; consumed = true; }
            }
            if (ks.fn) {
                // navigation only
            } else if (!consumed) {
                _showHelp = false;
                notifyInteraction();
                _apps[_currentApp]->requestRedraw();
                while (M5Cardputer.Keyboard.isPressed()) { delay(20); M5Cardputer.update(); }
                return;
            }
            while (M5Cardputer.Keyboard.isPressed()) { delay(20); M5Cardputer.update(); }
        }
        if (helpRedraw) {
            _helpNeedsRedraw = false;
            _apps[_currentApp]->requestRedraw();
            _apps[_currentApp]->onUpdate();
            _drawHelpOverlay(_apps[_currentApp]);
        }
        return;
    }

    if (_currentApp < (int)_apps.size()) _apps[_currentApp]->onUpdate();
}

void UIManager::_drawHelpOverlay(AppBase* app) {
    auto& disp = M5Cardputer.Display;
    int pw = disp.width();
    int ph = disp.height();

    int margin = 8;
    int pw2 = pw - margin * 2;
    int ph2 = ph - margin * 2;
    uint16_t bg     = disp.color565(10, 20, 40);
    uint16_t border = disp.color565(60, 120, 200);
    uint16_t titleC = disp.color565(100, 180, 255);
    uint16_t bodyC  = disp.color565(200, 210, 220);
    uint16_t dimC   = disp.color565(80, 90, 100);
    uint16_t scrollC= disp.color565(60, 120, 200);

    disp.fillRoundRect(margin, margin, pw2, ph2, 5, bg);
    disp.drawRoundRect(margin, margin, pw2, ph2, 5, border);
    disp.setTextSize(1);

    // Title row
    int ty = margin + 5;
    disp.setTextColor(titleC, bg);
    disp.drawString(String(app->appName()) + " Help", margin + 6, ty);
    const char* tbadge = "TAB";
    int tbw = disp.textWidth(tbadge) + 6;
    disp.fillRoundRect(pw - margin - tbw - 2, ty - 1, tbw, 12, 3, disp.color565(30,50,90));
    disp.setTextColor(disp.color565(100,150,255), disp.color565(30,50,90));
    disp.drawString(tbadge, pw - margin - tbw + 1, ty);

    ty += 14;
    disp.drawFastHLine(margin + 4, ty, pw2 - 8, border);
    ty += 4;

    // Build all lines from help text (word-wrap + \n splits)
    int maxChars = (pw2 - 14) / 6;
    String txt = String(app->appHelp());
    std::vector<String> lines;
    while (txt.length() > 0) {
        // Find next newline
        int nl = txt.indexOf('\n');
        String seg = (nl >= 0) ? txt.substring(0, nl) : txt;
        txt = (nl >= 0) ? txt.substring(nl + 1) : String("");
        // Word-wrap the segment
        while (seg.length() > 0) {
            if ((int)seg.length() <= maxChars) {
                lines.push_back(seg); seg = "";
            } else {
                int brk = maxChars;
                while (brk > 0 && seg[brk] != ' ') brk--;
                if (brk == 0) brk = maxChars;
                lines.push_back(seg.substring(0, brk));
                seg = seg.substring(brk + 1);
            }
        }
        if (nl >= 0) lines.push_back(""); // blank line after paragraph break
    }

    // Content area: from ty to (ph - margin - 14) for hint bar
    int contentH = ph - margin - 14 - ty;
    int lineH    = 12;
    int visLines = contentH / lineH;

    // Clamp scroll
    int maxScroll = max(0, (int)lines.size() - visLines);
    if (_helpScroll > maxScroll) _helpScroll = maxScroll;
    if (_helpScroll < 0) _helpScroll = 0;

    // Draw visible lines
    disp.setTextColor(bodyC, bg);
    for (int i = 0; i < visLines && (_helpScroll + i) < (int)lines.size(); i++) {
        disp.drawString(lines[_helpScroll + i], margin + 6, ty + i * lineH);
    }

    // Scroll indicators
    if (_helpScroll > 0) {
        disp.setTextColor(scrollC, bg);
        disp.drawString("^", pw - margin - 10, ty);
    }
    if (_helpScroll < maxScroll) {
        disp.setTextColor(scrollC, bg);
        disp.drawString("v", pw - margin - 10, ph - margin - 20);
    }

    // Hint bar
    String hint = maxScroll > 0 ? "fn+;/. scroll  any key close" : "any key to close";
    disp.setTextColor(dimC, bg);
    disp.drawString(hint, margin + 6, ph - margin - 11);
}
} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
