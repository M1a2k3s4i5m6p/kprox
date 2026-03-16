#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_schedprox.h"
#include "../scheduled_tasks.h"
#include <time.h>

namespace Cardputer {

static constexpr uint16_t SP_BG    = 0x0821;
static constexpr int      SP_BAR_H = 16;
static constexpr int      SP_BOT_H = 14;
static constexpr int      SP_Y     = SP_BAR_H + 3;

void AppSchedProx::_resetDraft() {
    _draft         = ScheduledTask{};
    _draft.enabled = true;
    _draft.repeat  = false;
}

// ---- datetime field helpers ----

int AppSchedProx::_dtGet(int cursor) const {
    switch (cursor) {
        case 0: return _draft.year;
        case 1: return _draft.month;
        case 2: return _draft.day;
        case 3: return _draft.hour;
        case 4: return _draft.minute;
        case 5: return _draft.second;
    }
    return 0;
}

void AppSchedProx::_dtSet(int cursor, int val) {
    switch (cursor) {
        case 0: _draft.year   = val; break;
        case 1: _draft.month  = val; break;
        case 2: _draft.day    = val; break;
        case 3: _draft.hour   = val; break;
        case 4: _draft.minute = val; break;
        case 5: _draft.second = val; break;
    }
}

int AppSchedProx::_dtMax(int cursor) const {
    switch (cursor) {
        case 0: return 2099;
        case 1: return 12;
        case 2: return 31;
        case 3: return 23;
        case 4: return 59;
        case 5: return 59;
    }
    return 0;
}

const char* AppSchedProx::_dtLabel(int cursor) const {
    static const char* labels[6] = { "Y", "Mo", "D", "H", "Mi", "S" };
    return labels[cursor];
}

void AppSchedProx::_dtIncrement(int cursor, int delta) {
    int maxVal = _dtMax(cursor);
    int cur    = _dtGet(cursor);
    cur += delta;
    if (cur < 0)       cur = maxVal;
    if (cur > maxVal)  cur = 0;
    _dtSet(cursor, cur);
}

// ---- bars ----

void AppSchedProx::_drawTopBar(const char* title) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(130, 90, 0);
    disp.fillRect(0, 0, disp.width(), SP_BAR_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bc);
    disp.drawString("SchedProx", 4, 4);
    int tw = disp.textWidth(title);
    disp.setTextColor(disp.color565(255, 220, 100), bc);
    disp.drawString(title, disp.width() - tw - 4, 4);
}

void AppSchedProx::_drawBottomBar(const char* hint) {
    auto& disp = M5Cardputer.Display;
    uint16_t bc = disp.color565(16, 16, 16);
    disp.fillRect(0, disp.height() - SP_BOT_H, disp.width(), SP_BOT_H, bc);
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(110, 110, 110), bc);
    disp.drawString(hint, 2, disp.height() - SP_BOT_H + 2);
}

// ---- List ----

