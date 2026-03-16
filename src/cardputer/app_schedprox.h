#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include "../scheduled_tasks.h"

namespace Cardputer {

class AppSchedProx : public AppBase {
public:
    void onEnter() override;
    void onExit() override;
    void onUpdate() override;
    void requestRedraw() override { _needsRedraw = true; }
    const char* appName() const override { return "SchedProx"; }
    uint16_t iconColor() const override { return 0x8C20; } // amber
    bool handlesGlobalBtnA() const override { return true; }

private:
    enum State { ST_LIST, ST_VIEW, ST_ADD_FIELD, ST_CONFIRM_DELETE };
    State  _state       = ST_LIST;
    bool   _needsRedraw = true;

    // List view
    int _listSel  = 0;
    int _listScroll = 0;

    // Add/edit — field index maps to: 0=label 1=year 2=month 3=day 4=hour 5=minute 6=second 7=payload 8=repeat
    int    _fieldSel  = 0;
    String _editBuf;
    ScheduledTask _draft;

    void _resetDraft();
    void _drawList();
    void _drawView(const ScheduledTask& t);
    void _drawAdd();
    void _drawConfirmDelete(const ScheduledTask& t);

    void _handleList(KeyInput ki);
    void _handleView(KeyInput ki);
    void _handleAdd(KeyInput ki);
    void _handleConfirmDelete(KeyInput ki);

    void _drawTopBar(const char* title);
    void _drawBottomBar(const char* hint);
    bool _fieldIsNumeric(int fi);
    String _draftField(int fi);
    void _applyField(int fi, const String& val);
};

} // namespace Cardputer
#endif
