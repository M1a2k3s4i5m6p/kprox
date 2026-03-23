#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_schedprox.h"
#include "ui_manager.h"
#include "../scheduled_tasks.h"
#include "../registers.h"
#include <time.h>

namespace Cardputer {

static constexpr uint16_t SP_BG = 0x0821;

// ---- helpers ----

void AppSchedProx::_resetDraft() {
    _draft         = ScheduledTask{};
    _draft.enabled = true;
    _draft.repeat  = false;
    _usingReg      = true;
    _p1Field       = 0;
    _regScroll     = 0;
    _p2Sel         = 0;
    _dtCursor      = 0;
    _dtActive      = false;
    _dtTyping      = false;
    _dtTypeBuf     = "";
    _labelBuf      = "";
    _payloadBuf    = "";
    _labelActive   = false;
    _payloadActive = false;
    _applyNow();
}

void AppSchedProx::_applyReg(int idx) {
    if (idx < 0 || idx >= (int)registers.size()) return;
    _draft.payload = "{EXEC " + String(idx + 1) + "}";
    String name = (idx < (int)registerNames.size()) ? registerNames[idx] : "";
    if (!name.isEmpty()) _draft.payload = "{EXEC " + String(idx + 1) + "}"; // keep token
    _payloadBuf = _draft.payload;
}

void AppSchedProx::_applyNow() {
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    _draft.year   = tm->tm_year + 1900;
    _draft.month  = tm->tm_mon  + 1;
    _draft.day    = tm->tm_mday;
    _draft.hour   = tm->tm_hour;
    _draft.minute = tm->tm_min;
    _draft.second = 0;
}

void AppSchedProx::_shiftTime(long secs) {
    struct tm tm = {};
    tm.tm_year = _draft.year  - 1900;
    tm.tm_mon  = _draft.month - 1;
    tm.tm_mday = _draft.day;
    tm.tm_hour = _draft.hour;
    tm.tm_min  = _draft.minute;
    tm.tm_sec  = _draft.second;
    time_t t   = mktime(&tm) + secs;
    struct tm* nt = localtime(&t);
    _draft.year   = nt->tm_year + 1900;
    _draft.month  = nt->tm_mon  + 1;
    _draft.day    = nt->tm_mday;
    _draft.hour   = nt->tm_hour;
    _draft.minute = nt->tm_min;
    _draft.second = nt->tm_sec;
}

int AppSchedProx::_dtGet(int c) const {
    switch (c) {
        case 0: return _draft.year;   case 1: return _draft.month;
        case 2: return _draft.day;    case 3: return _draft.hour;
        case 4: return _draft.minute; case 5: return _draft.second;
    }
    return 0;
}
void AppSchedProx::_dtSet(int c, int v) {
    switch (c) {
        case 0: _draft.year   = v; break; case 1: _draft.month  = v; break;
        case 2: _draft.day    = v; break; case 3: _draft.hour   = v; break;
        case 4: _draft.minute = v; break; case 5: _draft.second = v; break;
    }
}
int AppSchedProx::_dtMax(int c) const {
    switch (c) { case 0: return 2099; case 1: return 12; case 2: return 31;
                 case 3: return 23;   case 4: return 59; case 5: return 59; }
    return 0;
}

String AppSchedProx::_countdown(const ScheduledTask& t) const {
    if (!t.enabled) return "off";
    time_t now = time(nullptr);
    struct tm tm = {};
    tm.tm_year = t.year  - 1900;
    tm.tm_mon  = t.month - 1;
    tm.tm_mday = t.day;
    tm.tm_hour = t.hour;
    tm.tm_min  = t.minute;
    tm.tm_sec  = t.second;
    time_t target = mktime(&tm);
    long diff = (long)(target - now);
    if (diff <= 0) return "due";
    if (diff < 60)   { char b[8];  snprintf(b, sizeof(b), "%lds",  diff);           return b; }
    if (diff < 3600) { char b[10]; snprintf(b, sizeof(b), "%ldm",  diff/60);         return b; }
    if (diff < 86400){ char b[10]; snprintf(b, sizeof(b), "%ldh",  diff/3600);       return b; }
                     { char b[10]; snprintf(b, sizeof(b), "%ldd",  diff/86400);      return b; }
}

// ---- drawing ----

void AppSchedProx::_drawTopBar(const char* title) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(130, 90, 0);
    d.fillRect(0, 0, d.width(), SP_BAR_H, bc);
    d.setTextSize(1); d.setTextColor(TFT_WHITE, bc);
    d.drawString("SchedProx", 4, 4);
    int tw = d.textWidth(title);
    d.setTextColor(d.color565(255, 220, 100), bc);
    d.drawString(title, d.width() - tw - 4, 4);
    drawTabHint(4 + d.textWidth("SchedProx") + 3);
}

