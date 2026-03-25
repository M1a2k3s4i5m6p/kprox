#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_regedit.h"
#include "../registers.h"
#include "ui_manager.h"
#include <algorithm>

namespace Cardputer {

static constexpr int RE_BG    = 0x0821;
static constexpr int RE_BAR_H = 18;
static constexpr int RE_BOT_H = 14;
static constexpr int RE_LINE_H = 13;

// ---- Raw keyboard ----
// Navigation aliases (,  .  ;  /) are NOT applied here so all printable
// characters are available in edit fields. Browse mode checks k.ch directly
// for those aliases.

struct REKey {
    char ch     = 0;
    bool del    = false;
    bool enter  = false;
    bool esc    = false;   // bare HID ESC (0x29) or 0x1B char
    bool fnEsc  = false;   // fn + ESC/backtick
    bool tab    = false;
    bool up     = false;
    bool down   = false;
    bool left   = false;
    bool right  = false;
    bool fn     = false;
    bool any    = false;
};

static REKey pollREKey(bool editing = false) {
    REKey k;
    if (!M5Cardputer.Keyboard.isChange()) return k;
    if (!M5Cardputer.Keyboard.isPressed()) return k;

    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    k.any   = true;
    k.del   = ks.del;
    k.enter = ks.enter;
    k.fn    = ks.fn;

    for (uint8_t hk : ks.hid_keys) {
        switch (hk) {
            case 0x52: k.up    = true; break;
            case 0x51: k.down  = true; break;
            case 0x50: k.left  = true; break;
            case 0x4F: k.right = true; break;
        }
    }

    for (char c : ks.word) {
        if (c == 0x1B) continue;
        if (c == '`' && ks.fn)              { k.fnEsc = true; continue; }
        if (c == '`' && !ks.fn && !editing) { k.esc   = true; continue; }
        if (c == '`' && !ks.fn &&  editing) { k.ch    = '`'; continue; }
        if (ks.fn) {
            switch (c) {
                case ';': k.up    = true; continue;
                case '.': k.down  = true; continue;
                case ',': k.left  = true; continue;
                case '/': k.right = true; continue;
            }
        }
        if (c >= 0x20 && c < 0x7F) k.ch = c;
    }
    return k;
}

// ---- Visual line helpers ----

std::vector<AppRegEdit::VisLine>
AppRegEdit::_buildVisLines(const String& buf, int cpl) const {
    std::vector<VisLine> lines;
    int n = buf.length();
    int pos = 0;
    while (pos <= n) {
        VisLine vl;
        vl.startPos = pos;
        vl.hardBreak = false;
        int col = 0;
        while (pos < n && col < cpl && buf[pos] != '\n') {
            pos++; col++;
        }
        vl.len = col;
        if (pos < n && buf[pos] == '\n') {
            vl.hardBreak = true;
            pos++;
        }
        lines.push_back(vl);
        if (pos >= n && !vl.hardBreak) break;
    }
    if (lines.empty()) lines.push_back({0, 0, false});
    return lines;
}

int AppRegEdit::_cursorLine(int cpl) const {
    auto vls = _buildVisLines(_contentBuf, cpl);
    for (int i = 0; i < (int)vls.size(); i++) {
        int end = vls[i].startPos + vls[i].len + (vls[i].hardBreak ? 1 : 0);
        bool last = (i == (int)vls.size() - 1);
        if (_cursorPos <= vls[i].startPos + vls[i].len || last)
            return i;
    }
    return (int)vls.size() - 1;
}

int AppRegEdit::_cursorCol(int cpl) const {
    auto vls = _buildVisLines(_contentBuf, cpl);
    int cl = _cursorLine(cpl);
    if (cl < (int)vls.size())
        return _cursorPos - vls[cl].startPos;
    return 0;
}

int AppRegEdit::_posFromLineCol(int line, int col, int cpl) const {
    auto vls = _buildVisLines(_contentBuf, cpl);
    if (line >= (int)vls.size()) {
        // last position in buffer
        return _contentBuf.length();
    }
    int base = vls[line].startPos;
    int maxCol = vls[line].len;
    col = constrain(col, 0, maxCol);
    return base + col;
}

// ---- Edit helpers ----

void AppRegEdit::_insertAt(int pos, char c) {
    _contentBuf = _contentBuf.substring(0, pos) + c + _contentBuf.substring(pos);
    _cursorPos = pos + 1;
}

void AppRegEdit::_deleteAt(int pos) {
    if (pos <= 0 || pos > (int)_contentBuf.length()) return;
    _contentBuf = _contentBuf.substring(0, pos - 1) + _contentBuf.substring(pos);
    _cursorPos = pos - 1;
}

// ---- Common draw helpers ----

static void _drawTopBar(int regIdx, int total, const char* modeLabel) {
    auto& disp = M5Cardputer.Display;
    uint16_t bg = disp.color565(20, 60, 130);
    disp.fillRect(0, 0, disp.width(), RE_BAR_H, bg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bg);
    char lbl[36];
    snprintf(lbl, sizeof(lbl), "RegEdit  %s", modeLabel);
    disp.drawString(lbl, 4, 3);
    drawTabHint(4 + disp.textWidth("RegEdit") + 3);
    char right[12];
    if (total == 0) snprintf(right, sizeof(right), "empty");
    else            snprintf(right, sizeof(right), "%d/%d", regIdx + 1, total);
    int rw = disp.textWidth(right);
    disp.drawString(right, disp.width() - rw - 4, 3);
}

static void _drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bg = disp.color565(16,16,16);
    disp.fillRect(0, disp.height() - RE_BOT_H, disp.width(), RE_BOT_H, bg);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(100,100,100), bg);
    disp.drawString(hint, 2, disp.height() - RE_BOT_H + 2);
}

