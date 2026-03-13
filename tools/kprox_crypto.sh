#!/usr/bin/env bash
# kprox_crypto.sh — sourced by kprox helper scripts, never executed directly.
# Requires: python3, openssl, curl

KPROX_API_KEY="${KPROX_API_KEY:-kprox1337}"
KPROX_API_ENDPOINT="${KPROX_API_ENDPOINT:-kprox.local}"

_kprox_check_deps() {
    local missing=()
    for cmd in curl python3 openssl; do
        command -v "$cmd" &>/dev/null || missing+=("$cmd")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "Error: missing required dependencies: ${missing[*]}" >&2
        exit 1
    fi
}

# Fetch a fresh nonce from the device. Echoes the nonce string.
_kprox_get_nonce() {
    local nonce response
    # Capture output and check for curl exit status (e.g., host not found)
    response=$(curl -sf "http://${KPROX_API_ENDPOINT}/api/nonce" 2>&1)
    if [[ $? -ne 0 ]]; then
        echo "Error: Could not reach host '${KPROX_API_ENDPOINT}'. Check your network or KPROX_API_ENDPOINT." >&2
        exit 1
    fi

    nonce=$(echo "$response" | python3 -c "import sys,json; print(json.load(sys.stdin)['nonce'])" 2>/dev/null)
    
    if [[ -z "$nonce" ]]; then
        echo "Error: failed to parse nonce from device response" >&2
        exit 1
    fi
    echo "$nonce"
}

# Compute HMAC-SHA256(apiKey, nonce) and echo the hex string.
_kprox_hmac() {
    local nonce="$1"
    echo -n "$nonce" | openssl dgst -sha256 -hmac "$KPROX_API_KEY" | awk '{print $2}'
}

# Decrypt an encrypted API response (base64 payload from X-Encrypted:1 responses).
_kprox_decrypt() {
    local b64="$1"
    python3 - "$b64" "$KPROX_API_KEY" << 'PYEOF'
import sys, hmac, hashlib, base64, subprocess

b64_payload, api_key = sys.argv[1], sys.argv[2]
raw = base64.b64decode(b64_payload)
if len(raw) < 48:
    print("Error: payload too short", file=sys.stderr); sys.exit(1)

iv_raw    = raw[:16]
ciphertext = raw[16:-32]
tag        = raw[-32:]

key = hashlib.sha256(api_key.encode()).digest()

expected_mac = hmac.new(key, iv_raw + ciphertext, hashlib.sha256).digest()
if not hmac.compare_digest(expected_mac, tag):
    print("Error: HMAC verification failed — wrong API key", file=sys.stderr)
    sys.exit(1)

ctr_iv = bytearray(iv_raw)
ctr_iv[15] = (ctr_iv[15] & 0xfe) | 0x01

proc = subprocess.run(
    ['openssl', 'enc', '-d', '-aes-256-ctr',
     '-K', key.hex(), '-iv', bytes(ctr_iv).hex(), '-nosalt'],
    input=ciphertext, capture_output=True
)
if proc.returncode != 0:
    print(f"Error: AES decrypt failed: {proc.stderr.decode()}", file=sys.stderr)
    sys.exit(1)

sys.stdout.buffer.write(proc.stdout)
PYEOF
}

# Encrypt a JSON string for a request body using AES-256-CTR + HMAC-SHA256.
_kprox_encrypt_body() {
    local json="$1"
    python3 - "$json" "$KPROX_API_KEY" << 'PYEOF'
import sys, hmac, hashlib, base64, subprocess, os

json_body, api_key = sys.argv[1], sys.argv[2]
key = hashlib.sha256(api_key.encode()).digest()

iv = os.urandom(16)
ctr_iv = bytearray(iv)
ctr_iv[15] = (ctr_iv[15] & 0xfe) | 0x01

proc = subprocess.run(
    ['openssl', 'enc', '-aes-256-ctr',
     '-K', key.hex(), '-iv', bytes(ctr_iv).hex(), '-nosalt'],
    input=json_body.encode(), capture_output=True
)
if proc.returncode != 0:
    print(f"Error: AES encrypt failed: {proc.stderr.decode()}", file=sys.stderr)
    sys.exit(1)

ciphertext = proc.stdout
mac = hmac.new(key, bytes(iv) + ciphertext, hashlib.sha256).digest()
blob = bytes(iv) + ciphertext + mac
print(base64.b64encode(blob).decode(), end='')
PYEOF
}

