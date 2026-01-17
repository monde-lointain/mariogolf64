#!/usr/bin/env bash
# Run permuter on a function with (nproc - 1) threads, best-only
# Usage: ./run-permuter.sh <func_name> [permuter.py args...]
# Example: ./run-permuter.sh ___udivmoddi4
#          ./run-permuter.sh ___udivmoddi4 --stop-on-zero

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

THREADS=$(($(nproc) - 1))
if [ "$THREADS" -lt 1 ]; then
    THREADS=1
fi

echo "Running permuter on $PERMUTER_DIR with $THREADS threads (best-only mode)"

./tools/decomp-permuter/permuter.py "$PERMUTER_DIR" -j "$THREADS" --best-only "$@"
