#include "nostr_client.h"
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>
#include <ArduinoJson.h>

NostrClient nostrClient;

// ---- Helpers ----------------------------------------------------------------

static const NostrMsg s_emptyMsg = {};

void NostrClient::_toHex(const uint8_t* b, size_t n, char* out) {
    static const char* h = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i*2]   = h[b[i] >> 4];
        out[i*2+1] = h[b[i] & 0xF];
    }
    out[n*2] = '\0';
}

bool NostrClient::_fromHex(const char* hex, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(hex[i*2]);
        int lo = nibble(hex[i*2+1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

void NostrClient::_sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

int NostrClient::_rng(void*, unsigned char* buf, size_t len) {
    for (size_t i = 0; i < len; i += 4) {
        uint32_t r = esp_random();
        size_t chunk = (len - i < 4) ? (len - i) : 4;
        memcpy(buf + i, &r, chunk);
    }
    return 0;
}

// tagged_hash(tag, data) = SHA256(SHA256(tag) || SHA256(tag) || data)
void NostrClient::_taggedHash(const char* tag, const uint8_t* data, size_t len,
                               uint8_t out[32]) {
    uint8_t tagHash[32];
    _sha256((const uint8_t*)tag, strlen(tag), tagHash);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, tagHash, 32);
    mbedtls_sha256_update(&ctx, tagHash, 32);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

void NostrClient::_taggedHash3(const char* tag,
                                const uint8_t* a, size_t la,
                                const uint8_t* b, size_t lb,
                                const uint8_t* c, size_t lc,
                                uint8_t out[32]) {
    uint8_t tagHash[32];
    _sha256((const uint8_t*)tag, strlen(tag), tagHash);

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, tagHash, 32);
    mbedtls_sha256_update(&ctx, tagHash, 32);
    mbedtls_sha256_update(&ctx, a, la);
    mbedtls_sha256_update(&ctx, b, lb);
    mbedtls_sha256_update(&ctx, c, lc);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

// ---- BIP-340 Schnorr signing -----------------------------------------------

bool NostrClient::_schnorrSign(const uint8_t priv[32], const uint8_t msgHash[32],
                                uint8_t sig[64]) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d0, d, k0, k, e, s;
    mbedtls_ecp_point P, R;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d0); mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&k0); mbedtls_mpi_init(&k);
    mbedtls_mpi_init(&e);  mbedtls_mpi_init(&s);
    mbedtls_ecp_point_init(&P); mbedtls_ecp_point_init(&R);

    bool ok = false;

    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0) goto done;
    if (mbedtls_mpi_read_binary(&d0, priv, 32) != 0) goto done;

    // P = d0 * G
    if (mbedtls_ecp_mul(&grp, &P, &d0, &grp.G, _rng, nullptr) != 0) goto done;
    feedWatchdog();

    // BIP-340: d = d0 if P.y is even, else n - d0
    if (mbedtls_mpi_get_bit(&P.Y, 0)) {
        if (mbedtls_mpi_sub_mpi(&d, &grp.N, &d0) != 0) goto done;
    } else {
        if (mbedtls_mpi_copy(&d, &d0) != 0) goto done;
    }

    {
        // t = bytes(d) XOR tagged_hash("BIP0340/aux", aux_rand)
        uint8_t d_bytes[32], aux[32], t[32], aux_hash[32];
        if (mbedtls_mpi_write_binary(&d, d_bytes, 32) != 0) goto done;
        _rng(nullptr, aux, 32);
        _taggedHash("BIP0340/aux", aux, 32, aux_hash);
        for (int i = 0; i < 32; i++) t[i] = d_bytes[i] ^ aux_hash[i];

        uint8_t Px[32];
        if (mbedtls_mpi_write_binary(&P.X, Px, 32) != 0) goto done;

        // k0 = tagged_hash("BIP0340/nonce", t || Px || msg) mod n
        uint8_t rand_bytes[32];
        _taggedHash3("BIP0340/nonce", t, 32, Px, 32, msgHash, 32, rand_bytes);
        if (mbedtls_mpi_read_binary(&k0, rand_bytes, 32) != 0) goto done;
        if (mbedtls_mpi_mod_mpi(&k0, &k0, &grp.N) != 0) goto done;
        if (mbedtls_mpi_cmp_int(&k0, 0) == 0) goto done;

        // R = k0 * G
        if (mbedtls_ecp_mul(&grp, &R, &k0, &grp.G, _rng, nullptr) != 0) goto done;
        feedWatchdog();

        // k = k0 if R.y even, else n - k0
        if (mbedtls_mpi_get_bit(&R.Y, 0)) {
            if (mbedtls_mpi_sub_mpi(&k, &grp.N, &k0) != 0) goto done;
        } else {
            if (mbedtls_mpi_copy(&k, &k0) != 0) goto done;
        }

        uint8_t Rx[32];
        if (mbedtls_mpi_write_binary(&R.X, Rx, 32) != 0) goto done;

        // e = tagged_hash("BIP0340/challenge", Rx || Px || msg) mod n
        uint8_t e_bytes[32];
        _taggedHash3("BIP0340/challenge", Rx, 32, Px, 32, msgHash, 32, e_bytes);
        if (mbedtls_mpi_read_binary(&e, e_bytes, 32) != 0) goto done;
        if (mbedtls_mpi_mod_mpi(&e, &e, &grp.N) != 0) goto done;

        // s = (k + e * d) mod n
        if (mbedtls_mpi_mul_mpi(&s, &e, &d) != 0) goto done;
        if (mbedtls_mpi_add_mpi(&s, &s, &k) != 0) goto done;
        if (mbedtls_mpi_mod_mpi(&s, &s, &grp.N) != 0) goto done;

        // sig = Rx || s
        uint8_t s_bytes[32];
        if (mbedtls_mpi_write_binary(&s, s_bytes, 32) != 0) goto done;
        memcpy(sig,      Rx,      32);
        memcpy(sig + 32, s_bytes, 32);
        ok = true;
    }

done:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d0); mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&k0); mbedtls_mpi_free(&k);
    mbedtls_mpi_free(&e);  mbedtls_mpi_free(&s);
    mbedtls_ecp_point_free(&P); mbedtls_ecp_point_free(&R);
    return ok;
}

// ---- Key generation ---------------------------------------------------------

bool NostrClient::generateKeypair(String& privkeyHex, String& pubkeyHex) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point P;

    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&P);

    bool ok = false;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0) goto done;

    // Generate random private key in [1, n-1]
    {
        uint8_t privBytes[32];
        do {
            for (int i = 0; i < 32; i += 4) {
                uint32_t r = esp_random();
                memcpy(privBytes + i, &r, 4);
            }
            mbedtls_mpi_read_binary(&d, privBytes, 32);
            feedWatchdog();
        } while (mbedtls_mpi_cmp_int(&d, 0) == 0 ||
                 mbedtls_mpi_cmp_mpi(&d, &grp.N) >= 0);

        // P = d * G
        if (mbedtls_ecp_mul(&grp, &P, &d, &grp.G, _rng, nullptr) != 0) goto done;
        feedWatchdog();

        // BIP-340: if P.y is odd, negate d so the stored key always pairs with an even-Y pubkey
        if (mbedtls_mpi_get_bit(&P.Y, 0)) {
            mbedtls_mpi_sub_mpi(&d, &grp.N, &d);
            // Recompute P with negated d
            mbedtls_ecp_point_free(&P); mbedtls_ecp_point_init(&P);
            if (mbedtls_ecp_mul(&grp, &P, &d, &grp.G, _rng, nullptr) != 0) goto done;
            feedWatchdog();
            if (mbedtls_mpi_write_binary(&d, privBytes, 32) != 0) goto done;
        }

        uint8_t pubBytes[32];
        if (mbedtls_mpi_write_binary(&P.X, pubBytes, 32) != 0) goto done;

        char privHex[65], pubHex[65];
        _toHex(privBytes, 32, privHex);
        _toHex(pubBytes,  32, pubHex);
        privkeyHex = String(privHex);
        pubkeyHex  = String(pubHex);
        ok = true;
    }

done:
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&P);
    return ok;
}

String NostrClient::pubkeyFromPrivkey(const String& privkeyHex) {
    if (privkeyHex.length() != 64) return "";

    uint8_t priv[32];
    if (!_fromHex(privkeyHex.c_str(), priv, 32)) return "";

    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point P;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&P);

    String result;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) == 0 &&
        mbedtls_mpi_read_binary(&d, priv, 32) == 0 &&
        mbedtls_ecp_mul(&grp, &P, &d, &grp.G, _rng, nullptr) == 0) {
        feedWatchdog();
        uint8_t pub[32];
        if (mbedtls_mpi_write_binary(&P.X, pub, 32) == 0) {
            char hex[65];
            _toHex(pub, 32, hex);
            result = String(hex);
        }
    }
    mbedtls_ecp_group_free(&grp);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_point_free(&P);
    return result;
}

