#!/usr/bin/env bash
# tools/build_nusys_ref.sh — compile the nusys-2.07 library as a coddog reference object.
#
# The ultralib coddog sweep (tools/coddog_sweep.sh) consumes a *prebuilt* ultralib object tree, but
# nusys has no prebuilt tree at the project's compile profile. So this script builds one: every
# nusys-2.07 mainlib .c is compiled with the project's KMC GCC + LIBNUSYS_CFLAGS (identical recipe to
# the in-tree mirror — mk/src.mk + mk/libnusys.mk), then all TUs merge into one relocatable ELF that
# coddog/objdiff can read (it can't parse a .a archive or a dir of .o). Output is gitignored under
# build/nusys-ref/. Re-run only when the nusys source or LIBNUSYS_CFLAGS change; tools/nusys_sweep.sh
# then fingerprints MG64 against build/nusys-ref/nusys.o. See docs/hazards.md#coddog-cross-ref.
#
# Config via env (defaults match the project's additional working dirs):
#   NUSYS_SRC  nusys-2.07 mainlib source dir
#              (default ~/development/repos/n64sdkmod/.../nusys-2.07/nusys/src/mainlib)
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
# shellcheck source=tools/sweep_lib.sh
source "$REPO/tools/sweep_lib.sh"

MG_BUILD_DIR="${MG64_BUILD_DIR:-build/us}"
[[ "$MG_BUILD_DIR" = /* ]] || MG_BUILD_DIR="$REPO/$MG_BUILD_DIR"
OBJCOPY="${OBJCOPY:-$(python3 -c 'import sys; sys.path.insert(0,"tools"); from rom_build import tool_path; print(tool_path("objcopy"))')}"
LD="${LD:-$(python3 -c 'import sys; sys.path.insert(0,"tools"); from rom_build import tool_path; print(tool_path("ld"))')}"
STRIP="${STRIP:-$(python3 -c 'import sys; sys.path.insert(0,"tools"); from rom_build import tool_path; print(tool_path("strip"))')}"
NM="${NM:-$(python3 -c 'import sys; sys.path.insert(0,"tools"); from rom_build import tool_path; print(tool_path("nm"))')}"

OUT="$MG_BUILD_DIR/nusys-ref"
REF_ELF="$OUT/nusys.o"
SYM2FILE="$OUT/sym2file.tsv"

# Compiler flags and file-fed assembly come from the authoritative libnusys profile.

rm -rf "$OUT"; mkdir -p "$OUT/mainlib"
: > "$SYM2FILE"

echo "build_nusys_ref: compiling $(ls "$NUSYS_SRC"/*.c | wc -l) mainlib TUs from $NUSYS_SRC ..." >&2
ok=0; fail=0; failed=()
for src in "$NUSYS_SRC"/*.c; do
  base="$(basename "${src%.c}")"
  obj="$OUT/mainlib/$base.o"
  if python3 tools/rom_build.py compile --source "$src" --output "$obj" --profile libnusys 2>"$obj.err"; then
    "$STRIP" "$obj" -N dummy-symbol-name 2>/dev/null || true
    # Strip the KMC-specific ECOFF/debug sections GNU ld rejects (same set as the ultralib sweep).
    "$OBJCOPY" "${MG_KMC_STRIP_SECTIONS[@]}" "$obj" "$obj" 2>/dev/null || true
    # symbol -> source-file map (relative path the pick_target nusys resolvers expect).
    "$NM" "$obj" 2>/dev/null | awk -v f="mainlib/$base.c" '/ T | t /{print $3"\t"f}' >> "$SYM2FILE"
    rm -f "$s" "$obj.err"
    ok=$((ok + 1))
  else
    failed+=("$base"); fail=$((fail + 1))
    echo "build_nusys_ref: FAIL $base: $(head -1 "$obj.err" 2>/dev/null)" >&2
    rm -f "$s" "$obj"
  fi
done

[ "$ok" -gt 0 ] || { echo "build_nusys_ref: every TU failed to compile; aborting" >&2; exit 1; }

# Merge all per-TU objects into one relocatable ELF. --allow-multiple-definition tolerates shared
# static/inline duplicates across TUs.
"$LD" -r --allow-multiple-definition "$OUT"/mainlib/*.o -o "$REF_ELF"

echo "build_nusys_ref: compiled $ok, failed $fail -> $REF_ELF" >&2
[ "$fail" -eq 0 ] || printf 'build_nusys_ref: skipped (logged): %s\n' "${failed[*]}" >&2
echo "build_nusys_ref: $(wc -l < "$SYM2FILE") symbols mapped in $SYM2FILE" >&2
