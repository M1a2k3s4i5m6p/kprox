#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_timerprox.h"
#include "ui_manager.h"
#include "../registers.h"
#include "../hid.h"
#include <Preferences.h>
#include "../storage.h"

namespace Cardputer {

static constexpr uint16_t TP_BG = 0x0000;

static AppTimerProx* s_activeInstance = nullptr;

static void timerProxParseHook() {
    if (s_activeInstance) s_activeInstance->_pollDisplay();
}

// ---- Helpers ----

void AppTimerProx::_save() {
    Preferences p;
    p.begin("timerprox", false);
    p.putInt("reg",   _regIdx);
    p.putInt("fireH", _fireH); p.putInt("fireM", _fireM); p.putInt("fireS", _fireS);
    p.putInt("haltH", _haltH); p.putInt("haltM", _haltM); p.putInt("haltS", _haltS);
    p.putInt("repH",  _repH);  p.putInt("repM",  _repM);  p.putInt("repS",  _repS);
    p.end();
}

void AppTimerProx::_load() {
    Preferences p;
    p.begin("timerprox", true);
    _regIdx = p.getInt("reg",   0);
    _fireH  = p.getInt("fireH", 0); _fireM = p.getInt("fireM", 0); _fireS = p.getInt("fireS", 0);
    _haltH  = p.getInt("haltH", 0); _haltM = p.getInt("haltM", 0); _haltS = p.getInt("haltS", 0);
    _repH   = p.getInt("repH",  0); _repM  = p.getInt("repM",  0); _repS  = p.getInt("repS",  0);
    p.end();
    if (!registers.empty() && _regIdx >= (int)registers.size()) _regIdx = 0;
}

static String _fmtMs(long ms) {
    if (ms < 0) ms = 0;
    long s = ms / 1000;
    char buf[10];
    snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", s/3600, (s%3600)/60, s%60);
    return buf;
}

int& AppTimerProx::_fieldRef(Field f) {
    switch (f) {
        case F_REG:    return _regIdx;
        case F_FIRE_H: return _fireH; case F_FIRE_M: return _fireM; case F_FIRE_S: return _fireS;
        case F_HALT_H: return _haltH; case F_HALT_M: return _haltM; case F_HALT_S: return _haltS;
        case F_REP_H:  return _repH;  case F_REP_M:  return _repM;  case F_REP_S:  return _repS;
        default:       return _regIdx;
    }
}

int AppTimerProx::_fieldMax(Field f) {
    if (f == F_REG) return max(0, (int)registers.size() - 1);
    if (f == F_FIRE_H || f == F_HALT_H || f == F_REP_H) return 23;
    return 59;
}

// ---- Drawing ----

void AppTimerProx::_drawTopBar() {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(100, 60, 0);
    d.fillRect(0, 0, d.width(), TP_BAR_H, bc);
    d.setTextSize(1); d.setTextColor(TFT_WHITE, bc);
    d.drawString("TimerProx", 4, 3);
    drawTabHint(4 + d.textWidth("TimerProx") + 3);

    if (_state == ST_RUNNING) {
        const char* s = _fired ? "HALTING" : (_repEndMs ? "REPEAT" : "RUNNING");
        uint16_t sc = _fired   ? d.color565(255,80,80)
                    : _repEndMs ? d.color565(80,150,255)
                    :             d.color565(80,220,80);
        int sw = d.textWidth(s);
        d.setTextColor(sc, bc);
        d.drawString(s, d.width() - sw - 4, 3);
    } else {
        const char* hint = "BtnG0 start timer";
        int hw = d.textWidth(hint);
        d.setTextColor(d.color565(200, 180, 100), bc);
        d.drawString(hint, d.width() - hw - 4, 3);
    }
}

void AppTimerProx::_drawBottomBar(const char* hint) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(16, 16, 16);
    d.fillRect(0, d.height() - TP_BOT_H, d.width(), TP_BOT_H, bc);
    d.setTextSize(1); d.setTextColor(d.color565(110, 110, 110), bc);
    d.drawString(hint, 2, d.height() - TP_BOT_H + 2);
}