void AppRegEdit::_drawInputField(int x, int y, int w,
                                  const String& buf, bool active) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = active ? disp.color565(55,55,55) : disp.color565(30,30,30);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);
    int maxChars = (w - 6) / 6;
    String d = buf;
    if ((int)d.length() > maxChars) d = d.substring(d.length() - maxChars);
    if (active) d += '_';
    disp.drawString(d, x + 3, y + 3);
}

void AppRegEdit::_drawContentWithCursor(int x, int y, int w, int h,
                                         const String& buf,
                                         int cursorPos, bool editing) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = editing ? disp.color565(28,28,52) : disp.color565(18,18,28);
    disp.fillRect(x, y, w, h, fbg);
    disp.setTextSize(1);

    int cpl      = (w - 6) / 6;
    int maxLines = h / RE_LINE_H;

    auto vls = _buildVisLines(buf, cpl);

    // Find which visual line the cursor is on
    int curLine = 0;
    if (editing && cursorPos >= 0) {
        for (int i = 0; i < (int)vls.size(); i++) {
            int end = vls[i].startPos + vls[i].len + (vls[i].hardBreak ? 1 : 0);
            bool last = (i == (int)vls.size() - 1);
            if (cursorPos <= vls[i].startPos + vls[i].len || last) {
                curLine = i; break;
            }
        }
    }

    // Scroll so cursor line is visible (show lines [scrollStart, scrollStart+maxLines))
    int scrollStart = 0;
    if (curLine >= maxLines) scrollStart = curLine - maxLines + 1;

    for (int vi = scrollStart; vi < (int)vls.size() && vi < scrollStart + maxLines; vi++) {
        int row = vi - scrollStart;
        int drawY = y + row * RE_LINE_H + 2;

        String lineText = buf.substring(vls[vi].startPos,
                                        vls[vi].startPos + vls[vi].len);

        if (editing && vi == curLine) {
            // Draw characters up to cursor, then cursor, then rest
            int col = cursorPos - vls[vi].startPos;
            col = constrain(col, 0, vls[vi].len);

            String before = lineText.substring(0, col);
            String after  = lineText.substring(col);

            int drawX = x + 3;

            // Text before cursor
            if (!before.isEmpty()) {
                disp.setTextColor(disp.color565(200,200,220), fbg);
                disp.drawString(before, drawX, drawY);
                drawX += disp.textWidth(before);
            }

            // Cursor block
            uint16_t cursorBg = disp.color565(180,180,80);
            char cursorChar[2] = { (after.isEmpty() ? ' ' : after[0]), 0 };
            disp.fillRect(drawX, drawY - 1, 6, RE_LINE_H - 1, cursorBg);
            disp.setTextColor(disp.color565(20,20,20), cursorBg);
            disp.drawString(cursorChar, drawX, drawY);
            drawX += 6;

            // Text after cursor (skip the char under cursor)
            if (!after.isEmpty()) {
                String afterRest = after.substring(1);
                if (!afterRest.isEmpty()) {
                    disp.setTextColor(disp.color565(200,200,220), fbg);
                    disp.drawString(afterRest, drawX, drawY);
                }
            }

            // Show newline marker when cursor is at end of a hard-break line
            if (vls[vi].hardBreak && col == vls[vi].len) {
                // cursor block already drawn, no extra needed
            }
        } else {
            disp.setTextColor(disp.color565(200,200,220), fbg);
            disp.drawString(lineText, x + 3, drawY);
        }
    }
}

