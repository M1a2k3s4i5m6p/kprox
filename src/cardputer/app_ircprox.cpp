#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../credential_store.h"
#include "app_ircprox.h"
#include <mbedtls/base64.h>

namespace Cardputer {

static constexpr const char* DEFAULT_IRC_SERVER  = "irc.libera.chat:6697";
static constexpr const char* DEFAULT_IRC_CHANNEL = "#kprox";
static constexpr const char* CS_IRC_PASSWORD     = "__irc__password";

// ---- Config persistence -----------------------------------------------------

void AppIRCProx::_loadConfig() {
    preferences.begin("kprox_irc", true);
    _server  = preferences.getString("server",  DEFAULT_IRC_SERVER);
    _channel = preferences.getString("channel", DEFAULT_IRC_CHANNEL);
    _nick    = preferences.getString("nick",    "");
    preferences.end();

    if (_nick.isEmpty()) {
        uint32_t r = esp_random();
        char buf[IRC_NICK_MAX];
        snprintf(buf, sizeof(buf), "KProx%04X", (unsigned)(r & 0xFFFF));
        _nick = String(buf);
        preferences.begin("kprox_irc", false);
        preferences.putString("nick", _nick);
        preferences.end();
    }

    // Password lives in CredStore — only load when unlocked
    _password = credStoreLocked ? "" : credStoreGet(CS_IRC_PASSWORD);
}

void AppIRCProx::_saveConfig() {
    preferences.begin("kprox_irc", false);
    preferences.putString("server",  _server);
    preferences.putString("channel", _channel);
    preferences.putString("nick",    _nick);
    preferences.end();
}

void AppIRCProx::_savePassword() {
    if (!credStoreLocked)
        credStoreSet(CS_IRC_PASSWORD, _password);
}

// ---- IRC TCP layer ----------------------------------------------------------

bool AppIRCProx::_connect() {
    _disconnect();   // always starts with a clean slate
    _lastError = "";

    String host = _server;
    int    port = 6697;
    bool   tls  = true;

    int colon = host.lastIndexOf(':');
    if (colon > 0) {
        port = host.substring(colon + 1).toInt();
        host = host.substring(0, colon);
        tls  = (port == 6697 || port == 6698);
    }

    _state = IrcState::CONNECTING;

    // Allocate a fresh client object — never reuse a stopped TLS context
    if (tls) {
        _secureCli = new WiFiClientSecure();
        _secureCli->setInsecure();
        _client = _secureCli;
    } else {
        _plainCli = new WiFiClient();
        _client = _plainCli;
    }

    feedWatchdog();
    if (!_client->connect(host.c_str(), port)) {
        _lastError = "TCP connect failed";
        _state = IrcState::ERROR;
        // Free immediately on failure
        delete _secureCli; _secureCli = nullptr;
        delete _plainCli;  _plainCli  = nullptr;
        _client = nullptr;
        return false;
    }

    _state            = IrcState::REGISTERING;
    _capAcked         = false;
    _saslPending      = false;
    _historyRequested = false;
    _capLsAccum       = "";
    _joinedChannel    = false;
    _lastRxMs         = millis();
    _sendRaw("CAP LS 302");
    _sendRaw("NICK " + _nick);
    _sendRaw("USER " + _nick + " 0 * :KProx IRC");
    return true;
}

void AppIRCProx::_disconnect() {
    if (_client) {
        if (_state == IrcState::CONNECTED)
            _sendRaw("QUIT :KProx disconnect");
        _client->stop();
        _client = nullptr;
    }
    // Free and null the heap objects — next connect gets a fresh context
    delete _secureCli; _secureCli = nullptr;
    delete _plainCli;  _plainCli  = nullptr;

    _state            = IrcState::DISCONNECTED;
    _rxBuf            = "";
    _lastSysMsg       = "";
    _histStatus       = "";
    _saslPending      = false;
    _capAcked         = false;
    _historyRequested = false;
    _joinedChannel    = false;
    _capLsAccum       = "";
    _lastRxMs         = 0;
}

void AppIRCProx::_sendRaw(const String& line) {
    if (!_client || !_client->connected()) return;
    _client->print(line + "\r\n");
    _client->flush();
}

void AppIRCProx::_processLine(const String& raw) {
    if (raw.isEmpty()) return;
    feedWatchdog();

    // Strip IRCv3 message tags (@tag=val;tag2=val2 <space> rest)
    String line = raw;
    if (line.startsWith("@")) {
        int sp = line.indexOf(' ');
        line = (sp > 0) ? line.substring(sp + 1) : "";
        line.trim();
    }

    if (line.startsWith("PING ")) {
        _sendRaw("PONG " + line.substring(5));
        return;
    }

    // Parse: [:prefix] <command> [params...]
    String prefix;
    if (line.startsWith(":")) {
        int sp = line.indexOf(' ');
        prefix = (sp > 0) ? line.substring(1, sp) : line.substring(1);
        line   = (sp > 0) ? line.substring(sp + 1) : "";
    }
    line.trim();

    int sp = line.indexOf(' ');
    String cmd    = (sp > 0) ? line.substring(0, sp) : line;
    String params = (sp > 0) ? line.substring(sp + 1) : "";

    // ---- CAP negotiation ----
    if (cmd == "CAP") {
        // params: "<nick> LS * :cap1 cap2..." (multi-line) or "<nick> LS :caps" (final)
        // or "<nick> ACK :chathistory" / "<nick> NAK :..."
        int firstSp = params.indexOf(' ');
        String subCmd = (firstSp > 0) ? params.substring(firstSp + 1) : params;

        if (subCmd.startsWith("LS")) {
            // Multi-line: "LS * :caps" = more coming; "LS :caps" = final line
            bool isLast = !subCmd.startsWith("LS *");
            int colon = subCmd.indexOf(':');
            if (colon >= 0) _capLsAccum += subCmd.substring(colon + 1) + " ";

            if (isLast) {
                String padded   = " " + _capLsAccum + " ";
                bool hasCH      = padded.indexOf(" chathistory ")       >= 0;
                bool hasDraftCH = padded.indexOf(" draft/chathistory ") >= 0;
                bool hasSASL    = padded.indexOf(" sasl ")              >= 0;
                _capLsAccum = "";

                String req = "batch";
                if (hasCH)      req += " chathistory";
                if (hasDraftCH) req += " draft/chathistory";
                if (hasSASL && _hasPassword()) { req += " sasl"; _saslPending = true; }

                bool wantCaps = hasCH || hasDraftCH || (hasSASL && _hasPassword());
                if (wantCaps) {
                    _sendRaw("CAP REQ :" + req);
                    _histStatus = "CAP REQ sent";
                } else {
                    _histStatus = "No chathistory cap";
                    _sendRaw("CAP END");
                }
            }
        } else if (subCmd.startsWith("ACK")) {
            int colon = subCmd.indexOf(':');
            String acked  = " " + ((colon >= 0) ? subCmd.substring(colon + 1) : subCmd) + " ";
            if (acked.indexOf(" chathistory ")       >= 0 ||
                acked.indexOf(" draft/chathistory ") >= 0) {
                _capAcked = true;
                _histStatus = "CAP ACK ok";
            }
            if (_saslPending && acked.indexOf(" sasl ") >= 0) {
                _sendRaw("AUTHENTICATE PLAIN");
                // Don't send CAP END yet — wait for 903 or 904
            } else {
                _saslPending = false;
                _sendRaw("CAP END");
            }
        } else if (subCmd.startsWith("NAK")) {
            _saslPending = false;
            _histStatus = "CAP NAK";
            _sendRaw("CAP END");
        }
        return;
    }

    // BATCH: track message count for history diagnostics
    if (cmd == "BATCH") {
        if (_historyRequested && params.startsWith("-")) {
            // Batch closed — update status with how many messages arrived
            char buf[32];
            snprintf(buf, sizeof(buf), "History: %d msgs", _msgCount);
            _histStatus = buf;
        }
        return;
    }

    // ---- SASL PLAIN authentication ----
    if (cmd == "AUTHENTICATE") {
        if (_saslPending && params == "+") {
            // Build PLAIN token: \0nick\0password (base64-encoded)
            String plain = String('\0') + _nick + String('\0') + _password;
            size_t inLen = plain.length();
            size_t outLen = 0;
            mbedtls_base64_encode(nullptr, 0, &outLen,
                                  (const unsigned char*)plain.c_str(), inLen);
            uint8_t* b64 = (uint8_t*)malloc(outLen + 1);
            if (b64) {
                mbedtls_base64_encode(b64, outLen + 1, &outLen,
                                      (const unsigned char*)plain.c_str(), inLen);
                b64[outLen] = '\0';
                _sendRaw("AUTHENTICATE " + String((char*)b64));
                free(b64);
            } else {
                _sendRaw("AUTHENTICATE *");  // abort
                _saslPending = false;
                _sendRaw("CAP END");
            }
        }
        return;
    }

    // 900 = RPL_LOGGEDIN (informational, ignore)
    if (cmd == "900") return;

    // 903 = RPL_SASLSUCCESS
    if (cmd == "903") {
        _saslPending = false;
        _lastSysMsg = "Identified!";
        _sendRaw("CAP END");
        return;
    }

    // 904 = ERR_SASLFAIL
    if (cmd == "904") {
        _saslPending = false;
        _lastError = "SASL auth failed";
        _sendRaw("AUTHENTICATE *");
        _sendRaw("CAP END");
        return;
    }

    if (cmd == "001") {
        _state = IrcState::CONNECTED;
        _lastSysMsg = "Connected!";
        String ch = _channel;
        if (!ch.startsWith("#")) ch = "#" + ch;
        _sendRaw("JOIN " + ch);
        return;
    }

    // 366 = RPL_ENDOFNAMES — channel join complete, request history now
    if (cmd == "366") {
        if (_capAcked && !_historyRequested) {
            _historyRequested = true;
            String ch = _channel;
            if (!ch.startsWith("#")) ch = "#" + ch;
            _sendRaw("CHATHISTORY LATEST " + ch + " * 50");
            _histStatus = "History requested";
        } else if (!_capAcked) {
            _histStatus = "No history (cap not acked)";
        }
        _joinedChannel = true;
        return;
    }

    // If CAP ACK arrived after JOIN (unusual but possible), catch up
    if (cmd == "JOIN" && _capAcked && !_historyRequested && _joinedChannel) {
        _historyRequested = true;
        String ch = _channel;
        if (!ch.startsWith("#")) ch = "#" + ch;
        _sendRaw("CHATHISTORY LATEST " + ch + " * 50");
        _histStatus = "History requested (late)";
        return;
    }

    // FAIL = IRCv3 standard error reply for CHATHISTORY and other commands
    if (cmd == "FAIL") {
        int sp2 = params.indexOf(' ');
        String failCmd = (sp2 > 0) ? params.substring(0, sp2) : params;
        if (failCmd == "CHATHISTORY") {
            String rest = (sp2 > 0) ? params.substring(sp2 + 1) : "";
            int sp3 = rest.indexOf(' ');
            String code = (sp3 > 0) ? rest.substring(0, sp3) : rest;
            _histStatus = "FAIL: " + code;
        }
        return;
    }

    // 421 = ERR_UNKNOWNCOMMAND — server doesn't recognise CHATHISTORY at all
    if (cmd == "421") {
        if (params.indexOf("CHATHISTORY") >= 0)
            _histStatus = "No history (421)";
        return;
    }

    if (cmd == "433") {
        // ERR_NICKNAMEINUSE — generate a fresh random fallback, don't mutate _nick
        uint32_t r = esp_random();
        char tmp[IRC_NICK_MAX];
        snprintf(tmp, sizeof(tmp), "%.12s%04X", _nick.c_str(), (unsigned)(r & 0xFFFF));
        _sendRaw(String("NICK ") + tmp);
        return;
    }

    if (cmd == "ERROR") {
        _lastError = params.startsWith(":") ? params.substring(1) : params;
        _state = IrcState::ERROR;
        if (_client) { _client->stop(); _client = nullptr; }
        return;
    }

    if (cmd == "PRIVMSG") {
        int chanEnd = params.indexOf(' ');
        if (chanEnd < 0) return;
        String target  = params.substring(0, chanEnd);
        String text    = params.substring(chanEnd + 1);
        if (text.startsWith(":")) text = text.substring(1);

        String ch = _channel;
        if (!ch.startsWith("#")) ch = "#" + ch;
        if (!target.equalsIgnoreCase(ch)) return;

        String nick = prefix;
        int bang = nick.indexOf('!');
        if (bang > 0) nick = nick.substring(0, bang);

        _pushMsg(nick.c_str(), text.c_str());
        return;
    }

    if (cmd == "KICK") {
        int sp2 = params.indexOf(' ');
        String kicked = (sp2 > 0) ? params.substring(sp2 + 1) : "";
        if (kicked.startsWith(":")) kicked = kicked.substring(1);
        if (kicked.equalsIgnoreCase(_nick)) {
            _lastSysMsg = "Kicked!";
            String ch = _channel;
            if (!ch.startsWith("#")) ch = "#" + ch;
            _sendRaw("JOIN " + ch);
        }
    }
}

void AppIRCProx::_poll() {
    if (!_client) return;

    // A TLS ping-timeout leaves connected()=true but reads block forever.
    // Detect it: if we sent a PING and got no data for DEAD_SECS, force-close.
    static constexpr unsigned long DEAD_MS = 40000UL; // 40s > Libera's 30s ping window
    // Stale-connection guard: track last received byte time.
    // _lastPingMs=0 disables dead detection until we've actually sent a ping.
    unsigned long now = millis();
    if (_state == IrcState::CONNECTED && _lastPingMs > 0 &&
        now - _lastRxMs > DEAD_MS) {
        _client->stop();
        _client = nullptr;
        _state = IrcState::DISCONNECTED;
        _lastSysMsg = "Ping timeout";
        return;
    }

    if (!_client->connected()) {
        if (_state != IrcState::DISCONNECTED && _state != IrcState::ERROR)
            _state = IrcState::DISCONNECTED;
        return;
    }

    // Cap bytes per poll call so we never block long enough to trip the watchdog.
    // At 230400 baud TLS throughput, 512 bytes ≈ < 1ms — safe ceiling.
    static constexpr int MAX_BYTES_PER_POLL = 512;
    int bytesRead = 0;

    _client->setTimeout(0);
    while (bytesRead < MAX_BYTES_PER_POLL && _client->available()) {
        feedWatchdog();
        char c = (char)_client->read();
        bytesRead++;
        _lastRxMs = millis();
        if (c == '\n') {
            _rxBuf.trim();
            if (!_rxBuf.isEmpty()) _processLine(_rxBuf);
            _rxBuf = "";
        } else if (c != '\r') {
            if (_rxBuf.length() < 512) _rxBuf += c;
        }
    }
}

bool AppIRCProx::_publish(const String& msg) {
    if (_state != IrcState::CONNECTED) return false;
    String ch = _channel;
    if (!ch.startsWith("#")) ch = "#" + ch;
    _sendRaw("PRIVMSG " + ch + " :" + msg);
    _pushMsg(_nick.c_str(), msg.c_str());
    return true;
}

// ---- Message buffer ---------------------------------------------------------

void AppIRCProx::_pushMsg(const char* nick, const char* content) {
    if (_msgCount < IRC_MSG_MAX) {
        _msgCount++;
    } else {
        // Evict oldest (index 0) by shifting everything down
        for (int i = 0; i < IRC_MSG_MAX - 1; i++) _msgs[i] = _msgs[i + 1];
    }

    IrcMsg& m = _msgs[_msgCount - 1];  // newest at end
    strncpy(m.nick,    nick,    16); m.nick[16] = '\0';
    strncpy(m.content, content, IRC_CONTENT_MAX); m.content[IRC_CONTENT_MAX] = '\0';
    _needsRedraw = true;
}

// ---- Shared UI helpers ------------------------------------------------------

void AppIRCProx::_drawTopBar(int page, const char* extra) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(10, 40, 60);
    d.fillRect(0, 0, d.width(), BAR_H, bc);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, bc);

    static const char* labels[NUM_PAGES] = {"Feed", "Status", "Config"};
    d.drawString(labels[page], 4, 3);
    drawTabHint(4 + d.textWidth(labels[page]) + 3);

    bool conn = (_state == IrcState::CONNECTED);
    bool reg  = (_state == IrcState::REGISTERING || _state == IrcState::CONNECTING);
    uint16_t sb = conn ? d.color565(20, 100, 20) : reg ? d.color565(100, 80, 0) : d.color565(80, 20, 20);
    const char* sl = conn ? "CONN" : reg ? "..." : "OFF";

    int bx = d.width() - 2;
    if (extra && *extra) {
        int ew = d.textWidth(extra) + 6;
        bx -= ew;
        d.fillRoundRect(bx, 2, ew, BAR_H - 4, 2, d.color565(20, 40, 60));
        d.setTextColor(d.color565(140, 200, 255), d.color565(20, 40, 60));
        d.drawString(extra, bx + 3, 3);
    }
    int bw = d.textWidth(sl) + 8;
    bx -= bw;
    d.fillRoundRect(bx, 2, bw, BAR_H - 4, 2, sb);
    d.setTextColor(TFT_WHITE, sb);
    d.drawString(sl, bx + 4, 3);
}

