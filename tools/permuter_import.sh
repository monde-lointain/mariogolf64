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
# Use this instead of `./setup-permuter.sh` only when no isolated `nonmatchings/<func>/base.c` exists
# and the host TU contains nested functions: it copies the TU with the nested-function parents
# deleted (inside the project root, because import.py rejects an outside path) and imports that.
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
source tools/lib.sh
mg_activate_venv
PYTHON="$PWD/.venv/bin/python3"

FUNC=${1:-}
if [[ -z $FUNC ]]; then
    echo "usage: tools/permuter_import.sh <func> [<c-file>]" >&2
    exit 1
fi

# The shared resolver (tools/lib.sh) finds the defining TU and the target assembly; a declaration
# elsewhere does not select the wrong file.
mg_resolve_c_asm "$FUNC"
C_FILE=${2:-$C_FILE}
if [[ ! -f $C_FILE ]]; then
    echo "permuter_import: $C_FILE does not exist" >&2
    exit 1
fi

mkdir -p nonmatchings/permsrc
STRIPPED=nonmatchings/permsrc/$(basename "$C_FILE")

if [[ ! -x "$PYTHON" ]]; then
    echo "permuter_import: configured Python is missing: $PYTHON" >&2
    exit 1
fi
"$PYTHON" - "$C_FILE" "$STRIPPED" "$FUNC" <<'PY'
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

exec "$PYTHON" tools/rom_build.py import-permuter \
    --source "$C_FILE" --input "$STRIPPED" --asm "$ASM_FILE"
