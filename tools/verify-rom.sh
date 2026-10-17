#!/usr/bin/env bash
# Stable entry point for the successful-build + authoritative SHA-1 ROM gate.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export UV_PROJECT_ENVIRONMENT="$ROOT/.venv"
exec uv run --quiet --locked --project "$ROOT" python "$ROOT/tools/verify_rom.py" "$@"