void AppSchedProx::_drawList() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    _drawTopBar("Tasks");

    int y       = SP_Y;
    int rowH    = 17;
    int visible = (disp.height() - SP_BAR_H - SP_BOT_H - 4) / rowH;

    if (scheduledTasks.empty()) {
        disp.setTextColor(disp.color565(100, 100, 100), SP_BG);
        disp.setTextSize(1);
        disp.drawString("No scheduled tasks.", 4, y + 10);
        disp.drawString("Press N to add one.", 4, y + 26);
        _drawBottomBar("N=new  ESC back");
        return;
    }

    if (_listSel < _listScroll) _listScroll = _listSel;
    if (_listSel >= _listScroll + visible) _listScroll = _listSel - visible + 1;

    for (int i = 0; i < visible; i++) {
        int idx = i + _listScroll;
        if (idx >= (int)scheduledTasks.size()) break;
        const ScheduledTask& t = scheduledTasks[idx];
        bool sel = (idx == _listSel);
        uint16_t rowBg = sel ? disp.color565(60, 40, 0) : (uint16_t)SP_BG;
        if (sel) disp.fillRect(0, y - 1, disp.width(), rowH, rowBg);

        disp.fillCircle(6, y + 6, 3,
            t.enabled ? disp.color565(80, 220, 80) : disp.color565(100, 100, 100));

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(200, 200, 200), rowBg);
        String lbl = (sel ? "> " : "  ") +
                     (t.label.isEmpty() ? String("Task " + String(t.id)) : t.label);
        if ((int)lbl.length() > 18) lbl = lbl.substring(0, 18);
        disp.drawString(lbl, 12, y);

        char sched[20];
        if (t.year > 0)
            snprintf(sched, sizeof(sched), "%d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour, t.minute);
        else if (t.day > 0)
            snprintf(sched, sizeof(sched), "*-%02d %02d:%02d:%02d", t.day, t.hour, t.minute, t.second);
        else
            snprintf(sched, sizeof(sched), "daily %02d:%02d:%02d", t.hour, t.minute, t.second);

        int sw = disp.textWidth(sched);
        disp.setTextColor(sel ? disp.color565(255, 200, 80) : disp.color565(120, 120, 120), rowBg);
        disp.drawString(sched, disp.width() - sw - 2, y);
        y += rowH;
    }

    if ((int)scheduledTasks.size() > visible) {
        char pg[10]; snprintf(pg, sizeof(pg), "%d/%d", _listSel + 1, (int)scheduledTasks.size());
        disp.setTextColor(disp.color565(80, 80, 80), SP_BG);
        disp.drawString(pg, disp.width() - disp.textWidth(pg) - 2, disp.height() - SP_BOT_H - 10);
    }

    _drawBottomBar("up/dn  ENTER view  N=new  D=del  ESC");
}

void AppSchedProx::_handleList(KeyInput ki) {
    int n = max(1, (int)scheduledTasks.size());
    if (ki.arrowUp)   { _listSel = (_listSel - 1 + n) % n; _needsRedraw = true; }
    if (ki.arrowDown) { _listSel = (_listSel + 1) % n;     _needsRedraw = true; }
    if (ki.esc)       { uiManager.returnToLauncher(); return; }
    if (ki.enter && !scheduledTasks.empty()) { _state = ST_VIEW; _needsRedraw = true; return; }
    if (ki.ch == 'n' || ki.ch == 'N') {
        _resetDraft(); _addSection = 0; _dtCursor = 0; _editBuf = ""; _editingText = false;
        _state = ST_ADD; _needsRedraw = true;
    }
    if ((ki.ch == 'd' || ki.ch == 'D') && !scheduledTasks.empty()) {
        _state = ST_CONFIRM_DELETE; _needsRedraw = true;
    }
}

// ---- View ----