void AppTimerProx::_drawTimeField(int x, int y, int val, bool sel, const char* lbl) {
    auto& d = M5Cardputer.Display;
    uint16_t fbg = sel ? d.color565(20, 60, 120) : d.color565(35, 35, 35);
    uint16_t ftc = sel ? TFT_WHITE : d.color565(180, 180, 180);
    d.fillRoundRect(x, y, 22, 14, 2, fbg);
    d.setTextColor(ftc, fbg);
    char buf[4]; snprintf(buf, sizeof(buf), "%02d", val);
    d.drawString(buf, x + 3, y + 3);
    d.setTextColor(d.color565(70, 70, 70), TP_BG);
    d.drawString(lbl, x + 4, y + 16);
}

void AppTimerProx::_drawSetup() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(TP_BG);
    _drawTopBar();

    int y = TP_BAR_H + 4;
    d.setTextSize(1);

    // Register row
    {
        bool sel = (_sel == F_REG);
        uint16_t bg = sel ? d.color565(20, 50, 20) : (uint16_t)TP_BG;
        if (sel) d.fillRect(0, y-1, d.width(), 14, bg);
        d.setTextColor(sel ? TFT_WHITE : d.color565(160,160,160), bg);
        d.drawString(sel ? "> Register:" : "  Register:", 4, y);
        if (!registers.empty()) {
            String rname = (_regIdx < (int)registerNames.size()) ? registerNames[_regIdx] : "";
            String rtxt = "[" + String(_regIdx+1) + "]" + (rname.isEmpty() ? "" : " " + rname);
            if ((int)rtxt.length() > 16) rtxt = rtxt.substring(0, 14) + "..";
            uint16_t rbg = d.color565(30, 30, 30);
            d.fillRoundRect(90, y, d.width()-94, 14, 2, rbg);
            d.setTextColor(TFT_YELLOW, rbg);
            d.drawString(rtxt, 93, y+3);
        } else {
            d.setTextColor(d.color565(120,80,80), bg);
            d.drawString("No registers", 90, y);
        }
        y += 18;
    }

    // Helper lambda for a H:M:S row
    auto drawHMSRow = [&](const char* label, uint16_t labelColor,
                          Field fh, Field fm, Field fs,
                          const char* offNote) {
        d.setTextColor(labelColor, TP_BG);
        d.drawString(label, 4, y);
        int fx = 80;
        struct { Field f; const char* sep; const char* lbl; } fields[] = {
            {fh, ":", "H"}, {fm, ":", "M"}, {fs, nullptr, "S"}
        };
        for (auto& fi : fields) {
            _drawTimeField(fx, y, _fieldRef(fi.f), _sel == fi.f, fi.lbl);
            fx += 24;
            if (fi.sep) { d.setTextColor(d.color565(100,100,100), TP_BG); d.drawString(fi.sep, fx, y+3); fx += 7; }
        }
        if (offNote) {
            d.setTextColor(d.color565(70,70,70), TP_BG);
            d.drawString(offNote, fx+2, y+3);
        }
        y += 22;
    };

    bool haltOn = (_haltH + _haltM + _haltS) > 0;
    bool repOn  = (_repH  + _repM  + _repS)  > 0;

    drawHMSRow("Fire after:", d.color565(140,140,140), F_FIRE_H, F_FIRE_M, F_FIRE_S, nullptr);
    drawHMSRow("Halt after:", haltOn ? d.color565(220,100,100) : d.color565(100,100,100),
               F_HALT_H, F_HALT_M, F_HALT_S, haltOn ? nullptr : "(0=off)");
    drawHMSRow("Repeat:",     repOn  ? d.color565(100,160,255) : d.color565(100,100,100),
               F_REP_H,  F_REP_M,  F_REP_S,  repOn  ? nullptr : "(0=off)");

    // Start button
    {
        bool sel = (_sel == F_START);
        uint16_t bbg = d.color565(20, 80, 20);
        d.fillRoundRect(4, y, 70, 14, 3, bbg);
        d.setTextColor(sel ? TFT_WHITE : d.color565(100,200,100), bbg);
        d.drawString(sel ? "> START" : "  start", 8, y+3);
    }

    _drawBottomBar("up/dn field  </> value  ENTER select  ESC back");
}

