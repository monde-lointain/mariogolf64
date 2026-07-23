#!/usr/bin/env bash
# Per-function instruction-stream comparator: the extracted .s vs a freshly built object.
#
# Usage: tools/cmpfn.sh <func> [<object>]
#   tools/cmpfn.sh func_80088890
#   tools/cmpfn.sh func_80088890 build/src/main/func_80080220.o
#
# Why this exists (S258): `tools/asm-differ/diff.py` reads `build/*.map`, which an incremental
# per-object build does not refresh, so mid-iteration it goes STALE in BOTH directions (see
# `docs/agent-workflow.md` and the memory `subagent-diff-crack-not-a-bank`). This reads the object
# directly, so it never lies about the code you just compiled. It is an ITERATION oracle only —
# every bank still gates on `tools/verify-rom.sh` (full-make ROM SHA-1).
#
# Both sides are normalised so that only real differences show: `$` register prefixes, `%hi/%lo`
# relocations, immediates, `(0xX >> 16)` / `(0xX & 0xFFFF)` splat spellings, `addu rX,rY,zero` vs
# `move`, `addiu rX,zero,N` vs `li`, the SDK FP register aliases (`fv0`/`fs1` vs `f0`/`f22`), and
# branch/jal targets. The first output line is the instruction COUNT of each side, which is the most
# actionable number when a body is structurally right but the wrong length.
#
# The object is auto-resolved from the function's segment when not given. Rebuild it first:
#   find build -name '<obj>.o' -delete && make build/src/<tree>/<obj>.o

set -euo pipefail
source "$(dirname "$0")/lib.sh"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <func> [<object>]" >&2
    exit 1
fi

FUNC="$1"
OBJ="${2:-}"

ASM_FILE=$(find asm/nonmatchings -name "${FUNC}.s" -print -quit)
if [ -z "$ASM_FILE" ]; then
    echo "$0: no asm/nonmatchings/**/${FUNC}.s (is the function already banked?)" >&2
    exit 1
fi

if [ -z "$OBJ" ]; then
    # asm/nonmatchings/<tree>/<stem>/<func>.s  ->  build/src/<tree>/<stem>.o
    STEM=$(dirname "${ASM_FILE#asm/nonmatchings/}")
    OBJ="build/src/${STEM}.o"
fi

if [ ! -f "$OBJ" ]; then
    echo "$0: $OBJ not built" >&2
    exit 1
fi

norm() {
    sed -E '
        s/\$//g
        s/%[a-z]+\([^)]*\)/N/g
        s/<[^>]*>//g
        s/0x[0-9a-fA-F]+/N/g
        s/\b[0-9]+\b/N/g
        s/\(N >> N\)/N/g
        s/\(N & N\)/N/g
        s/[[:space:]]+/ /g
        s/, /,/g
        s/^ //
        s/ $//
        s/\b(addu|or) ([a-z0-9]+),([a-z0-9]+),zero/move \2,\3/
        s/\baddiu ([a-z0-9]+),zero,/li \1,/
        s/\b(jal|j|b[a-z]*) [.A-Za-z_][A-Za-z0-9_.]*$/\1 T/
        s/\b(fv0|fv1|fs0|fs1|fs2)\b/fN/g
        s/\bf(0|2|20|22|24)\b/fN/g
        s/(b[a-z]+ [a-z0-9,]*),[.A-Za-z0-9_]+$/\1,T/
    '
}

ROM_N=$(mktemp)
MINE_N=$(mktemp)
trap 'rm -f "$ROM_N" "$MINE_N"' EXIT

grep -oP '^\s+/\*.*?\*/\s+\K.*' "$ASM_FILE" | norm > "$ROM_N"
mips-linux-gnu-objdump -d "$OBJ" \
    | awk "/<${FUNC}>:/,/^\$/" \
    | grep -E '^\s+[0-9a-f]+:' \
    | sed -E 's/^\s+[0-9a-f]+:\s+[0-9a-f]{8}\s+//' \
    | norm > "$MINE_N"

echo "rom=$(wc -l < "$ROM_N") mine=$(wc -l < "$MINE_N")  ($OBJ)"
diff <(cat -n "$ROM_N") <(cat -n "$MINE_N") || true