void AppSchedProx::_drawBottomBar(const char* hint) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(16, 16, 16);
    d.fillRect(0, d.height() - SP_BOT_H, d.width(), SP_BOT_H, bc);
    d.setTextSize(1); d.setTextColor(d.color565(110, 110, 110), bc);
    d.drawString(hint, 2, d.height() - SP_BOT_H + 2);
}

void AppSchedProx::_drawList() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(SP_BG);
    _drawTopBar("Tasks");
    int y    = SP_BAR_H + 3;
    int rowH = 17;
    int vis  = (d.height() - SP_BAR_H - SP_BOT_H - 4) / rowH;

    if (scheduledTasks.empty()) {
        d.setTextColor(d.color565(100,100,100), SP_BG);
        d.setTextSize(1);
        d.drawString("No tasks.  Press N to add.", 4, y + 10);
        _drawBottomBar("N=new  ESC back");
        return;
    }

    if (_listSel < _listScroll) _listScroll = _listSel;
    if (_listSel >= _listScroll + vis) _listScroll = _listSel - vis + 1;

    for (int i = 0; i < vis; i++) {
        int idx = i + _listScroll;
        if (idx >= (int)scheduledTasks.size()) break;
        const ScheduledTask& t = scheduledTasks[idx];
        bool sel = (idx == _listSel);
        uint16_t bg = sel ? d.color565(60, 40, 0) : (uint16_t)SP_BG;
        if (sel) d.fillRect(0, y - 1, d.width(), rowH, bg);

        d.fillCircle(6, y + 7, 3, t.enabled ? d.color565(80,220,80) : d.color565(80,80,80));

        d.setTextSize(1);
        d.setTextColor(sel ? TFT_WHITE : d.color565(200,200,200), bg);
        String lbl = t.label.isEmpty() ? ("Task " + String(t.id)) : t.label;
        if ((int)lbl.length() > 16) lbl = lbl.substring(0,15) + ".";
        d.drawString(lbl, 14, y + 3);

        // countdown on right
        String cd = _countdown(t);
        int cw = d.textWidth(cd);
        uint16_t cc = t.enabled ? d.color565(255,200,80) : d.color565(90,90,90);
        d.setTextColor(cc, bg);
        d.drawString(cd, d.width() - cw - 3, y + 3);

        y += rowH;
    }

    _drawBottomBar("up/dn  ENTER=view  N=new  D=del  ESC");
}

void AppSchedProx::_drawView() {
    auto& d = M5Cardputer.Display;
    if (_listSel >= (int)scheduledTasks.size()) { _state = ST_LIST; _needsRedraw = true; return; }
    const ScheduledTask& t = scheduledTasks[_listSel];
    d.fillScreen(SP_BG);
    String title = t.label.isEmpty() ? ("Task " + String(t.id)) : t.label;
    _drawTopBar(title.c_str());

    int y = SP_BAR_H + 4;
    d.setTextSize(1);

    auto row = [&](const char* lbl, const String& val, uint16_t vc) {
        d.setTextColor(d.color565(130,130,130), SP_BG);
        d.drawString(lbl, 4, y);
        d.setTextColor(vc, SP_BG);
        d.drawString(val, 4 + d.textWidth(lbl) + 4, y);
        y += 13;
    };

    char sched[32];
    snprintf(sched, sizeof(sched), "%04d-%02d-%02d  %02d:%02d:%02d",
             t.year, t.month, t.day, t.hour, t.minute, t.second);
    row("When:",    sched, TFT_YELLOW);
    row("Countdown:", _countdown(t), d.color565(200,200,80));
    row("Repeat:",  t.repeat  ? "yes" : "no", t.repeat  ? d.color565(80,220,80) : d.color565(180,80,80));
    row("Enabled:", t.enabled ? "yes" : "no", t.enabled ? d.color565(80,220,80) : d.color565(160,160,160));

    d.setTextColor(d.color565(130,130,130), SP_BG); d.drawString("Payload:", 4, y); y += 12;
    d.setTextColor(d.color565(180,220,180), SP_BG);
    String pay = t.payload;
    int mc = (d.width() - 8) / 6;
    if ((int)pay.length() > mc * 2) pay = pay.substring(0, mc * 2 - 3) + "...";
    if ((int)pay.length() > mc) { d.drawString(pay.substring(0, mc), 4, y); y += 12; pay = pay.substring(mc); }
    d.drawString(pay, 4, y);

    _drawBottomBar("E=toggle  D=delete  ESC back");
}

