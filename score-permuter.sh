#!/usr/bin/env bash
# Score base.c without iterating (debug mode)
# Usage: ./score-permuter.sh <func_name> [permuter.py args...]
# Example: ./score-permuter.sh ___udivmoddi4

set -euo pipefail
source tools/lib.sh

mg_activate_venv

if [ $# -lt 1 ]; then
    echo "Usage: $0 <func_name> [permuter.py args...]"
    echo "Example: $0 ___udivmoddi4"
    exit 1
fi

FUNC_NAME="$1"
shift

mg_find_permuter_dir "$FUNC_NAME"

echo "Scoring $PERMUTER_DIR..."

./tools/decomp-permuter/permuter.py --debug "$PERMUTER_DIR" "$@"