// ---- Nostr event building ---------------------------------------------------

String NostrClient::_buildEventJson(const uint8_t priv[32], const uint8_t pub[32],
                                     int kind, const String& content,
                                     const String& channelId) {
    char pubHex[65];
    _toHex(pub, 32, pubHex);

    uint32_t ts = (uint32_t)time(nullptr);

    // Build tags JSON
    String tagsJson = "[]";
    if (kind == 42 && channelId.length() == 64) {
        tagsJson = "[[\"e\",\"" + channelId + "\",\"\",\"root\"]]";
    } else if (channelId.startsWith("#") && channelId.length() > 1) {
        String tag = channelId.substring(1);
        tagsJson = "[[\"t\",\"" + tag + "\"]]";
    }

    // Serialise for hashing: [0, pubkey, created_at, kind, tags, content]
    String serial = "[0,\"";
    serial += pubHex;
    serial += "\",";
    serial += ts;
    serial += ",";
    serial += kind;
    serial += ",";
    serial += tagsJson;
    serial += ",\"";
    // Escape content
    for (char c : content) {
        if (c == '"')       serial += "\\\"";
        else if (c == '\\') serial += "\\\\";
        else if (c == '\n') serial += "\\n";
        else if (c == '\r') serial += "\\r";
        else                serial += c;
    }
    serial += "\"]";

    // Event ID = SHA256(serial)
    uint8_t idBytes[32];
    _sha256((const uint8_t*)serial.c_str(), serial.length(), idBytes);
    char idHex[65];
    _toHex(idBytes, 32, idHex);

    // Sign
    uint8_t sig[64];
    if (!_schnorrSign(priv, idBytes, sig)) return "";
    char sigHex[129];
    _toHex(sig, 64, sigHex);

    // Assemble full event object
    String ev = "{\"id\":\"";
    ev += idHex;
    ev += "\",\"pubkey\":\"";
    ev += pubHex;
    ev += "\",\"created_at\":";
    ev += ts;
    ev += ",\"kind\":";
    ev += kind;
    ev += ",\"tags\":";
    ev += tagsJson;
    ev += ",\"content\":\"";
    for (char c : content) {
        if (c == '"')       ev += "\\\"";
        else if (c == '\\') ev += "\\\\";
        else if (c == '\n') ev += "\\n";
        else if (c == '\r') ev += "\\r";
        else                ev += c;
    }
    ev += "\",\"sig\":\"";
    ev += sigHex;
    ev += "\"}";
    return ev;
}

// ---- WebSocket --------------------------------------------------------------

static bool parseRelayUrl(const String& url, bool& isSecure,
                           String& host, int& port, String& path) {
    if (url.startsWith("wss://"))      { isSecure = true;  host = url.substring(6); port = 443; }
    else if (url.startsWith("ws://"))  { isSecure = false; host = url.substring(5); port = 80;  }
    else return false;

    int slash = host.indexOf('/');
    if (slash >= 0) { path = host.substring(slash); host = host.substring(0, slash); }
    else path = "/";

    int colon = host.indexOf(':');
    if (colon >= 0) { port = host.substring(colon + 1).toInt(); host = host.substring(0, colon); }

    return !host.isEmpty();
}

bool NostrClient::connect(const String& relayUrl) {
    disconnect();
    _lastError = "";

    String url = relayUrl;

    // Follow up to 2 redirects (relay commonly redirects ws:// → wss://)
    for (int attempt = 0; attempt <= 2; attempt++) {
        if (!parseRelayUrl(url, _isSecure, _host, _port, _path)) {
            _lastError = "Bad URL: " + url;
            _state = NostrState::ERROR;
            return false;
        }

        _state = NostrState::CONNECTING;

        if (_isSecure) {
            _secure.setInsecure();
            _client = &_secure;
        } else {
            _client = &_plain;
        }

        if (!_client->connect(_host.c_str(), _port)) {
            _lastError = "TCP connect failed";
            _state = NostrState::ERROR;
            return false;
        }

        _state = NostrState::HANDSHAKING;
        String redirectUrl;
        if (_wsHandshake(&redirectUrl)) {
            _state = NostrState::CONNECTED;
            return true;
        }

        _client->stop();

        if (redirectUrl.isEmpty()) {
            _state = NostrState::ERROR;
            return false;
        }

        // Got a redirect — try the new URL
        url = redirectUrl;
    }

    _state = NostrState::ERROR;
    _lastError = "Too many redirects";
    return false;
}