// ---- Browse ----

void AppRegEdit::_drawBrowse() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    int total = (int)registers.size();
    _drawTopBar(_regIdx, total, "");

    if (total == 0) {
        disp.setTextColor(disp.color565(160,160,160), RE_BG);
        disp.setTextSize(1);
        disp.drawString("No registers.  N = add new", 4, RE_BAR_H + 20);
        _drawBottomBar("N add  ESC back");
        return;
    }

    int y = RE_BAR_H + 4;
    String name = (_regIdx < (int)registerNames.size()) ? registerNames[_regIdx] : "";
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(140,140,140), RE_BG);
    disp.drawString("Name:", 4, y);
    disp.setTextColor(name.isEmpty() ? disp.color565(80,80,80) : TFT_YELLOW, RE_BG);
    String nameDisp = name.isEmpty() ? "(unnamed)" : name;
    if ((int)nameDisp.length() > 24) nameDisp = nameDisp.substring(0,21) + "...";
    disp.drawString(nameDisp, 4 + disp.textWidth("Name:") + 2, y);
    y += 14;

    disp.setTextColor(disp.color565(140,140,140), RE_BG);
    disp.drawString("Content:", 4, y);
    y += 12;

    String content = (_regIdx < (int)registers.size()) ? registers[_regIdx] : "";
    int contentH = disp.height() - RE_BOT_H - y - 16;
    _drawContentWithCursor(4, y, disp.width()-8, contentH, content, -1, false);

    if (_statusMsg.length() > 0) {
        int sy = disp.height() - RE_BOT_H - 14;
        disp.fillRect(0, sy, disp.width(), 13, RE_BG);
        disp.setTextColor(_statusOk ? TFT_GREEN : disp.color565(220,80,80), RE_BG);
        disp.drawString(_statusMsg, 4, sy);
    }

    _drawBottomBar("</> reg  E edit  R name  N new  M move  DEL del  fn+X del all");
}

