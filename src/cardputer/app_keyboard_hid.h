#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "../globals.h"

namespace Cardputer {

class AppKeyboardHID : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "Keyboard"; }
    bool handlesGlobalBtnA() const override { return true; }
    uint16_t iconColor() const override { return 0x4A49; }  // ~rgb(80,80,80)

private:
    bool _needsInfoRedraw = true;
    bool _lastBleState    = false;

    void _drawInfo();
    void _sendKeyPress(const KeyInput& ki);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