void NostrClient::disconnect() {
    if (_client) { _client->stop(); _client = nullptr; }
    _state          = NostrState::DISCONNECTED;
    _rxBuf          = "";
    _subId          = "";
    _lastRelayMsg   = "";
    _pendingEventId = "";
}

bool NostrClient::_wsHandshake(String* redirectUrl) {
    if (redirectUrl) *redirectUrl = "";

    uint8_t keyBytes[16];
    for (int i = 0; i < 16; i += 4) { uint32_t r = esp_random(); memcpy(keyBytes+i,&r,4); }
    size_t b64Len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Len, keyBytes, 16);
    uint8_t* b64 = (uint8_t*)malloc(b64Len + 1);
    if (!b64) return false;
    mbedtls_base64_encode(b64, b64Len+1, &b64Len, keyBytes, 16);
    b64[b64Len] = '\0';
    String wsKey((char*)b64);
    free(b64);

    String req = "GET " + _path + " HTTP/1.1\r\n";
    req += "Host: " + _host + "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Origin: http://" + _host + "\r\n";
    req += "Sec-WebSocket-Key: " + wsKey + "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "\r\n";
    _client->print(req);

    // Read response headers (up to 12s — wss:// TLS adds latency)
    unsigned long deadline = millis() + 12000;
    String headers;
    headers.reserve(512);
    while (millis() < deadline) {
        feedWatchdog();
        while (_client->available()) {
            char c = (char)_client->read();
            if (headers.length() < 2048) headers += c;
            if (headers.endsWith("\r\n\r\n")) goto headers_done;
        }
        delay(5);
    }
    _lastError = "Handshake timeout";
    return false;

headers_done:
    if (headers.startsWith("HTTP/1.1 101") || headers.startsWith("HTTP/1.0 101"))
        return true;

    // Extract status code
    int statusCode = 0;
    if (headers.length() >= 12) statusCode = headers.substring(9, 12).toInt();

    // On redirect, extract Location and convert http(s) → ws(s)
    if ((statusCode == 301 || statusCode == 302 || statusCode == 307 || statusCode == 308)
        && redirectUrl) {
        int loc = headers.indexOf("Location: ");
        if (loc < 0) loc = headers.indexOf("location: ");
        if (loc >= 0) {
            loc += 10;
            int eol = headers.indexOf('\r', loc);
            String location = headers.substring(loc, eol > loc ? eol : loc + 256);
            location.trim();
            // Translate http(s):// → ws(s)://
            if      (location.startsWith("https://")) location = "wss://"  + location.substring(8);
            else if (location.startsWith("http://"))  location = "ws://"   + location.substring(7);
            *redirectUrl = location;
        }
    }

    int eol = headers.indexOf('\r');
    _lastError = (eol > 0) ? headers.substring(0, eol) : "Bad WS response";
    return false;
}

bool NostrClient::_wsSendText(const String& text) {
    if (!_client || !_client->connected()) return false;
    size_t len = text.length();
    uint8_t header[10];
    size_t hLen = 0;

    header[hLen++] = 0x81; // FIN + text opcode

    // Masked (client→server always masked per RFC 6455)
    if (len <= 125) {
        header[hLen++] = 0x80 | (uint8_t)len;
    } else if (len <= 65535) {
        header[hLen++] = 0x80 | 126;
        header[hLen++] = (len >> 8) & 0xFF;
        header[hLen++] =  len       & 0xFF;
    } else {
        return false; // messages should never be this big
    }

    uint8_t mask[4];
    for (int i = 0; i < 4; i += 4) { uint32_t r = esp_random(); memcpy(mask+i,&r,4); }
    for (int i = 0; i < 4; i++) header[hLen++] = mask[i];

    _client->write(header, hLen);

    // Write payload in masked chunks
    const uint8_t* src = (const uint8_t*)text.c_str();
    uint8_t chunk[64];
    for (size_t off = 0; off < len; ) {
        size_t n = min((size_t)64, len - off);
        for (size_t i = 0; i < n; i++) chunk[i] = src[off+i] ^ mask[(off+i)%4];
        _client->write(chunk, n);
        off += n;
    }
    return true;
}

