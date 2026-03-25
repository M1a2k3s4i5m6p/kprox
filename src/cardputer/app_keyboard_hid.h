#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"
#include "../globals.h"

namespace Cardputer {

class AppKeyboardHID : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "Keyboard"; }
    const char* appHelp()  const override { return "Direct keyboard forwarding to paired host.\nType on Cardputer to send HID input.\nAll keys including ;.,/` are forwarded.\nBtnG0 returns to launcher."; }
    bool handlesGlobalBtnA() const override { return true; }
    const uint16_t* appIcon() const override { return fa_keyboard_48; }  // ~rgb(80,80,80)

private:
    bool _needsInfoRedraw = true;
    bool _lastBleState    = false;

    void _drawInfo();
    void _sendKeyPress(const KeyInput& ki);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
