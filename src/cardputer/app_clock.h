#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

struct TZEntry {
    const char* abbr;
    int         offsetSeconds;
};

class AppClock : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* appName() const override { return "Clock"; }
    uint16_t iconColor() const override { return 0x1C12; }

private:
    unsigned long _lastDraw = 0;
    int           _tzIdx    = 0;   // index into built-in TZ table

    static const TZEntry _tzTable[];
    static const int     _tzCount;

    void _drawClock();
    void _applyTZ();
    int  _findTZByOffset(long offsetSec);
};

} // namespace Cardputer
#endif
