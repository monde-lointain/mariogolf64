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

# Guard the array expansion: under `set -u`, "${SETTINGS_ARGS[@]}" errors on an EMPTY array (bash
# < 4.4), which aborted the wrapper silently while a direct import.py call worked (S189).
./tools/decomp-permuter/import.py ${SETTINGS_ARGS[@]+"${SETTINGS_ARGS[@]}"} "$C_FILE" "$ASM_FILE" "$@"

# Per-file -ffast-math: six src/main TUs carry a `C_PROFILE_CFLAGS := $(MAIN_CFLAGS) -ffast-math`
# override in mk/main.mk, and neither permuter settings file's compiler_command has the flag. Left
# alone, the permuter scores a compile the build never performs -- on an FP-heavy TU that changes
# constant folding (`x * C1 / C2` collapses to one `mul.s`), so every candidate is measured against
# the wrong codegen (S306). Patch the generated compile.sh to match the real per-file profile.
OBJ_REL="${C_FILE#src/}"
OBJ_REL="${OBJ_REL%.c}.o"
COMPILE_SH="nonmatchings/$FUNC_NAME/compile.sh"
if [ -f "$COMPILE_SH" ] && grep -qE "/${OBJ_REL}:.*-ffast-math" mk/*.mk 2>/dev/null; then
    if grep -q -- '-ffast-math' "$COMPILE_SH"; then
        echo "compile.sh already carries -ffast-math"
    else
        sed -i 's/ -O2 / -O2 -ffast-math /' "$COMPILE_SH"
        echo "Patched $COMPILE_SH with -ffast-math (per-file override in mk/main.mk)"
    fi
fi
