#!/usr/bin/env bash
# Import a function into decomp-permuter, stripping GCC nested functions first.
#
# Usage: tools/permuter_import.sh <func> [<c-file>]
#   tools/permuter_import.sh func_8008D3F4
#   tools/permuter_import.sh func_8008D3F4 src/main/func_8008D100.c
#
# Why this exists (S301): `import.py` runs pycparser, which cannot parse a GCC nested function
# definition. One nested function anywhere in the TU disables the permuter for EVERY function in
# that file — and `import.py` does not stop, it prints "Syntax error in base.c ... Proceeding
# anyway" and imports a broken parse, so the failure is silent until the run produces nothing.
# `docs/workflow/loop.md` describes the workaround (copy the TU with the nested-function parents
# deleted, keep the copy inside the project root because import.py rejects an outside path); this
# script performs it.
#
# The copy goes to `nonmatchings/permsrc/<stem>.c`, which is gitignored along with the rest of
# `nonmatchings/`. Only the target function's body matters to the permuter, so deleting unrelated
# parents costs nothing.
#
# The permuter is an ITERATION tool, and on a register or scheduling permutation its deliverable is
# a LEVER rather than a zero: read `nonmatchings/<func>/output-<score>-1/source.c` against `base.c`
# after every hand fix, not once. Every bank still gates on `tools/verify-rom.sh`.
set -euo pipefail

cd "$(dirname "$0")/.."

FUNC=${1:-}
if [[ -z $FUNC ]]; then
    echo "usage: tools/permuter_import.sh <func> [<c-file>]" >&2
    exit 1
fi

C_FILE=${2:-}
if [[ -z $C_FILE ]]; then
    C_FILE=$(grep -rl "^[A-Za-z_].*\b${FUNC}\b" src --include='*.c' | head -1 || true)
fi
if [[ -z $C_FILE || ! -f $C_FILE ]]; then
    echo "permuter_import: cannot find the C file defining ${FUNC}; pass it explicitly" >&2
    exit 1
fi

ASM_FILE=$(find asm/nonmatchings -name "${FUNC}.s" | head -1 || true)
if [[ -z $ASM_FILE ]]; then
    echo "permuter_import: no asm/nonmatchings/**/${FUNC}.s" >&2
    exit 1
fi

mkdir -p nonmatchings/permsrc
STRIPPED=nonmatchings/permsrc/$(basename "$C_FILE")

venv/bin/python3 - "$C_FILE" "$STRIPPED" "$FUNC" <<'PY'
import re
import sys

src, dst, func = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(src).read()

# A nested function is a `type name(args) {` at an indentation level inside another body. Find each
# top-level definition, and drop the whole definition when it contains one -- unless it is the
# target itself, in which case the permuter cannot run on this file at all.
starts = [m.start() for m in re.finditer(r'^[A-Za-z_][\w \t*]*\**\w+\([^;]*?\)\s*\{', text, re.M)]
starts.append(len(text))

kept, dropped = [], []
prev = 0
for i, start in enumerate(starts[:-1]):
    end = starts[i + 1]
    body = text[start:end]
    name = re.match(r'^[A-Za-z_][\w \t*]*?\**(\w+)\(', body).group(1)
    nested = re.search(r'^\s+[A-Za-z_][\w \t*]*\**\w+\([^;]*?\)\s*\{', body, re.M)
    if nested and name != func:
        dropped.append(name)
        kept.append(text[prev:start])
    else:
        if nested:
            print(f'permuter_import: {func} itself contains a nested function; '
                  'the permuter cannot parse it', file=sys.stderr)
            sys.exit(1)
        kept.append(text[prev:end])
    prev = end
kept.append(text[prev:])

open(dst, 'w').write(''.join(kept))
if dropped:
    print('permuter_import: stripped nested-function parents: ' + ', '.join(dropped))
PY

if ! grep -qE "^[A-Za-z_][A-Za-z_0-9 \t*]*\\b${FUNC}\\(" "$STRIPPED"; then
    echo "permuter_import: no definition of ${FUNC} in ${STRIPPED} (still an INCLUDE_ASM stub?)" >&2
    exit 1
fi

exec venv/bin/python3 ./tools/decomp-permuter/import.py \
    --settings permuter_settings_main.toml "$STRIPPED" "$ASM_FILE"
