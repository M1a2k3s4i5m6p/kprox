#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "fa_radio_icon.h"
#include "ui_manager.h"
#include "../globals.h"

namespace Cardputer {

class AppMediaControl : public AppBase {
public:
    void onEnter()  override;
    void onUpdate() override;
    void onExit()   override {}
    void requestRedraw() override { _needsRedraw = true; }

    const char*      appName() const override { return "MediaCtrl"; }
    const uint16_t*  appIcon() const override { return fa_radio_48; }
    bool handlesGlobalBtnA()   const override { return true; }

    const char* appHelp() const override {
        return
            "Media controls via HID consumer keys.\n"
            "Arrow/;.,/ select  ENTER fires\n"
            "SPC=Play  M=Mute  U/D=Vol\n"
            "N=Next  P=Prev  S=Stop  U/+=VolUp";
    }

private:
    int  _sel        = 0;
    bool _needsRedraw = true;
    bool _flashActive = false;
    unsigned long _flashUntil = 0;
    int  _flashSel   = -1;

    void _draw();
    void _fire(int idx);
    void _flashAndFire(int idx);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
