#!/usr/bin/env bash
# Setup permuter for a function
# Usage: ./setup-permuter.sh <func_name> [import.py args...]
# Example: ./setup-permuter.sh ___udivmoddi4
#          ./setup-permuter.sh ___udivmoddi4 --no-prune

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
echo "Running import.py..."

./tools/decomp-permuter/import.py "$C_FILE" "$ASM_FILE" "$@"
