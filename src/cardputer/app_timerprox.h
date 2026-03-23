#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "fa_icons.h"
#include "ui_manager.h"

namespace Cardputer {

class AppTimerProx : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "TimerProx"; }
    const char* appHelp() const override {
        return "Fire a register after a countdown timer.\n\n"
               "FIRE DELAY: time after starting before the register plays.\n"
               "HALT DELAY: time after the register fires before halting.\n"
               "Set halt delay to 00:00:00 to disable.\n\n"
               "Uses device uptime - no WiFi or NTP required.\n"
               "Settings persist across reboots.\n"
               "Press ENTER on Start to begin. ESC cancels.";
    }
    const uint16_t* appIcon() const override { return fa_stopwatch_48; }
    bool handlesGlobalBtnA() const override { return true; }
    void requestRedraw() override { _needsRedraw = true; }
    void _pollDisplay();

private:
    static constexpr int TP_BAR_H = 16;
    static constexpr int TP_BOT_H = 13;

    enum State { ST_SETUP, ST_RUNNING };
    enum Field { F_REG=0, F_FIRE_H, F_FIRE_M, F_FIRE_S,
                 F_HALT_H, F_HALT_M, F_HALT_S, F_START, F_COUNT };

    State         _state      = ST_SETUP;
    bool          _needsRedraw = true;
    Field         _sel        = F_REG;

    int    _regIdx  = 0;
    int    _fireH=0, _fireM=0, _fireS=0;
    int    _haltH=0, _haltM=0, _haltS=0;

    unsigned long _startMs    = 0;
    unsigned long _firedMs    = 0;
    unsigned long _lastDrawMs = 0;
    bool          _fired      = false;

    void _save();
    void _load();

    void _drawTopBar();
    void _drawBottomBar(const char* hint);
    void _drawSetup();
    void _drawRunning(bool full = true);

    void _handleSetup(KeyInput ki);
    void _handleRunning(KeyInput ki);

    int&  _fieldRef(Field f);
    int   _fieldMax(Field f);

    long  _fireMs() { return (_fireH*3600L + _fireM*60L + _fireS) * 1000L; }
    long  _haltMs() { return (_haltH*3600L + _haltM*60L + _haltS) * 1000L; }

    static void _drawTimeField(int x, int y, int val, bool sel, const char* lbl);
};

} // namespace Cardputer
#endif