bool NostrClient::_wsPollFrame(String& out) {
    if (!_client) return false;
    if (!_client->connected()) { _state = NostrState::DISCONNECTED; return false; }
    if (!_client->available()) return false;

    _client->setTimeout(2000); // 2s for any byte to arrive

    // Read 2-byte header
    uint8_t hdr[2];
    if (_client->readBytes(hdr, 2) != 2) return false;

    uint8_t op    = hdr[0] & 0x0F;
    bool masked   = (hdr[1] & 0x80) != 0;
    uint64_t plen = (hdr[1] & 0x7F);

    if (plen == 126) {
        uint8_t ext[2];
        if (_client->readBytes(ext, 2) != 2) return false;
        plen = ((uint16_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8];
        if (_client->readBytes(ext, 8) != 8) return false;
        plen = 0;
        for (int i = 0; i < 8; i++) plen = (plen << 8) | ext[i];
    }

    uint8_t mask[4] = {0,0,0,0};
    if (masked && _client->readBytes(mask, 4) != 4) return false;

    feedWatchdog();

    if (op == 8) { _state = NostrState::DISCONNECTED; _client->stop(); return false; }
    if (op == 9) {
        // ping — drain body and send pong
        for (uint64_t i = 0; i < plen; i++) { uint8_t b; _client->readBytes(&b, 1); }
        uint8_t pong[] = {0x8A, 0x00};
        _client->write(pong, 2);
        return false;
    }
    if (op != 1 && op != 0) {
        // drain unrecognised frame
        for (uint64_t i = 0; i < plen; i++) { uint8_t b; _client->readBytes(&b, 1); feedWatchdog(); }
        return false;
    }

    // Cap at 8KB — discard oversized frames rather than OOM
    if (plen > 8192) {
        for (uint64_t i = 0; i < plen; i++) { uint8_t b; _client->readBytes(&b, 1); feedWatchdog(); }
        return false;
    }

    // Read payload in chunks, unmasking as we go
    out = "";
    out.reserve((size_t)plen);
    uint8_t chunk[64];
    uint64_t remaining = plen;
    uint64_t pos = 0;
    while (remaining > 0) {
        size_t n = (size_t)min(remaining, (uint64_t)sizeof(chunk));
        size_t got = _client->readBytes(chunk, n);
        if (got == 0) return false; // timeout — stream corrupt, drop frame
        for (size_t i = 0; i < got; i++) {
            out += (char)(chunk[i] ^ mask[pos % 4]);
            pos++;
        }
        remaining -= got;
        feedWatchdog();
    }
    return true;
}

// ---- Message buffer ---------------------------------------------------------

const NostrMsg& NostrClient::msg(int i) const {
    if (i < 0 || i >= _msgCount) return s_emptyMsg;
    return _msgs[i];
}

void NostrClient::clearMessages() { _msgCount = 0; }

void NostrClient::_pushMessage(const char* pubkey64, const char* eventId64,
                                uint32_t ts, const char* content,
                                const char* name) {
    for (int i = 0; i < _msgCount; i++) {
        if (strncmp(_msgs[i].eventId, eventId64, 8) == 0) return;
    }

    // Find insertion position — sorted ascending (oldest first; [0]=oldest, [N-1]=newest)
    int pos = _msgCount;
    for (int i = 0; i < _msgCount; i++) {
        if (ts < _msgs[i].created_at) { pos = i; break; }
    }

    if (_msgCount < NOSTR_MAX_MESSAGES) {
        for (int i = _msgCount; i > pos; i--) _msgs[i] = _msgs[i - 1];
        _msgCount++;
    } else {
        if (pos == 0) return;           // older than everything — discard
        pos--;                           // shift window: evict oldest ([0])
        for (int i = 0; i < pos; i++) _msgs[i] = _msgs[i + 1];
    }

    NostrMsg& m = _msgs[pos];
    strncpy(m.pubkey,  pubkey64,  8); m.pubkey[8]  = '\0';
    strncpy(m.eventId, eventId64, 8); m.eventId[8] = '\0';
    m.created_at = ts;
    strncpy(m.content, content, NOSTR_CONTENT_MAX);
    m.content[NOSTR_CONTENT_MAX] = '\0';
    // Use provided name or fall back to pubkey prefix
    const char* n = (name && *name) ? name : pubkey64;
    strncpy(m.name, n, 16); m.name[16] = '\0';
}

// ---- Event parsing ----------------------------------------------------------

void NostrClient::_processEvent(const String& json) {
    if (json.isEmpty()) return;

    if (json.startsWith("[\"NOTICE\"")) {
        // ["NOTICE","message"] — store for display
        int s = json.indexOf('"', 9);
        int e = json.lastIndexOf('"');
        if (s >= 0 && e > s) _lastRelayMsg = "RELAY: " + json.substring(s+1, e);
        return;
    }

    if (json.startsWith("[\"OK\"")) {
        JsonDocument doc;
        if (!deserializeJson(doc, json)) {
            const char* evId = doc[1] | "";
            bool ok = doc[2].as<bool>();
            const char* reason = doc[3] | "";
            _lastPublishOk = ok;
            if (!ok) {
                _lastRelayMsg = String("REJECT: ") + reason;
            } else if (_pendingEventId.length() > 0 &&
                       strncmp(evId, _pendingEventId.c_str(), 8) == 0) {
                _lastRelayMsg = "Accepted";
            }
            _pendingEventId = "";
        }
        return;
    }

    if (!json.startsWith("[\"EVENT\"")) return;

    JsonDocument doc;
    if (deserializeJson(doc, json)) return;
    if (!doc[2].is<JsonObject>()) return;

    JsonObject ev = doc[2].as<JsonObject>();
    const char* pubkey  = ev["pubkey"] | "";
    const char* eventId = ev["id"]     | "";
    const char* content = ev["content"] | "";
    int         kind    = ev["kind"]    | 1;
    uint32_t    ts      = ev["created_at"] | (uint32_t)0;

    if (strlen(pubkey) < 8) return;

    // kind 0 = metadata — extract name and update cache
    if (kind == 0) {
        JsonDocument meta;
        if (!deserializeJson(meta, content)) {
            const char* n = meta["name"] | meta["display_name"] | (const char*)nullptr;
            if (n && *n) _cacheSetName(pubkey, n);
        }
        return;
    }

    if (strlen(content) == 0) return;

    // Ensure this pubkey is in the name cache so seenPubkeys() can return it.
    // If we already have a name, use it; otherwise seed with pubkey prefix.
    const char* cachedName = _cacheLookupName(pubkey);
    if (!cachedName) {
        // Seed with pubkey prefix — will be overwritten when kind 0 arrives
        char prefix[9]; strncpy(prefix, pubkey, 8); prefix[8] = '\0';
        _cacheSetName(pubkey, prefix);
        cachedName = nullptr;  // not a real name yet
    }
    _pushMessage(pubkey, eventId, ts, content, cachedName);
}

// ---- Public API -------------------------------------------------------------

const char* NostrClient::stateLabel() const {
    switch (_state) {
        case NostrState::DISCONNECTED: return "DISCONNECTED";
        case NostrState::CONNECTING:   return "CONNECTING";
        case NostrState::HANDSHAKING:  return "HANDSHAKING";
        case NostrState::CONNECTED:    return "CONNECTED";
        case NostrState::ERROR:        return "ERROR";
    }
    return "";
}

bool NostrClient::poll() {
    if (_state != NostrState::CONNECTED) return false;
    String frame;
    bool got = false;
    // Drain all available frames
    while (_wsPollFrame(frame)) {
        _processEvent(frame);
        got = true;
    }
    return got;
}

bool NostrClient::subscribe(const String& channelId) {
    if (_state != NostrState::CONNECTED) return false;

    // Close previous subscription cleanly before opening a new one
    if (!_subId.isEmpty()) {
        String closeMsg = "[\"CLOSE\",\"" + _subId + "\"]";
        _wsSendText(closeMsg);
    }

    uint32_t r = esp_random();
    char sid[9]; snprintf(sid, sizeof(sid), "%08lx", (unsigned long)r);
    _subId = String(sid);

    String msg = "[\"REQ\",\"";
    msg += _subId;
    msg += "\",{";

    if (channelId.length() == 64) {
        msg += "\"kinds\":[42],\"#e\":[\"" + channelId + "\"]";
    } else if (channelId.startsWith("#") && channelId.length() > 1) {
        String tag = channelId.substring(1);
        msg += "\"kinds\":[1],\"#t\":[\"" + tag + "\"]";
    } else {
        msg += "\"kinds\":[1]";
    }

    msg += ",\"limit\":" + String(NOSTR_MAX_MESSAGES) + "}]";
    return _wsSendText(msg);
}

void NostrClient::removePendingMessage() {
    if (_pendingEventId.isEmpty()) return;
    const char* prefix = _pendingEventId.c_str();
    for (int i = 0; i < _msgCount; i++) {
        if (strncmp(_msgs[i].eventId, prefix, 8) == 0) {
            for (int j = i; j < _msgCount - 1; j++) _msgs[j] = _msgs[j + 1];
            _msgCount--;
            break;
        }
    }
}

// ---- Name cache -------------------------------------------------------------

void NostrClient::_cacheSetName(const char* pubkey64, const char* name) {
    if (!pubkey64 || strlen(pubkey64) < 8 || !name || !*name) return;
    // Update existing entry
    for (int i = 0; i < _nameCacheCount; i++) {
        if (strncmp(_nameCache[i].pubkey64, pubkey64, 8) == 0) {
            // Store full key if we now have it
            if (strlen(pubkey64) == 64)
                strncpy(_nameCache[i].pubkey64, pubkey64, 64);
            _nameCache[i].pubkey64[64] = '\0';
            strncpy(_nameCache[i].name, name, 16);
            _nameCache[i].name[16] = '\0';
            for (int j = 0; j < _msgCount; j++) {
                if (strncmp(_msgs[j].pubkey, pubkey64, 8) == 0) {
                    strncpy(_msgs[j].name, name, 16);
                    _msgs[j].name[16] = '\0';
                }
            }
            return;
        }
    }
    // Insert new entry (evict oldest if full)
    int slot = _nameCacheCount < NAME_CACHE_SIZE
               ? _nameCacheCount++ : NAME_CACHE_SIZE - 1;
    if (_nameCacheCount == NAME_CACHE_SIZE) {
        for (int i = 0; i < NAME_CACHE_SIZE - 1; i++) _nameCache[i] = _nameCache[i + 1];
    }
    strncpy(_nameCache[slot].pubkey64, pubkey64, 64);
    _nameCache[slot].pubkey64[64] = '\0';
    strncpy(_nameCache[slot].name, name, 16);
    _nameCache[slot].name[16] = '\0';
    // Back-fill existing messages
    for (int j = 0; j < _msgCount; j++) {
        if (strncmp(_msgs[j].pubkey, pubkey64, 8) == 0 &&
            strncmp(_msgs[j].name, pubkey64, 8) == 0) {
            strncpy(_msgs[j].name, name, 16);
            _msgs[j].name[16] = '\0';
        }
    }
}

const char* NostrClient::_cacheLookupName(const char* pubkey8) const {
    for (int i = 0; i < _nameCacheCount; i++) {
        if (strncmp(_nameCache[i].pubkey64, pubkey8, 8) == 0)
            return _nameCache[i].name;
    }
    return nullptr;
}

String NostrClient::seenPubkeys() const {
    // Collect unique full pubkeys seen in the feed via the name cache
    String result;
    for (int i = 0; i < _nameCacheCount; i++) {
        if (strlen(_nameCache[i].pubkey64) == 64) {
            if (!result.isEmpty()) result += ',';
            result += _nameCache[i].pubkey64;
        }
    }
    return result;
}

bool NostrClient::publishMetadata(const String& privkeyHex, const String& name) {
    if (_state != NostrState::CONNECTED) return false;
    if (privkeyHex.length() != 64 || name.isEmpty()) return false;

    // kind 0 content: {"name":"...","display_name":"..."}
    String content = "{\"name\":\"" + name + "\",\"display_name\":\"" + name + "\"}";

    uint8_t priv[32], pub[32];
    if (!_fromHex(privkeyHex.c_str(), priv, 32)) return false;

    mbedtls_ecp_group grp; mbedtls_mpi d; mbedtls_ecp_point P;
    mbedtls_ecp_group_init(&grp); mbedtls_mpi_init(&d); mbedtls_ecp_point_init(&P);
    bool ok = false;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) == 0 &&
        mbedtls_mpi_read_binary(&d, priv, 32) == 0 &&
        mbedtls_ecp_mul(&grp, &P, &d, &grp.G, _rng, nullptr) == 0) {
        feedWatchdog();
        ok = (mbedtls_mpi_write_binary(&P.X, pub, 32) == 0);
    }
    mbedtls_ecp_group_free(&grp); mbedtls_mpi_free(&d); mbedtls_ecp_point_free(&P);
    if (!ok) return false;

    String ev = _buildEventJson(priv, pub, 0, content, "");
    if (ev.isEmpty()) return false;

    // Cache locally immediately
    char pubHex[65]; _toHex(pub, 32, pubHex);
    _cacheSetName(pubHex, name.c_str());

    return _wsSendText("[\"EVENT\"," + ev + "]");
}

