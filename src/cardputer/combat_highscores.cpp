#ifdef BOARD_M5STACK_CARDPUTER

#include "combat_highscores.h"
#include <Preferences.h>
#include <ArduinoJson.h>

CombatHighScores::CombatHighScores() {}

// ---- Key loading (shared NVS namespace with kproxchat) ----

void CombatHighScores::_loadKeys() {
    Preferences prefs;
    prefs.begin(COMBAT_NVS_NS, true);
    _privkey = prefs.getString("privkey", "");
    _pubkey  = prefs.getString("pubkey",  "");
    prefs.end();

    bool needGenerate = (_privkey.length() != 64 || _pubkey.length() != 64);
    if (needGenerate) {
        String priv, pub;
        if (NostrClient::generateKeypair(priv, pub)) {
            _privkey = priv;
            _pubkey  = pub;
            Preferences pw;
            pw.begin(COMBAT_NVS_NS, false);
            pw.putString("privkey", _privkey);
            pw.putString("pubkey",  _pubkey);
            pw.end();
        }
    }
    _keysReady = (_privkey.length() == 64 && _pubkey.length() == 64);
}

// ---- Local score persistence (own NVS namespace) ----

void CombatHighScores::_loadLocalScores() {
    Preferences prefs;
    prefs.begin(COMBAT_HS_NVS_NS, true);
    _localCount = (int)prefs.getInt("count", 0);
    if (_localCount > COMBAT_MAX_LOCAL_HS) _localCount = COMBAT_MAX_LOCAL_HS;
    for (int i = 0; i < _localCount; i++) {
        char key[8];
        snprintf(key, sizeof(key), "s%d", i);
        String blob = prefs.getString(key, "");
        if (blob.length() == 0) { _localCount = i; break; }
        // Format: "INI score round ts"
        int score = 0, round = 0; unsigned int ts = 0;
        char ini[4] = {};
        sscanf(blob.c_str(), "%3s %d %d %u", ini, &score, &round, &ts);
        strncpy(_local[i].initials, ini, 3); _local[i].initials[3] = 0;
        _local[i].score     = score;
        _local[i].round     = round;
        _local[i].timestamp = (uint32_t)ts;
    }
    prefs.end();
}

void CombatHighScores::_saveLocalScores() {
    Preferences prefs;
    prefs.begin(COMBAT_HS_NVS_NS, false);
    prefs.putInt("count", _localCount);
    for (int i = 0; i < _localCount; i++) {
        char key[8], val[32];
        snprintf(key, sizeof(key), "s%d", i);
        snprintf(val, sizeof(val), "%.3s %d %d %u",
                 _local[i].initials, _local[i].score,
                 _local[i].round, (unsigned)_local[i].timestamp);
        prefs.putString(key, val);
    }
    prefs.end();
}

void CombatHighScores::_insertLocal(const CombatScore& s) {
    // Find insertion point (descending score)
    int pos = _localCount;
    for (int i = 0; i < _localCount; i++) {
        if (s.score > _local[i].score) { pos = i; break; }
    }
    if (pos >= COMBAT_MAX_LOCAL_HS) return;  // below cut-off
    // Shift down
    int newCount = (_localCount < COMBAT_MAX_LOCAL_HS) ? _localCount + 1 : COMBAT_MAX_LOCAL_HS;
    for (int i = newCount - 1; i > pos; i--) _local[i] = _local[i-1];
    _local[pos] = s;
    _localCount = newCount;
}

// ---- Public API ----

void CombatHighScores::begin() {
    _loadKeys();
    _loadLocalScores();
    _state = CombatHSState::IDLE;
}

int CombatHighScores::submitScore(const char* initials, int score, int round) {
    CombatScore s;
    strncpy(s.initials, initials, 3); s.initials[3] = 0;
    s.score     = score;
    s.round     = round;
    s.timestamp = (uint32_t)(millis() / 1000);  // approximate epoch offset

    _insertLocal(s);
    _saveLocalScores();

    // Find rank
    for (int i = 0; i < _localCount; i++)
        if (_local[i].score == score && strncmp(_local[i].initials, initials, 3) == 0)
            return i + 1;
    return 0;
}

void CombatHighScores::startFetch() {
    if (!isOnline() || !_keysReady) { _state = CombatHSState::OFFLINE; return; }
    _state      = CombatHSState::CONNECTING;
    _stateEnter = millis();
    _statusMsg  = "Connecting...";
    _onlineCount = 0;
    if (!_connectRelay()) {
        _state = CombatHSState::OFFLINE;
        _statusMsg = "Relay unavailable";
    }
}

