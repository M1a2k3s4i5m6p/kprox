#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppQRProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "QRProx"; }
    uint16_t iconColor() const override { return 0x0518; } // dark teal
    bool handlesGlobalBtnA() const override { return true; }

private:
    bool _needsRedraw = true;
    void _draw();
};

} // namespace Cardputer
#endif