static void _spDrawFieldBox(int x, int y, int w, const String& text, bool active) {
    auto& d = M5Cardputer.Display;
    uint16_t fbg = active ? d.color565(20,50,20) : d.color565(40,40,40);
    d.fillRect(x, y, w, 14, fbg);
    d.setTextColor(TFT_WHITE, fbg); d.setTextSize(1);
    int mx = (w - 6) / 6;
    String s = text;
    if ((int)s.length() > mx) s = s.substring(s.length() - mx);
    if (active) s += "_";
    d.drawString(s, x + 3, y + 3);
}

void AppSchedProx::_drawAddP1() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(SP_BG);
    _drawTopBar("New Task 1/2");
    int y = SP_BAR_H + 3;

    auto selBg = [&](int f) -> uint16_t {
        return (_p1Field == f) ? d.color565(40,30,0) : (uint16_t)SP_BG;
    };

    d.setTextSize(1);

    // Label row
    {
        uint16_t bg = selBg(0);
        if (_p1Field == 0) d.fillRect(0, y-1, d.width(), 16, bg);
        d.setTextColor(_p1Field==0 ? TFT_WHITE : d.color565(160,160,160), bg);
        d.drawString("Label:", 4, y);
        _spDrawFieldBox(46, y, d.width()-50, _labelBuf, _labelActive);
        y += 18;
    }

    // Register / payload toggle
    {
        uint16_t bg = selBg(1);
        if (_p1Field == 1) d.fillRect(0, y-1, d.width(), 16, bg);
        d.setTextColor(_p1Field==1 ? TFT_WHITE : d.color565(160,160,160), bg);
        d.drawString(_usingReg ? "Reg:" : "Token:", 4, y);

        if (_usingReg) {
            // show selected register name + number
            int regIdx = _regScroll;
            if (!registers.empty()) {
                String rname = (regIdx < (int)registerNames.size()) ? registerNames[regIdx] : "";
                String rtxt = "[" + String(regIdx+1) + "] " + (rname.isEmpty() ? String("Reg "+String(regIdx+1)) : rname);
                if ((int)rtxt.length() > 18) rtxt = rtxt.substring(0,17)+".";
                uint16_t bg2 = (_p1Field==1) ? d.color565(20,20,60) : d.color565(40,40,40);
                d.fillRect(50, y, d.width()-54, 14, bg2);
                d.setTextColor(TFT_WHITE, bg2);
                d.drawString(rtxt, 54, y+3);
            }
        } else {
            _spDrawFieldBox(50, y, d.width()-54, _payloadBuf, _payloadActive);
        }
        y += 18;
    }

    // Toggle reg/manual
    {
        uint16_t bg = selBg(2);
        if (_p1Field == 2) d.fillRect(0, y-1, d.width(), 14, bg);
        d.setTextColor(_p1Field==2 ? TFT_WHITE : d.color565(120,120,120), bg);
        d.drawString(_usingReg ? "> using register (ENTER=toggle)" : "> manual token  (ENTER=toggle)", 4, y);
        y += 16;
    }

    // Repeat toggle
    {
        uint16_t bg = selBg(3);
        if (_p1Field == 3) d.fillRect(0, y-1, d.width(), 14, bg);
        d.setTextColor(_p1Field==3 ? TFT_WHITE : d.color565(160,160,160), bg);
        char buf[32]; snprintf(buf, sizeof(buf), "Repeat: %s", _draft.repeat ? "YES" : "no");
        d.drawString(buf, 4, y);
        y += 16;
    }

    // Next page button
    {
        uint16_t bg = selBg(4);
        uint16_t bc = d.color565(20, 70, 20);
        d.fillRoundRect(4, y, 80, 14, 3, bc);
        d.setTextColor(_p1Field==4 ? TFT_WHITE : d.color565(120,200,120), bc);
        d.drawString(_p1Field==4 ? "> Next >" : "  Next >", 8, y+3);
    }

    _drawBottomBar(_labelActive || _payloadActive
        ? "type  DEL backspace  ENTER confirm"
        : "up/dn field  ENTER edit/toggle  ESC back");
}

