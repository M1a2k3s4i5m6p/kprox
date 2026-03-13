#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/kprox_crypto.sh"

if [[ ! -f "registers.json" ]]; then
    echo "Error: registers.json not found in the current directory." >&2
    exit 1
fi

echo "Uploading registers to ${KPROX_API_ENDPOINT}..."

_kprox_post /api/registers/import "$(cat registers.json)"

echo "Registers loaded successfully."
