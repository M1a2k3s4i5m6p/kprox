#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

if [[ ! -f "settings.json" ]]; then
    echo "Error: settings.json not found in the current directory." >&2
    exit 1
fi

echo "Uploading settings to ${KPROX_API_ENDPOINT}..."

_kprox_post /api/settings "$(cat settings.json)"

echo "Settings loaded successfully."
