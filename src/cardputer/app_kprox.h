#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppKProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "KProx"; }
    const char* appHelp()  const override { return "Register playback and device status.\nUp/dn: browse registers  ENTER: play\nD: delete  BtnG0: play active"; }
    bool handlesGlobalBtnA() const override { return true; }
    const uint16_t* appIcon() const override { return fa_jet_fighter_48; } // header blue

private:
    String _numberBuf;
    bool   _needsRedraw = true;

    String _lastNetLine;
    String _lastRegContent;
    String _lastRegLine;
    String _lastNumberBuf;
    bool   _lastHalted     = false;
    bool   _lastCredLocked = true;

    unsigned long _lastButtonRelease  = 0;
    unsigned long _lastButtonPress    = 0;
    int           _buttonPressCount   = 0;
    bool          _haltTriggered      = false;
    bool          _skipNextRelease    = false;

    static constexpr unsigned long DOUBLE_CLICK_MS = 350;
    static constexpr unsigned long DEBOUNCE_MS     = 50;

    void _drawScreen();
    void _checkHomeButton();
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