bool NostrClient::subscribeMetadata(const String& pubkeys) {
    if (_state != NostrState::CONNECTED || pubkeys.isEmpty()) return false;

    // Close previous metadata subscription
    if (!_metaSubId.isEmpty()) {
        _wsSendText("[\"CLOSE\",\"" + _metaSubId + "\"]");
    }

    uint32_t r = esp_random();
    char sid[9]; snprintf(sid, sizeof(sid), "%08lxm", (unsigned long)r);
    _metaSubId = String(sid).substring(0, 8) + "m";

    // pubkeys is a comma-separated list of full 64-char hex pubkeys
    String authorsArr = "[";
    int start = 0;
    while (start < (int)pubkeys.length()) {
        int comma = pubkeys.indexOf(',', start);
        String pk = (comma < 0) ? pubkeys.substring(start) : pubkeys.substring(start, comma);
        pk.trim();
        if (pk.length() == 64) { authorsArr += "\"" + pk + "\","; }
        if (comma < 0) break;
        start = comma + 1;
    }
    if (authorsArr.endsWith(",")) authorsArr.remove(authorsArr.length() - 1);
    authorsArr += "]";

    String msg = "[\"REQ\",\"" + _metaSubId + "\",{\"kinds\":[0],\"authors\":" + authorsArr + ",\"limit\":20}]";
    return _wsSendText(msg);
}