void AppIRCProx::_drawBottomBar(const char* hint) {
    auto& d = M5Cardputer.Display;
    uint16_t bc = d.color565(16, 16, 16);
    d.fillRect(0, d.height() - BOT_H, d.width(), BOT_H, bc);
    d.setTextSize(1);
    d.setTextColor(d.color565(110, 110, 110), bc);
    d.drawString(hint, 2, d.height() - BOT_H + 2);
}

// ---- Page 0: Feed + Compose -------------------------------------------------

static constexpr int IRC_FEED_Y      = AppIRCProx::BAR_H + 1;
static constexpr int IRC_INPUT_H     = 15;
static constexpr int IRC_INPUT_Y     = 135 - AppIRCProx::BOT_H - IRC_INPUT_H;
static constexpr int IRC_FEED_BOTTOM = IRC_INPUT_Y - 2;
static constexpr int IRC_FEED_ROW_H  = 12;

void AppIRCProx::_drawPage0() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(IP_BG);
    char extra[6]; snprintf(extra, sizeof(extra), "%d", _msgCount);
    _drawTopBar(0, extra);

    int feedRows = (IRC_FEED_BOTTOM - IRC_FEED_Y) / IRC_FEED_ROW_H;
    int total    = _msgCount;

    if (total == 0) {
        d.setTextSize(1);
        d.setTextColor(d.color565(80, 80, 80), IP_BG);
        const char* msg = (_state != IrcState::CONNECTED)
            ? (!_p1Status.isEmpty() ? _p1Status.c_str() : "Not connected")
            : "Waiting for messages...";
        d.drawString(msg, 4, IRC_FEED_Y + 20);
    } else {
        int feedRows = (IRC_FEED_BOTTOM - IRC_FEED_Y) / IRC_FEED_ROW_H;
        // Auto-pin scroll to bottom unless user has scrolled up
        int maxScroll = max(0, total - feedRows);
        if (_feedScrollPinned) _feedScroll = maxScroll;
        if (_feedScroll > maxScroll) _feedScroll = maxScroll;

        int y = IRC_FEED_Y;
        // _msgs[0] = oldest; display from _feedScroll upward
        for (int i = 0; i < feedRows && (_feedScroll + i) < total; i++) {
            const IrcMsg& m = _msgs[_feedScroll + i];
            d.setTextSize(1);
            char pfx[20]; snprintf(pfx, sizeof(pfx), "%s: ", m.nick);
            int pfxW = d.textWidth(pfx);
            d.setTextColor(d.color565(80, 180, 255), IP_BG);
            d.drawString(pfx, 4, y);
            d.setTextColor(d.color565(200, 220, 240), IP_BG);
            int maxChars = (d.width() - pfxW - 8) / 6;
            String content = String(m.content);
            if ((int)content.length() > maxChars) content = content.substring(0, maxChars - 1) + "~";
            d.drawString(content, 4 + pfxW, y);
            y += IRC_FEED_ROW_H;
        }

        if (total > feedRows) {
            char sb[10]; snprintf(sb, sizeof(sb), "%d/%d", _feedScroll + feedRows, total);
            d.setTextColor(d.color565(60, 60, 60), IP_BG);
            d.drawString(sb, d.width() - d.textWidth(sb) - 2, IRC_FEED_Y);
        }
    }

    d.drawFastHLine(0, IRC_INPUT_Y - 1, d.width(), d.color565(20, 60, 80));

    bool canPost = (_state == IrcState::CONNECTED);
    uint16_t ibg = _compEditing ? d.color565(10, 35, 55) : d.color565(8, 20, 35);
    d.fillRect(0, IRC_INPUT_Y, d.width(), IRC_INPUT_H, ibg);
    d.setTextSize(1);
    if (_compEditing) {
        int maxChars = (d.width() - 8) / 6;
        String disp = _compBuf;
        if ((int)disp.length() >= maxChars) disp = disp.substring(disp.length() - maxChars + 1);
        disp += '_';
        d.setTextColor(TFT_WHITE, ibg);
        d.drawString(disp, 4, IRC_INPUT_Y + 3);
    } else if (!_compStatus.isEmpty()) {
        d.setTextColor(_compStatusOk ? TFT_GREEN : d.color565(220, 80, 80), ibg);
        d.drawString(_compStatus, 4, IRC_INPUT_Y + 3);
    } else if (!_lastSysMsg.isEmpty()) {
        d.setTextColor(d.color565(140, 200, 140), ibg);
        d.drawString(_lastSysMsg, 4, IRC_INPUT_Y + 3);
    } else {
        d.setTextColor(d.color565(40, 80, 100), ibg);
        d.drawString(canPost ? "ENTER to compose..." : "Connect to post", 4, IRC_INPUT_Y + 3);
    }

    if (_compEditing)
        _drawBottomBar("type  ENTER=send  ESC=cancel  DEL=del");
    else
        _drawBottomBar("ENTER=compose  up/dn=scroll  </>=page");
}

