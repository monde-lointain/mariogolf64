#!/usr/bin/env bash
# Run permuter on a function with (nproc - 1) threads, best-only
# Usage: ./run-permuter.sh <func_name> [permuter.py args...]
# Example: ./run-permuter.sh ___udivmoddi4
#          ./run-permuter.sh ___udivmoddi4 --stop-on-zero

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

THREADS=$(($(nproc) - 1))
if [ "$THREADS" -lt 1 ]; then
    THREADS=1
fi

echo "Running permuter on $PERMUTER_DIR with $THREADS threads (best-only mode)"

./tools/decomp-permuter/permuter.py "$PERMUTER_DIR" -j "$THREADS" --best-only "$@"
