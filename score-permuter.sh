#!/usr/bin/env bash
# Score base.c without iterating (debug mode)
# Usage: ./score-permuter.sh <func_name> [permuter.py args...]
# Example: ./score-permuter.sh ___udivmoddi4

set -euo pipefail

# Activate venv if exists
if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 <func_name> [permuter.py args...]"
    echo "Example: $0 ___udivmoddi4"
    exit 1
fi

FUNC_NAME="$1"
shift

# Find the permuter directory for this function
PERMUTER_DIR=$(find nonmatchings -maxdepth 1 -type d -name "${FUNC_NAME}*" | head -1)

if [ -z "$PERMUTER_DIR" ]; then
    echo "Error: Permuter directory not found for ${FUNC_NAME}"
    echo "Run ./setup-permuter.sh ${FUNC_NAME} first"
    exit 1
fi

echo "Scoring $PERMUTER_DIR..."

./tools/decomp-permuter/permuter.py --debug "$PERMUTER_DIR" "$@"
