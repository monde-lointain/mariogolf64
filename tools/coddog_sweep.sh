#!/usr/bin/env bash
# tools/coddog_sweep.sh (S71) — fingerprint MG64's functions against ultralib VERSION_J via coddog.
#
# Reveals each remaining func_<addr>'s ultralib source file, turning "none/classical"-flagged
# libultra candidates into known verbatim-mirror targets (crc.c was mis-seeded pts-13 classical;
# coddog proved it a verbatim 2-fn mirror). Writes tools/coddog/coddog_map.tsv, which
# tools/pick_target.py reads to re-price + flag (coddog-mirror:<file>@<pct>). See
# docs/hazards.md#coddog-cross-ref. Re-run after a `make` (the MG64 ELF must be fresh).
#
# Config via env (defaults match the project's additional working dirs):
#   CODDOG    coddog binary           (default ~/development/repos/coddog/target/release/coddog)
#   ULTRALIB  ultralib repo root      (default ~/development/repos/ultralib)  -- the VERSION_J pin
#   ULVER     ultralib build version  (default J)
#   ULLIB     ultralib variant        (default libgultra, the release build)
#   THRESH    coddog similarity floor (default 0.95)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CODDOG="${CODDOG:-$HOME/development/repos/coddog/target/release/coddog}"
ULTRALIB="${ULTRALIB:-$HOME/development/repos/ultralib}"
ULVER="${ULVER:-J}"
ULLIB="${ULLIB:-libgultra}"
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
LD="${LD:-mips-linux-gnu-ld}"
THRESH="${THRESH:-0.95}"

MG_ELF="$REPO/build/mariogolf64.elf"
UL_OBJDIR="$ULTRALIB/build/$ULVER/$ULLIB"
WORK="$REPO/build/coddog"
UL_ELF="$WORK/ultralib_$ULVER.o"
MAP="$REPO/tools/coddog/coddog_map.tsv"

for f in "$CODDOG" "$MG_ELF"; do
  [ -e "$f" ] || { echo "coddog_sweep: missing $f (build the project / coddog first)" >&2; exit 1; }
done
[ -d "$UL_OBJDIR" ] || { echo "coddog_sweep: missing $UL_OBJDIR (build ultralib $ULVER first)" >&2; exit 1; }

mkdir -p "$WORK" "$REPO/tools/coddog"

echo "coddog_sweep: building combined ultralib $ULVER ELF from $UL_OBJDIR ..." >&2
# KMC objects carry a non-standard symtab + ECOFF .reginfo that GNU ld rejects; objcopy -R both
# strips those sections and normalizes the symtab to GNU order. Then a relocatable link merges all
# TUs into one ELF coddog/objdiff can parse (it cannot read a .a archive).
rm -rf "$WORK/stripped" && mkdir -p "$WORK/stripped"
( cd "$UL_OBJDIR" && find . -name '*.o' -print0 | while IFS= read -r -d '' o; do
    out="$WORK/stripped/$(echo "$o" | sed 's#[/.]#_#g').o"
    "$OBJCOPY" -R .mdebug -R .mdebug.abi32 -R .reginfo -R .pdr -R .comment "$o" "$out"
  done )
"$LD" -r --allow-multiple-definition "$WORK"/stripped/*.o -o "$UL_ELF"

# ultralib symbol -> source-file map (basename .o -> .c, relative to ultralib src tree).
SYM2FILE="$WORK/sym2file.tsv"; : > "$SYM2FILE"
( cd "$UL_OBJDIR" && for o in $(find . -name '*.o'); do
    src="${o#./}"; src="${src%.o}.c"
    "${OBJCOPY%objcopy}nm" "$o" 2>/dev/null | awk -v f="$src" '/ T | t /{print $3"\t"f}'
  done ) > "$SYM2FILE"

echo "coddog_sweep: writing decomp.yamls + running compare2 (threshold $THRESH) ..." >&2
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
cat > "$WORK/ultralib.decomp.yaml" <<EOF
name: ultralib
platform: n64
versions:
  - name: $ULVER
    fullname: ultralib VERSION_$ULVER $ULLIB
    paths:
      target: $UL_ELF
      build_dir: $UL_OBJDIR
      map: $UL_ELF
      compiled_target: $UL_ELF
      elf: $UL_ELF
EOF

# compare2 emits "MGNAME - ULNAME (NN.NN%)"; parse_map.py joins ULNAME -> source file (4-col map).
"$CODDOG" compare2 -t "$THRESH" --sort-by similarity \
    "$WORK/mg64.decomp.yaml" jp "$WORK/ultralib.decomp.yaml" "$ULVER" 2>/dev/null \
  | python3 "$REPO/tools/coddog/parse_map.py" "$SYM2FILE" "$MAP"
echo "coddog_sweep: done. pick_target.py now reads $MAP" >&2