void AppRegEdit::_handleBrowse() {
    REKey k = pollREKey();
    if (!k.any) return;
    uiManager.notifyInteraction();

    int total = (int)registers.size();
    if (k.esc || k.fnEsc) { uiManager.returnToLauncher(); return; }

    // Navigation via HID arrows or keyboard aliases
    bool goLeft  = k.left  || k.ch == ',';
    bool goRight = k.right || k.ch == '/';
    bool goUp    = k.up    || k.ch == ';';
    bool goDown  = k.down  || k.ch == '.';

    if (goLeft || goUp) {
        if (total > 0) { _regIdx=(_regIdx-1+total)%total; _statusMsg=""; _needsRedraw=true; }
        return;
    }
    if (goRight || goDown) {
        if (total > 0) { _regIdx=(_regIdx+1)%total; _statusMsg=""; _needsRedraw=true; }
        return;
    }
    if (k.ch=='e'||k.ch=='E') {
        if (total>0) {
            _contentBuf = registers[_regIdx];
            _cursorPos  = _contentBuf.length();
            _mode=M_EDIT_CONTENT; _statusMsg=""; _needsRedraw=true;
        }
        return;
    }
    if (k.ch=='r'||k.ch=='R') {
        if (total>0) {
            _nameBuf = (_regIdx<(int)registerNames.size()) ? registerNames[_regIdx] : "";
            _mode=M_EDIT_NAME; _statusMsg=""; _needsRedraw=true;
        }
        return;
    }
    if (k.ch=='n'||k.ch=='N') {
        addRegister("","");
        _regIdx    = (int)registers.size()-1;
        _contentBuf = "";
        _cursorPos  = 0;
        _mode=M_EDIT_CONTENT; _statusMsg=""; _needsRedraw=true;
        return;
    }
    if (k.ch=='m'||k.ch=='M') {
        if (total>0) { _mode=M_MOVE; _statusMsg=""; _needsRedraw=true; }
        return;
    }
    if (k.del && total>0) {
        _mode=M_CONFIRM_DEL; _statusMsg=""; _needsRedraw=true; return;
    }
    if (k.fn && (k.ch=='x'||k.ch=='X')) {
        if (total>0) { _mode=M_CONFIRM_DEL_ALL; _statusMsg=""; _needsRedraw=true; }
        return;
    }
}

// ---- Edit name ----

void AppRegEdit::_drawEditName() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    _drawTopBar(_regIdx, (int)registers.size(), "[rename]");
    int y = RE_BAR_H + 8;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(180,180,180), RE_BG);
    disp.drawString("Register name:", 4, y); y += 14;
    _drawInputField(4, y, disp.width()-8, _nameBuf, true);
    _drawBottomBar("type  ENTER save  ESC cancel");
}

void AppRegEdit::_handleEditName() {
    REKey k = pollREKey(true);
    if (!k.any) return;
    uiManager.notifyInteraction();
    if (k.esc) { _mode=M_BROWSE; _needsRedraw=true; return; }
    if (k.enter) {
        while ((int)registerNames.size()<=_regIdx) registerNames.push_back("");
        saveRegisterName(_regIdx, _nameBuf);
        _statusMsg="Name saved"; _statusOk=true;
        _mode=M_BROWSE; _needsRedraw=true; return;
    }
    if (k.del && _nameBuf.length()>0) { _nameBuf.remove(_nameBuf.length()-1); _needsRedraw=true; return; }
    if (k.ch) { _nameBuf+=k.ch; _needsRedraw=true; }
}

// ---- Edit content ----

void AppRegEdit::_drawEditContent() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    _drawTopBar(_regIdx, (int)registers.size(), "[edit]");

    int y = RE_BAR_H + 4;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(140,140,140), RE_BG);
    disp.drawString("Content:", 4, y); y += 12;

    int contentH = disp.height() - RE_BOT_H - y - 2;
    _drawContentWithCursor(4, y, disp.width()-8, contentH,
                           _contentBuf, _cursorPos, true);

    _drawBottomBar("fn+arrows cursor  fn+ENTER save  fn+ESC exit");
}

