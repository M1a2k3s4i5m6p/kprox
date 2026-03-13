#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include <stdint.h>

namespace Cardputer {

class AppBase {
public:
    virtual ~AppBase() = default;

    // Called once when entering the app
    virtual void onEnter() {}

    // Called every loop tick while active
    virtual void onUpdate() = 0;

    // Called once when leaving the app (before next app enters)
    virtual void onExit() {}

    virtual const char* appName() const = 0;

    // Optional 48x48 RGB565 icon; null = draw solid color tile
    virtual const uint16_t* appIcon() const { return nullptr; }

    // Icon tile background color when no icon image is provided
    virtual uint16_t iconColor() const { return 0x4A69; }
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
