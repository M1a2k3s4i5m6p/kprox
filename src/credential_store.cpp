#include "credential_store.h"
#include <ArduinoJson.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>

// NVS namespace — separate from "kprox" so a settings wipe doesn't touch credentials
static const char* CS_NS       = "kprox_cs";
static const char* CS_SENTINEL = "KPROX_CS_v1";

String credStoreRuntimeKey = "";
bool   credStoreLocked     = true;

static std::vector<Credential> _creds;
static String                  _keycheck = "";
static bool                    _loaded   = false;

// ---- Low-level AES-256-CTR + HMAC-SHA256 ----
// Wire format: base64( iv[16] || ciphertext[n] || hmac[32] )

static void deriveKey(const String& keyStr, uint8_t out[32]) {
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, (const uint8_t*)keyStr.c_str(), keyStr.length());
    mbedtls_sha256_finish(&sha, out);
    mbedtls_sha256_free(&sha);
}

String credEncrypt(const String& plaintext, const String& key) {
    uint8_t aesKey[32];
    deriveKey(key, aesKey);

    uint8_t iv[16];
    for (int i = 0; i < 16; i++) iv[i] = (uint8_t)esp_random();

    size_t len = plaintext.length();
    uint8_t* ct = (uint8_t*)malloc(len);
    if (!ct) return "";

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, aesKey, 256) != 0) {
        mbedtls_aes_free(&aes); free(ct); return "";
    }
    uint8_t ctr[16];
    memcpy(ctr, iv, 16);
    ctr[15] = (ctr[15] & 0xfe) | 0x01;
    size_t ncOff = 0;
    uint8_t stream[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, len, &ncOff, ctr, stream, (const uint8_t*)plaintext.c_str(), ct);
    mbedtls_aes_free(&aes);

    uint8_t hmac[32];
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&md, aesKey, 32);
    mbedtls_md_hmac_update(&md, iv, 16);
    mbedtls_md_hmac_update(&md, ct, len);
    mbedtls_md_hmac_finish(&md, hmac);
    mbedtls_md_free(&md);

    size_t blobLen = 16 + len + 32;
    uint8_t* blob = (uint8_t*)malloc(blobLen);
    if (!blob) { free(ct); return ""; }
    memcpy(blob,            iv,   16);
    memcpy(blob + 16,       ct,   len);
    memcpy(blob + 16 + len, hmac, 32);
    free(ct);

    size_t b64Len = 0;
    mbedtls_base64_encode(nullptr, 0, &b64Len, blob, blobLen);
    uint8_t* b64 = (uint8_t*)malloc(b64Len + 1);
    if (!b64) { free(blob); return ""; }
    mbedtls_base64_encode(b64, b64Len + 1, &b64Len, blob, blobLen);
    free(blob);
    b64[b64Len] = '\0';
    String result((char*)b64);
    free(b64);
    return result;
}

String credDecrypt(const String& b64, const String& key) {
    size_t outLen = 0;
    mbedtls_base64_decode(nullptr, 0, &outLen, (const uint8_t*)b64.c_str(), b64.length());
    if (outLen < 48) return "";

    uint8_t* raw = (uint8_t*)malloc(outLen);
    if (!raw) return "";
    mbedtls_base64_decode(raw, outLen, &outLen, (const uint8_t*)b64.c_str(), b64.length());

    uint8_t* iv  = raw;
    uint8_t* ct  = raw + 16;
    size_t ctLen = outLen - 48;
    uint8_t* tag = raw + 16 + ctLen;

    uint8_t aesKey[32];
    deriveKey(key, aesKey);

    uint8_t expected[32];
    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&md, aesKey, 32);
    mbedtls_md_hmac_update(&md, iv, 16);
    mbedtls_md_hmac_update(&md, ct, ctLen);
    mbedtls_md_hmac_finish(&md, expected);
    mbedtls_md_free(&md);

    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= expected[i] ^ tag[i];
    if (diff != 0) { free(raw); return ""; }

    uint8_t ctr[16];
    memcpy(ctr, iv, 16);
    ctr[15] = (ctr[15] & 0xfe) | 0x01;

    uint8_t* plain = (uint8_t*)malloc(ctLen + 1);
    if (!plain) { free(raw); return ""; }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aesKey, 256);
    size_t ncOff = 0;
    uint8_t stream[16] = {0};
    mbedtls_aes_crypt_ctr(&aes, ctLen, &ncOff, ctr, stream, ct, plain);
    mbedtls_aes_free(&aes);
    free(raw);

    plain[ctLen] = '\0';
    String result((char*)plain);
    free(plain);
    return result;
}

