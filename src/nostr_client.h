#pragma once
#include "globals.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#define NOSTR_MAX_MESSAGES 24
#define NOSTR_CONTENT_MAX  180
#define NOSTR_PUBKEY_HEX   64  // 32 bytes → 64 hex chars
#define NOSTR_ID_HEX       64
#define NOSTR_SIG_HEX      128

struct NostrMsg {
    char pubkey[9];           // first 8 hex chars + NUL
    char eventId[9];          // first 8 hex chars + NUL
    char name[17];            // display name (up to 16 chars) or pubkey prefix
    char content[NOSTR_CONTENT_MAX + 1];
    uint32_t created_at;
};

enum class NostrState : uint8_t {
    DISCONNECTED,
    CONNECTING,
    HANDSHAKING,
    CONNECTED,
    ERROR
};

class NostrClient {
public:
    NostrState  state()       const { return _state; }
    const char* stateLabel()  const;
    bool        isConnected() const { return _state == NostrState::CONNECTED; }

    // Returns false if URL is invalid or WiFi is not up
    bool connect(const String& relayUrl);
    void disconnect();

    // Call every loop tick; returns true if a new message was added
    bool poll();

    // Publish a kind-1 note or kind-42 channel message (channelId empty → kind 1)
    bool publish(const String& privkeyHex, const String& content,
                 const String& channelId = "");

    // Subscribe for incoming events
    bool subscribe(const String& channelId = "");

    // Key operations (static — no connection needed)
    static bool generateKeypair(String& privkeyHex, String& pubkeyHex);
    static String pubkeyFromPrivkey(const String& privkeyHex);

    // Message ring buffer (newest first via index 0)
    int            msgCount()        const { return _msgCount; }
    const NostrMsg& msg(int i)       const;
    void           clearMessages();

    const String& lastError()     const { return _lastError; }
    const String& relayHost()     const { return _host; }
    const String& lastRelayMsg()  const { return _lastRelayMsg; }
    bool          lastPublishOk() const { return _lastPublishOk; }

    void setLocalName(const String& name) { _localName = name; }

private:
    bool          _isSecure  = false;
    WiFiClient    _plain;
    WiFiClientSecure _secure;
    Client*       _client    = nullptr;
    NostrState    _state     = NostrState::DISCONNECTED;
    String        _host;
    int           _port      = 80;
    String        _path      = "/";
    String        _subId;
    String        _lastError;
    String        _lastRelayMsg;
    String        _localName;   // display name for locally published messages
    String        _rxBuf;
    String        _pendingEventId;  // event id of the most recently published event
    bool          _lastPublishOk = true;

    // Sorted by created_at descending: _msgs[0] = newest, _msgs[_msgCount-1] = oldest.
    // Deduplication by eventId prefix. Evicts oldest when full.
    NostrMsg _msgs[NOSTR_MAX_MESSAGES];
    int      _msgCount = 0;

    bool _wsHandshake(String* redirectUrl = nullptr);
    bool _wsSendText(const String& text);
    bool _wsPollFrame(String& out);
    void _pushMessage(const char* pubkey64, const char* eventId64,
                      uint32_t ts, const char* content,
                      const char* name = nullptr);
    void _processEvent(const String& json);

    // Schnorr BIP-340
    static bool _schnorrSign(const uint8_t priv[32], const uint8_t msgHash[32],
                              uint8_t sig[64]);
    static void _taggedHash(const char* tag, const uint8_t* data, size_t len,
                             uint8_t out[32]);
    static void _taggedHash3(const char* tag,
                              const uint8_t* a, size_t la,
                              const uint8_t* b, size_t lb,
                              const uint8_t* c, size_t lc,
                              uint8_t out[32]);
    static int  _rng(void*, unsigned char* buf, size_t len);
    static void _toHex(const uint8_t* b, size_t n, char* out);
    static bool _fromHex(const char* hex, uint8_t* out, size_t n);
    static String _buildEventJson(const uint8_t priv[32], const uint8_t pub[32],
                                   int kind, const String& content,
                                   const String& channelId);
    static void _sha256(const uint8_t* data, size_t len, uint8_t out[32]);
};

extern NostrClient nostrClient;
