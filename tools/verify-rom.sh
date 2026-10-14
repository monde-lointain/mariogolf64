#!/usr/bin/env bash
# Stable entry point for the successful-build + authoritative SHA-1 ROM gate.
set -euo pipefail
exec python3 "$(dirname "$0")/verify_rom.py" "$@"
