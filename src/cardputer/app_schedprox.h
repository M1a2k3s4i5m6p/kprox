#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "../scheduled_tasks.h"

namespace Cardputer {

class AppSchedProx : public AppBase {
public:
    void onEnter() override;
    void onExit() override {}
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "SchedProx"; }
    uint16_t iconColor() const override  { return 0x8C20; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    static constexpr int SP_BAR_H = 16;
    static constexpr int SP_BOT_H = 14;

    enum State { ST_LIST, ST_VIEW, ST_ADD_P1, ST_ADD_P2, ST_CONFIRM_DELETE };

    State  _state      = ST_LIST;
    bool   _needsRedraw = true;

    int _listSel    = 0;
    int _listScroll = 0;

    ScheduledTask _draft;
    bool          _isEdit     = false;
    int           _editTaskId = -1;

    // page 1: label / register / payload / repeat
    int    _p1Field      = 0;
    int    _regScroll    = 0;
    bool   _usingReg     = true;
    String _labelBuf;
    String _payloadBuf;
    bool   _labelActive   = false;
    bool   _payloadActive = false;

    // page 2: datetime + convenience
    // _p2Sel: 0=Now 1=+1m 2=+10m 3=+30m 4=+1h 5=+1d 6=+1w 7=datetime 8=enabled 9=save
    int  _p2Sel      = 0;
    int  _dtCursor   = 0;
    bool _dtActive   = false;  // true = field is being edited (arrows/digits captured)
    bool _dtTyping   = false;
    String _dtTypeBuf;

    unsigned long _enterConsumedAt = 0;

    void _resetDraft();
    void _applyReg(int idx);
    void _applyNow();
    void _shiftTime(long secs);

    void _drawTopBar(const char* title);
    void _drawBottomBar(const char* hint);
    void _drawList();
    void _drawView();
    void _drawAddP1();
    void _drawAddP2();
    void _drawConfirmDelete();
    String _countdown(const ScheduledTask& t) const;

    void _handleList(KeyInput ki);
    void _handleView(KeyInput ki);
    void _handleAddP1(KeyInput ki);
    void _handleAddP2(KeyInput ki);
    void _handleConfirmDelete(KeyInput ki);

    int  _dtGet(int c) const;
    void _dtSet(int c, int v);
    int  _dtMax(int c) const;
};

} // namespace Cardputer
#endif
