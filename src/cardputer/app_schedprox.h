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
    uint16_t iconColor() const override { return 0x8C20; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    enum State { ST_LIST, ST_VIEW, ST_ADD, ST_CONFIRM_DELETE };
    State  _state       = ST_LIST;
    bool   _needsRedraw = true;

    int _listSel    = 0;
    int _listScroll = 0;

    // Add form state — 4 sections: 0=label 1=datetime 2=payload 3=options
    int    _addSection  = 0;  // which row is selected
    // datetime cursor: 0=year 1=month 2=day 3=hour 4=minute 5=second
    int    _dtCursor    = 0;
    String _editBuf;
    bool   _editingText = false;  // for label/payload text fields
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

    // datetime helpers
    void  _dtIncrement(int cursor, int delta);
    int   _dtGet(int cursor) const;
    void  _dtSet(int cursor, int val);
    const char* _dtLabel(int cursor) const;
    int   _dtMax(int cursor) const;
};

} // namespace Cardputer
#endif
