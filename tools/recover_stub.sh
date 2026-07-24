#!/usr/bin/env bash
# recover_stub.sh — regenerate lost/corrupt nonmatchings .s stubs (S271).
#
# splat's `c`-mode `make extract` leaves DISASSEMBLY GAPS at some curated /
# decompose-split function addresses, so those functions' `asm/nonmatchings/<tree>/<stem>/<fn>.s`
# stubs are STALE-PERSISTENT RELICS: once deleted they do NOT regenerate (and they are
# gitignored, so there is no `git restore`). This recovers them: it flips the subseg `c`->`asm`
# (asm-mode disassembles the WHOLE range — every function, all relocs resolved — and each per-fn
# block is BYTE-IDENTICAL to the nonmatchings stub format), carves each requested function's block
# into its `c`-mode stub path, then flips the subseg back to `c`.
#
# Usage:  tools/recover_stub.sh <SUBSEG_ROM_OFFSET_HEX> <fn> [<fn> ...]
#   e.g.  tools/recover_stub.sh 0x3A490 init_terrain_vertex_texcoords gen_terrain_detail_texture
#         tools/recover_stub.sh 0x34FA0 func_8005DDAC
# The offset must match an existing `- [0x<off>, c, <tree>/<stem>]` line in mariogolf64.yaml.
set -euo pipefail

cd "$(dirname "$0")/.."
YAML=mariogolf64.yaml

[ $# -ge 2 ] || { echo "usage: $0 <0xSUBSEG_OFFSET> <fn>..." >&2; exit 2; }
OFF="$1"; shift
OFF="${OFF#0x}"; OFF="0x${OFF}"   # normalise to 0x-prefixed

# Locate the `c` subseg line and capture its <tree>/<stem>.
LINE=$(grep -nE "^\s*-\s*\[${OFF}, c, [^]]+\]" "$YAML" | head -1) \
  || { echo "no '[${OFF}, c, ...]' subseg in $YAML" >&2; exit 1; }
LNO="${LINE%%:*}"
CPATH=$(printf '%s\n' "$LINE" | sed -E "s#.*\[${OFF}, c, ([^]]+)\].*#\1#")   # e.g. main/bgm_load_song_from_rom
STEM="${CPATH##*/}"                                                          # e.g. bgm_load_song_from_rom
TREE="${CPATH%/*}"                                                           # e.g. main
DST="asm/nonmatchings/${CPATH}"
ASM_S="asm/$(printf '%s' "$OFF" | sed 's/^0x//').s"                          # e.g. asm/3A490.s

echo "subseg ${OFF} = ${CPATH}; recovering into ${DST}/ : $*"

# Save the yaml (full-file backup) so the flip is always reverted, even on error.
BAK=$(mktemp)
cp "$YAML" "$BAK"
restore() { cp "$BAK" "$YAML"; rm -f "$BAK" "$ASM_S"; }
trap restore EXIT

# Flip c -> asm and disassemble the whole range.
sed -i "${LNO}s#.*#      - [${OFF}, asm]#" "$YAML"
make extract >/dev/null 2>&1 || { echo "extract (asm-mode) failed" >&2; exit 1; }
[ -f "$ASM_S" ] || { echo "expected $ASM_S not produced" >&2; exit 1; }

# Carve each requested function's `nonmatching <fn>, ..` .. `endlabel <fn>` block.
mkdir -p "$DST"
rc=0
for fn in "$@"; do
  awk -v fn="$fn" '
    $0 ~ ("^nonmatching " fn ",") {g=1}
    g {print}
    $0 ~ ("^endlabel " fn "$") {g=0}
  ' "$ASM_S" > "$DST/$fn.s"
  if [ -s "$DST/$fn.s" ] && grep -q "^glabel $fn$" "$DST/$fn.s"; then
    printf '  recovered %-40s (%s lines)\n' "$fn" "$(wc -l < "$DST/$fn.s")"
  else
    echo "  FAILED to carve $fn (not found in $ASM_S)" >&2
    rm -f "$DST/$fn.s"
    rc=1
  fi
done

# trap restores the `c` subseg + removes the temp asm .s.
[ $rc -eq 0 ] || { echo "one or more functions not recovered" >&2; exit $rc; }
echo "done. Re-run 'make extract && tools/verify-rom.sh' to confirm the tree builds green."