bool NostrClient::publish(const String& privkeyHex, const String& content,
                           const String& channelId) {
    if (_state != NostrState::CONNECTED) return false;
    if (privkeyHex.length() != 64) return false;

    uint8_t priv[32], pub[32];
    if (!_fromHex(privkeyHex.c_str(), priv, 32)) return false;

    // Derive public key
    mbedtls_ecp_group grp; mbedtls_mpi d; mbedtls_ecp_point P;
    mbedtls_ecp_group_init(&grp); mbedtls_mpi_init(&d); mbedtls_ecp_point_init(&P);
    bool ok = false;
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) == 0 &&
        mbedtls_mpi_read_binary(&d, priv, 32) == 0 &&
        mbedtls_ecp_mul(&grp, &P, &d, &grp.G, _rng, nullptr) == 0) {
        feedWatchdog();
        ok = (mbedtls_mpi_write_binary(&P.X, pub, 32) == 0);
    }
    mbedtls_ecp_group_free(&grp); mbedtls_mpi_free(&d); mbedtls_ecp_point_free(&P);
    if (!ok) return false;

    int kind = channelId.length() == 64 ? 42 : 1;
    String ev = _buildEventJson(priv, pub, kind, content, channelId);
    if (ev.isEmpty()) return false;

    // Extract event id and pubkey from the JSON blob to insert locally immediately.
    // This ensures the message appears in the feed regardless of relay echo.
    {
        char pubHex[65]; _toHex(pub, 32, pubHex);
        int idStart = ev.indexOf("\"id\":\"");
        if (idStart >= 0) {
            idStart += 6;
            String idHex = ev.substring(idStart, idStart + 64);
            _pendingEventId = idHex;
            uint32_t ts = (uint32_t)time(nullptr);
            _pushMessage(pubHex, idHex.c_str(), ts, content.c_str(), _localName.c_str());
        }
    }
    _lastPublishOk = true;

    String frame = "[\"EVENT\"," + ev + "]";
    return _wsSendText(frame);
}
