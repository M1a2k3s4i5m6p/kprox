#!/usr/bin/env bash
set -euo pipefail
NAK="$HOME/.local/bin/nak"
RELAY="wss://relay.damus.io"
LIMIT=500
TAG_WORD="#kprox_combat"
TOP_N=20

# Download nak only if not already present
if [[ ! -x "$NAK" ]]; then
    echo "Installing nak..." >&2
    mkdir -p "$(dirname "$NAK")"
    curl -sL https://github.com/fiatjaf/nak/releases/download/v0.19.3/nak-v0.19.3-linux-amd64 \
        -o "$NAK" && chmod +x "$NAK"
fi
export PATH="$HOME/.local/bin:$PATH"

# Fetch kind-1 notes and grep for the content tag.
# The combat app puts the tag in content only (no nostr tags[] entry),
# so we can't use -t; instead fetch a broad window and grep.
echo "Fetching notes from relay..." >&2
MESSAGES=$(
    nak req -k 1 --limit "$LIMIT" "$RELAY" 2>/dev/null \
    | grep -F "$TAG_WORD" \
    || true
)

if [[ -z "$MESSAGES" ]]; then
    echo "No high scores found." >&2
    exit 0
fi

# Collect unique full pubkeys
mapfile -t PUBKEY_LIST < <(echo "$MESSAGES" | jq -r '.pubkey' | sort -u)

# Build -a flags for nak metadata fetch
A_FLAGS=()
for pk in "${PUBKEY_LIST[@]}"; do
    A_FLAGS+=(-a "$pk")
done

# Fetch kind 0 metadata → pubkey->name map
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

# Parse scores. Content format: "#kprox_combat INI score round"
declare -A BEST_SCORE BEST_INITIALS BEST_ROUND BEST_TS

while IFS=$'\t' read -r ts pubkey content; do
    # Strip the tag word prefix
    content="${content#"$TAG_WORD"}"
    content="${content#"${content%%[![:space:]]*}"}"   # ltrim

    read -r ini sc rnd _ <<< "$content" 2>/dev/null || continue
    [[ -z "$ini" || -z "$sc" ]] && continue
    [[ "$sc" =~ ^[0-9]+$ ]]    || continue
    [[ "$rnd" =~ ^[0-9]+$ ]]   || rnd=0

    prev="${BEST_SCORE[$pubkey]:-0}"
    if (( sc > prev )); then
        BEST_SCORE["$pubkey"]="$sc"
        BEST_INITIALS["$pubkey"]="$ini"
        BEST_ROUND["$pubkey"]="$rnd"
        BEST_TS["$pubkey"]="$ts"
    fi
done < <(echo "$MESSAGES" | jq -r '[.created_at, .pubkey, .content] | @tsv')

if [[ ${#BEST_SCORE[@]} -eq 0 ]]; then
    echo "No valid score entries found." >&2
    exit 0
fi

# Sort by score descending, take top N
mapfile -t SORTED < <(
    for pk in "${!BEST_SCORE[@]}"; do
        printf '%010d\t%s\n' "${BEST_SCORE[$pk]}" "$pk"
    done | sort -rn | head -n "$TOP_N"
)

# Print table
printf '\n  psCombatProx — Global High Scores\n\n'
printf '  %-5s  %-6s  %-6s  %-5s  %-16s  %s\n' "Rank" "Inits" "Score" "Round" "Player" "Date"
printf '  %s\n' "──────────────────────────────────────────────────────────────"

rank=1
for entry in "${SORTED[@]}"; do
    pk="${entry##*$'\t'}"
    sc="${BEST_SCORE[$pk]}"
    ini="${BEST_INITIALS[$pk]}"
    rnd="${BEST_ROUND[$pk]}"
    ts="${BEST_TS[$pk]}"

    name="${NAMES[$pk]:-${pk:0:8}}"
    name="${name#"${name%%[![:space:]]*}"}"
    name="${name%"${name##*[![:space:]]}"}"
    name="${name:0:16}"

    date_str=$(date -d "@$ts" '+%Y-%m-%d %H:%M' 2>/dev/null \
               || date -r "$ts"  '+%Y-%m-%d %H:%M' 2>/dev/null \
               || echo "?")

    medal=""
    case $rank in 1) medal="🥇 ";; 2) medal="🥈 ";; 3) medal="🥉 ";; *) medal="   ";; esac

    printf '  %s%-3s  %-6s  %-6s  %-5s  %-16s  %s\n' \
        "$medal" "$rank" "$ini" "$sc" "R${rnd}" "$name" "$date_str"
    (( rank++ ))
done

printf '\n  Players: %d  |  Showing top %d\n\n' "${#BEST_SCORE[@]}" "$(( rank - 1 ))"