void AppTimerProx::_drawRunning(bool full) {
    auto& d = M5Cardputer.Display;
    unsigned long now = millis();

    if (full) {
        d.fillScreen(TP_BG);
        _drawTopBar();
        _drawBottomBar("ESC cancel");
    } else {
        d.fillRect(0, TP_BAR_H, d.width(), d.height() - TP_BAR_H - TP_BOT_H, TP_BG);
    }

    int y = TP_BAR_H + 6;
    d.setTextSize(1);

    if (!registers.empty() && _regIdx < (int)registers.size()) {
        String rname = (_regIdx < (int)registerNames.size()) ? registerNames[_regIdx] : "";
        String rtxt = "Reg [" + String(_regIdx+1) + "]" + (rname.isEmpty() ? "" : ": " + rname);
        if ((int)rtxt.length() > 26) rtxt = rtxt.substring(0, 24) + "..";
        d.setTextColor(d.color565(160, 160, 160), TP_BG);
        d.drawString(rtxt, 4, y); y += 14;
    }

    if (_repEndMs && !_fired) {
        // Waiting for repeat interval to expire before next fire
        long repRemain = max(0L, (long)(_repEndMs - now));
        long repTotal  = _repMs();
        long repElapsed= repTotal - repRemain;

        d.setTextColor(d.color565(80, 150, 255), TP_BG);
        d.drawString("Next fire in:", 4, y); y += 12;

        d.setTextSize(3);
        d.setTextColor(d.color565(80, 150, 255), TP_BG);
        String ct = _fmtMs(repRemain);
        int tw = d.textWidth(ct);
        d.drawString(ct, (d.width() - tw) / 2, y); y += 34;
        d.setTextSize(1);

        int barW = d.width() - 16;
        d.drawRect(8, y, barW, 6, d.color565(40,40,40));
        if (repTotal > 0) {
            int fill = (int)((long)barW * min(repElapsed, repTotal) / repTotal);
            if (fill > 0) d.fillRect(9, y+1, fill, 4, d.color565(60,120,220));
        }
        y += 14;
        d.setTextColor(d.color565(70,70,70), TP_BG);
        String rs = "Fired " + String(_repeatCount) + " time" + (_repeatCount == 1 ? "" : "s");
        d.drawString(rs, 4, y);

    } else if (!_fired) {
        long fireMs  = _fireMs();
        long elapsed = (long)(now - _startMs);
        long remaining = fireMs - elapsed;
        if (remaining < 0) remaining = 0;

        d.setTextColor(d.color565(100, 160, 100), TP_BG);
        d.drawString("Fires in:", 4, y); y += 12;

        d.setTextSize(3);
        d.setTextColor(TFT_GREEN, TP_BG);
        String ct = _fmtMs(remaining);
        int tw = d.textWidth(ct);
        d.drawString(ct, (d.width() - tw) / 2, y); y += 34;
        d.setTextSize(1);

        int barW = d.width() - 16;
        d.drawRect(8, y, barW, 6, d.color565(40,40,40));
        if (fireMs > 0) {
            int fill = (int)((long)barW * min(elapsed, fireMs) / fireMs);
            if (fill > 0) d.fillRect(9, y+1, fill, 4, d.color565(60,180,60));
        }
        y += 14;

        if (_haltMs() > 0) {
            d.setTextColor(d.color565(120,80,80), TP_BG);
            d.drawString("Halt after fire: " + _fmtMs(_haltMs()), 4, y);
        } else if (_repMs() > 0) {
            d.setTextColor(d.color565(80,120,200), TP_BG);
            d.drawString("Repeat every: " + _fmtMs(_repMs()), 4, y);
        }
    } else {
        // HALTING countdown
        long hms       = _haltMs();
        long sincefire = (long)(now - _firedMs);
        long remaining = hms - sincefire;
        if (remaining < 0) remaining = 0;

        d.setTextColor(d.color565(200, 120, 80), TP_BG);
        d.drawString("Register fired!", 4, y); y += 14;

        if (hms > 0) {
            d.setTextColor(d.color565(200, 80, 80), TP_BG);
            d.drawString("Halting in:", 4, y); y += 12;

            d.setTextSize(3);
            d.setTextColor(d.color565(255, 80, 80), TP_BG);
            String ct = _fmtMs(remaining);
            int tw = d.textWidth(ct);
            d.drawString(ct, (d.width() - tw) / 2, y); y += 34;
            d.setTextSize(1);

            int barW = d.width() - 16;
            d.drawRect(8, y, barW, 6, d.color565(40,40,40));
            int fill = (int)((long)barW * min(sincefire, hms) / hms);
            if (fill > 0) d.fillRect(9, y+1, fill, 4, d.color565(180,60,60));
        } else {
            d.setTextColor(d.color565(120,120,120), TP_BG);
            d.drawString("No halt configured.", 4, y);
        }
    }
}