void AppSchedProx::_drawAddP2() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(SP_BG);
    _drawTopBar("New Task 2/2");

    int y = SP_BAR_H + 3;
    d.setTextSize(1);

    // Row 1: quick-shift buttons
    struct { const char* lbl; int sel; int w; } row1[] = {
        {"Now", 0, 36}, {"+1h", 1, 33}, {"+1d", 2, 33}, {"+1w", 3, 33}
    };
    int bx = 4;
    for (auto& b : row1) {
        bool sel = (_p2Sel == b.sel);
        uint16_t bc = sel ? d.color565(40,80,120) : d.color565(30,30,50);
        d.fillRoundRect(bx, y, b.w, 14, 3, bc);
        d.setTextColor(sel ? TFT_WHITE : d.color565(160,160,200), bc);
        d.drawString(b.lbl, bx+4, y+3);
        bx += b.w + 3;
    }
    y += 17;

    // Row 2: minute-level quick-shift buttons (sel 4,5,6)
    struct { const char* lbl; int sel; } row2[] = {
        {"+1min",4}, {"+10min",5}, {"+30min",6}
    };
    bx = 4;
    for (auto& b : row2) {
        bool sel = (_p2Sel == b.sel);
        uint16_t bc = sel ? d.color565(40,80,120) : d.color565(30,30,50);
        int bw = d.textWidth(b.lbl) + 10;
        d.fillRoundRect(bx, y, bw, 14, 3, bc);
        d.setTextColor(sel ? TFT_WHITE : d.color565(160,160,200), bc);
        d.drawString(b.lbl, bx+5, y+3);
        bx += bw + 4;
    }
    y += 17;

    // Datetime row (sel 7) — shows ">" cursor when selected; ENTER activates field editing
    bool dtRowSel = (_p2Sel == 7);
    uint16_t dtRowBg = dtRowSel ? d.color565(0,25,45) : (uint16_t)SP_BG;
    if (dtRowSel) d.fillRect(0, y-1, d.width(), _dtActive ? 28 : 14, dtRowBg);
    d.setTextColor(dtRowSel ? TFT_WHITE : d.color565(160,160,160), dtRowBg);
    d.drawString(dtRowSel ? "> When:" : "  When:", 4, y);

    static const int fw[6] = {34,20,18,18,18,18};
    static const char* sep[6] = {"-","-"," ",":",":",""},
                     * lbl[6] = {"Y","Mo","D","H","Mi","S"};
    int fx = 52;
    for (int i = 0; i < 6; i++) {
        bool cur = _dtActive && (_dtCursor == i);
        uint16_t fbg = cur ? d.color565(20,60,100) :
                       (dtRowSel && !_dtActive) ? d.color565(0,35,55) : d.color565(35,35,35);
        uint16_t ftc = cur ? TFT_WHITE : d.color565(180,180,180);
        d.fillRoundRect(fx, y, fw[i], 14, 2, fbg);
        d.setTextColor(ftc, fbg);
        int v = _dtGet(i);
        char buf[8];
        if (_dtTyping && cur) {
            String tb = _dtTypeBuf + "_";
            d.drawString(tb, fx + (fw[i]-d.textWidth(tb))/2, y+3);
        } else {
            if (i==0) snprintf(buf,sizeof(buf), v ? "%d" : "----", v);
            else      snprintf(buf,sizeof(buf), v ? "%02d" : "--",  v);
            d.drawString(buf, fx + (fw[i]-d.textWidth(buf))/2, y+3);
        }
        fx += fw[i];
        if (sep[i][0]) {
            d.setTextColor(d.color565(100,100,100), dtRowBg);
            d.drawString(sep[i], fx, y+3);
            fx += d.textWidth(sep[i])+1;
        }
    }

    if (_dtActive) {
        y += 16;
        d.setTextColor(d.color565(100,160,220), dtRowBg);
        d.drawString("</> field  up/dn value  0-9 type  ENTER done", 4, y);
        y += 12;
    } else {
        y += 16;
    }

    // Enabled row (sel 8)
    bool enSel = (_p2Sel == 8);
    uint16_t enBg = enSel ? d.color565(30,30,0) : (uint16_t)SP_BG;
    if (enSel) d.fillRect(0, y-1, d.width(), 14, enBg);
    d.setTextColor(enSel ? TFT_WHITE : d.color565(160,160,160), enBg);
    char buf[32]; snprintf(buf,sizeof(buf),"%sEnabled: %s", enSel?"> ":"  ", _draft.enabled?"YES":"no");
    d.drawString(buf, 4, y); y += 16;

    // Save button (sel 9)
    bool savSel = (_p2Sel == 9);
    uint16_t savBg = d.color565(20,60,20);
    d.fillRoundRect(4, y, 64, 14, 3, savBg);
    d.setTextColor(savSel ? TFT_WHITE : d.color565(120,200,120), savBg);
    d.drawString(savSel ? "> SAVE" : "  save", 8, y+3);

    const char* hint = _dtActive
        ? "</> field  up/dn val  0-9 type  ENTER done  ESC cancel"
        : "up/dn nav  ENTER=action  < back  ESC cancel";
    _drawBottomBar(hint);
}


