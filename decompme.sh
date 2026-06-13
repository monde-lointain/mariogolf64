#!/usr/bin/env bash
# Upload function to decomp.me
# Usage: ./decompme.sh <func_name> [import.py args...]
# Example: ./decompme.sh ___udivmoddi4

set -euo pipefail
source tools/lib.sh

mg_activate_venv

if [ $# -lt 1 ]; then
    echo "Usage: $0 <func_name> [import.py args...]"
    echo "Example: $0 ___udivmoddi4"
    exit 1
fi

FUNC_NAME="$1"
shift

mg_resolve_c_asm "$FUNC_NAME"

echo "C file: $C_FILE"
echo "ASM file: $ASM_FILE"
echo "Uploading to decomp.me..."

./tools/decomp-permuter/import.py "$C_FILE" "$ASM_FILE" --decompme "$@"
