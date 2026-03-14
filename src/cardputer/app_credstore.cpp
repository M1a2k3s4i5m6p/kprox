#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_credstore.h"
#include "../credential_store.h"
#include "ui_manager.h"

namespace Cardputer {

static constexpr int CS_BG    = 0x1082;
static constexpr int CS_BAR_H = 18;
static constexpr int CS_BOT_H = 14;

static const char* PAGE_LABELS[4] = {
    "Status / Unlock", "Add / Update", "Change Key", "Wipe Store"
};

// ---- Raw keyboard ----
// Reads directly from hardware, bypassing pollKeys() navigation aliases.
// This allows , . ; / ` and all other printable chars in input fields.

static CSRawKey pollRaw() {
    CSRawKey rk;
    if (!M5Cardputer.Keyboard.isChange()) return rk;
    if (!M5Cardputer.Keyboard.isPressed()) return rk;

    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    rk.any   = true;
    rk.del   = ks.del;
    rk.enter = ks.enter;
    rk.tab   = ks.tab;
    rk.fn    = ks.fn;

    for (uint8_t hk : ks.hid_keys) {
        switch (hk) {
            case 0x29: rk.esc  = true; break;
            case 0x52: if (ks.fn) rk.up   = true; break;
            case 0x51: if (ks.fn) rk.down = true; break;
        }
    }

    for (char c : ks.word) {
        if (c == 0x1B) { rk.esc = true; continue; }
        if (c == '`')  { rk.esc = true; continue; }
        if (ks.fn) {
            if (c == ';') { rk.up   = true; continue; }
            if (c == '.') { rk.down = true; continue; }
        }
        if (c >= 0x20 && c < 0x7F) rk.ch = c;
    }
    return rk;
}

// ---- Helpers ----

void AppCredStore::_drawTopBar(int page) {
    auto& disp = M5Cardputer.Display;
    uint16_t bg = disp.color565(120, 20, 20);
    disp.fillRect(0, 0, disp.width(), CS_BAR_H, bg);
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, bg);
    disp.drawString(PAGE_LABELS[page], 4, 3);
    char pg[8];
    snprintf(pg, sizeof(pg), "%d/%d", page + 1, NUM_PAGES);
    int pw = disp.textWidth(pg);
    disp.drawString(pg, disp.width() - pw - 4, 3);
}

void AppCredStore::_drawInputField(int x, int y, int w,
                                    const String& buf, bool active, bool masked) {
    auto& disp = M5Cardputer.Display;
    uint16_t fbg = active ? disp.color565(60, 60, 60) : disp.color565(38, 38, 38);
    disp.fillRect(x, y, w, 14, fbg);
    disp.setTextColor(TFT_WHITE, fbg);
    disp.setTextSize(1);
    int maxChars = (w - 6) / 6;
    String d = masked ? String(buf.length(), '*') : buf;
    if ((int)d.length() > maxChars)
        d = d.substring(d.length() - maxChars);
    if (active) d += '_';
    disp.drawString(d, x + 3, y + 3);
}

void AppCredStore::_resetCredFields() {
    _credLabel     = "";
    _credValue     = "";
    _credStatus    = "";
    _credStatusOk  = false;
    _credField     = CF_LABEL;
    _credListIdx   = -1;
    _deletePrompted = false;
}

// ---- Page 0: Status / Unlock ----

