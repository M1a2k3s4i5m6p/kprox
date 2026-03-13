#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppKProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "KProx"; }
    uint16_t iconColor() const override { return 0x3297; } // header blue

private:
    String _numberBuf;
    bool   _needsRedraw = true;

    String _lastNetLine;
    String _lastRegContent;
    String _lastRegLine;
    String _lastNumberBuf;
    bool   _lastHalted  = false;

    unsigned long _lastButtonRelease  = 0;
    unsigned long _lastButtonPress    = 0;
    int           _buttonPressCount   = 0;
    bool          _haltTriggered      = false;

    static constexpr unsigned long DOUBLE_CLICK_MS = 350;
    static constexpr unsigned long DEBOUNCE_MS     = 50;

    void _drawScreen();
    void _checkHomeButton();
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
