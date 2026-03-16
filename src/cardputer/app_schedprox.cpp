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

static const char* FIELD_NAMES[9] = {
    "Label", "Year (0=any)", "Month (0=any)", "Day (0=any)",
    "Hour", "Minute", "Second", "Payload", "Repeat? (y/n)"
};

bool AppSchedProx::_fieldIsNumeric(int fi) {
    return fi >= 1 && fi <= 6;
}

void AppSchedProx::_resetDraft() {
    _draft          = ScheduledTask{};
    _draft.enabled  = true;
    _draft.repeat   = false;
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

    int y        = SP_Y;
    int rowH     = 17;
    int visible  = (disp.height() - SP_BAR_H - SP_BOT_H - 4) / rowH;

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
            t.enabled ? disp.color565(80,220,80) : disp.color565(100,100,100));

        disp.setTextSize(1);
        disp.setTextColor(sel ? TFT_WHITE : disp.color565(200,200,200), rowBg);
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
        disp.setTextColor(sel ? disp.color565(255,200,80) : disp.color565(120,120,120), rowBg);
        disp.drawString(sched, disp.width() - sw - 2, y);
        y += rowH;
    }

    if ((int)scheduledTasks.size() > visible) {
        char pg[10]; snprintf(pg, sizeof(pg), "%d/%d", _listSel+1, (int)scheduledTasks.size());
        disp.setTextColor(disp.color565(80,80,80), SP_BG);
        disp.drawString(pg, disp.width()-disp.textWidth(pg)-2, disp.height()-SP_BOT_H-10);
    }

    _drawBottomBar("up/dn  ENTER view  N=new  D=del  ESC");
}

void AppSchedProx::_handleList(KeyInput ki) {
    int n = max(1, (int)scheduledTasks.size());
    if (ki.arrowUp)   { _listSel = (_listSel-1+n)%n; _needsRedraw = true; }
    if (ki.arrowDown) { _listSel = (_listSel+1)%n;   _needsRedraw = true; }
    if (ki.esc)       { uiManager.returnToLauncher(); return; }
    if (ki.enter && !scheduledTasks.empty()) { _state = ST_VIEW; _needsRedraw = true; return; }
    if ((ki.ch=='n'||ki.ch=='N')) {
        _resetDraft(); _fieldSel = 0; _editBuf = "";
        _state = ST_ADD_FIELD; _needsRedraw = true;
    }
    if ((ki.ch=='d'||ki.ch=='D') && !scheduledTasks.empty()) {
        _state = ST_CONFIRM_DELETE; _needsRedraw = true;
    }
}

// ---- View ----