# Perform an authenticated GET request.
_kprox_get() {
    local path="$1"
    local nonce hmac tmpheaders tmpbody http_code

    nonce=$(_kprox_get_nonce)
    hmac=$(_kprox_hmac "$nonce")

    tmpheaders=$(mktemp)
    tmpbody=$(mktemp)

    http_code=$(curl -s -o "$tmpbody" -w "%{http_code}" \
        -D "$tmpheaders" \
        -H "X-Auth: $hmac" \
        "http://${KPROX_API_ENDPOINT}${path}")

    if [[ "$http_code" == "000" ]]; then
        echo "Error: Could not connect to host ${KPROX_API_ENDPOINT}" >&2
        rm -f "$tmpheaders" "$tmpbody"
        exit 1
    elif [[ "$http_code" != "200" ]]; then
        echo "Error: HTTP $http_code from ${path}" >&2
        cat "$tmpbody" >&2
        rm -f "$tmpheaders" "$tmpbody"
        exit 1
    fi

    if grep -qi "x-encrypted: 1" "$tmpheaders"; then
        _kprox_decrypt "$(cat "$tmpbody")"
    else
        cat "$tmpbody"
    fi
    rm -f "$tmpheaders" "$tmpbody"
}

# Perform an authenticated POST request.
_kprox_post() {
    local path="$1"
    local body="$2"
    local nonce hmac tmpheaders tmpbody http_code

    nonce=$(_kprox_get_nonce)
    hmac=$(_kprox_hmac "$nonce")

    tmpheaders=$(mktemp)
    tmpbody=$(mktemp)

    local encrypted_body
    encrypted_body=$(_kprox_encrypt_body "$body")

    http_code=$(curl -s -o "$tmpbody" -w "%{http_code}" \
        -D "$tmpheaders" \
        -X POST \
        -H "Content-Type: text/plain" \
        -H "X-Auth: $hmac" \
        -H "X-Encrypted: 1" \
        -d "$encrypted_body" \
        "http://${KPROX_API_ENDPOINT}${path}")

    if [[ "$http_code" == "000" ]]; then
         echo "Error: Could not connect to host ${KPROX_API_ENDPOINT}" >&2
         rm -f "$tmpheaders" "$tmpbody"
         exit 1
    elif [[ "$http_code" != "200" ]]; then
        echo "Error: HTTP $http_code from ${path}" >&2
        cat "$tmpbody" >&2
        rm -f "$tmpheaders" "$tmpbody"
        exit 1
    fi

    if grep -qi "x-encrypted: 1" "$tmpheaders"; then
        _kprox_decrypt "$(cat "$tmpbody")"
    else
        cat "$tmpbody"
    fi
    rm -f "$tmpheaders" "$tmpbody"
}

# Perform an authenticated DELETE request.
_kprox_delete() {
    local path="$1"
    local nonce hmac tmpheaders tmpbody http_code

    nonce=$(_kprox_get_nonce)
    hmac=$(_kprox_hmac "$nonce")

    tmpheaders=$(mktemp)
    tmpbody=$(mktemp)

    http_code=$(curl -s -o "$tmpbody" -w "%{http_code}" \
        -D "$tmpheaders" \
        -X DELETE \
        -H "X-Auth: $hmac" \
        "http://${KPROX_API_ENDPOINT}${path}")

    if [[ "$http_code" == "000" ]]; then
         echo "Error: Could not connect to host ${KPROX_API_ENDPOINT}" >&2
         rm -f "$tmpheaders" "$tmpbody"
         exit 1
    elif [[ "$http_code" != "200" ]]; then
        echo "Error: HTTP $http_code" >&2
        rm -f "$tmpheaders" "$tmpbody"
        exit 1
    fi

    if grep -qi "x-encrypted: 1" "$tmpheaders"; then
        _kprox_decrypt "$(cat "$tmpbody")"
    else
        cat "$tmpbody"
    fi
    rm -f "$tmpheaders" "$tmpbody"
}

_kprox_check_deps
