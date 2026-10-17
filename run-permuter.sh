#!/usr/bin/env bash
# Run a bounded permuter experiment; preserve output when its budget expires.
# Usage: MG64_PERMUTER_SECONDS=600 ./run-permuter.sh <func_name> [permuter.py args...]
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

# Default to all-but-one core unless the caller chose `-j`; passing both made the permuter's
# argparse silently keep the last one.
JOBS=()
if [[ " $* " != *" -j "* && " $* " != *" -j"[0-9]* ]]; then
    THREADS=$(($(nproc) - 1))
    [ "$THREADS" -ge 1 ] || THREADS=1
    JOBS=(-j "$THREADS")
fi
BUDGET="${MG64_PERMUTER_SECONDS:-600}"

echo "Running bounded permuter on $PERMUTER_DIR (budget ${BUDGET}s; set MG64_PERMUTER_SECONDS to change)"

python tools/decomp_budget.py --seconds "$BUDGET" --reserve 120 -- \
    ./tools/decomp-permuter/permuter.py "$PERMUTER_DIR" "${JOBS[@]}" "$@"
