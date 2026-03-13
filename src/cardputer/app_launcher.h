#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppLauncher : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    const char* appName() const override { return "Launcher"; }

private:
    int  _selected     = 0;
    bool _needsRedraw  = true;

    void _drawMenu();
    void _drawIcon(int appIndex, int screenX, bool selected);
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
