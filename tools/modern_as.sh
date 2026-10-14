#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/lib.sh
exec "$(mg_tool_path as)" "$@"