void AppRegEdit::_handleEditContent() {
    REKey k = pollREKey(true);
    if (!k.any) return;
    uiManager.notifyInteraction();

    // fn+ESC → save/discard prompt (ONLY exit path from edit mode)
    if (k.fnEsc) {
        _mode=M_CONFIRM_SAVE_EXIT; _needsRedraw=true; return;
    }

    // Plain ESC is ignored in edit mode so accidental presses don't exit

    // fn+ENTER = save immediately and exit
    if (k.fn && k.enter) {
        while ((int)registers.size()<=_regIdx) registers.push_back("");
        saveRegister(_regIdx, _contentBuf);
        _statusMsg="Saved"; _statusOk=true;
        _mode=M_BROWSE; _needsRedraw=true; return;
    }

    // ENTER = insert newline at cursor
    if (k.enter) {
        _insertAt(_cursorPos, '\n');
        _needsRedraw=true; return;
    }

    // fn+arrows = cursor movement
    int cpl = (M5Cardputer.Display.width()-8-6)/6;
    if (k.fn) {
        if (k.left) {
            if (_cursorPos>0) _cursorPos--;
            _needsRedraw=true; return;
        }
        if (k.right) {
            if (_cursorPos<(int)_contentBuf.length()) _cursorPos++;
            _needsRedraw=true; return;
        }
        if (k.up) {
            int cl  = _cursorLine(cpl);
            int col = _cursorCol(cpl);
            if (cl>0) _cursorPos = _posFromLineCol(cl-1, col, cpl);
            _needsRedraw=true; return;
        }
        if (k.down) {
            int cl  = _cursorLine(cpl);
            int col = _cursorCol(cpl);
            auto vls = _buildVisLines(_contentBuf, cpl);
            if (cl<(int)vls.size()-1) _cursorPos = _posFromLineCol(cl+1, col, cpl);
            _needsRedraw=true; return;
        }
    }

    // DEL = delete character before cursor
    if (k.del) {
        _deleteAt(_cursorPos);
        _needsRedraw=true; return;
    }

    // Printable character — insert at cursor
    if (k.ch) {
        _insertAt(_cursorPos, k.ch);
        _needsRedraw=true;
    }
}

// ---- Confirm save / discard ----

void AppRegEdit::_drawConfirmSaveExit() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    _drawTopBar(_regIdx, (int)registers.size(), "[edit]");

    // Show a snippet of the content for context
    int y = RE_BAR_H + 6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(160,160,160), RE_BG);
    disp.drawString("Unsaved changes", 4, y); y += 18;

    int bx=16, by=y, bw=disp.width()-32, bh=54;
    disp.fillRoundRect(bx, by, bw, bh, 6, disp.color565(20,40,80));
    disp.drawRoundRect(bx, by, bw, bh, 6, disp.color565(80,120,200));
    disp.setTextColor(TFT_WHITE, disp.color565(20,40,80));
    disp.setTextDatum(MC_DATUM);
    int cx = bx+bw/2;
    disp.drawString("Save changes?", cx, by+14);
    disp.drawString("Y = save & exit", cx, by+28);
    disp.drawString("N = discard & exit", cx, by+42);
    disp.setTextDatum(TL_DATUM);
}

void AppRegEdit::_handleConfirmSaveExit() {
    REKey k = pollREKey();
    if (!k.any) return;
    uiManager.notifyInteraction();

    if (k.ch=='y'||k.ch=='Y') {
        while ((int)registers.size()<=_regIdx) registers.push_back("");
        saveRegister(_regIdx, _contentBuf);
        _statusMsg="Saved"; _statusOk=true;
        _mode=M_BROWSE; _needsRedraw=true;
    } else if (k.ch=='n'||k.ch=='N') {
        // Discard — go back to browse without saving
        _mode=M_BROWSE; _needsRedraw=true;
    }
    // All other keys (including ESC) are ignored — user must choose Y or N
}

// ---- Confirm delete ----

