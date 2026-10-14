#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
exec "$(python3 -c 'import sys; sys.path.insert(0, "tools"); from rom_build import tool_path; print(tool_path("as"))')" "$@"
