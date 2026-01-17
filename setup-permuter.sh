#!/usr/bin/env bash
# Setup permuter for a function
# Usage: ./setup-permuter.sh <func_name> [import.py args...]
# Example: ./setup-permuter.sh ___udivmoddi4
#          ./setup-permuter.sh ___udivmoddi4 --no-prune

set -euo pipefail

# Activate venv if exists
if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <func_name> [import.py args...]"
    echo "Example: $0 ___udivmoddi4"
    exit 1
fi

FUNC_NAME="$1"
shift

# Find the .c file containing this function
C_FILE=$(grep -rl "INCLUDE_ASM.*${FUNC_NAME}" src/ 2>/dev/null | head -1)

if [ -z "$C_FILE" ]; then
    echo "Error: Could not find C file containing INCLUDE_ASM for ${FUNC_NAME}"
    exit 1
fi

# Derive the asm directory from the INCLUDE_ASM line
# Format: INCLUDE_ASM("path", func_name);
ASM_LINE=$(grep "INCLUDE_ASM.*${FUNC_NAME}" "$C_FILE")
ASM_DIR=$(echo "$ASM_LINE" | sed 's/.*INCLUDE_ASM("\([^"]*\)".*/\1/')
ASM_FILE="${ASM_DIR}/${FUNC_NAME}.s"

if [ ! -f "$ASM_FILE" ]; then
    echo "Error: Assembly file not found: $ASM_FILE"
    exit 1
fi

echo "C file: $C_FILE"
echo "ASM file: $ASM_FILE"
echo "Running import.py..."

./tools/decomp-permuter/import.py "$C_FILE" "$ASM_FILE" "$@"