// ---- NVS persistence ----
// Keys: "cs_kc" = keycheck, "cs_n" = count,
//       "cs_l<i>" = label i, "cs_v<i>" = encrypted value i

static void saveToNVS() {
    preferences.begin(CS_NS, false);
    preferences.putString("cs_kc", _keycheck);
    preferences.putInt("cs_n", (int)_creds.size());
    for (int i = 0; i < (int)_creds.size(); i++) {
        preferences.putString(("cs_l" + String(i)).c_str(), _creds[i].label);
        preferences.putString(("cs_v" + String(i)).c_str(), _creds[i].encValue);
    }
    preferences.end();
}

static void loadFromNVS() {
    _creds.clear();
    _keycheck = "";
    preferences.begin(CS_NS, true);
    _keycheck = preferences.getString("cs_kc", "");
    int n = preferences.getInt("cs_n", 0);
    for (int i = 0; i < n; i++) {
        Credential c;
        c.label    = preferences.getString(("cs_l" + String(i)).c_str(), "");
        c.encValue = preferences.getString(("cs_v" + String(i)).c_str(), "");
        if (!c.label.isEmpty()) _creds.push_back(c);
    }
    preferences.end();
    _loaded = true;
}

static void wipeNVS() {
    preferences.begin(CS_NS, false);
    preferences.clear();
    preferences.end();
    _creds.clear();
    _keycheck = "";
    _loaded   = true;
}

// ---- Public API ----

void credStoreInit() {
    loadFromNVS();
    credStoreLocked     = true;
    credStoreRuntimeKey = "";
}

void credStoreWipe() {
    wipeNVS();
    credStoreLock();
}

bool credStoreUnlock(const String& key) {
    if (!_loaded) loadFromNVS();

    if (_keycheck.isEmpty()) {
        _keycheck = credEncrypt(CS_SENTINEL, key);
        saveToNVS();
        credStoreRuntimeKey = key;
        credStoreLocked     = false;
        return true;
    }

    String sentinel = credDecrypt(_keycheck, key);
    if (sentinel != CS_SENTINEL) return false;

    credStoreRuntimeKey = key;
    credStoreLocked     = false;
    return true;
}

void credStoreLock() {
    credStoreRuntimeKey = "";
    credStoreLocked     = true;
}

bool credStoreRekey(const String& oldKey, const String& newKey) {
    if (!_loaded) loadFromNVS();
    if (_keycheck.isEmpty()) return false;
    if (credDecrypt(_keycheck, oldKey) != CS_SENTINEL) return false;

    for (auto& c : _creds) {
        String plain = credDecrypt(c.encValue, oldKey);
        if (plain.isEmpty() && !c.encValue.isEmpty()) return false;
        c.encValue = credEncrypt(plain, newKey);
    }

    _keycheck = credEncrypt(CS_SENTINEL, newKey);
    credStoreRuntimeKey = newKey;
    credStoreLocked     = false;
    saveToNVS();
    return true;
}

int credStoreCount() {
    if (!_loaded) loadFromNVS();
    return (int)_creds.size();
}

bool credStoreLabelExists(const String& label) {
    if (!_loaded) loadFromNVS();
    for (auto& c : _creds) {
        if (c.label.equalsIgnoreCase(label)) return true;
    }
    return false;
}

std::vector<String> credStoreListLabels() {
    if (!_loaded) loadFromNVS();
    std::vector<String> labels;
    for (auto& c : _creds) labels.push_back(c.label);
    return labels;
}

String credStoreGet(const String& label) {
    if (credStoreLocked) return "";
    if (!_loaded) loadFromNVS();
    for (auto& c : _creds) {
        if (c.label.equalsIgnoreCase(label))
            return credDecrypt(c.encValue, credStoreRuntimeKey);
    }
    return "";
}

bool credStoreSet(const String& label, const String& value) {
    if (credStoreLocked) return false;
    if (!_loaded) loadFromNVS();
    String enc = credEncrypt(value, credStoreRuntimeKey);
    for (auto& c : _creds) {
        if (c.label.equalsIgnoreCase(label)) {
            c.encValue = enc;
            saveToNVS();
            return true;
        }
    }
    Credential c;
    c.label    = label;
    c.encValue = enc;
    _creds.push_back(c);
    saveToNVS();
    return true;
}

bool credStoreDelete(const String& label) {
    if (credStoreLocked) return false;
    if (!_loaded) loadFromNVS();
    for (auto it = _creds.begin(); it != _creds.end(); ++it) {
        if (it->label.equalsIgnoreCase(label)) {
            _creds.erase(it);
            saveToNVS();
            return true;
        }
    }
    return false;
}
