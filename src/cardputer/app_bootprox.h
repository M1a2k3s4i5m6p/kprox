#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppBootProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override {}
    const char* appName() const override { return "BootProx"; }
    const char* appHelp()  const override { return "Configure a register to run on every boot.\nSet a fire limit to auto-disable after N boots.\n0 = fire on every boot indefinitely."; }
    const uint16_t* appIcon() const override { return fa_rocket_48; }
    void requestRedraw() override        { _needsRedraw = true; }

private:
    static constexpr int BP_BAR_H = 16;
    static constexpr int BP_BOT_H = 13;

    enum Field { F_ENABLED = 0, F_REGISTER, F_LIMIT, F_RESET, F_COUNT };

    bool          _needsRedraw = true;
    Field         _sel         = F_ENABLED;
    bool          _editing     = false;
    String        _editBuf;
    String        _statusMsg;
    bool          _statusOk    = false;

    void _draw();
    void _drawTopBar();
    void _drawBottomBar(const char* hint);
    void _handle(KeyInput ki);
    void _save();
};

} // namespace Cardputer
#endif
