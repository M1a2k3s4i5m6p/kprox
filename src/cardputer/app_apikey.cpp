#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_apikey.h"
#include "../storage.h"

namespace Cardputer {

static const int BG = 0x1863;

void AppApiKey::_render() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(BG);

    disp.fillRect(0, 0, disp.width(), 18, disp.color565(120, 30, 140));
    disp.setTextColor(TFT_WHITE, disp.color565(120, 30, 140));
    disp.setTextSize(1);
    disp.drawString("Set API Key", 4, 3);
    drawTabHint(4 + disp.textWidth("Set API Key") + 3);

    int y = 24;
    disp.setTextColor(disp.color565(180, 180, 180), BG);
    disp.drawString("Current key:", 4, y);
    y += 16;

    // Show current key with middle chars masked
    String display;
    if (apiKey.length() <= 4) {
        display = apiKey;
    } else {
        display = apiKey.substring(0, 2);
        for (size_t i = 2; i < apiKey.length() - 2; i++) display += '*';
        display += apiKey.substring(apiKey.length() - 2);
    }
    disp.setTextColor(TFT_YELLOW, BG);
    disp.drawString(display, 4, y);
    y += 20;

    disp.setTextColor(disp.color565(180, 180, 180), BG);
    disp.drawString("New key:", 4, y);
    y += 16;

    // Input field
    disp.fillRect(4, y - 1, disp.width() - 8, 18, disp.color565(50, 50, 50));
    disp.setTextColor(TFT_WHITE, disp.color565(50, 50, 50));
    String displayInput = _inputBuf;
    if (displayInput.length() > 26) displayInput = displayInput.substring(displayInput.length() - 26);
    disp.drawString(displayInput + "_", 6, y + 1);
    y += 22;

    if (_saved) {
        disp.setTextColor(TFT_GREEN, BG);
        disp.drawString("Saved!", 4, y);
    }

    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
    disp.setTextColor(disp.color565(110, 110, 110), disp.color565(16, 16, 16));
    disp.drawString("ENTER save  DEL backspace  ESC back", 2, disp.height() - 13);
}

void AppApiKey::onEnter() {
    _inputBuf = "";
    _saved    = false;
    _render();
}

void AppApiKey::onExit() {
    _inputBuf = "";
    _saved    = false;
}

void AppApiKey::onUpdate() {
    KeyInput ki = pollKeys(true);
    if (!ki.anyKey) return;

    uiManager.notifyInteraction();

    if (ki.esc) { uiManager.returnToLauncher(); return; }

    if (ki.enter) {
        if (_inputBuf.length() > 0) {
            apiKey = _inputBuf;
            saveApiKeySettings();
            _saved    = true;
            _inputBuf = "";
        }
        _render();
        return;
    }

    if (ki.del && _inputBuf.length() > 0) {
        _inputBuf.remove(_inputBuf.length() - 1);
        _render();
        return;
    }

    if (ki.ch) {
        _inputBuf += ki.ch;
        _render();
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
