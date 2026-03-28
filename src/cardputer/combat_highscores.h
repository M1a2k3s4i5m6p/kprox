#pragma once
#ifdef BOARD_M5STACK_CARDPUTER

#include "../globals.h"
#include "../nostr_client.h"
#include <Preferences.h>
#include <WiFi.h>

static constexpr const char* COMBAT_NVS_NS      = "kprox_chat";   // shared with chat app
static constexpr const char* COMBAT_HS_NVS_NS   = "combat_hs";
static constexpr const char* COMBAT_NOSTR_RELAY  = "wss://relay.damus.io";
static constexpr const char* COMBAT_NOSTR_TAG    = "#kprox_combat";
static constexpr int         COMBAT_MAX_LOCAL_HS = 10;
static constexpr int         COMBAT_MAX_ONLINE_HS= 20;

struct CombatScore {
    char    initials[4];   // 3 chars + NUL
    int     score;
    int     round;
    uint32_t timestamp;
};

// State machine for async nostr operations
enum class CombatHSState : uint8_t {
    IDLE,
    CONNECTING,
    FETCHING,
    POSTING,
    DONE,
    OFFLINE
};

class CombatHighScores {
public:
    CombatHighScores();

    // Call once at app startup to load keys and local scores
    void begin();

    // Submit a new score. Posts to nostr if online; always saves locally.
    // Returns the local rank (1-based), 0 if not in top 10.
    int submitScore(const char* initials, int score, int round);

    // Non-blocking tick — call every onUpdate cycle when in HS or post state.
    // Returns true when online fetch/post is complete (or timed out).
    bool poll();

    // Trigger an online fetch (call after connect attempt)
    void startFetch();

    // Start an async post of a pending score
    void startPost(const char* initials, int score, int round);

    // True when wifi is available
    bool isOnline() const { return WiFi.status() == WL_CONNECTED; }
    bool isConnected() const { return _client.isConnected(); }
    CombatHSState state() const { return _state; }
    const char* statusMsg() const { return _statusMsg; }

    // Local scores (sorted descending)
    int localCount() const { return _localCount; }
    const CombatScore& localScore(int i) const { return _local[i]; }

    // Online scores received from relay (sorted descending)
    int onlineCount() const { return _onlineCount; }
    const CombatScore& onlineScore(int i) const { return _online[i]; }

    bool hasKeys() const { return _keysReady; }
    const String& pubkeyHex() const { return _pubkey; }

    void disconnect();

private:
    void _loadKeys();
    void _loadLocalScores();
    void _saveLocalScores();
    void _insertLocal(const CombatScore& s);
    void _parseNostrMsg(const NostrMsg& m);
    bool _connectRelay();

    String        _privkey;
    String        _pubkey;
    bool          _keysReady    = false;

    CombatScore   _local[COMBAT_MAX_LOCAL_HS];
    int           _localCount   = 0;

    CombatScore   _online[COMBAT_MAX_ONLINE_HS];
    int           _onlineCount  = 0;

    NostrClient   _client;
    CombatHSState _state        = CombatHSState::IDLE;
    unsigned long _stateEnter   = 0;
    unsigned long _timeout      = 12000;

    // Pending post data
    char          _pendingInitials[4] = {};
    int           _pendingScore  = 0;
    int           _pendingRound  = 0;
    bool          _hasPending    = false;

    const char*   _statusMsg     = "";

    static constexpr unsigned long CONNECT_TIMEOUT_MS = 10000;
    static constexpr unsigned long FETCH_TIMEOUT_MS   = 15000;
    static constexpr unsigned long POST_TIMEOUT_MS    = 10000;
};

#endif