void AppSchedProx::_drawView(const ScheduledTask& t) {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    String title = t.label.isEmpty() ? String("Task " + String(t.id)) : t.label;
    _drawTopBar(title.c_str());

    int y = SP_Y;
    disp.setTextSize(1);

    auto row = [&](const char* lbl, const String& val, uint16_t vc) {
        disp.setTextColor(disp.color565(130, 130, 130), SP_BG);
        disp.drawString(lbl, 4, y);
        int lw = disp.textWidth(lbl) + 4;
        disp.setTextColor(vc, SP_BG);
        String v = val;
        while ((int)v.length() > 0 && disp.textWidth(v) > (disp.width() - lw - 6))
            v = v.substring(0, v.length() - 1);
        disp.drawString(v, 4 + lw, y);
        y += 13;
    };

    char sched[32];
    snprintf(sched, sizeof(sched), "%04d-%02d-%02d  %02d:%02d:%02d",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
    row("When:",    sched,          TFT_YELLOW);
    row("Repeat:",  t.repeat  ? "yes" : "no", t.repeat  ? disp.color565(80, 220, 80) : disp.color565(180, 80, 80));
    row("Enabled:", t.enabled ? "yes" : "no", t.enabled ? disp.color565(80, 220, 80) : disp.color565(160, 160, 160));

    disp.setTextColor(disp.color565(130, 130, 130), SP_BG);
    disp.drawString("Payload:", 4, y); y += 12;
    disp.setTextColor(disp.color565(180, 220, 180), SP_BG);
    String pay = t.payload;
    int maxCh = (disp.width() - 8) / 6;
    if ((int)pay.length() > maxCh * 2) pay = pay.substring(0, maxCh * 2 - 3) + "...";
    if ((int)pay.length() > maxCh) {
        disp.drawString(pay.substring(0, maxCh), 4, y); y += 12;
        disp.drawString(pay.substring(maxCh),    4, y);
    } else {
        disp.drawString(pay, 4, y);
    }

    _drawBottomBar("E=toggle enable  D=delete  ESC back");
}

void AppSchedProx::_handleView(KeyInput ki) {
    if (ki.esc) { _state = ST_LIST; _needsRedraw = true; return; }
    if (_listSel >= (int)scheduledTasks.size()) { _state = ST_LIST; _needsRedraw = true; return; }
    ScheduledTask& t = scheduledTasks[_listSel];
    if (ki.ch == 'e' || ki.ch == 'E') { t.enabled = !t.enabled; saveScheduledTasks(); _needsRedraw = true; }
    if (ki.ch == 'd' || ki.ch == 'D') { _state = ST_CONFIRM_DELETE; _needsRedraw = true; }
}

// ---- Add form ----
// Sections: 0=label  1=datetime  2=payload  3=options(repeat)

static void drawFieldBox(int x, int y, int w, const String& text, bool active) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = disp.color565(40, 40, 40);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);
    int maxCh = (w - 6) / 6;
    String s = text;
    if ((int)s.length() > maxCh) s = s.substring(s.length() - maxCh);
    if (active) s += "_";
    disp.drawString(s, x + 3, y + 3);
}

