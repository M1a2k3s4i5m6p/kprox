#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"

namespace Cardputer {

class AppApiKey : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "API Key"; }
    const char* appHelp()  const override { return "Change the device API key.\nENTER: edit key  save to apply.\nDefault key is kprox1337."; }
    uint16_t iconColor() const override { return 0x8C40; } // ~#882080

private:
    String _inputBuf;
    bool   _saved = false;

    void _render();
};

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
