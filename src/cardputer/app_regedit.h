#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "app_base.h"
#include "ui_manager.h"
#include <vector>

namespace Cardputer {

class AppRegEdit : public AppBase {
public:
    void onEnter() override;
    void onUpdate() override;
    void onExit() override;
    const char* appName() const override { return "RegEdit"; }
    uint16_t iconColor() const override  { return 0xFFE0; }
    void requestRedraw() override { _needsRedraw = true; }
    bool handlesGlobalBtnA() const override { return true; }

private:
    enum Mode {
        M_BROWSE,
        M_EDIT_NAME,
        M_EDIT_CONTENT,
        M_CONFIRM_SAVE_EXIT,
        M_CONFIRM_DEL,
        M_CONFIRM_DEL_ALL,
        M_MOVE
    };

    Mode   _mode        = M_BROWSE;
    bool   _needsRedraw = true;
    int    _regIdx      = 0;

    String _nameBuf;
    String _contentBuf;
    int    _cursorPos   = 0;   // char offset in _contentBuf

    String _statusMsg;
    bool   _statusOk  = false;

    static constexpr int RE_BAR_H = 18;
    static constexpr int RE_BOT_H = 14;

    // Visual line structure built from _contentBuf + charsPerLine
    struct VisLine { int startPos; int len; bool hardBreak; };
    std::vector<VisLine> _buildVisLines(const String& buf, int charsPerLine) const;
    int  _cursorLine(int charsPerLine) const;
    int  _cursorCol(int charsPerLine) const;
    int  _posFromLineCol(int line, int col, int charsPerLine) const;

    void _draw();
    void _drawBrowse();
    void _drawEditName();
    void _drawEditContent();
    void _drawConfirmSaveExit();
    void _drawConfirmDelete();
    void _drawConfirmDeleteAll();
    void _drawMove();

    void _drawInputField(int x, int y, int w, const String& buf, bool active);
    void _drawContentWithCursor(int x, int y, int w, int h,
                                const String& buf, int cursorPos, bool editing);

    void _handleBrowse();
    void _handleEditName();
    void _handleEditContent();
    void _handleConfirmSaveExit();
    void _handleConfirmDelete();
    void _handleConfirmDeleteAll();
    void _handleMove();

    void _insertAt(int pos, char c);
    void _deleteAt(int pos);
    void _swapWithAdjacent(int direction);
};

} // namespace Cardputer
#endif
