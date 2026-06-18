#!/usr/bin/env bash
# tools/nusys_sweep.sh — fingerprint MG64's functions against the nusys-2.07 reference via coddog.
#
# The nusys analog of tools/coddog_sweep.sh (which targets ultralib VERSION_J). Reveals each remaining
# nusys func_<addr>'s mainlib source file, turning un-named libnusys candidates into known
# verbatim-mirror targets. Writes tools/coddog/nusys_map.tsv, which tools/pick_target.py reads to
# re-price + flag (coddog-mirror:mainlib/<file>.c). Needs build/nusys-ref/nusys.o (run
# tools/build_nusys_ref.sh first) and a fresh MG64 ELF (run `make`). See docs/hazards.md#coddog-cross-ref.
#
# Config via env:
#   CODDOG   coddog binary    (default ~/development/repos/coddog/target/release/coddog)
#   THRESH   similarity floor (default 0.95)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CODDOG="${CODDOG:-$HOME/development/repos/coddog/target/release/coddog}"
THRESH="${THRESH:-0.95}"

MG_ELF="$REPO/build/mariogolf64.elf"
NUSYS_ELF="$REPO/build/nusys-ref/nusys.o"
SYM2FILE="$REPO/build/nusys-ref/sym2file.tsv"
WORK="$REPO/build/nusys-ref"
MAP="$REPO/tools/coddog/nusys_map.tsv"

for f in "$CODDOG" "$MG_ELF" "$NUSYS_ELF" "$SYM2FILE"; do
  [ -e "$f" ] || { echo "nusys_sweep: missing $f (run the project build + tools/build_nusys_ref.sh first)" >&2; exit 1; }
done
mkdir -p "$REPO/tools/coddog"

cat > "$WORK/mg64.decomp.yaml" <<EOF
name: Mario Golf 64
platform: n64
versions:
  - name: jp
    fullname: Mario Golf 64 JP
    paths:
      target: $REPO/baserom.z64
      build_dir: $REPO/build
      map: $REPO/build/mariogolf64.map
      compiled_target: $REPO/build/mariogolf64.z64
      elf: $MG_ELF
EOF
cat > "$WORK/nusys.decomp.yaml" <<EOF
name: nusys
platform: n64
versions:
  - name: nusys
    fullname: NuSystem 2.07 mainlib
    paths:
      target: $NUSYS_ELF
      build_dir: $WORK
      map: $NUSYS_ELF
      compiled_target: $NUSYS_ELF
      elf: $NUSYS_ELF
EOF

# compare2 emits "MGNAME - NUSYSNAME (NN.NN%)"; parse_map.py joins NUSYSNAME -> source file (4-col map).
echo "nusys_sweep: running compare2 (threshold $THRESH) ..." >&2
"$CODDOG" compare2 -t "$THRESH" --sort-by similarity \
    "$WORK/mg64.decomp.yaml" jp "$WORK/nusys.decomp.yaml" nusys 2>/dev/null \
  | python3 "$REPO/tools/coddog/parse_map.py" "$SYM2FILE" "$MAP"
echo "nusys_sweep: done. pick_target.py now reads $MAP" >&2