void AppSchedProx::_drawAdd() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    _drawTopBar("New Task");

    int y = SP_Y + 2;
    disp.setTextSize(1);

    // --- Section 0: Label ---
    bool s0sel = (_addSection == 0);
    uint16_t s0bg = s0sel ? disp.color565(40, 30, 0) : (uint16_t)SP_BG;
    if (s0sel) disp.fillRect(0, y - 1, disp.width(), 16, s0bg);
    disp.setTextColor(s0sel ? TFT_WHITE : disp.color565(160, 160, 160), s0bg);
    disp.drawString(s0sel ? "> Label:" : "  Label:", 4, y);
    drawFieldBox(70, y, disp.width() - 74, _draft.label,
                 s0sel && _editingText);
    y += 18;

    // --- Section 1: DateTime ---
    bool s1sel = (_addSection == 1);
    uint16_t s1bg = s1sel ? disp.color565(0, 30, 50) : (uint16_t)SP_BG;
    if (s1sel) disp.fillRect(0, y - 1, disp.width(), 28, s1bg);

    disp.setTextColor(s1sel ? TFT_WHITE : disp.color565(160, 160, 160), s1bg);
    disp.drawString(s1sel ? "> When:" : "  When:", 4, y);

    // hint: 0=any for Y/Mo/D
    disp.setTextColor(disp.color565(80, 80, 80), s1bg);
    disp.drawString("0=any", disp.width() - 36, y);
    y += 11;

    // Draw 6 datetime fields inline
    static const int fieldW[6] = { 34, 20, 18, 18, 18, 18 };
    static const char* sep[6]  = { "-", "-", " ", ":", ":", "" };
    int fx = 10;
    for (int i = 0; i < 6; i++) {
        bool cur = (s1sel && _dtCursor == i);
        uint16_t fbg = cur ? disp.color565(20, 60, 100) : disp.color565(35, 35, 35);
        uint16_t ftc = cur ? TFT_WHITE : disp.color565(180, 180, 180);
        disp.fillRoundRect(fx, y, fieldW[i], 14, 2, fbg);
        disp.setTextColor(ftc, fbg);

        int v = _dtGet(i);
        char buf[8];
        if (i == 0) snprintf(buf, sizeof(buf), v ? "%d" : "----", v);
        else        snprintf(buf, sizeof(buf), v ? "%02d" : "--", v);

        int tw = disp.textWidth(buf);
        disp.drawString(buf, fx + (fieldW[i] - tw) / 2, y + 3);
        fx += fieldW[i];

        if (sep[i][0]) {
            disp.setTextColor(disp.color565(100, 100, 100), s1bg);
            disp.drawString(sep[i], fx, y + 3);
            fx += disp.textWidth(sep[i]) + 1;
        }
    }

    if (s1sel) {
        disp.setTextColor(disp.color565(100, 160, 220), s1bg);
        disp.drawString("</> field  up/dn value", 10, y + 16);
    }
    y += s1sel ? 32 : 18;

    // --- Section 2: Payload ---
    bool s2sel = (_addSection == 2);
    uint16_t s2bg = s2sel ? disp.color565(0, 40, 20) : (uint16_t)SP_BG;
    if (s2sel) disp.fillRect(0, y - 1, disp.width(), 16, s2bg);
    disp.setTextColor(s2sel ? TFT_WHITE : disp.color565(160, 160, 160), s2bg);
    disp.drawString(s2sel ? "> Payload:" : "  Payload:", 4, y);
    drawFieldBox(66, y, disp.width() - 70, _draft.payload,
                 s2sel && _editingText);
    y += 18;

    // --- Section 3: Options ---
    bool s3sel = (_addSection == 3);
    uint16_t s3bg = s3sel ? disp.color565(40, 20, 40) : (uint16_t)SP_BG;
    if (s3sel) disp.fillRect(0, y - 1, disp.width(), 14, s3bg);
    disp.setTextColor(s3sel ? TFT_WHITE : disp.color565(160, 160, 160), s3bg);
    char optBuf[32];
    snprintf(optBuf, sizeof(optBuf), "%s Repeat: %s", s3sel ? ">" : " ", _draft.repeat ? "YES" : "no");
    disp.drawString(optBuf, 4, y);
    y += 18;

    // --- Save button row ---
    bool saveSel = (_addSection == 4);
    uint16_t saveBg = saveSel ? disp.color565(20, 70, 20) : disp.color565(20, 50, 20);
    disp.fillRoundRect(4, y, 60, 14, 3, saveBg);
    disp.setTextColor(saveSel ? TFT_WHITE : disp.color565(120, 200, 120), saveBg);
    disp.drawString(saveSel ? "> SAVE" : "  save", 8, y + 3);

    if (_editingText)
        _drawBottomBar("type  DEL=backspace  ENTER=confirm  ESC=cancel");
    else
        _drawBottomBar("up/dn section  ENTER=select  ESC=back");
}

