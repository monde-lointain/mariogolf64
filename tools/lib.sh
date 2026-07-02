#!/usr/bin/env bash
# Shared helpers for the permuter / decomp.me wrapper scripts
# (run-permuter.sh, setup-permuter.sh, decompme.sh, score-permuter.sh).
#
# Source from the project root:  source tools/lib.sh
#
# The lookup helpers set variables in the CALLER's scope (PERMUTER_DIR, C_FILE,
# ASM_FILE) rather than echoing a result, so their `exit 1` on error aborts the
# script itself — a `VAR=$(helper)` command-substitution form would run the
# helper (and its exit) in a subshell and silently continue with an empty VAR.

# Activate ./venv if present (carries the permuter/asm-differ deps).
mg_activate_venv() {
    if [ -f "venv/bin/activate" ]; then
        # shellcheck disable=SC1091
        source venv/bin/activate
    fi
}

# Resolve the nonmatchings/<func>* scratch dir into PERMUTER_DIR; abort if absent.
mg_find_permuter_dir() {
    local func="$1"
    PERMUTER_DIR=$(find nonmatchings -maxdepth 1 -type d -name "${func}*" | head -1)
    if [ -z "$PERMUTER_DIR" ]; then
        echo "Error: Permuter directory not found for ${func}" >&2
        echo "Run ./setup-permuter.sh ${func} first" >&2
        exit 1
    fi
}

# Locate the src/ .c file holding INCLUDE_ASM for <func> and derive its asm path.
# Sets C_FILE and ASM_FILE; aborts if either cannot be found.
mg_resolve_c_asm() {
    local func="$1" asm_line asm_dir
    # Anchor on the exact `, <func>);` argument position, NOT a bare `.*${func}`:
    # in a multi-fn subseg the target's stem also appears inside a SIBLING stub's
    # asm PATH string (INCLUDE_ASM("asm/nonmatchings/main/func_8006EA90", func_8006ED34)),
    # so a loose grep matches both lines and garbles ASM_FILE (S160).
    C_FILE=$(grep -rl "INCLUDE_ASM(.*, ${func});" src/ 2>/dev/null | head -1)
    if [ -z "$C_FILE" ]; then
        echo "Error: Could not find C file containing INCLUDE_ASM for ${func}" >&2
        exit 1
    fi
    # INCLUDE_ASM line format: INCLUDE_ASM("path", func_name);
    asm_line=$(grep "INCLUDE_ASM(.*, ${func});" "$C_FILE" | head -1)
    asm_dir=$(echo "$asm_line" | sed 's/.*INCLUDE_ASM("\([^"]*\)".*/\1/')
    ASM_FILE="${asm_dir}/${func}.s"
    if [ ! -f "$ASM_FILE" ]; then
        echo "Error: Assembly file not found: $ASM_FILE" >&2
        exit 1
    fi
}