void AppSchedProx::_handleAddP2(KeyInput ki) {
    // ── digit typing inside an active field ──────────────────────────────
    if (_dtActive && _dtTyping) {
        if (ki.esc || ki.del) {
            if (_dtTypeBuf.length() > 0) { _dtTypeBuf.remove(_dtTypeBuf.length()-1); }
            else { _dtTyping=false; }
            _needsRedraw=true; return;
        }
        if (ki.ch>='0' && ki.ch<='9') {
            _dtTypeBuf += ki.ch;
            int v = _dtTypeBuf.toInt();
            int mx = _dtMax(_dtCursor);
            if (v > mx) { _dtTypeBuf=String(ki.ch); v=ki.ch-'0'; }
            _dtSet(_dtCursor, v);
            _needsRedraw=true; return;
        }
        // Any other key (including enter/arrows): confirm typed value and continue
        _dtTyping=false; _dtTypeBuf="";
        // fall through to active-field navigation below
    }

    // ── active datetime field navigation ─────────────────────────────────
    if (_dtActive) {
        if (ki.esc) { _dtActive=false; _dtTyping=false; _dtTypeBuf=""; _needsRedraw=true; return; }
        if (ki.enter) { _dtActive=false; _dtTyping=false; _dtTypeBuf=""; _needsRedraw=true; return; }
        if (ki.arrowLeft)  { _dtCursor=(_dtCursor-1+6)%6; _needsRedraw=true; return; }
        if (ki.arrowRight) { _dtCursor=(_dtCursor+1)%6;   _needsRedraw=true; return; }
        if (ki.arrowUp)    { int v=_dtGet(_dtCursor)+1; if(v>_dtMax(_dtCursor)) v=0; _dtSet(_dtCursor,v); _needsRedraw=true; return; }
        if (ki.arrowDown)  { int v=_dtGet(_dtCursor)-1; if(v<0) v=_dtMax(_dtCursor); _dtSet(_dtCursor,v); _needsRedraw=true; return; }
        if (ki.ch>='0'&&ki.ch<='9') {
            _dtTyping=true; _dtTypeBuf=String(ki.ch);
            _dtSet(_dtCursor, ki.ch-'0'); _needsRedraw=true; return;
        }
        return;
    }

    // ── normal row navigation ─────────────────────────────────────────────
    static const int P2MAX = 10; // rows 0..9

    if (ki.esc) { _state=ST_ADD_P1; _needsRedraw=true; return; }

    if (ki.arrowUp)   { _p2Sel=(_p2Sel-1+P2MAX)%P2MAX; _needsRedraw=true; return; }
    if (ki.arrowDown) { _p2Sel=(_p2Sel+1)%P2MAX;       _needsRedraw=true; return; }

    if (!ki.enter) return;
    switch (_p2Sel) {
        case 0: _applyNow();          _needsRedraw=true; break;
        case 1: _shiftTime(3600);     _needsRedraw=true; break;
        case 2: _shiftTime(86400);    _needsRedraw=true; break;
        case 3: _shiftTime(604800);   _needsRedraw=true; break;
        case 4: _shiftTime(60);       _needsRedraw=true; break;
        case 5: _shiftTime(600);      _needsRedraw=true; break;
        case 6: _shiftTime(1800);     _needsRedraw=true; break;
        case 7:
            // Activate the datetime field editor
            _dtActive=true; _dtTyping=false; _dtTypeBuf=""; _dtCursor=3; // start at Hour
            _needsRedraw=true; break;
        case 8: _draft.enabled=!_draft.enabled; _needsRedraw=true; break;
        case 9:
            if (_draft.payload.isEmpty()) { _state=ST_ADD_P1; _p1Field=1; _needsRedraw=true; break; }
            if (_isEdit) updateScheduledTask(_draft);
            else         addScheduledTask(_draft);
            _state=ST_LIST; _listSel=(int)scheduledTasks.size()-1;
            _needsRedraw=true; break;
    }
}

