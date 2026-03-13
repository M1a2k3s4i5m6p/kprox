#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <old_key> <new_key>" >&2
    exit 1
fi

OLD_KEY="$1"
NEW_KEY="$2"

if [[ ${#NEW_KEY} -lt 8 ]]; then
    echo "Error: new key must be at least 8 characters." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KPROX_API_KEY="$OLD_KEY"
source "${SCRIPT_DIR}/kprox_crypto.sh"

echo "Updating API key on ${KPROX_API_ENDPOINT}..."

nonce=$(_kprox_get_nonce)
hmac=$(_kprox_hmac "$nonce")

tmpheaders=$(mktemp)
tmpbody=$(mktemp)
trap 'rm -f "$tmpheaders" "$tmpbody"' EXIT

http_code=$(curl -s -o "$tmpbody" -w "%{http_code}" \
    -D "$tmpheaders" \
    -X POST \
    -H "Content-Type: application/json" \
    -H "X-Auth: $hmac" \
    -d "{\"api_key\":\"${NEW_KEY}\"}" \
    "http://${KPROX_API_ENDPOINT}/api/settings")

if [[ "$http_code" != "200" ]]; then
    echo "Error: HTTP $http_code" >&2
    cat "$tmpbody" >&2
    exit 1
fi

# Device applies the new key before encrypting the response, so decrypt with
# the new key rather than the old one.
if grep -qi "x-encrypted: 1" "$tmpheaders"; then
    KPROX_API_KEY="$NEW_KEY" _kprox_decrypt "$(cat "$tmpbody")" > /dev/null
fi

echo "API key updated successfully."
echo "Set KPROX_API_KEY=\"${NEW_KEY}\" for future requests."
