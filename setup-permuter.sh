#!/usr/bin/env bash
# Setup permuter for a function
# Usage: ./setup-permuter.sh [--main] <func_name> [import.py args...]
# Example: ./setup-permuter.sh ___udivmoddi4
#          ./setup-permuter.sh ___udivmoddi4 --no-prune
#          ./setup-permuter.sh --main func_800329F4   # src/main or overlay -O2/F3DEX2 fn
#
# --main selects permuter_settings_main.toml (game -O2 / F3DEX2 profile + modern-GAS target
# assembler for the `.set gp=64` target .s). See
# docs/hazards.md#permuter-setup-for-kmc-toolchain-mirrors.

set -euo pipefail
source tools/lib.sh

mg_activate_venv

SETTINGS_ARGS=()
if [ "${1:-}" = "--main" ]; then
    SETTINGS_ARGS=(--settings permuter_settings_main.toml)
    shift
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 [--main] <func_name> [import.py args...]"
    echo "Example: $0 ___udivmoddi4"
    echo "         $0 --main func_800329F4"
    exit 1
fi

FUNC_NAME="$1"
shift

mg_resolve_c_asm "$FUNC_NAME"

echo "C file: $C_FILE"
echo "ASM file: $ASM_FILE"
echo "Running import.py${SETTINGS_ARGS:+ (${SETTINGS_ARGS[*]})}..."

./tools/decomp-permuter/import.py "${SETTINGS_ARGS[@]}" "$C_FILE" "$ASM_FILE" "$@"