void AppTimerProx::_pollDisplay() {
    unsigned long now = millis();
    if ((now - _lastDrawMs) < 250UL) return;
    _lastDrawMs = now;
    _drawRunning(false);
}

// ---- Input ----

void AppTimerProx::_handleSetup(KeyInput ki) {
    if (ki.esc) { uiManager.returnToLauncher(); return; }

    int n = F_COUNT;
    if (ki.arrowUp)   { _sel = (Field)(((int)_sel - 1 + n) % n); _needsRedraw = true; return; }
    if (ki.arrowDown) { _sel = (Field)(((int)_sel + 1) % n);     _needsRedraw = true; return; }

    if (_sel != F_START) {
        int delta = 0;
        if (ki.arrowLeft)  delta = -1;
        if (ki.arrowRight) delta =  1;
        if (delta && _sel != F_REG) {
            int& v = _fieldRef(_sel); int mx = _fieldMax(_sel);
            v = (v + delta + mx + 1) % (mx + 1);
            _needsRedraw = true; return;
        }
        if (delta && _sel == F_REG && !registers.empty()) {
            int mx = _fieldMax(F_REG);
            _regIdx = (_regIdx + delta + mx + 1) % (mx + 1);
            _needsRedraw = true; return;
        }
        if (ki.ch >= '0' && ki.ch <= '9' && _sel != F_REG) {
            int& v = _fieldRef(_sel); int mx = _fieldMax(_sel);
            int nv = v * 10 + (ki.ch - '0');
            if (nv > mx) nv = ki.ch - '0';
            v = nv; _needsRedraw = true; return;
        }
    }

    if (ki.enter && _sel == F_START) {
        if (registers.empty()) return;
        timerProxRegIdx = _regIdx;
        timerProxFireH = _fireH; timerProxFireM = _fireM; timerProxFireS = _fireS;
        timerProxHaltH = _haltH; timerProxHaltM = _haltM; timerProxHaltS = _haltS;
        timerProxRepH  = _repH;  timerProxRepM  = _repM;  timerProxRepS  = _repS;
        saveTimerProxSettings();
        _save();
        _startTimer();
        _state       = ST_RUNNING;
        _needsRedraw = true;
    }
}

void AppTimerProx::_startTimer() {
    _startMs      = millis();
    _firedMs      = 0;
    _fired        = false;
    _repEndMs     = 0;
    _repeatCount  = 0;
    _lastDrawMs   = 0;
}

void AppTimerProx::_handleRunning(KeyInput ki) {
    if (ki.esc) {
        g_haltDeadlineMs     = 0;
        g_parseInterruptHook = nullptr;
        s_activeInstance     = nullptr;
        _fired    = false;
        _repEndMs = 0;
        _state    = ST_SETUP;
        _needsRedraw = true;
    }
}

// ---- Lifecycle ----

