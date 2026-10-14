#!/usr/bin/env bash
# Re-extract generated stubs for a configured CMake binary tree.
set -euo pipefail

cd "$(dirname "$0")/.."
[ $# -ge 2 ] || { echo "usage: $0 <0xSUBSEG_OFFSET> <fn>..." >&2; exit 2; }
OFF="${1#0x}"
shift
LINE=$(grep -E "^\s*-\s*\[0x${OFF}, c, [^]]+\]" mariogolf64.yaml | head -1) \
    || { echo "no [0x${OFF}, c, ...] subsegment in mariogolf64.yaml" >&2; exit 1; }
CPATH=$(printf '%s\n' "$LINE" | sed -E "s#.*\[0x${OFF}, c, ([^]]+)\].*#\1#")
BUILD_DIR="${MG64_BUILD_DIR:-build/us}"
[[ "$BUILD_DIR" = /* ]] || BUILD_DIR="$PWD/$BUILD_DIR"

cmake --build "$BUILD_DIR" --target extract
ASM_DIR="$BUILD_DIR/work/asm/nonmatchings/$CPATH"
for fn in "$@"; do
    if [[ ! -s "$ASM_DIR/$fn.s" ]]; then
        echo "recover_stub: extraction did not produce $ASM_DIR/$fn.s" >&2
        exit 1
    fi
    printf '  recovered %s\n' "$fn"
done
