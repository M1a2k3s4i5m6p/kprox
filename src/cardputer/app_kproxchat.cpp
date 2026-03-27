#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "app_kproxchat.h"
#include "fa_connectdevelop_icon.h"

namespace Cardputer {

// ---- Identity ---------------------------------------------------------------

static bool isAutoName(const String& s) {
    if (s.length() < 7 || !s.startsWith("KProx")) return false;
    for (int i = 5; i < (int)s.length(); i++) {
        char c = s.charAt(i);
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

void AppKProxChat::_initIdentity() {
    preferences.begin(NVS_KPROXCHAT, true);
    _privkey = preferences.getString("privkey", "");
    _pubkey  = preferences.getString("pubkey",  "");
    _name    = preferences.getString("name",    "");
    preferences.end();

    bool needKeys = (_privkey.length() != 64 || _pubkey.length() != 64);
    bool needName = _name.isEmpty();

    if (needName) {
        uint32_t r = esp_random();
        char buf[12];
        snprintf(buf, sizeof(buf), "KProx%04X", (unsigned)(r & 0xFFFF));
        _name = String(buf);
    }

    if (needKeys) {
        String priv, pub;
        if (NostrClient::generateKeypair(priv, pub)) {
            _privkey = priv;
            _pubkey  = pub;
        }
    }

    if (needKeys || needName) {
        preferences.begin(NVS_KPROXCHAT, false);
        if (needKeys) {
            preferences.putString("privkey", _privkey);
            preferences.putString("pubkey",  _pubkey);
        }
        if (needName) preferences.putString("name", _name);
        preferences.end();
    }

    _keysReady = (_privkey.length() == 64 && _pubkey.length() == 64);
    _client.setLocalName(_name);
}

// ---- UI helpers -------------------------------------------------------------

void AppKProxChat::_drawTopBar(int page) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(0, 40, 80);
    d.fillRect(0, 0, d.width(), BAR_H, bc);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, bc);

    static const char* labels[NUM_PAGES] = { "Chat", "Status" };
    d.drawString(labels[page], 4, 3);
    drawTabHint(4 + d.textWidth(labels[page]) + 3);

    // Right side: [relay status badge] [connection badge]
    int bx = d.width() - 4;

    NostrState st = _client.state();
    const char* sl;
    uint16_t    sb;
    if (st == NostrState::CONNECTED) {
        sl = "Connected";    sb = d.color565(20, 120, 20);
    } else if (st == NostrState::CONNECTING || st == NostrState::HANDSHAKING) {
        sl = "Connecting";   sb = d.color565(140, 90, 0);
    } else {
        sl = "Disconnected"; sb = d.color565(160, 20, 20);
    }
    int connW = d.textWidth(sl) + 8;
    bx -= connW;
    d.fillRoundRect(bx, 2, connW, BAR_H - 4, 2, sb);
    d.setTextColor(TFT_WHITE, sb);
    d.drawString(sl, bx + 4, 3);

    // Relay response badge — only when there's a recent status to show
    const String& rm = _client.lastRelayMsg();
    if (!rm.isEmpty()) {
        String label = rm.startsWith("REJECT") ? rm.substring(0, min((int)rm.length(), 10))
                                               : rm.substring(0, min((int)rm.length(), 8));
        uint16_t rmCol = rm.startsWith("REJECT") ? d.color565(160, 60, 0)
                                                  : d.color565(0, 80, 60);
        int rmW = d.textWidth(label) + 6;
        bx -= rmW + 2;
        d.fillRoundRect(bx, 2, rmW, BAR_H - 4, 2, rmCol);
        d.setTextColor(TFT_WHITE, rmCol);
        d.drawString(label, bx + 3, 3);
    }
}

void AppKProxChat::_drawBottomBar(const char* hint) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(16, 16, 16);
    d.fillRect(0, d.height() - BOT_H, d.width(), BOT_H, bc);
    d.setTextSize(1);
    d.setTextColor(d.color565(110, 110, 110), bc);
    d.drawString(hint, 2, d.height() - BOT_H + 2);
}

// ---- Page 0: Feed + Compose -------------------------------------------------