void AppSchedProx::_handleAdd(KeyInput ki) {
    // --- Text editing mode ---
    if (_editingText) {
        if (ki.esc)  { _editingText = false; _editBuf = ""; _needsRedraw = true; return; }
        if (ki.del && _editBuf.length() > 0) { _editBuf.remove(_editBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.enter) {
            if      (_addSection == 0) _draft.label   = _editBuf;
            else if (_addSection == 2) _draft.payload = _editBuf;
            _editingText = false; _editBuf = ""; _needsRedraw = true; return;
        }
        if (ki.ch) { _editBuf += ki.ch; _needsRedraw = true; }
        return;
    }

    // --- DateTime section ---
    if (_addSection == 1) {
        if (ki.arrowLeft)  { _dtCursor = (_dtCursor - 1 + 6) % 6; _needsRedraw = true; return; }
        if (ki.arrowRight) { _dtCursor = (_dtCursor + 1) % 6;      _needsRedraw = true; return; }
        if (ki.arrowUp)    { _dtIncrement(_dtCursor, 1);  _needsRedraw = true; return; }
        if (ki.arrowDown)  { _dtIncrement(_dtCursor, -1); _needsRedraw = true; return; }
        // digits: type directly into the current field
        if (ki.ch >= '0' && ki.ch <= '9') {
            int cur = _dtGet(_dtCursor);
            int nxt = cur * 10 + (ki.ch - '0');
            int mx  = _dtMax(_dtCursor);
            if (nxt > mx) nxt = ki.ch - '0';
            _dtSet(_dtCursor, nxt);
            _needsRedraw = true; return;
        }
        if (ki.enter) { _addSection = 2; _needsRedraw = true; return; }
        if (ki.esc)   { _state = ST_LIST; _needsRedraw = true; return; }
        return;
    }

    // --- Navigation between sections ---
    if (ki.esc)       { _state = ST_LIST; _needsRedraw = true; return; }
    if (ki.arrowUp)   { _addSection = (_addSection - 1 + 5) % 5; _needsRedraw = true; return; }
    if (ki.arrowDown) { _addSection = (_addSection + 1) % 5;      _needsRedraw = true; return; }

    if (ki.enter) {
        switch (_addSection) {
            case 0:
                _editBuf = _draft.label; _editingText = true; _needsRedraw = true; break;
            case 1:
                // handled above, but belt-and-suspenders
                break;
            case 2:
                _editBuf = _draft.payload; _editingText = true; _needsRedraw = true; break;
            case 3:
                _draft.repeat = !_draft.repeat; _needsRedraw = true; break;
            case 4:
                if (_draft.payload.isEmpty()) { _addSection = 2; _editingText = true; _editBuf = ""; _needsRedraw = true; break; }
                addScheduledTask(_draft);
                _state = ST_LIST;
                _listSel = (int)scheduledTasks.size() - 1;
                _needsRedraw = true; break;
        }
    }
}

// ---- Confirm Delete ----

void AppSchedProx::_drawConfirmDelete(const ScheduledTask& t) {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    _drawTopBar("Delete?");
    String lbl = t.label.isEmpty() ? String("Task " + String(t.id)) : t.label;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220, 80, 80), SP_BG);
    disp.drawString("Delete task:", 4, SP_Y + 6);
    disp.setTextColor(TFT_WHITE, SP_BG);
    disp.drawString(lbl, 4, SP_Y + 20);
    disp.setTextColor(disp.color565(160, 160, 160), SP_BG);
    disp.drawString("Y = confirm  N/ESC = cancel", 4, SP_Y + 40);
    _drawBottomBar("Y=delete  N/ESC=cancel");
}

void AppSchedProx::_handleConfirmDelete(KeyInput ki) {
    if (ki.esc || ki.ch == 'n' || ki.ch == 'N') {
        _state = (_listSel < (int)scheduledTasks.size()) ? ST_VIEW : ST_LIST;
        _needsRedraw = true; return;
    }
    if (ki.ch == 'y' || ki.ch == 'Y') {
        if (_listSel < (int)scheduledTasks.size()) {
            deleteScheduledTask(scheduledTasks[_listSel].id);
            if (_listSel >= (int)scheduledTasks.size() && _listSel > 0) _listSel--;
        }
        _state = ST_LIST; _needsRedraw = true;
    }
}

// ---- AppBase ----

void AppSchedProx::onEnter() {
    _state = ST_LIST; _listSel = 0; _listScroll = 0; _needsRedraw = true;
}

void AppSchedProx::onExit() {}

void AppSchedProx::onUpdate() {
    if (_needsRedraw) {
        switch (_state) {
            case ST_LIST:
                _drawList();
                break;
            case ST_VIEW:
                if (_listSel < (int)scheduledTasks.size())
                    _drawView(scheduledTasks[_listSel]);
                else { _state = ST_LIST; _drawList(); }
                break;
            case ST_ADD:
                _drawAdd();
                break;
            case ST_CONFIRM_DELETE:
                if (_listSel < (int)scheduledTasks.size())
                    _drawConfirmDelete(scheduledTasks[_listSel]);
                else { _state = ST_LIST; _drawList(); }
                break;
        }
        _needsRedraw = false;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    switch (_state) {
        case ST_LIST:           _handleList(ki);          break;
        case ST_VIEW:           _handleView(ki);          break;
        case ST_ADD:            _handleAdd(ki);           break;
        case ST_CONFIRM_DELETE: _handleConfirmDelete(ki); break;
    }
}

} // namespace Cardputer
#endif