void AppSchedProx::_drawView(const ScheduledTask& t) {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    String title = t.label.isEmpty() ? String("Task "+String(t.id)) : t.label;
    _drawTopBar(title.c_str());

    int y = SP_Y;
    disp.setTextSize(1);

    auto row = [&](const char* lbl, const String& val, uint16_t vc) {
        disp.setTextColor(disp.color565(130,130,130), SP_BG);
        disp.drawString(lbl, 4, y);
        int lw = disp.textWidth(lbl)+4;
        disp.setTextColor(vc, SP_BG);
        String v = val;
        while ((int)v.length()>0 && disp.textWidth(v)>(disp.width()-lw-6))
            v = v.substring(0, v.length()-1);
        disp.drawString(v, 4+lw, y);
        y += 13;
    };

    char sched[32];
    snprintf(sched, sizeof(sched), "%04d-%02d-%02d  %02d:%02d:%02d",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
    row("When:",    sched,          TFT_YELLOW);
    row("Repeat:",  t.repeat?"yes":"no", t.repeat?disp.color565(80,220,80):disp.color565(180,80,80));
    row("Enabled:", t.enabled?"yes":"no", t.enabled?disp.color565(80,220,80):disp.color565(160,160,160));

    // Payload — may be long, wrap across 2 lines
    disp.setTextColor(disp.color565(130,130,130), SP_BG);
    disp.drawString("Payload:", 4, y); y += 12;
    disp.setTextColor(disp.color565(180,220,180), SP_BG);
    String pay = t.payload;
    int maxCh = (disp.width()-8) / 6;
    if ((int)pay.length() > maxCh*2) pay = pay.substring(0, maxCh*2-3) + "...";
    if ((int)pay.length() > maxCh) {
        disp.drawString(pay.substring(0, maxCh), 4, y); y += 12;
        disp.drawString(pay.substring(maxCh),    4, y); y += 12;
    } else {
        disp.drawString(pay, 4, y); y += 12;
    }

    // Toggle enable hint
    disp.setTextColor(disp.color565(80,80,100), SP_BG);
    disp.drawString("E=toggle enable", 4, y);

    _drawBottomBar("E=toggle  D=delete  ESC back");
}

void AppSchedProx::_handleView(KeyInput ki) {
    if (ki.esc) { _state = ST_LIST; _needsRedraw = true; return; }
    if (_listSel >= (int)scheduledTasks.size()) { _state = ST_LIST; _needsRedraw = true; return; }
    ScheduledTask& t = scheduledTasks[_listSel];
    if (ki.ch=='e'||ki.ch=='E') { t.enabled = !t.enabled; saveScheduledTasks(); _needsRedraw = true; }
    if (ki.ch=='d'||ki.ch=='D') { _state = ST_CONFIRM_DELETE; _needsRedraw = true; }
}

// ---- Add / Edit ----

void AppSchedProx::_drawAdd() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    _drawTopBar("New Task");

    int y = SP_Y;
    disp.setTextSize(1);

    // Show filled fields summary at top
    auto preview = [&](const char* lbl, const String& val) {
        if (val.isEmpty() || val == "0") return;
        disp.setTextColor(disp.color565(100,100,100), SP_BG);
        disp.drawString(lbl, 4, y);
        disp.setTextColor(disp.color565(180,180,180), SP_BG);
        disp.drawString(val.substring(0,18), 4+disp.textWidth(lbl)+4, y);
        y += 11;
    };
    if (!_draft.label.isEmpty()) preview("Label:", _draft.label);
    char sched[24];
    snprintf(sched,sizeof(sched),"%04d-%02d-%02d %02d:%02d:%02d",
             _draft.year,_draft.month,_draft.day,_draft.hour,_draft.minute,_draft.second);
    preview("When: ", sched);
    if (!_draft.payload.isEmpty()) {
        String ps = _draft.payload;
        if ((int)ps.length()>22) ps = ps.substring(0,19)+"...";
        preview("Pay:  ", ps);
    }

    y = max(y + 4, SP_Y + 44);

    // Current field
    disp.setTextColor(TFT_YELLOW, SP_BG);
    char fieldHdr[40]; snprintf(fieldHdr, sizeof(fieldHdr), "Field %d/9: %s", _fieldSel+1, FIELD_NAMES[_fieldSel]);
    disp.drawString(fieldHdr, 4, y); y += 14;

    // Input box
    uint16_t fbg = disp.color565(40,40,40);
    disp.fillRect(4, y, disp.width()-8, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    String display = _editBuf;
    int maxCh = (disp.width()-12)/6;
    if ((int)display.length() > maxCh) display = display.substring(display.length()-maxCh);
    disp.drawString(display + "_", 6, y+2);

    _drawBottomBar("type  ENTER next  ESC cancel  </> field");
}

void AppSchedProx::_handleAdd(KeyInput ki) {
    static const int NFIELDS = 9;

    if (ki.esc)        { _state = ST_LIST; _needsRedraw = true; return; }
    if (ki.arrowLeft)  { _fieldSel = (_fieldSel-1+NFIELDS)%NFIELDS; _editBuf = _draftField(_fieldSel); _needsRedraw = true; return; }
    if (ki.arrowRight) { _fieldSel = (_fieldSel+1)%NFIELDS;          _editBuf = _draftField(_fieldSel); _needsRedraw = true; return; }

    if (ki.del && _editBuf.length()>0) { _editBuf.remove(_editBuf.length()-1); _needsRedraw = true; return; }

    if (ki.enter) {
        _applyField(_fieldSel, _editBuf);
        if (_fieldSel < NFIELDS-1) {
            _fieldSel++;
            _editBuf = _draftField(_fieldSel);
        } else {
            // Save
            addScheduledTask(_draft);
            _state = ST_LIST;
            _listSel = (int)scheduledTasks.size()-1;
        }
        _needsRedraw = true;
        return;
    }

    // Numeric fields: digits only
    if (_fieldIsNumeric(_fieldSel)) {
        if (ki.ch >= '0' && ki.ch <= '9') { _editBuf += ki.ch; _needsRedraw = true; }
    } else if (_fieldSel == 8) {
        // repeat: y/n
        if (ki.ch=='y'||ki.ch=='Y') { _editBuf = "y"; _needsRedraw = true; }
        if (ki.ch=='n'||ki.ch=='N') { _editBuf = "n"; _needsRedraw = true; }
    } else {
        if (ki.ch) { _editBuf += ki.ch; _needsRedraw = true; }
    }
}

String AppSchedProx::_draftField(int fi) {
    switch (fi) {
        case 0: return _draft.label;
        case 1: return _draft.year   ? String(_draft.year)   : "";
        case 2: return _draft.month  ? String(_draft.month)  : "";
        case 3: return _draft.day    ? String(_draft.day)    : "";
        case 4: return String(_draft.hour);
        case 5: return String(_draft.minute);
        case 6: return String(_draft.second);
        case 7: return _draft.payload;
        case 8: return _draft.repeat ? "y" : "n";
    }
    return "";
}

void AppSchedProx::_applyField(int fi, const String& val) {
    switch (fi) {
        case 0: _draft.label   = val; break;
        case 1: _draft.year    = val.toInt(); break;
        case 2: _draft.month   = val.toInt(); break;
        case 3: _draft.day     = val.toInt(); break;
        case 4: _draft.hour    = val.toInt(); break;
        case 5: _draft.minute  = val.toInt(); break;
        case 6: _draft.second  = val.toInt(); break;
        case 7: _draft.payload = val; break;
        case 8: _draft.repeat  = (val == "y" || val == "Y"); break;
    }
}

// ---- Confirm Delete ----

void AppSchedProx::_drawConfirmDelete(const ScheduledTask& t) {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(SP_BG);
    _drawTopBar("Delete?");

    String lbl = t.label.isEmpty() ? String("Task "+String(t.id)) : t.label;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220,80,80), SP_BG);
    disp.drawString("Delete task:", 4, SP_Y+6);
    disp.setTextColor(TFT_WHITE, SP_BG);
    disp.drawString(lbl, 4, SP_Y+20);
    disp.setTextColor(disp.color565(160,160,160), SP_BG);
    disp.drawString("Y = confirm  N/ESC = cancel", 4, SP_Y+40);

    _drawBottomBar("Y=delete  N/ESC=cancel");
}

void AppSchedProx::_handleConfirmDelete(KeyInput ki) {
    if (ki.esc || ki.ch=='n' || ki.ch=='N') {
        _state = (_listSel < (int)scheduledTasks.size()) ? ST_VIEW : ST_LIST;
        _needsRedraw = true; return;
    }
    if (ki.ch=='y' || ki.ch=='Y') {
        if (_listSel < (int)scheduledTasks.size()) {
            int id = scheduledTasks[_listSel].id;
            deleteScheduledTask(id);
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
            case ST_ADD_FIELD:
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
        case ST_ADD_FIELD:      _handleAdd(ki);           break;
        case ST_CONFIRM_DELETE: _handleConfirmDelete(ki); break;
    }
}

} // namespace Cardputer
#endif
