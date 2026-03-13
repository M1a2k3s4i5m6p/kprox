#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppClock : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* appName() const override { return "Clock"; }
    uint16_t iconColor() const override { return 0x1C12; }  // ~rgb(30,80,150)

private:
    unsigned long _lastDraw = 0;

    void _drawClock();
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
