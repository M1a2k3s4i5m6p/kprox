#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

if ! command -v jq &>/dev/null; then
    echo "Error: 'jq' is required." >&2
    exit 1
fi

_kprox_get /api/settings | jq .
