#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_kprox.h"
#include "../registers.h"
#include "../hid.h"
#include "../config.h"
#include "../credential_store.h"
#include <WiFi.h>

namespace Cardputer {

static constexpr int BG       = 0x1863; // ~#181818
static constexpr int HDR_BG   = 0x3297; // ~#326EB8
static constexpr int FTR_BG   = 0x10A2; // #101010

void AppKProx::onEnter() {
    _needsRedraw      = true;
    _numberBuf        = "";
    _buttonPressCount = 0;
    _haltTriggered    = false;
    _drawScreen();
}

void AppKProx::onExit() {
    _numberBuf = "";
}

// Truncate text to fit within maxChars, appending "..." if cut
static String truncate(const String& s, int maxChars) {
    if (s.length() <= (size_t)maxChars) return s;
    return s.substring(0, maxChars - 3) + "...";
}

void AppKProx::_drawScreen() {
    auto& disp = M5Cardputer.Display;
    const int W    = disp.width();   // 240
    const int H    = disp.height();  // 135
    const int HDR  = 18;
    const int FTR  = 14;
    const int LH   = 16;             // line height at textSize 1
    const int BODY_TOP = HDR + 2;

    disp.fillScreen(BG);

    // ---- Header ----
    disp.fillRect(0, 0, W, HDR, HDR_BG);
    disp.setTextSize(1);

    // Left: app name + halted state
    disp.setTextColor(TFT_WHITE, HDR_BG);
    if (isHalted) {
        disp.drawString("KProx [HALTED]", 4, 3);
    } else {
        disp.drawString("KProx", 4, 3);
    }
    drawTabHint(4 + disp.textWidth("KProx") + 3);

    // Right: credential store lock badge
    {
        bool csLocked = credStoreLocked;
        const char* csStr = csLocked ? "CS:LOCKED" : "CS:UNLOCKED";
        uint16_t csBg = csLocked
            ? disp.color565(140, 30, 30)
            : disp.color565(20, 100, 20);
        int csW = disp.textWidth(csStr) + 8;
        disp.fillRoundRect(W - csW - 2, 2, csW, 14, 3, csBg);
        disp.setTextColor(TFT_WHITE, csBg);
        disp.drawString(csStr, W - csW + 2, 4);
    }

    // ---- Body ----
    int y = BODY_TOP;

    // Reg ID + name on one line — display as 1-indexed
    disp.setTextColor(TFT_YELLOW, BG);
    String regLine = "Reg " + String(activeRegister + 1);
    if (!registers.empty()) regLine += "/" + String((int)registers.size());
    if (!registerNames.empty() && activeRegister < (int)registerNames.size()
        && registerNames[activeRegister].length() > 0) {
        regLine += "  " + registerNames[activeRegister];
    }
    disp.drawString(truncate(regLine, 32), 4, y);
    y += LH;

    // Register content preview — max 2 display lines (~30 chars each at size 1)
    if (!registers.empty() && activeRegister < (int)registers.size()) {
        String content = registers[activeRegister];
        // Strip token braces for readability (first pass only)
        const int maxPerLine = 29;
        disp.setTextColor(disp.color565(180, 180, 180), BG);
        if (content.length() <= (size_t)maxPerLine) {
            disp.drawString(content, 4, y);
            y += LH;
        } else {
            // Line 1: first maxPerLine chars
            String line1 = content.substring(0, maxPerLine);
            disp.drawString(line1, 4, y);
            y += LH;
            // Line 2: next maxPerLine chars + ellipsis if more remains
            String rest = content.substring(maxPerLine);
            if (rest.length() > (size_t)maxPerLine) {
                disp.drawString(rest.substring(0, maxPerLine - 3) + "...", 4, y);
            } else {
                disp.drawString(rest, 4, y);
            }
            y += LH;
        }
    }

    // IP address
    uint16_t labelCol = disp.color565(130, 130, 130);
    uint16_t valueCol = disp.color565(100, 255, 100);
    if (WiFi.status() == WL_CONNECTED) {
        disp.setTextColor(labelCol, BG);
        disp.drawString("IP  ", 4, y);
        disp.setTextColor(valueCol, BG);
        disp.drawString(WiFi.localIP().toString(), 4 + disp.textWidth("IP  "), y);
    } else {
        disp.setTextColor(disp.color565(220, 100, 60), BG);
        disp.drawString("IP   WiFi not connected", 4, y);
    }
    y += LH;

    // SSID
    if (WiFi.status() == WL_CONNECTED) {
        disp.setTextColor(labelCol, BG);
        disp.drawString("SSID ", 4, y);
        disp.setTextColor(valueCol, BG);
        disp.drawString(truncate(WiFi.SSID(), 22), 4 + disp.textWidth("SSID "), y);
        y += LH;
    }

    // Hostname
    disp.setTextColor(labelCol, BG);
    disp.drawString("HOST ", 4, y);
    disp.setTextColor(disp.color565(100, 200, 255), BG);
    disp.drawString(String(hostname) + ".local", 4 + disp.textWidth("HOST "), y);
    y += LH;

    // Number input buffer
    if (_numberBuf.length() > 0) {
        disp.setTextColor(TFT_ORANGE, BG);
        disp.drawString("Goto: " + _numberBuf, 4, y);
    }

    // ---- Footer ----
    disp.fillRect(0, H - FTR, W, FTR, FTR_BG);
    disp.setTextColor(disp.color565(110, 110, 110), FTR_BG);
    disp.drawString("< > reg  ENTER play  <n>+ENTER goto  ESC menu", 2, H - FTR + 1);

    _needsRedraw = false;
}

void AppKProx::_checkHomeButton() {
    unsigned long now = millis();
    auto& btn = M5Cardputer.BtnA;

    if (btn.wasPressed()) {
        _lastButtonPress = now;
        _haltTriggered   = false;
        uiManager.notifyInteraction();
    }

    // If BtnA triggered a parser halt, its wasReleased fires on the next frame
    // inside this function — skip it so it doesn't count as a play intent.
    if (g_btnAHaltedPlayback) {
        _skipNextRelease     = true;
        g_btnAHaltedPlayback = false;
    }

    if (btn.wasReleased()) {
        if (_skipNextRelease) {
            _skipNextRelease = false;
        } else {
            _lastButtonRelease = now;
            if (!_haltTriggered) _buttonPressCount++;
        }
    }

    // Long-press → halt / resume
    if (btn.isPressed() && !_haltTriggered && (now - _lastButtonPress >= 2000)) {
        _haltTriggered    = true;
        _buttonPressCount = 0;
        isHalted ? resumeOperations() : haltAllOperations();
        _needsRedraw = true;
    }

    // Dispatch after double-click window expires
    if (_buttonPressCount > 0 && (now - _lastButtonRelease > DOUBLE_CLICK_MS)) {
        if (_buttonPressCount == 1) {
            if (!registers.empty() && !registers[activeRegister].isEmpty()) {
                pendingTokenStrings.push_back(registers[activeRegister]);
            }
        } else {
            if (!registers.empty()) {
                activeRegister = (activeRegister + 1) % (int)registers.size();
                saveActiveRegister();
            }
        }
        _buttonPressCount = 0;
        _needsRedraw = true;
    }
}

void AppKProx::onUpdate() {
    _checkHomeButton();

    // Rebuild state snapshot; only redraw if something actually changed
    String newIP      = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "not connected";
    String newSSID    = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "";
    String newNetLine = newIP + newSSID;
    String newRegContent  = registers.empty() ? "" : registers[activeRegister];
    String newRegLine     = "Reg " + String(activeRegister + 1);
    if (!registers.empty()) newRegLine += "/" + String((int)registers.size());
    if (!registerNames.empty() && activeRegister < (int)registerNames.size()
        && registerNames[activeRegister].length() > 0) {
        newRegLine += "  " + registerNames[activeRegister];
    }
    bool haltedNow    = isHalted;
    bool credLocked   = credStoreLocked;

    if (_needsRedraw
        || newNetLine    != _lastNetLine
        || newRegContent != _lastRegContent
        || newRegLine    != _lastRegLine
        || haltedNow     != _lastHalted
        || credLocked    != _lastCredLocked
        || _numberBuf    != _lastNumberBuf) {
        _lastNetLine    = newNetLine;
        _lastRegContent = newRegContent;
        _lastRegLine    = newRegLine;
        _lastHalted     = haltedNow;
        _lastCredLocked = credLocked;
        _lastNumberBuf  = _numberBuf;
        _drawScreen();
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;

    uiManager.notifyInteraction();

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    if (ki.arrowLeft || ki.arrowUp) {
        if (!registers.empty()) {
            int sz = (int)registers.size();
            activeRegister = (activeRegister - 1 + sz) % sz;
            saveActiveRegister();
            _needsRedraw = true;
        }
        return;
    }
    if (ki.arrowRight || ki.arrowDown) {
        if (!registers.empty()) {
            activeRegister = (activeRegister + 1) % (int)registers.size();
            saveActiveRegister();
            _needsRedraw = true;
        }
        return;
    }
    if (ki.ch >= '0' && ki.ch <= '9') { _numberBuf += ki.ch; _needsRedraw = true; return; }
    if (ki.enter && _numberBuf.length() > 0) {
        int target = _numberBuf.toInt() - 1;  // user types 1-based, convert to 0-based
        _numberBuf = "";
        if (!registers.empty()) {
            activeRegister = constrain(target, 0, (int)registers.size() - 1);
            saveActiveRegister();
        }
        _needsRedraw = true;
        return;
    }
    if (ki.enter && _numberBuf.length() == 0) {
        if (!registers.empty() && !registers[activeRegister].isEmpty()) {
            pendingTokenStrings.push_back(registers[activeRegister]);
        }
        return;
    }
    if (ki.del && _numberBuf.length() > 0) { _numberBuf.remove(_numberBuf.length() - 1); _needsRedraw = true; }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