void AppSchedProx::_handleList(KeyInput ki) {
    int n = max(1, (int)scheduledTasks.size());
    if (ki.arrowUp)   { _listSel = (_listSel-1+n)%n; _needsRedraw=true; return; }
    if (ki.arrowDown) { _listSel = (_listSel+1)%n;   _needsRedraw=true; return; }
    if (ki.esc) { uiManager.returnToLauncher(); return; }
    if (ki.enter && !scheduledTasks.empty()) { _state=ST_VIEW; _needsRedraw=true; return; }
    if (ki.ch=='n'||ki.ch=='N') {
        _resetDraft(); _isEdit=false; _state=ST_ADD_P1; _needsRedraw=true; return;
    }
    if ((ki.ch=='d'||ki.ch=='D') && !scheduledTasks.empty()) {
        _state=ST_CONFIRM_DELETE; _needsRedraw=true;
    }
}

void AppSchedProx::_handleView(KeyInput ki) {
    if (ki.esc) { _state=ST_LIST; _needsRedraw=true; return; }
    if (_listSel >= (int)scheduledTasks.size()) { _state=ST_LIST; _needsRedraw=true; return; }
    ScheduledTask& t = scheduledTasks[_listSel];
    if (ki.ch=='e'||ki.ch=='E') { t.enabled=!t.enabled; saveScheduledTasks(); _needsRedraw=true; }
    if (ki.ch=='d'||ki.ch=='D') { _state=ST_CONFIRM_DELETE; _needsRedraw=true; }
}

