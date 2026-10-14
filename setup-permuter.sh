#!/usr/bin/env bash
# Import a function with its parent TU's exact ROM compiler profile.
# Usage: ./setup-permuter.sh <func_name> [import.py args...]
set -euo pipefail
source tools/lib.sh
mg_activate_venv
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <func_name> [import.py args...]" >&2
    exit 2
fi
FUNC_NAME="$1"
shift
mg_resolve_c_asm "$FUNC_NAME"
INPUT_ARGS=()
if [[ -f "nonmatchings/$FUNC_NAME/base.c" ]]; then
    INPUT_ARGS=(--input "nonmatchings/$FUNC_NAME/base.c")
fi
echo "C file: $C_FILE"
echo "ASM file: $ASM_FILE"
exec python3 tools/rom_build.py import-permuter \
    --source "$C_FILE" --asm "$ASM_FILE" "${INPUT_ARGS[@]}" -- "$@"
