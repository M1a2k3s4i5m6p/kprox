#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

BATCH_SIZE="${KPROX_BATCH_SIZE:-100}"
DELAY_MS="${KPROX_DELAY_MS:-50}"

# Read and sanitise stdin — strip non-ASCII
full_text=""
while IFS= read -r line; do
    full_text+="$line"$'\n'
done
full_text="${full_text%$'\n'}"

if command -v perl &>/dev/null; then
    cleaned=$(printf '%s' "$full_text" | perl -pe 's/[^\x00-\x7F]//g')
else
    cleaned=$(printf '%s' "$full_text" | tr -cd '\000-\177')
    echo "Warning: perl not found, using tr for ASCII stripping." >&2
fi

total=${#cleaned}
offset=0

echo "Sending ${total} characters to ${KPROX_API_ENDPOINT} in batches of ${BATCH_SIZE}..." >&2

while [[ "$offset" -lt "$total" ]]; do
    batch="${cleaned:$offset:$BATCH_SIZE}"

    # Escape the batch for JSON
    json_text=$(printf '%s' "$batch" | python3 -c "
import sys, json
text = sys.stdin.read()
print(json.dumps(text), end='')
")

    echo "Sending batch at offset ${offset} (length: ${#batch})..." >&2
    _kprox_post /send/text "{\"text\":${json_text}}" > /dev/null

    offset=$(( offset + BATCH_SIZE ))

    if [[ "$offset" -lt "$total" ]]; then
        sleep "$(python3 -c "print($DELAY_MS / 1000)")"
    fi
done

echo "Done." >&2
