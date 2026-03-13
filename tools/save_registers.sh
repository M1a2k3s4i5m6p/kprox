#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

if ! command -v jq &>/dev/null; then
    echo "Error: 'jq' is required to format the output." >&2
    exit 1
fi

echo "Downloading registers from ${KPROX_API_ENDPOINT}..."

_kprox_get /api/registers | jq . > registers.json

echo "Registers saved to registers.json"