void AppTimerProx::onEnter() {
    _state = ST_SETUP; _sel = F_REG; _needsRedraw = true;
    _fired = false; _startMs = 0; _firedMs = 0; _lastDrawMs = 0;
    _repEndMs = 0; _repeatCount = 0;
    _regIdx = timerProxRegIdx;
    _fireH = timerProxFireH; _fireM = timerProxFireM; _fireS = timerProxFireS;
    _haltH = timerProxHaltH; _haltM = timerProxHaltM; _haltS = timerProxHaltS;
    _repH  = timerProxRepH;  _repM  = timerProxRepM;  _repS  = timerProxRepS;
}

void AppTimerProx::onExit() {
    g_haltDeadlineMs     = 0;
    g_parseInterruptHook = nullptr;
    s_activeInstance     = nullptr;
    _state = ST_SETUP;
}

void AppTimerProx::onUpdate() {
    if (_state == ST_RUNNING) {
        uiManager.notifyInteraction();
        unsigned long now = millis();

        // ---- Waiting for repeat interval ----
        if (_repEndMs && !_fired) {
            if (now >= _repEndMs) {
                _repEndMs = 0;
                _startMs  = now;   // reset fire countdown for next cycle
                _needsRedraw = true;
            }
            if (_needsRedraw || (now - _lastDrawMs) >= 250UL) {
                _drawRunning(_needsRedraw);
                _lastDrawMs  = now;
                _needsRedraw = false;
            }
            return;
        }

        // ---- Fire countdown ----
        if (!_fired) {
            if ((long)(now - _startMs) >= _fireMs()) {
                _fired           = true;
                _firedMs         = now;
                _repeatCount++;
                g_haltDeadlineMs = (_haltMs() > 0)
                                 ? (now + (unsigned long)_haltMs())
                                 : 0UL;
                s_activeInstance     = this;
                g_parseInterruptHook = timerProxParseHook;
                if (!registers.empty() && _regIdx < (int)registers.size())
                    pendingTokenStrings.push_back(registers[_regIdx]);
                _drawRunning(true);
                _lastDrawMs  = now;
                _needsRedraw = false;
                return;
            }
        } else {
            // ---- Halt / repeat logic after fire ----
            bool haltTriggered = isHalted
                              || (_haltMs() > 0 && (long)(now - _firedMs) >= _haltMs());
            bool repeatNow     = !haltTriggered
                              && (_haltMs() == 0)
                              && (_repMs() > 0)
                              && (long)(now - _firedMs) >= 0;  // immediate after fire

            if (haltTriggered) {
                if (!isHalted) haltAllOperations();
                g_haltDeadlineMs     = 0;
                g_parseInterruptHook = nullptr;
                s_activeInstance     = nullptr;
                _fired       = false;
                _repEndMs    = 0;
                _state       = ST_SETUP;
                _needsRedraw = true;
                return;
            }

            // After halt window (or immediately if no halt), start repeat
            bool haltExpired = (_haltMs() > 0 && (long)(now - _firedMs) >= _haltMs());
            bool canRepeat   = _repMs() > 0 && !_repEndMs;
            if (canRepeat && (_haltMs() == 0 || haltExpired)) {
                g_haltDeadlineMs     = 0;
                g_parseInterruptHook = nullptr;
                s_activeInstance     = nullptr;
                _fired    = false;
                _repEndMs = now + (unsigned long)_repMs();
                _needsRedraw = true;
            }
        }

        if (_needsRedraw || (now - _lastDrawMs) >= 250UL) {
            _drawRunning(_needsRedraw);
            _lastDrawMs  = now;
            _needsRedraw = false;
        }
        return;
    }

    if (_needsRedraw) {
        _drawSetup();
        _needsRedraw = false;
    }

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        if (_state == ST_SETUP && !registers.empty()) {
            _save();
            _startTimer();
            _state       = ST_RUNNING;
            _needsRedraw = true;
        } else if (_state == ST_RUNNING) {
            g_haltDeadlineMs     = 0;
            g_parseInterruptHook = nullptr;
            s_activeInstance     = nullptr;
            _fired    = false;
            _repEndMs = 0;
            _state    = ST_SETUP;
            _needsRedraw = true;
        }
        return;
    }
    KeyInput ki = pollKeys();
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();
    (_state == ST_SETUP) ? _handleSetup(ki) : _handleRunning(ki);
}

} // namespace Cardputer
#endif
