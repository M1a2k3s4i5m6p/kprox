#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

echo "Downloading settings from ${KPROX_API_ENDPOINT}..."

if ! command -v jq &>/dev/null; then
    echo "Error: 'jq' is required to format the output." >&2
    exit 1
fi

_kprox_get /api/settings | jq . > settings.json

echo "Settings saved to settings.json"
