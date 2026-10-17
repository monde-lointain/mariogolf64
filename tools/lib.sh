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

# Resolve the configured binary tree once for all wrappers.
mg_build_dir() {
    MG_BUILD_DIR="${MG64_BUILD_DIR:-build/us}"
    if [[ "$MG_BUILD_DIR" != /* ]]; then
        MG_BUILD_DIR="$PWD/$MG_BUILD_DIR"
    fi
}

# Synchronize and activate the uv-managed project environment.
mg_activate_venv() {
    mg_build_dir
    local root
    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    UV_PROJECT_ENVIRONMENT="$root/.venv" uv sync --quiet --locked --project "$root" || exit $?
    # shellcheck disable=SC1091
    source "$root/.venv/bin/activate"
}

mg_tool_path() {
    local name="$1" root
    root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    "$root/.venv/bin/python3" - "$name" "$root/tools" <<'PY'
import sys
sys.path.insert(0, sys.argv[2])
from rom_build import tool_path
print(tool_path(sys.argv[1]))
PY
}

# Resolve the newest nonmatchings/<func>[-N] permuter workspace into PERMUTER_DIR; abort if absent.
mg_find_permuter_dir() {
    local func="$1" d newest=0 mtime
    # Only a dir with compile.sh is a permuter workspace: import.py suffixes `-N` when
    # `nonmatchings/<func>/` is already a decomp_loop seed dir (base.c + current.o). Every
    # re-import adds another `-N`, so pick the most recently imported one; the old alphabetical
    # first-hit silently ran a stale case. Match `<func>` and `<func>-<digits>` exactly so a
    # shorter name (lerp_int) never claims a longer one's workspace (lerp_int_v2).
    PERMUTER_DIR=""
    for d in "nonmatchings/${func}" nonmatchings/"${func}"-[0-9]*; do
        [ -f "$d/compile.sh" ] || continue
        mtime=$(stat -c %Y "$d/compile.sh")
        if [ "$mtime" -ge "$newest" ]; then
            newest=$mtime
            PERMUTER_DIR="$d"
        fi
    done
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
    # `set -o pipefail` + a no-match grep makes the whole pipeline exit 1, and under `set -e` that
    # aborts the caller BEFORE the emptiness check below, so the wrapper died with no message when
    # the target was already inlined into its host (S308). Keep the `|| true` on every probe.
    C_FILE=$(grep -rl "INCLUDE_ASM(.*, ${func});" src/ 2>/dev/null | head -1 || true)
    if [ -n "$C_FILE" ]; then
        # INCLUDE_ASM line format: INCLUDE_ASM("path", func_name);
        asm_line=$(grep "INCLUDE_ASM(.*, ${func});" "$C_FILE" | head -1)
        asm_dir=$(echo "$asm_line" | sed 's/.*INCLUDE_ASM("\([^"]*\)".*/\1/')
        ASM_FILE="$MG_BUILD_DIR/work/${asm_dir}/${func}.s"
    else
        # A carried partial one-TU may already contain the target's C body. Require a definition:
        # an `extern f(...);` declaration elsewhere (nuboot.c declares mainproc) selected the
        # wrong translation unit and therefore the wrong compiler profile.
        C_FILE=$(grep -rnE "^[A-Za-z_][A-Za-z0-9_* ]*[ *]${func}\([^;]*$" src/ 2>/dev/null \
            | grep -v ':extern ' | head -1 | cut -d: -f1 || true)
        if [ -z "$C_FILE" ]; then
            echo "Error: no INCLUDE_ASM stub AND no C definition found for ${func} in src/" >&2
            exit 1
        fi
        ASM_FILE=$(find "$MG_BUILD_DIR/work/asm/nonmatchings" -name "${func}.s" 2>/dev/null | head -1 || true)
        # Extraction stops emitting a stub's .s once its body is C; seed_c.py's snapshot remains.
        if [ -z "$ASM_FILE" ] && [ -f "nonmatchings/${func}/target.s" ]; then
            ASM_FILE="nonmatchings/${func}/target.s"
        fi
    fi
    if [ -z "$ASM_FILE" ] || [ ! -f "$ASM_FILE" ]; then
        echo "Error: Assembly file not found for ${func}: ${ASM_FILE:-<none>}" >&2
        exit 1
    fi
}