void AppKProxChat::_drawPage0() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(KC_BG);
    char extra[6]; snprintf(extra, sizeof(extra), "%d", _client.msgCount());
    _drawTopBar(0);
    d.setTextSize(1);
    d.setTextColor(d.color565(60, 60, 80), KC_BG);
    d.drawString(extra, d.width() - d.textWidth(extra) - 2, 3);

    int feedRows = (FEED_BOTTOM - FEED_Y) / FEED_ROW_H;
    int total    = _client.msgCount();

    if (total == 0) {
        d.setTextColor(d.color565(80, 80, 80), KC_BG);
        const char* msg = !_keysReady            ? "Generating identity..."
                        : !_client.isConnected() ? "Connecting..."
                        :                          "Waiting for messages...";
        d.drawString(msg, 4, FEED_Y + 20);
    } else {
        int maxScroll = max(0, total - feedRows);
        if (_feedScrollPinned) _feedScroll = maxScroll;
        if (_feedScroll > maxScroll) _feedScroll = maxScroll;

        int y = FEED_Y;
        for (int i = 0; i < feedRows && (_feedScroll + i) < total; i++) {
            const NostrMsg& m = _client.msg(_feedScroll + i);
            char pfx[20]; snprintf(pfx, sizeof(pfx), "%s: ", m.name);
            int pfxW = d.textWidth(pfx);
            d.setTextColor(d.color565(80, 160, 255), KC_BG);
            d.drawString(pfx, 4, y);
            d.setTextColor(d.color565(210, 210, 210), KC_BG);
            int maxChars = (d.width() - pfxW - 8) / 6;
            String content = String(m.content);
            if ((int)content.length() > maxChars)
                content = content.substring(0, maxChars - 1) + "~";
            d.drawString(content, 4 + pfxW, y);
            y += FEED_ROW_H;
        }

        if (total > feedRows) {
            char sb[10]; snprintf(sb, sizeof(sb), "%d/%d", _feedScroll + feedRows, total);
            d.setTextColor(d.color565(60, 60, 60), KC_BG);
            d.drawString(sb, d.width() - d.textWidth(sb) - 2, FEED_Y);
        }
    }

    d.drawFastHLine(0, INPUT_Y - 1, d.width(), d.color565(0, 60, 120));

    bool canPost = _client.isConnected() && _keysReady;
    uint16_t ibg = _compEditing ? d.color565(0, 25, 55) : d.color565(0, 15, 35);
    d.fillRect(0, INPUT_Y, d.width(), INPUT_H, ibg);
    d.setTextSize(1);

    if (_compEditing) {
        int maxChars = (d.width() - 8) / 6;
        String disp = _compBuf;
        if ((int)disp.length() >= maxChars) disp = disp.substring(disp.length() - maxChars + 1);
        disp += '_';
        d.setTextColor(TFT_WHITE, ibg);
        d.drawString(disp, 4, INPUT_Y + 3);
    } else if (!_compStatus.isEmpty()) {
        d.setTextColor(_compStatusOk ? TFT_GREEN : d.color565(220, 80, 80), ibg);
        d.drawString(_compStatus, 4, INPUT_Y + 3);
    } else if (!_client.lastRelayMsg().isEmpty()) {
        String rm = _client.lastRelayMsg();
        if ((int)rm.length() > 34) rm = rm.substring(0, 33) + "~";
        uint16_t mc = rm.startsWith("REJECT") ? d.color565(220, 80, 80) : d.color565(140, 140, 100);
        d.setTextColor(mc, ibg);
        d.drawString(rm, 4, INPUT_Y + 3);
    } else {
        d.setTextColor(d.color565(40, 80, 120), ibg);
        d.drawString(canPost ? "ENTER to chat..." : "Connecting...", 4, INPUT_Y + 3);
    }

    if (_compEditing)
        _drawBottomBar("type  ENTER=send  ESC=cancel  DEL=del");
    else
        _drawBottomBar("ENTER=chat  up/dn=scroll  </>=page");
}

