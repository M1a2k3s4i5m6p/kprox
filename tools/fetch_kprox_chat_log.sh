#!/usr/bin/env bash
set -euo pipefail

NAK="$HOME/.local/bin/nak"
RELAY="wss://relay.damus.io"
LIMIT=50

# Download nak only if not already present
if [[ ! -x "$NAK" ]]; then
    echo "Installing nak..." >&2
    mkdir -p "$(dirname "$NAK")"
    curl -sL https://github.com/fiatjaf/nak/releases/download/v0.19.3/nak-v0.19.3-linux-amd64 \
        -o "$NAK" && chmod +x "$NAK"
fi

export PATH="$HOME/.local/bin:$PATH"

# Fetch the last $LIMIT messages tagged #kprox
MESSAGES=$(nak req -t t=kprox --limit "$LIMIT" "$RELAY" 2>/dev/null)

if [[ -z "$MESSAGES" ]]; then
    echo "No messages found." >&2
    exit 0
fi

# Collect unique full pubkeys
mapfile -t PUBKEY_LIST < <(echo "$MESSAGES" | jq -r '.pubkey' | sort -u)

# Build -a flags for each pubkey (nak requires one -a per pubkey, not comma-separated)
A_FLAGS=()
for pk in "${PUBKEY_LIST[@]}"; do
    A_FLAGS+=(-a "$pk")
done

# Fetch kind 0 metadata and build pubkey->name map
declare -A NAMES
if [[ ${#A_FLAGS[@]} -gt 0 ]]; then
    while IFS=$'\t' read -r pk name; do
        [[ -n "$pk" && -n "$name" ]] && NAMES["$pk"]="$name"
    done < <(
        nak req -k 0 "${A_FLAGS[@]}" "$RELAY" 2>/dev/null \
        | jq -r 'select(.kind == 0) | [.pubkey, (.content | fromjson | .name // .display_name // "")] | select(.[1] != "") | @tsv' \
        2>/dev/null || true
    )
fi

# Print messages sorted by timestamp
echo "$MESSAGES" | jq -r '[.created_at, .pubkey, .content] | @tsv' | sort | \
while IFS=$'\t' read -r ts pubkey content; do
    name="${NAMES[$pubkey]:-${pubkey:0:8}}"
    name="${name#"${name%%[![:space:]]*}"}"  # ltrim
    name="${name%"${name##*[![:space:]]}"}"  # rtrim
    content="${content#"${content%%[![:space:]]*}"}"
    content="${content%"${content##*[![:space:]]}"}"
    printf '%s  %s: %s\n' \
        "$(date -d "@$ts" '+%Y-%m-%d %H:%M:%S' 2>/dev/null || date -r "$ts" '+%Y-%m-%d %H:%M:%S')" \
        "$name" \
        "$content"
done