void AppIRCProx::_handlePage0(KeyInput ki) {
    int total    = _msgCount;
    int feedRows = (IRC_FEED_BOTTOM - IRC_FEED_Y) / IRC_FEED_ROW_H;

    if (_compEditing) {
        if (ki.esc)   { _compEditing = false; _compBuf = ""; _compStatus = ""; _needsRedraw = true; return; }
        if (ki.del && !_compBuf.isEmpty()) { _compBuf.remove(_compBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.enter) {
            if (_compBuf.isEmpty()) { _compEditing = false; _compStatus = ""; _needsRedraw = true; return; }
            if (_publish(_compBuf)) {
                _compStatus  = "Sent!";
                _compStatusOk = true;
                _compBuf     = "";
                _compEditing = false;
            } else {
                _compStatus  = "Send failed";
                _compStatusOk = false;
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
        if (ki.ch && (int)_compBuf.length() < 400) { _compBuf += ki.ch; _compStatus = ""; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft)  { _page = NUM_PAGES - 1; _needsRedraw = true; return; }
    if (ki.arrowRight) { _page = 1;             _needsRedraw = true; return; }
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
        if (_state == IrcState::CONNECTED) {
            _compEditing = true; _compBuf = ""; _compStatus = ""; _needsRedraw = true;
        } else {
            _page = 1; _needsRedraw = true;
        }
    }
}

// ---- Page 1: Status / Connect -----------------------------------------------

void AppIRCProx::_drawPage1() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(IP_BG);
    _drawTopBar(1);
    int y = BAR_H + 4;
    d.setTextSize(1);

    d.setTextColor(d.color565(140, 140, 140), IP_BG);
    d.drawString("Nick:", 4, y); y += 12;
    d.setTextColor(d.color565(100, 200, 255), IP_BG);
    d.drawString(_nick.isEmpty() ? "(none)" : _nick, 4, y); y += 14;

    d.setTextColor(d.color565(140, 140, 140), IP_BG);
    d.drawString("Server:", 4, y); y += 12;
    d.setTextColor(d.color565(100, 200, 255), IP_BG);
    String sd = _server; if ((int)sd.length() > 34) sd = sd.substring(0, 32) + "..";
    d.drawString(sd, 4, y); y += 14;

    d.setTextColor(d.color565(140, 140, 140), IP_BG);
    d.drawString("Channel:", 4, y); y += 12;
    d.setTextColor(d.color565(100, 200, 120), IP_BG);
    d.drawString(_channel.isEmpty() ? "(none)" : _channel, 4, y); y += 14;

    if (!_p1Status.isEmpty()) {
        d.setTextColor(_p1StatusOk ? TFT_GREEN : d.color565(220, 80, 80), IP_BG);
        d.drawString(_p1Status, 4, y); y += 12;
    } else if (!_lastError.isEmpty()) {
        d.setTextColor(d.color565(220, 80, 80), IP_BG);
        d.drawString(_lastError, 4, y); y += 12;
    }

    if (!_histStatus.isEmpty()) {
        d.setTextColor(d.color565(120, 180, 120), IP_BG);
        d.drawString(_histStatus, 4, y);
    }

    const char* connLabel = (_state == IrcState::CONNECTED) ? "ENTER=disconnect" : "ENTER=connect";
    char hint[64]; snprintf(hint, sizeof(hint), "%s  </>=page  ESC", connLabel);
    _drawBottomBar(hint);
}

void AppIRCProx::_handlePage1(KeyInput ki) {
    if (ki.arrowLeft)  { _page = 0; _needsRedraw = true; return; }
    if (ki.arrowRight) { _page = 2; _needsRedraw = true; return; }

    if (ki.enter) {
        if (_state == IrcState::CONNECTED || _state == IrcState::REGISTERING) {
            _disconnect();
            _p1Status = "Disconnected"; _p1StatusOk = false;
        } else {
            _p1Status = "Connecting..."; _p1StatusOk = true;
            _needsRedraw = true; _drawPage1();
            if (WiFi.status() != WL_CONNECTED) {
                _p1Status = "No WiFi"; _p1StatusOk = false;
            } else if (_connect()) {
                _p1Status = "Connecting..."; _p1StatusOk = true;
                _page = 0;
            } else {
                _p1Status = _lastError; _p1StatusOk = false;
            }
        }
        _needsRedraw = true;
    }
}

// ---- Page 2: Config ---------------------------------------------------------

void AppIRCProx::_drawPage2() {
    auto& d = M5Cardputer.Display;
    d.fillScreen(IP_BG);
    _drawTopBar(2);
    int y = BAR_H + 4;
    d.setTextSize(1);

    // Each row: 10px label + 14px field box + 2px gap = 26px
    static constexpr int ROW_H = 26;
    static constexpr int FLD_H = 13;

    struct { const char* label; const String* val; } plainRows[3] = {
        { "Server:",  &_server  },
        { "Channel:", &_channel },
        { "Nick:",    &_nick    },
    };
    for (int i = 0; i < 3; i++) {
        bool sel  = (_cfgSel == (CfgField)i);
        bool edit = (sel && _cfgEditing);
        uint16_t rowBg = sel ? d.color565(10, 40, 60) : (uint16_t)IP_BG;
        if (sel) d.fillRect(0, y - 1, d.width(), ROW_H + 1, rowBg);
        d.setTextColor(sel ? d.color565(140, 210, 255) : d.color565(140, 140, 140), rowBg);
        d.drawString(plainRows[i].label, 4, y); y += 10;
        uint16_t fbg = edit ? d.color565(20, 55, 80) : d.color565(10, 25, 45);
        d.fillRoundRect(4, y, d.width() - 8, FLD_H, 2, fbg);
        d.setTextColor(TFT_WHITE, fbg);
        String val = edit ? (_cfgBuf + "_") : *plainRows[i].val;
        if ((int)val.length() > 36) val = val.substring(val.length() - 36);
        d.drawString(val, 6, y + 2);
        y += FLD_H + 3;
    }

    // Password row — CredStore gated
    {
        bool sel  = (_cfgSel == CF_PASSWORD);
        bool edit = (sel && _cfgEditing);
        uint16_t rowBg = sel ? d.color565(10, 40, 60) : (uint16_t)IP_BG;
        if (sel) d.fillRect(0, y - 1, d.width(), ROW_H + 1, rowBg);
        d.setTextColor(sel ? d.color565(140, 210, 255) : d.color565(140, 140, 140), rowBg);
        d.drawString("Password:", 4, y); y += 10;
        uint16_t fbg = edit ? d.color565(20, 55, 80) : d.color565(10, 25, 45);
        d.fillRoundRect(4, y, d.width() - 8, FLD_H, 2, fbg);
        if (credStoreLocked) {
            d.setTextColor(d.color565(160, 100, 40), fbg);
            d.drawString("Unlock CredStore to edit", 6, y + 2);
        } else {
            d.setTextColor(TFT_WHITE, fbg);
            String val;
            if (edit) {
                val = _cfgBuf + "_";                              // plaintext while typing
            } else {
                val = _password.isEmpty() ? "(none)" : String(_password.length(), '*');
            }
            if ((int)val.length() > 36) val = val.substring(val.length() - 36);
            d.drawString(val, 6, y + 2);
        }
        y += FLD_H + 3;
    }

    if (!_cfgStatus.isEmpty()) {
        d.setTextColor(_cfgStatusOk ? TFT_GREEN : d.color565(220, 80, 80), IP_BG);
        d.drawString(_cfgStatus, 4, y);
    }

    if (_cfgEditing) _drawBottomBar("type  ENTER=save  ESC=cancel  DEL=del");
    else             _drawBottomBar("up/dn=row  ENTER=edit  </>=page  ESC");
}

// Returns true if s looks like the auto-generated "KProxXXXX" nick
static bool isAutoNick(const String& s) {
    if (s.length() != 9 || !s.startsWith("KProx")) return false;
    for (int i = 5; i < 9; i++) {
        char c = s.charAt(i);
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

void AppIRCProx::_handlePage2(KeyInput ki) {
    if (_cfgEditing) {
        // Re-check lock if user locked CredStore while editing the password field
        if (_cfgSel == CF_PASSWORD && credStoreLocked) {
            _cfgEditing = false; _cfgBuf = "";
            _cfgStatus = "CredStore locked"; _cfgStatusOk = false;
            _needsRedraw = true; return;
        }
        if (ki.esc)   { _cfgEditing = false; _cfgBuf = ""; _cfgStatus = ""; _needsRedraw = true; return; }
        if (ki.del && !_cfgBuf.isEmpty()) { _cfgBuf.remove(_cfgBuf.length() - 1); _needsRedraw = true; return; }
        if (ki.enter) {
            if      (_cfgSel == CF_SERVER)   _server  = _cfgBuf.isEmpty() ? String(DEFAULT_IRC_SERVER)  : _cfgBuf;
            else if (_cfgSel == CF_CHANNEL)  _channel = _cfgBuf.isEmpty() ? String(DEFAULT_IRC_CHANNEL) : _cfgBuf;
            else if (_cfgSel == CF_NICK && !_cfgBuf.isEmpty()) _nick = _cfgBuf;
            else if (_cfgSel == CF_PASSWORD) { _password = _cfgBuf; _savePassword(); }

            if (_cfgSel != CF_PASSWORD) _saveConfig();
            _cfgEditing = false; _cfgBuf = "";
            _cfgStatus = "Saved"; _cfgStatusOk = true;
            _needsRedraw = true; return;
        }
        if (ki.ch && (int)_cfgBuf.length() < 100) { _cfgBuf += ki.ch; _cfgStatus = ""; _needsRedraw = true; }
        return;
    }

    if (ki.arrowLeft)  { _page = 1; _needsRedraw = true; return; }
    if (ki.arrowRight) { _page = 0; _needsRedraw = true; return; }
    if (ki.arrowUp || ki.arrowDown) {
        _cfgSel = (CfgField)(((int)_cfgSel + 1) % CF_COUNT);
        _cfgStatus = ""; _needsRedraw = true; return;
    }
    if (ki.enter) {
        if (_cfgSel == CF_PASSWORD && credStoreLocked) {
            _cfgStatus = "Unlock CredStore first"; _cfgStatusOk = false; _needsRedraw = true; return;
        }
        switch (_cfgSel) {
            case CF_SERVER:   _cfgBuf = _server;   break;
            case CF_CHANNEL:  _cfgBuf = _channel;  break;
            // For auto-generated nick start blank so user types fresh; otherwise pre-fill
            case CF_NICK:     _cfgBuf = isAutoNick(_nick) ? "" : _nick; break;
            case CF_PASSWORD: _cfgBuf = "";         break;  // always blank for security
            default: break;
        }
        _cfgEditing = true; _cfgStatus = ""; _needsRedraw = true;
    }
}

// ---- Lifecycle --------------------------------------------------------------

void AppIRCProx::onEnter() {
    _needsRedraw = true;
    _feedScroll       = 0;
    _feedScrollPinned = true;
    _compBuf     = "";
    _compEditing = false;
    _compStatus  = "";
    _cfgSel      = CF_SERVER;
    _cfgEditing  = false;
    _cfgBuf      = "";
    _cfgStatus   = "";
    _p1Status    = "";
    _page        = 0;
    _loadConfig();
    _lastPollMs       = 0;
    _lastPingMs       = 0;
    _lastRxMs         = millis();
    _autoConnect      = (!(_state == IrcState::CONNECTED || _state == IrcState::REGISTERING)
                         && WiFi.status() == WL_CONNECTED);
    _capAcked         = false;
    _historyRequested = false;
    _capLsAccum       = "";
    _joinedChannel    = false;
    _prevState    = _state;
    _prevMsgCount = _msgCount;
    _prevSysMsg   = _lastSysMsg;
}

void AppIRCProx::onExit() {
    _disconnect();
    _compEditing = false;
    _cfgEditing  = false;
}

void AppIRCProx::onUpdate() {
    unsigned long now = millis();

    if (M5Cardputer.BtnA.wasPressed()) {
        uiManager.notifyInteraction();
        _compEditing = false; _cfgEditing = false;
        _page = (_page + 1) % NUM_PAGES;
        _needsRedraw = true;
        return;
    }

    if (_autoConnect) {
        _autoConnect = false;
        _p1Status = "Connecting..."; _p1StatusOk = true;
        _needsRedraw = true;
        _drawPage0();
        if (!_connect()) {
            _p1Status = _lastError; _p1StatusOk = false;
        }
        _needsRedraw = true;
    }

    if (now - _lastPollMs >= 30) {
        _lastPollMs = now;
        _poll();

        if (_state != _prevState || _msgCount != _prevMsgCount || _lastSysMsg != _prevSysMsg) {
            _needsRedraw  = true;
            _prevState    = _state;
            _prevMsgCount = _msgCount;
            _prevSysMsg   = _lastSysMsg;
        }
    }

    // Send PING every 90s to keep connection alive
    if (_state == IrcState::CONNECTED && now - _lastPingMs >= 90000UL) {
        _lastPingMs = now;
        _sendRaw("PING :kprox");
    }

    if (_needsRedraw) {
        switch (_page) {
            case 0: _drawPage0(); break;
            case 1: _drawPage1(); break;
            case 2: _drawPage2(); break;
        }
        _needsRedraw  = false;
    }

    bool editing = _compEditing || _cfgEditing;
    KeyInput ki  = pollKeys(editing);
    if (!ki.anyKey) return;
    uiManager.notifyInteraction();

    if (ki.esc && !editing) { uiManager.returnToLauncher(); return; }

    switch (_page) {
        case 0: _handlePage0(ki); break;
        case 1: _handlePage1(ki); break;
        case 2: _handlePage2(ki); break;
    }
}

} // namespace Cardputer
#endif // BOARD_M5STACK_CARDPUTER