void AppSchedProx::_handleAddP1(KeyInput ki) {
    if (_labelActive) {
        if (ki.esc)   { _labelActive=false; _needsRedraw=true; return; }
        if (ki.del && _labelBuf.length()>0) { _labelBuf.remove(_labelBuf.length()-1); _needsRedraw=true; return; }
        if (ki.enter) { _draft.label=_labelBuf; _labelActive=false; _p1Field=1; _needsRedraw=true; return; }
        if (ki.ch)    { _labelBuf += ki.ch; _needsRedraw=true; return; }
        return;
    }
    if (_payloadActive) {
        if (ki.esc)   { _payloadActive=false; _needsRedraw=true; return; }
        if (ki.del && _payloadBuf.length()>0) { _payloadBuf.remove(_payloadBuf.length()-1); _needsRedraw=true; return; }
        if (ki.enter) { _draft.payload=_payloadBuf; _payloadActive=false; _p1Field=3; _needsRedraw=true; return; }
        if (ki.ch)    { _payloadBuf += ki.ch; _needsRedraw=true; return; }
        return;
    }

    if (ki.esc) { _state=ST_LIST; _needsRedraw=true; return; }
    if (ki.arrowUp)   { _p1Field=(_p1Field-1+5)%5; _needsRedraw=true; return; }
    if (ki.arrowDown) { _p1Field=(_p1Field+1)%5;   _needsRedraw=true; return; }

    if (_p1Field==1 && _usingReg) {
        if (ki.arrowLeft  && _regScroll>0)                       { _regScroll--; _needsRedraw=true; return; }
        if (ki.arrowRight && _regScroll<(int)registers.size()-1) { _regScroll++; _needsRedraw=true; return; }
    }

    if (ki.enter) {
        switch (_p1Field) {
            case 0: _labelBuf=_draft.label; _labelActive=true; _needsRedraw=true; break;
            case 1:
                if (_usingReg) { _applyReg(_regScroll); _p1Field=3; _needsRedraw=true; }
                else           { _payloadBuf=_draft.payload; _payloadActive=true; _needsRedraw=true; }
                break;
            case 2: _usingReg=!_usingReg; if(_usingReg) _applyReg(_regScroll); _needsRedraw=true; break;
            case 3: _draft.repeat=!_draft.repeat; _needsRedraw=true; break;
            case 4:
                _draft.label=_labelBuf;
                if (!_usingReg) _draft.payload=_payloadBuf;
                else            _applyReg(_regScroll);
                _state=ST_ADD_P2; _needsRedraw=true; break;
        }
    }
}

void AppSchedProx::_drawConfirmDelete() {
    auto& d = M5Cardputer.Display;
    if (_listSel >= (int)scheduledTasks.size()) { _state=ST_LIST; _needsRedraw=true; return; }
    const ScheduledTask& t = scheduledTasks[_listSel];
    d.fillScreen(SP_BG);
    _drawTopBar("Delete?");
    d.setTextSize(1);
    d.setTextColor(d.color565(220,80,80), SP_BG);
    d.drawString("Delete:", 4, SP_BAR_H+6);
    d.setTextColor(TFT_WHITE, SP_BG);
    String lbl = t.label.isEmpty() ? ("Task "+String(t.id)) : t.label;
    d.drawString(lbl, 4, SP_BAR_H+20);
    _drawBottomBar("Y confirm   N/ESC cancel");
}

void AppSchedProx::_handleConfirmDelete(KeyInput ki) {
    if (ki.esc||ki.ch=='n'||ki.ch=='N') {
        _state=(_listSel<(int)scheduledTasks.size())?ST_VIEW:ST_LIST;
        _needsRedraw=true; return;
    }
    if (ki.ch=='y'||ki.ch=='Y') {
        if (_listSel<(int)scheduledTasks.size()) {
            deleteScheduledTask(scheduledTasks[_listSel].id);
            if (_listSel>=(int)scheduledTasks.size()&&_listSel>0) _listSel--;
        }
        _state=ST_LIST; _needsRedraw=true;
    }
}

// ---- AppBase ----

void AppSchedProx::onEnter() {
    _state=ST_LIST; _listSel=0; _listScroll=0; _needsRedraw=true;
}

void AppSchedProx::onUpdate() {
    // Refresh countdown every 5 seconds without user input
    static unsigned long _lastTick = 0;
    if (_state==ST_LIST && millis()-_lastTick>5000) { _lastTick=millis(); _needsRedraw=true; }

    if (_needsRedraw) {
        switch (_state) {
            case ST_LIST:           _drawList();          break;
            case ST_VIEW:           _drawView();          break;
            case ST_ADD_P1:         _drawAddP1();         break;
            case ST_ADD_P2:         _drawAddP2();         break;
            case ST_CONFIRM_DELETE: _drawConfirmDelete(); break;
        }
        _needsRedraw = false;
    }

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        uiManager.returnToLauncher(); return;
    }

    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    switch (_state) {
        case ST_LIST:           _handleList(ki);          break;
        case ST_VIEW:           _handleView(ki);          break;
        case ST_ADD_P1:         _handleAddP1(ki);         break;
        case ST_ADD_P2:         _handleAddP2(ki);         break;
        case ST_CONFIRM_DELETE: _handleConfirmDelete(ki); break;
    }
}

} // namespace Cardputer
#endif