void AppKProxChat::_handlePage0(KeyInput ki) {
    int total    = _client.msgCount();
    int feedRows = (FEED_BOTTOM - FEED_Y) / FEED_ROW_H;

    if (_compEditing) {
        if (ki.esc) { _compEditing = false; _compBuf = ""; _compStatus = ""; _needsRedraw = true; return; }
        if (ki.del && !_compBuf.isEmpty()) { _compBuf.remove(_compBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.enter) {
            if (_compBuf.isEmpty()) { _compEditing = false; _compStatus = ""; _needsRedraw = true; return; }
            _compStatus = "Sending..."; _compStatusOk = true;
            _needsRedraw = true; _drawPage0();
            if (_client.publish(_privkey, _compBuf, KPROXCHAT_CHANNEL)) {
                _compStatus = "Sent!"; _compStatusOk = true;
                _compBuf = ""; _compEditing = false;
                _feedScrollPinned = true;
                _pendingRelayCheck = true;
            } else {
                _compStatus = "Send failed"; _compStatusOk = false;
            }
            _needsRedraw = true; return;
        }
        if (ki.arrowUp) {
            if (_feedScroll > 0) { _feedScroll--; _feedScrollPinned = false; }
            _needsRedraw = true; return;
        }
        if (ki.arrowDown) {
            int maxScroll = max(0, total - feedRows);
            if (_feedScroll < maxScroll) _feedScroll++;
            if (_feedScroll >= maxScroll) _feedScrollPinned = true;
            _needsRedraw = true; return;
        }
        if (ki.ch && (int)_compBuf.length() < 280) { _compBuf += ki.ch; _compStatus = ""; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft)  { _page = 1; _needsRedraw = true; return; }
    if (ki.arrowRight) { _page = 1; _needsRedraw = true; return; }
    if (ki.arrowUp) {
        if (_feedScroll > 0) { _feedScroll--; _feedScrollPinned = false; }
        _needsRedraw = true; return;
    }
    if (ki.arrowDown) {
        int maxScroll = max(0, total - feedRows);
        if (_feedScroll < maxScroll) _feedScroll++;
        if (_feedScroll >= maxScroll) _feedScrollPinned = true;
        _needsRedraw = true; return;
    }
    if (ki.enter) {
        if (_client.isConnected() && _keysReady) {
            _compEditing = true; _compBuf = ""; _compStatus = "";
        } else {
            _page = 1;
        }
        _needsRedraw = true;
    }
}

// ---- Page 1: Status ---------------------------------------------------------

void AppKProxChat::_drawPage1() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(KC_BG);
    _drawTopBar(1);
    int y = BAR_H + 4;
    d.setTextSize(1);

    // Nick row — editable
    bool nickSel  = true;  // only one selectable row
    bool nickEdit = _nickEditing;
    uint16_t rowBg = d.color565(0, 25, 50);
    d.fillRect(0, y - 1, d.width(), 26, rowBg);
    d.setTextColor(d.color565(80, 180, 255), rowBg);
    d.drawString("Nick:", 4, y); y += 10;
    uint16_t fbg = nickEdit ? d.color565(0, 40, 80) : d.color565(0, 20, 50);
    d.fillRoundRect(4, y, d.width() - 8, 13, 2, fbg);
    d.setTextColor(TFT_WHITE, fbg);
    String nickVal = nickEdit ? (_nickBuf + "_") : _name;
    if ((int)nickVal.length() > 36) nickVal = nickVal.substring(nickVal.length() - 36);
    d.drawString(nickVal, 6, y + 2);
    y += 16;

    // Relay (fixed, read-only)
    y += 2;
    d.setTextColor(d.color565(100, 100, 100), KC_BG);
    d.drawString("Relay:", 4, y); y += 10;
    d.setTextColor(d.color565(60, 120, 180), KC_BG);
    String rd = String(KPROXCHAT_RELAY);
    if ((int)rd.length() > 34) rd = rd.substring(0, 32) + "..";
    d.drawString(rd, 4, y); y += 14;

    // Pubkey (truncated)
    d.setTextColor(d.color565(100, 100, 100), KC_BG);
    d.drawString("Pubkey:", 4, y); y += 10;
    d.setTextColor(d.color565(60, 120, 180), KC_BG);
    if (_keysReady)
        d.drawString(_pubkey.substring(0, 32) + "..", 4, y);
    else
        d.drawString("Generating...", 4, y);
    y += 14;

    // Status
    if (!_statusMsg.isEmpty()) {
        d.setTextColor(_statusOk ? TFT_GREEN : d.color565(220, 80, 80), KC_BG);
        d.drawString(_statusMsg, 4, y);
    }

    if (nickEdit)
        _drawBottomBar("type nick  ENTER=save  ESC=cancel  DEL=del");
    else {
        const char* connLabel = _client.isConnected() ? "ENTER=disconnect" : "ENTER=connect";
        char hint[64]; snprintf(hint, sizeof(hint), "N=nick  %s  </>=page", connLabel);
        _drawBottomBar(hint);
    }
}

void AppKProxChat::_handlePage1(KeyInput ki) {
    if (_nickEditing) {
        if (ki.esc)   { _nickEditing = false; _nickBuf = ""; _needsRedraw = true; return; }
        if (ki.del && !_nickBuf.isEmpty()) { _nickBuf.remove(_nickBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.enter) {
            if (!_nickBuf.isEmpty()) {
                _name = _nickBuf;
                _client.setLocalName(_name);
                preferences.begin(NVS_KPROXCHAT, false);
                preferences.putString("name", _name);
                preferences.end();
                _statusMsg = "Nick saved"; _statusOk = true;
            }
            _nickEditing = false; _nickBuf = ""; _needsRedraw = true; return;
        }
        if (ki.ch && (int)_nickBuf.length() < 24) { _nickBuf += ki.ch; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft || ki.arrowRight) { _page = 0; _needsRedraw = true; return; }

    if (ki.ch == 'n' || ki.ch == 'N') {
        _nickBuf = isAutoName(_name) ? "" : _name;
        _nickEditing = true; _needsRedraw = true; return;
    }

    if (ki.enter) {
        if (_client.isConnected()) {
            _client.disconnect();
            _statusMsg = "Disconnected"; _statusOk = false;
        } else {
            _statusMsg = "Connecting..."; _statusOk = true;
            _needsRedraw = true; _drawPage1();
            if (WiFi.status() != WL_CONNECTED) {
                _statusMsg = "No WiFi"; _statusOk = false;
            } else if (_client.connect(KPROXCHAT_RELAY)) {
                _client.subscribe(KPROXCHAT_CHANNEL);
                _lastRefreshMs = millis();
                _statusMsg = "Connected!"; _statusOk = true;
                _page = 0;
            } else {
                _statusMsg = _client.lastError(); _statusOk = false;
            }
        }
        _needsRedraw = true;
    }
}

// ---- Lifecycle --------------------------------------------------------------

void AppKProxChat::onEnter() {
    _needsRedraw      = true;
    _feedScroll       = 0;
    _feedScrollPinned = true;
    _compBuf          = "";
    _compEditing      = false;
    _compStatus       = "";
    _pendingRelayCheck = false;
    _nickEditing      = false;
    _nickBuf          = "";
    _statusMsg        = "";
    _page             = 0;
    _lastPollMs       = 0;
    _lastRefreshMs    = 0;

    feedWatchdog();
    _initIdentity();

    if (!_client.isConnected() && WiFi.status() == WL_CONNECTED && _keysReady)
        _autoConnect = true;
}

void AppKProxChat::onExit() {
    _client.disconnect();
    _compEditing = false;
    _nickEditing = false;
}

void AppKProxChat::onUpdate() {
    unsigned long now = millis();

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _compEditing = false; _nickEditing = false;
        _page = (_page + 1) % NUM_PAGES;
        _needsRedraw = true;
        return;
    }

    if (_autoConnect) {
        _autoConnect = false;
        _needsRedraw = true; _drawPage0();
        if (_client.connect(KPROXCHAT_RELAY)) {
            _client.subscribe(KPROXCHAT_CHANNEL);
            _lastRefreshMs = millis();
        } else {
            _statusMsg = _client.lastError(); _statusOk = false;
        }
        _needsRedraw = true;
    }

    if (now - _lastPollMs >= 50) {
        _lastPollMs = now;
        bool got = _client.poll();

        if (_pendingRelayCheck && !_client.lastRelayMsg().isEmpty()) {
            const String& rm = _client.lastRelayMsg();
            if (rm.startsWith("REJECT")) {
                // Remove the optimistically-inserted message from the feed
                _client.removePendingMessage();
                _compStatus   = rm.length() > 28 ? rm.substring(0, 27) + "~" : rm;
                _compStatusOk = false;
            }
            _pendingRelayCheck = false;
            got = true;
        }

        if (got) _needsRedraw = true;
    }

    // Periodic subscribe — pull for new messages once per minute
    if (_client.isConnected() && _lastRefreshMs > 0 &&
        now - _lastRefreshMs >= 60000UL) {
        _client.subscribe(KPROXCHAT_CHANNEL);
        _lastRefreshMs = now;
        _needsRedraw = true;
    }

    // Auto-reconnect if dropped
    if (!_client.isConnected() &&
        _client.state() == NostrState::DISCONNECTED &&
        _lastRefreshMs > 0 &&
        now - _lastRefreshMs >= 30000UL &&
        WiFi.status() == WL_CONNECTED && _keysReady) {
        _lastRefreshMs = now;
        if (_client.connect(KPROXCHAT_RELAY)) {
            _client.subscribe(KPROXCHAT_CHANNEL);
        }
        _needsRedraw = true;
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); break;
            case 1: _drawPage1(); break;
        }
        _needsRedraw  = false;
        _lastRedrawMs = now;
    }

    bool editing = _compEditing || _nickEditing;
    KeyInput ki  = pollKeys(editing);
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc && !editing) { uiManager.returnToLauncher(); return; }

    switch (_page) {
        case 0: _handlePage0(ki); break;
        case 1: _handlePage1(ki); break;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
