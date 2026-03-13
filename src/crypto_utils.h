#pragma once

#include "globals.h"
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>

// Encrypts plaintext with AES-256-GCM keyed from the API key.
// Output: base64(iv[12] + ciphertext + tag[16]). Returns "" on failure.
String encryptResponse(const String& plaintext);

// Returns a fresh random hex nonce (32 hex chars = 16 bytes).
String generateNonce();

// Verifies HMAC-SHA256(apiKey, nonce) == hmacHex. Constant-time compare.
bool verifyHMAC(const String& nonce, const String& hmacHex);

// Decrypts an encrypted request body. Returns decrypted JSON or "" on failure.
String decryptRequest(const String& b64Body);