void CombatHighScores::startPost(const char* initials, int score, int round) {
    strncpy(_pendingInitials, initials, 3); _pendingInitials[3] = 0;
    _pendingScore  = score;
    _pendingRound  = round;
    _hasPending    = true;

    if (!isOnline() || !_keysReady) {
        _state     = CombatHSState::OFFLINE;
        _statusMsg = "Offline — saved locally";
        return;
    }
    _state      = CombatHSState::CONNECTING;
    _stateEnter = millis();
    _statusMsg  = "Connecting...";
    if (!_connectRelay()) {
        _state     = CombatHSState::OFFLINE;
        _statusMsg = "Relay unavailable";
    }
}

bool CombatHighScores::_connectRelay() {
    if (_client.isConnected()) return true;
    return _client.connect(COMBAT_NOSTR_RELAY);
}

void CombatHighScores::_parseNostrMsg(const NostrMsg& m) {
    // Expected content format: "INI score round" e.g. "ACE 1420 5"
    char ini[4] = {};
    int  sc = 0, rnd = 0;
    if (sscanf(m.content, "%3s %d %d", ini, &sc, &rnd) < 2) return;
    if (sc <= 0) return;

    CombatScore s;
    strncpy(s.initials, ini, 3); s.initials[3] = 0;
    s.score     = sc;
    s.round     = rnd;
    s.timestamp = m.created_at;

    // Insert into online board (sorted descending, dedup by pubkey+score)
    int pos = _onlineCount;
    for (int i = 0; i < _onlineCount; i++) {
        if (s.score > _online[i].score) { pos = i; break; }
    }
    if (pos >= COMBAT_MAX_ONLINE_HS) return;
    int newCount = (_onlineCount < COMBAT_MAX_ONLINE_HS) ? _onlineCount + 1 : COMBAT_MAX_ONLINE_HS;
    for (int i = newCount - 1; i > pos; i--) _online[i] = _online[i-1];
    _online[pos] = s;
    _onlineCount = newCount;
}

bool CombatHighScores::poll() {
    if (_state == CombatHSState::IDLE || _state == CombatHSState::DONE ||
        _state == CombatHSState::OFFLINE) return true;

    unsigned long elapsed = millis() - _stateEnter;

    // Absorb all new messages from relay
    if (_client.poll()) {
        for (int i = 0; i < _client.msgCount(); i++)
            _parseNostrMsg(_client.msg(i));
    }

    switch (_state) {
        case CombatHSState::CONNECTING:
            if (_client.isConnected()) {
                // Subscribe to fetch existing scores
                _client.subscribe(COMBAT_NOSTR_TAG);
                _state     = _hasPending ? CombatHSState::POSTING : CombatHSState::FETCHING;
                _stateEnter = millis();
                _statusMsg  = _hasPending ? "Posting score..." : "Fetching scores...";
            } else if (elapsed > CONNECT_TIMEOUT_MS) {
                _state     = CombatHSState::OFFLINE;
                _statusMsg = "Timeout connecting";
            }
            break;

        case CombatHSState::FETCHING:
            if (elapsed > FETCH_TIMEOUT_MS) {
                _state     = CombatHSState::DONE;
                _statusMsg = "Done";
                _client.disconnect();
            }
            break;

        case CombatHSState::POSTING:
            if (elapsed > 1500) {  // brief wait for subscription to settle
                if (_hasPending && _keysReady) {
                    char content[48];
                    snprintf(content, sizeof(content), "%.3s %d %d",
                             _pendingInitials, _pendingScore, _pendingRound);
                    // publish as kind-1 with the kprox_combat tag in content
                    String tagged = String(COMBAT_NOSTR_TAG) + " " + String(content);
                    _client.publish(_privkey, tagged, "");
                    _hasPending = false;
                }
                _state      = CombatHSState::FETCHING;  // linger to collect others
                _stateEnter = millis();
                _statusMsg  = "Fetching scores...";
            }
            if (elapsed > POST_TIMEOUT_MS + FETCH_TIMEOUT_MS) {
                _state     = CombatHSState::DONE;
                _statusMsg = "Done";
                _client.disconnect();
            }
            break;

        default: break;
    }

    return (_state == CombatHSState::DONE || _state == CombatHSState::OFFLINE);
}

void CombatHighScores::disconnect() {
    _client.disconnect();
    _state = CombatHSState::IDLE;
}

#endif