void AppCredStore::_drawPage0() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(0);

    int y = CS_BAR_H + 6;
    bool locked = credStoreLocked;

    uint16_t sb = locked ? disp.color565(140,30,30) : disp.color565(30,120,30);
    const char* ss = locked ? "LOCKED" : "UNLOCKED";
    int bw = disp.textWidth(ss) + 12;
    disp.fillRoundRect(4, y, bw, 16, 3, sb);
    disp.setTextColor(TFT_WHITE, sb);
    disp.drawString(ss, 10, y + 3);
    y += 22;

    disp.setTextSize(1);
    disp.setTextColor(disp.color565(200,200,200), CS_BG);
    if (locked) {
        disp.drawString("Credentials: --", 4, y);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Credentials: %d", credStoreCount());
        disp.drawString(buf, 4, y);
    }
    y += 16;

    if (locked) {
        disp.setTextColor(disp.color565(180,180,180), CS_BG);
        disp.drawString("Store key:", 4, y);
        y += 12;
        _drawInputField(4, y, disp.width() - 8, _keyBuf, true, false);
        y += 18;
        if (_unlockFailed) {
            disp.setTextColor(disp.color565(220,80,80), CS_BG);
            disp.drawString("Invalid key", 4, y);
        }
    } else {
        auto labels = credStoreListLabels();
        int shown = 0;
        for (auto& lbl : labels) {
            if (shown >= 3) {
                disp.setTextColor(disp.color565(130,130,130), CS_BG);
                disp.drawString("...", 8, y);
                break;
            }
            disp.setTextColor(disp.color565(100,200,255), CS_BG);
            disp.drawString(("  * " + lbl).c_str(), 4, y);
            y += 13;
            shown++;
        }
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    if (locked)
        disp.drawString("type key  ENTER unlock  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
    else
        disp.drawString("ENTER=lock  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
}

void AppCredStore::_drawConfirmLock() {
    auto& disp = M5Cardputer.Display;
    int bx=20, by=44, bw=disp.width()-40, bh=50;
    disp.fillRoundRect(bx, by, bw, bh, 6, disp.color565(40,10,10));
    disp.drawRoundRect(bx, by, bw, bh, 6, disp.color565(200,60,60));
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, disp.color565(40,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx = bx + bw/2;
    disp.drawString("Lock credential store?", cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppCredStore::_handlePage0(CSRawKey rk) {
    if (_confirmingLock) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreLock();
            _snapLocked=true; _snapCount=-1;
            _confirmingLock=false; _keyBuf="";
        } else if (rk.ch=='n'||rk.ch=='N'||rk.esc) {
            _confirmingLock=false;
        }
        _needsRedraw=true; return;
    }

    bool locked = credStoreLocked;
    if (_keyBuf.isEmpty()) {
        if (rk.ch==',') { _page=NUM_PAGES-1; _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=1;           _needsRedraw=true; return; }
    }

    if (locked) {
        if (rk.enter && _keyBuf.length()>0) {
            if (credStoreUnlock(_keyBuf)) {
                _unlockFailed=false; _snapLocked=false; _snapCount=credStoreCount();
            } else {
                _unlockFailed=true;
            }
            _keyBuf=""; _needsRedraw=true; return;
        }
        if (rk.del && _keyBuf.length()>0) { _keyBuf.remove(_keyBuf.length()-1); _unlockFailed=false; _needsRedraw=true; return; }
        if (rk.esc) { _keyBuf=""; _unlockFailed=false; _needsRedraw=true; return; }
        if (rk.ch)  { _keyBuf+=rk.ch; _unlockFailed=false; _needsRedraw=true; }
    } else {
        if (rk.enter) { _confirmingLock=true; _needsRedraw=true; }
        if (rk.esc)   { uiManager.returnToLauncher(); }
    }
}

// ---- Page 1: Add / Update / Delete ----

void AppCredStore::_drawDeleteConfirm() {
    auto& disp = M5Cardputer.Display;
    int bx=10, by=52, bw=disp.width()-20, bh=50;
    disp.fillRoundRect(bx, by, bw, bh, 6, disp.color565(40,10,10));
    disp.drawRoundRect(bx, by, bw, bh, 6, disp.color565(200,60,60));
    disp.setTextSize(1);
    disp.setTextColor(TFT_WHITE, disp.color565(40,10,10));
    disp.setTextDatum(MC_DATUM);
    int cx = bx+bw/2;
    String msg = "Delete \"" + _credLabel + "\"?";
    if ((int)msg.length() > 30) msg = "Delete this credential?";
    disp.drawString(msg, cx, by+14);
    disp.drawString("Y confirm   N cancel", cx, by+30);
    disp.setTextDatum(TL_DATUM);
}

void AppCredStore::_drawPage1() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(1);

    int y = CS_BAR_H + 5;
    int fw = disp.width() - 8;

    if (!credStoreLocked) {
        bool labelActive = (_credField == CF_LABEL);
        bool valueActive = (_credField == CF_VALUE);

        // --- Label field ---
        disp.setTextSize(1);
        disp.setTextColor(labelActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Label:", 4, y);

        // Show browse position when cycling through existing labels
        if (labelActive && _credListIdx >= 0) {
            auto labels = credStoreListLabels();
            if (!labels.empty()) {
                char nav[12];
                snprintf(nav, sizeof(nav), "%d/%d", _credListIdx+1, (int)labels.size());
                int nw = disp.textWidth(nav);
                disp.setTextColor(disp.color565(100,200,255), CS_BG);
                disp.drawString(nav, disp.width()-nw-4, y);
            }
        }

        // Badge: EXISTS or NEW
        if (!_credLabel.isEmpty()) {
            bool exists = credStoreLabelExists(_credLabel);
            uint16_t badgeBg = exists ? disp.color565(30,80,120) : disp.color565(20,80,20);
            const char* badgeStr = exists ? "UPDATE" : "NEW";
            int bw2 = disp.textWidth(badgeStr)+8;
            int bx2 = disp.width()-bw2-4;
            if (_credListIdx < 0) { // don't overlap nav counter
                disp.fillRoundRect(bx2, y, bw2, 12, 2, badgeBg);
                disp.setTextColor(TFT_WHITE, badgeBg);
                disp.drawString(badgeStr, bx2+4, y+2);
            }
        }

        y += 11;
        _drawInputField(4, y, fw, _credLabel, labelActive, false);
        y += 17;

        // --- Value field ---
        // Value is never pre-filled from the store. When updating, user must retype.
        disp.setTextColor(valueActive ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
        disp.drawString("Value:", 4, y);
        y += 11;
        _drawInputField(4, y, fw, _credValue, valueActive, false);
        y += 17;

        // Navigation hint on label field
        if (labelActive && credStoreCount() > 0) {
            disp.setTextSize(1);
            disp.setTextColor(disp.color565(90,90,120), CS_BG);
            disp.drawString("fn+up/dn browse  DEL delete", 4, y);
            y += 13;
        }

        // Status message
        if (_credStatus.length() > 0) {
            disp.setTextColor(_credStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_credStatus, 4, y);
        }
    } else {
        disp.setTextColor(disp.color565(180,100,60), CS_BG);
        disp.drawString("Unlock the store first", 4, y+20);
        disp.drawString("(page 1)", 4, y+34);
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    disp.drawString("TAB switch  ENTER save  DEL delete  </> page", 2, disp.height()-CS_BOT_H+2);

    if (_deletePrompted) _drawDeleteConfirm();
}

void AppCredStore::_handlePage1(CSRawKey rk) {
    // Delete confirmation overlay
    if (_deletePrompted) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreDelete(_credLabel);
            _credStatus   = "Deleted";
            _credStatusOk = true;
            _snapCount    = credStoreCount();
            _resetCredFields();
        } else if (rk.ch=='n'||rk.ch=='N'||rk.esc) {
            _deletePrompted = false;
        }
        _needsRedraw = true;
        return;
    }

    bool bothEmpty = _credLabel.isEmpty() && _credValue.isEmpty();

    // Page navigation when fields are clear
    if (bothEmpty) {
        if (rk.ch==',') { _page=0; _resetCredFields(); _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=2; _resetCredFields(); _needsRedraw=true; return; }
    }

    if (rk.esc) {
        if (!bothEmpty) {
            _resetCredFields(); _needsRedraw=true;
        } else {
            _page=0; _needsRedraw=true;
        }
        return;
    }

    if (!credStoreLocked) {
        if (rk.tab) {
            _credField   = (_credField==CF_LABEL) ? CF_VALUE : CF_LABEL;
            _credListIdx = -1;
            _credStatus  = "";
            _needsRedraw = true;
            return;
        }

        if (_credField == CF_LABEL) {
            // Up/down: cycle through existing labels (never shows values)
            if (rk.up || rk.down) {
                auto labels = credStoreListLabels();
                if (!labels.empty()) {
                    int n = (int)labels.size();
                    if (_credListIdx < 0)
                        _credListIdx = rk.up ? n-1 : 0;
                    else
                        _credListIdx = rk.up ? (_credListIdx-1+n)%n : (_credListIdx+1)%n;
                    _credLabel  = labels[_credListIdx];
                    _credValue  = ""; // never pre-fill the value
                    _credStatus = "";
                    _needsRedraw = true;
                }
                return;
            }

            // DEL on label field with an existing credential = delete prompt
            if (rk.del) {
                if (!_credLabel.isEmpty() && credStoreLabelExists(_credLabel)) {
                    _deletePrompted = true;
                    _needsRedraw    = true;
                } else if (!_credLabel.isEmpty()) {
                    _credLabel.remove(_credLabel.length()-1);
                    _credListIdx=-1; _credStatus=""; _needsRedraw=true;
                }
                return;
            }

            if (rk.enter) {
                if (!_credLabel.isEmpty()) {
                    _credField=CF_VALUE; _credListIdx=-1; _needsRedraw=true;
                }
                return;
            }

            if (rk.ch) {
                _credLabel  += rk.ch;
                _credListIdx = -1; _credStatus=""; _needsRedraw=true;
            }

        } else { // CF_VALUE
            if (rk.enter) {
                if (_credLabel.isEmpty()) {
                    _credStatus="Label required"; _credStatusOk=false; _needsRedraw=true;
                    return;
                }
                bool existed = credStoreLabelExists(_credLabel);
                if (credStoreSet(_credLabel, _credValue)) {
                    _credStatus   = existed ? "Updated!" : "Saved!";
                    _credStatusOk = true;
                    _snapCount    = credStoreCount();
                    _resetCredFields();
                } else {
                    _credStatus="Save failed"; _credStatusOk=false;
                }
                _needsRedraw=true; return;
            }

            if (rk.del && _credValue.length()>0) {
                _credValue.remove(_credValue.length()-1);
                _credStatus=""; _needsRedraw=true; return;
            }
            if (rk.ch) {
                _credValue+=rk.ch; _credStatus=""; _needsRedraw=true;
            }
        }
    }
}

// ---- Page 2: Change Key ----

void AppCredStore::_drawPage2() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(2);

    int y = CS_BAR_H+6;

    if (!credStoreLocked) {
        static const char* labels[3] = { "Current key:", "New key:", "Confirm new:" };
        String* bufs[3] = { &_rkOld, &_rkNew, &_rkConfirm };
        int fw = disp.width()-8;

        for (int i=0; i<3; i++) {
            disp.setTextSize(1);
            disp.setTextColor((_rkField==(RekeyField)i) ? TFT_WHITE : disp.color565(160,160,160), CS_BG);
            disp.drawString(labels[i], 4, y); y+=11;
            _drawInputField(4, y, fw, *bufs[i], _rkField==(RekeyField)i, false);
            y+=18;
        }
        if (_rkStatus.length()>0) {
            disp.setTextColor(_rkStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
            disp.drawString(_rkStatus, 4, y);
        }
    } else {
        disp.setTextColor(disp.color565(180,100,60), CS_BG);
        disp.drawString("Unlock the store first", 4, y+10);
        disp.drawString("(page 1)", 4, y+26);
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    disp.drawString("TAB next  ENTER save  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
}

void AppCredStore::_handlePage2(CSRawKey rk) {
    bool allEmpty = _rkOld.isEmpty()&&_rkNew.isEmpty()&&_rkConfirm.isEmpty();
    if (allEmpty) {
        if (rk.ch==',') { _page=1; _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=3; _needsRedraw=true; return; }
    }
    if (rk.esc) {
        _page=0; _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus="";
        _needsRedraw=true; return;
    }

    if (!credStoreLocked) {
        if (rk.tab) {
            _rkField=(RekeyField)(((int)_rkField+1)%3);
            _needsRedraw=true; return;
        }
        String* cur = (_rkField==RK_OLD)?&_rkOld : (_rkField==RK_NEW)?&_rkNew : &_rkConfirm;

        if (rk.enter) {
            if (_rkField!=RK_CONFIRM) {
                _rkField=(RekeyField)((int)_rkField+1); _needsRedraw=true;
            } else {
                if (_rkNew!=_rkConfirm)           { _rkStatus="Keys don't match"; _rkStatusOk=false; }
                else if (_rkNew.length()<8)        { _rkStatus="Min 8 characters"; _rkStatusOk=false; }
                else if (credStoreRekey(_rkOld,_rkNew)) {
                    _rkStatus="Key changed!"; _rkStatusOk=true;
                    _rkOld=""; _rkNew=""; _rkConfirm=""; _rkField=RK_OLD;
                } else { _rkStatus="Wrong current key"; _rkStatusOk=false; }
                _needsRedraw=true;
            }
            return;
        }
        if (rk.del && cur->length()>0) { cur->remove(cur->length()-1); _rkStatus=""; _needsRedraw=true; return; }
        if (rk.ch) { *cur+=rk.ch; _rkStatus=""; _needsRedraw=true; }
    }
}

// ---- Page 3: Wipe ----

void AppCredStore::_drawPage3() {
    auto& disp = M5Cardputer.Display;
    disp.fillScreen(CS_BG);
    _drawTopBar(3);

    int y = CS_BAR_H+10;
    disp.setTextSize(1);
    disp.setTextColor(disp.color565(220,80,80), CS_BG);
    disp.drawString("! Deletes ALL credentials", 4, y); y+=14;
    disp.drawString("! Clears the key check",    4, y); y+=14;
    disp.drawString("! Cannot be undone",         4, y); y+=20;

    if (_wipePrompted) {
        uint16_t pb = disp.color565(60,15,15);
        disp.fillRoundRect(4, y, disp.width()-8, 22, 4, pb);
        disp.setTextColor(TFT_WHITE, pb);
        disp.drawString("Type Y to confirm wipe", 8, y+6);
        y+=30;
    } else {
        disp.setTextColor(disp.color565(180,180,180), CS_BG);
        disp.drawString("Press ENTER to begin wipe", 4, y);
        y+=18;
    }

    if (_wipeStatus.length()>0) {
        disp.setTextColor(_wipeStatusOk ? TFT_GREEN : disp.color565(220,80,80), CS_BG);
        disp.drawString(_wipeStatus, 4, y);
    }

    uint16_t bb = disp.color565(16,16,16);
    disp.fillRect(0, disp.height()-CS_BOT_H, disp.width(), CS_BOT_H, bb);
    disp.setTextColor(disp.color565(100,100,100), bb);
    disp.drawString("ENTER wipe  </> page  ESC back", 2, disp.height()-CS_BOT_H+2);
}

void AppCredStore::_handlePage3(CSRawKey rk) {
    if (!_wipePrompted) {
        if (rk.ch==',') { _page=2; _needsRedraw=true; return; }
        if (rk.ch=='/') { _page=0; _needsRedraw=true; return; }
    }
    if (rk.esc) {
        if (_wipePrompted) { _wipePrompted=false; _wipeStatus=""; _needsRedraw=true; }
        else { _page=0; _needsRedraw=true; }
        return;
    }
    if (_wipePrompted) {
        if (rk.ch=='y'||rk.ch=='Y') {
            credStoreWipe();
            _wipeStatus="Store wiped"; _wipeStatusOk=true;
            _wipePrompted=false;
            _snapLocked=true; _snapCount=-1;
            _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus="";
            _resetCredFields();
        } else if (rk.ch=='n'||rk.ch=='N') {
            _wipePrompted=false; _wipeStatus="";
        }
        _needsRedraw=true; return;
    }
    if (rk.enter) { _wipePrompted=true; _wipeStatus=""; _needsRedraw=true; }
}

// ---- Poll & lifecycle ----

void AppCredStore::_pollState() {
    bool curLocked = credStoreLocked;
    int  curCount  = curLocked ? -1 : credStoreCount();
    if (curLocked!=_snapLocked || curCount!=_snapCount) {
        _snapLocked=curLocked; _snapCount=curCount;
        _confirmingLock=false;
        _needsRedraw=true;
    }
}

void AppCredStore::onEnter() {
    _page=0; _needsRedraw=true;
    _confirmingLock=false; _keyBuf=""; _unlockFailed=false;
    _resetCredFields();
    _rkField=RK_OLD; _rkOld=""; _rkNew=""; _rkConfirm=""; _rkStatus="";
    _wipePrompted=false; _wipeStatus="";
    _lastPollMs=0;
    _snapLocked=!credStoreLocked; _snapCount=-1;
}

void AppCredStore::onExit() {
    _confirmingLock=false; _keyBuf=""; _wipePrompted=false;
    _resetCredFields();
    _rkOld=""; _rkNew=""; _rkConfirm="";
}

void AppCredStore::onUpdate() {
    unsigned long now = millis();

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _page=(_page+1)%NUM_PAGES;
        _confirmingLock=false; _wipePrompted=false; _deletePrompted=false;
        _needsRedraw=true;
        return;
    }

    if (now-_lastPollMs >= POLL_INTERVAL_MS) {
        _lastPollMs=now;
        _pollState();
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); if (_confirmingLock) _drawConfirmLock(); break;
            case 1: _drawPage1(); break; // delete confirm is drawn inside _drawPage1
            case 2: _drawPage2(); break;
            case 3: _drawPage3(); break;
        }
        _needsRedraw=false;
    }

    CSRawKey rk = pollRaw();
    if (!rk.any) return;
    uiManager.notifyInteraction();

    switch (_page) {
        case 0: _handlePage0(rk); break;
        case 1: _handlePage1(rk); break;
        case 2: _handlePage2(rk); break;
        case 3: _handlePage3(rk); break;
    }
}

} // namespace Cardputer
#endif