void AppRegEdit::_drawConfirmDelete() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    _drawTopBar(_regIdx, (int)registers.size(), "");
    int y = RE_BAR_H+6;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(200,200,200), RE_BG);
    String name = (_regIdx<(int)registerNames.size()) ? registerNames[_regIdx] : "";
    String label = name.isEmpty() ? "Register "+String(_regIdx) : name;
    if ((int)label.length()>28) label=label.substring(0,25)+"...";
    disp.drawString("Delete: "+label, 4, y); y+=20;
    int bx=20, by=y, bw=disp.width()-40, bh=46;
    disp.fillRoundRect(bx,by,bw,bh,6,disp.color565(50,10,10));
    disp.drawRoundRect(bx,by,bw,bh,6,disp.color565(200,60,60));
    disp.setTextColor(TFT_WHITE, disp.color565(50,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx=bx+bw/2;
    disp.drawString("Delete this register?", cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppRegEdit::_handleConfirmDelete() {
    REKey k = pollREKey();
    if (!k.any) return;
    uiManager.notifyInteraction();
    if (k.ch=='y'||k.ch=='Y') {
        deleteRegister(_regIdx);
        int total=(int)registers.size();
        if (total>0 && _regIdx>=total) _regIdx=total-1;
        if (total==0) _regIdx=0;
        _statusMsg="Deleted"; _statusOk=true;
        _mode=M_BROWSE; _needsRedraw=true;
    } else if (k.ch=='n'||k.ch=='N'||k.esc) {
        _mode=M_BROWSE; _needsRedraw=true;
    }
}

// ---- Confirm delete all ----

void AppRegEdit::_drawConfirmDeleteAll() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    _drawTopBar(0, (int)registers.size(), "");
    int y=RE_BAR_H+8;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220,80,80), RE_BG);
    disp.drawString("Delete ALL registers?", 4, y); y+=14;
    char cnt[32];
    snprintf(cnt,sizeof(cnt),"(%d registers)",(int)registers.size());
    disp.setTextColor(disp.color565(180,180,180), RE_BG);
    disp.drawString(cnt, 4, y); y+=20;
    int bx=20,by=y,bw=disp.width()-40,bh=46;
    disp.fillRoundRect(bx,by,bw,bh,6,disp.color565(50,10,10));
    disp.drawRoundRect(bx,by,bw,bh,6,disp.color565(200,60,60));
    disp.setTextColor(TFT_WHITE, disp.color565(50,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx=bx+bw/2;
    disp.drawString("Cannot be undone!", cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppRegEdit::_handleConfirmDeleteAll() {
    REKey k = pollREKey();
    if (!k.any) return;
    uiManager.notifyInteraction();
    if (k.ch=='y'||k.ch=='Y') {
        deleteAllRegisters();
        _regIdx=0; _statusMsg="All deleted"; _statusOk=true;
        _mode=M_BROWSE; _needsRedraw=true;
    } else if (k.ch=='n'||k.ch=='N'||k.esc) {
        _mode=M_BROWSE; _needsRedraw=true;
    }
}

// ---- Move / Reorder ----

void AppRegEdit::_swapWithAdjacent(int direction) {
    int total=(int)registers.size();
    if (total<2) return;
    int other=_regIdx+direction;
    if (other<0||other>=total) return;
    std::swap(registers[_regIdx], registers[other]);
    while ((int)registerNames.size()<=std::max(_regIdx,other))
        registerNames.push_back("");
    std::swap(registerNames[_regIdx], registerNames[other]);
    if (activeRegister==_regIdx)       activeRegister=other;
    else if (activeRegister==other)    activeRegister=_regIdx;
    _regIdx=other;
    saveRegisters();
}

void AppRegEdit::_drawMove() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(RE_BG);
    int total=(int)registers.size();
    _drawTopBar(_regIdx, total, "[move]");

    int y=RE_BAR_H+6;
    auto labelFor=[&](int i)->String{
        if(i<0||i>=total) return "";
        String n=(i<(int)registerNames.size())?registerNames[i]:"";
        String l=n.isEmpty()?"Reg "+String(i):n;
        if((int)l.length()>22) l=l.substring(0,19)+"...";
        return l;
    };

    if (_regIdx>0) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(90,90,90), RE_BG);
        disp.drawString(String(_regIdx-1)+": "+labelFor(_regIdx-1), 4, y);
    }
    y+=14;

    {
        uint16_t selBg=disp.color565(20,60,130);
        disp.fillRoundRect(2,y-1,disp.width()-4,16,3,selBg);
        disp.setTextColor(TFT_YELLOW, selBg);
        disp.drawString(String(_regIdx)+": "+labelFor(_regIdx), 6, y+1);
        if (_regIdx>0)      { disp.setTextColor(disp.color565(180,180,100),selBg); disp.drawString("<",disp.width()-20,y+1); }
        if (_regIdx<total-1){ disp.setTextColor(disp.color565(180,180,100),selBg); disp.drawString(">",disp.width()-10,y+1); }
    }
    y+=18;

    if (_regIdx<total-1) {
        disp.setTextSize(1);
        disp.setTextColor(disp.color565(90,90,90), RE_BG);
        disp.drawString(String(_regIdx+1)+": "+labelFor(_regIdx+1), 4, y);
    }
    y+=18;

    String content=(_regIdx<(int)registers.size())?registers[_regIdx]:"";
    int previewH=disp.height()-RE_BOT_H-y-2;
    if (previewH>12) _drawContentWithCursor(4,y,disp.width()-8,previewH,content,-1,false);
    _drawBottomBar("</> move position  ENTER confirm  ESC cancel");
}

void AppRegEdit::_handleMove() {
    REKey k=pollREKey();
    if (!k.any) return;
    uiManager.notifyInteraction();
    if (k.esc) { _mode=M_BROWSE; _needsRedraw=true; return; }
    if (k.enter) { _statusMsg="Moved to "+String(_regIdx); _statusOk=true; _mode=M_BROWSE; _needsRedraw=true; return; }
    bool goLeft  = k.left  || (!k.fn && k.ch==',');
    bool goRight = k.right || (!k.fn && k.ch=='/');
    if (goLeft)  { _swapWithAdjacent(-1); _needsRedraw=true; return; }
    if (goRight) { _swapWithAdjacent(1);  _needsRedraw=true; return; }
}

// ---- Dispatch ----

void AppRegEdit::_draw() {
    switch (_mode) {
        case M_BROWSE:            _drawBrowse();           break;
        case M_EDIT_NAME:         _drawEditName();         break;
        case M_EDIT_CONTENT:      _drawEditContent();      break;
        case M_CONFIRM_SAVE_EXIT: _drawConfirmSaveExit();  break;
        case M_CONFIRM_DEL:       _drawConfirmDelete();    break;
        case M_CONFIRM_DEL_ALL:   _drawConfirmDeleteAll(); break;
        case M_MOVE:              _drawMove();             break;
    }
    _needsRedraw = false;
}

void AppRegEdit::onEnter() {
    _mode       = M_BROWSE;
    _needsRedraw = true;
    _statusMsg  = "";
    _regIdx     = (!registers.empty() && activeRegister<(int)registers.size())
                  ? activeRegister : 0;
}

void AppRegEdit::onExit() {
    _nameBuf=""; _contentBuf=""; _statusMsg=""; _mode=M_BROWSE;
}

void AppRegEdit::onUpdate() {
    if (_needsRedraw) _draw();
    switch (_mode) {
        case M_BROWSE:            _handleBrowse();           break;
        case M_EDIT_NAME:         _handleEditName();         break;
        case M_EDIT_CONTENT:      _handleEditContent();      break;
        case M_CONFIRM_SAVE_EXIT: _handleConfirmSaveExit();  break;
        case M_CONFIRM_DEL:       _handleConfirmDelete();    break;
        case M_CONFIRM_DEL_ALL:   _handleConfirmDeleteAll(); break;
        case M_MOVE:              _handleMove();             break;
    }
}

} // namespace Cardputer
#endif
