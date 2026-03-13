#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../hid.h"
#include "app_keyboard_hid.h"

namespace Cardputer {

static const int BG = 0x1040; // ~#101010

void AppKeyboardHID::onEnter() {
    _needsInfoRedraw = true;
    _drawInfo();
}

void AppKeyboardHID::onExit() {}

void AppKeyboardHID::_drawInfo() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(BG);

    bool ble = isBLEConnected();

    disp.fillRect(0, 0, disp.width(), 18, disp.color565(50, 50, 60));
    disp.setTextColor(TFT_WHITE, disp.color565(50, 50, 60));
    disp.setTextSize(1);
    disp.drawString("HID Keyboard", 4, 3);

    disp.setTextColor(ble ? TFT_GREEN : TFT_RED, disp.color565(50, 50, 60));
    disp.drawString(ble ? "BLE OK" : "BLE off", disp.width() - 52, 3);

    disp.setTextColor(disp.color565(160, 160, 160), BG);
    disp.drawString("Keys forwarded as HID to host.", 4, 28);
    disp.drawString("BTNG0 = return to menu", 4, 46);

    disp.fillRect(0, disp.height() - 14, disp.width(), 14, disp.color565(16, 16, 16));
    disp.setTextColor(disp.color565(110, 110, 110), disp.color565(16, 16, 16));
    disp.drawString("type to send  BTNG0 menu", 2, disp.height() - 13);

    _needsInfoRedraw = false;
}

void AppKeyboardHID::_sendKeyPress(const KeyInput& ki) {
    if (!hasAnyConnection()) return;

    if (ki.esc)         { sendSpecialKey(KEY_ESC);         return; }
    if (ki.enter)       { sendSpecialKey(KEY_RETURN);      return; }
    if (ki.del)         { sendSpecialKey(KEY_BACKSPACE);   return; }
    if (ki.arrowUp)     { sendSpecialKey(KEY_UP_ARROW);    return; }
    if (ki.arrowDown)   { sendSpecialKey(KEY_DOWN_ARROW);  return; }
    if (ki.arrowLeft)   { sendSpecialKey(KEY_LEFT_ARROW);  return; }
    if (ki.arrowRight)  { sendSpecialKey(KEY_RIGHT_ARROW); return; }
    if (ki.ch) {
        char buf[2] = { ki.ch, 0 };
        sendPlainText(String(buf));
    }
}

void AppKeyboardHID::onUpdate() {
    bool bleNow = isBLEConnected();
    if (_needsInfoRedraw || bleNow != _lastBleState) {
        _lastBleState    = bleNow;
        _needsInfoRedraw = false;
        _drawInfo();
    }

    if (M5Cardputer.BtnA.wasPressed()) { uiManager.returnToLauncher(); return; }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    _sendKeyPress(ki);
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
