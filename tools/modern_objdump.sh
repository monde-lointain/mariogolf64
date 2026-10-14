#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/lib.sh
mg_activate_venv
exec "$(python3 -c 'import sys; sys.path.insert(0, "tools"); from rom_build import tool_path; print(tool_path("objdump"))')" "$@"
