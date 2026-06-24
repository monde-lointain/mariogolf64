#!/usr/bin/env bash
# tools/build_audio_refs.sh — compile the audio libraries (libmus / libnaudio / nuaulstl) as coddog
# reference objects, across a {compiler x opt} matrix for {each source version}, to pin the exact
# library version + toolchain + flags MG64 was built with. The audio analog of build_nusys_ref.sh,
# generalized over the matrix manifest tools/audio_ref_versions.tsv (one row per lib+version; this
# script expands compilers x opts per row). Each cell -> build/audio-ref/<cell>/{ref.o,sym2file.tsv};
# tools/audio_sweep.sh then fingerprints MG64 against each. See docs/hazards.md#coddog-cross-ref.
#
# Compilers (all run natively, no qemu; all emit ELF MIPS so the KMC strip+ld -r pipeline is shared):
#   kmc    project KMC gcc 2.7.2 (tools/cc/gcc + tools/cc/as) — the proven path (nusys @99.99%)
#   ido53  IDO 5.3 cc — the compiler the SDK Makefiles target (most authentic for vendor source)
#   ido71  IDO 7.1 cc
#
# Usage:  tools/build_audio_refs.sh [FILTER]
#   FILTER  optional substring on the cell-id or lib/ver (e.g. `libmus`, `sdk40`, `ido53`)
# Env:
#   COMPILERS  space list (default "kmc ido53 ido71")
#   OPTS       space list of opt labels (default "O1 O2 O3"); KMC gcc 2.7.2 has no -Os
#   MANIFEST   manifest path (default tools/audio_ref_versions.tsv)
set -uo pipefail   # NOTE: not -e; a failed cell must not abort the whole matrix.

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
# shellcheck source=tools/sweep_lib.sh
source "$REPO/tools/sweep_lib.sh"

FILTER="${1:-}"
MANIFEST="${MANIFEST:-$REPO/tools/audio_ref_versions.tsv}"
COMPILERS="${COMPILERS:-kmc ido53 ido71}"
OPTS="${OPTS:-O1 O2 O3}"

IDO53="${IDO53:-$HOME/development/repos/ido-static-recomp/build/5.3/out}"
IDO71="${IDO71:-$HOME/development/repos/ido-static-recomp/build/7.1/out}"
OBJCOPY="${OBJCOPY:-mips-linux-gnu-objcopy}"
LD="${LD:-mips-linux-gnu-ld}"
STRIP="${STRIP:-mips-linux-gnu-strip}"
NM="${NM:-mips-linux-gnu-nm}"

OUTROOT="$REPO/build/audio-ref"
SUMMARY="$OUTROOT/SUMMARY.tsv"

# Header set is compiler-aware. KMC uses the project headers (the proven path: same trick as
# build_nusys_ref.sh, fingerprints line up byte-for-byte). IDO's strict C89 cfe rejects the project
# headers' GNU-isms (`//` comments etc.), so IDO uses the ORIGINAL SDK headers it was built for.
PROJ_INC=(-I include -I include/libultra -I include/libultra/PR -I include/libultra/internal -I include/libkmc -I include/libnusys)
SDK_INC_BASE="${SDK_INC_BASE:-$HOME/n64sdk/J/pc/Ultra_Dev_Base/usr/include}"
SDK_INC=(-I "$SDK_INC_BASE" -I "$SDK_INC_BASE/PR")
BASE_DEFS=(-D_LANGUAGE_C -D_FINALROM)
# KMC and IDO take different arch flags; opt is per-cell. KMC mirrors the nusys recipe; IDO mirrors
# the SDK Makefile.sgi recipe (-mips2 -non_shared -Xcpluscomm).
KMC_ARCH=(-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls)
IDO_ARCH=(-G 0 -mips2 -non_shared -Xcpluscomm)

[ -f "$MANIFEST" ] || { echo "build_audio_refs: missing manifest $MANIFEST" >&2; exit 1; }
[ -x tools/cc/gcc ] || { echo "build_audio_refs: tools/cc/gcc missing (run make -C tools)" >&2; exit 1; }

mkdir -p "$OUTROOT"
# Fresh header on a full run; a filtered run appends so it doesn't clobber other cells' rows.
if [ -z "$FILTER" ] || [ ! -f "$SUMMARY" ]; then
  printf 'cell\tlib\tver\tcompiler\topt\tok\tfail\tnsyms\n' > "$SUMMARY"
fi

expand() { eval echo "${1/#\~/$HOME}"; }   # ~ -> $HOME
strip_cr() { find "$1" -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc' \) -exec sed -i 's/\r$//' {} + ; }

# IDO needs CR-free input. The PC-distribution SDK headers/source carry CRLF terminators that IDO's
# IRIX cfe mishandles at `\`-continued macros ("Illegal macro parameter name"); GNU/KMC cpp tolerates
# CR, so KMC reads the project headers + source unmodified. Stage normalized copies once per run.
NSDK="$OUTROOT/_sdkinc"      # normalized SDK header tree (IDO only)
NORM="$OUTROOT/_norm"        # normalized per-srcdir source stages (IDO only)
needs_ido=0; for c in $COMPILERS; do [ "$c" != kmc ] && needs_ido=1; done
rm -rf "$NORM"
if [ "$needs_ido" = 1 ]; then
  if [ -d "$SDK_INC_BASE" ]; then
    rm -rf "$NSDK"; mkdir -p "$NSDK"; cp -r "$SDK_INC_BASE/." "$NSDK/"; strip_cr "$NSDK"
    SDK_INC=(-I "$NSDK" -I "$NSDK/PR")
  else
    echo "build_audio_refs: WARN IDO requested but SDK_INC_BASE missing ($SDK_INC_BASE); IDO cells will fail" >&2
  fi
fi

# Stage a CR-normalized copy of an IDO source dir (cached per run), preserving the srcdir / srcdir/..
# / srcdir/../include relative layout the include flags rely on. Echoes the staged srcdir path.
stage_ido_src() {
  local srcdir="$1" key st
  key="$(echo "$srcdir" | sed 's#[^A-Za-z0-9]#_#g')"
  st="$NORM/$key"
  if [ ! -d "$st" ]; then
    mkdir -p "$st/p/src"; cp -r "$srcdir/." "$st/p/src/" 2>/dev/null
    if [ -d "$srcdir/../include" ]; then mkdir -p "$st/p/include"; cp -r "$srcdir/../include/." "$st/p/include/" 2>/dev/null; fi
    strip_cr "$st"
  fi
  echo "$st/p/src"
}

# Compile one TU to $2 (.o). Returns 0 on success. Logs first error to $2.err on failure.
compile_tu() {
  local compiler="$1" obj="$2" src="$3"; shift 3
  local cflags=("$@")   # opt + includes + defines
  local s="${obj%.o}.s"
  case "$compiler" in
    kmc)
      COMPILER_PATH=tools/cc tools/cc/gcc -S "${KMC_ARCH[@]}" "${cflags[@]}" -o "$s" "$src" 2>"$obj.err" \
        && tools/cc/as -EB -mips2 -G 0 -o "$obj" "$s" 2>>"$obj.err"
      local rc=$?; rm -f "$s"; return $rc ;;
    ido53|ido71)
      local cc; [ "$compiler" = ido53 ] && cc="$IDO53/cc" || cc="$IDO71/cc"
      "$cc" -c "${IDO_ARCH[@]}" "${cflags[@]}" -o "$obj" "$src" 2>"$obj.err"
      local rc=$?
      # IDO cc drops a `<srcbase>.u` usage file in CWD (the repo root); don't pollute the tree.
      rm -f "$REPO/$(basename "${src%.c}").u"
      return $rc ;;
    *) echo "unknown compiler $compiler" >"$obj.err"; return 2 ;;
  esac
}

build_cell() {
  local lib="$1" ver="$2" compiler="$3" opt="$4" srcdir="$5" defines="$6" incdirs="$7"
  local cell="${lib}_${ver}_${compiler}_${opt}"
  if [ -n "$FILTER" ] && [[ "$cell" != *"$FILTER"* ]]; then return; fi
  srcdir="$(expand "$srcdir")"
  if [ ! -d "$srcdir" ]; then echo "build_audio_refs: SKIP $cell (missing $srcdir)" >&2; return; fi

  local out="$OUTROOT/$cell"; rm -rf "$out"; mkdir -p "$out/obj"
  local ref="$out/ref.o" sym2file="$out/sym2file.tsv"; : > "$sym2file"

  # include + define flags — header base + source tree are compiler-aware: KMC uses the project
  # headers and reads source in place (its proven path); IDO uses normalized SDK headers + a
  # CR-stripped source stage.
  local srcdir_eff="$srcdir" hdr_inc=("${PROJ_INC[@]}")
  if [ "$compiler" != kmc ]; then hdr_inc=("${SDK_INC[@]}"); srcdir_eff="$(stage_ido_src "$srcdir")"; fi
  local incs=(-I "$srcdir_eff" -I "$srcdir_eff/.." -I "$srcdir_eff/../include" "${hdr_inc[@]}")
  if [ "$incdirs" != none ]; then
    local IFS=,; for d in $incdirs; do incs+=(-I "$(expand "$d")"); done; unset IFS
  fi
  local defs=("${BASE_DEFS[@]}")
  if [ "$defines" != none ]; then
    local IFS=,; for d in $defines; do defs+=("-D$d"); done; unset IFS
  fi
  local cflags=("-$opt" "${incs[@]}" "${defs[@]}")

  local ok=0 fail=0 failed=()
  shopt -s nullglob
  for src in "$srcdir_eff"/*.c; do
    local base; base="$(basename "${src%.c}")"
    local obj="$out/obj/$base.o"
    if compile_tu "$compiler" "$obj" "$src" "${cflags[@]}"; then
      "$STRIP" "$obj" -N dummy-symbol-name 2>/dev/null || true
      "$OBJCOPY" "${MG_KMC_STRIP_SECTIONS[@]}" "$obj" "$obj" 2>/dev/null || true
      # IDO objects also carry IRIX `.options` + `.gptab.*` sections GNU ld can't represent on output.
      [ "$compiler" != kmc ] && "$OBJCOPY" --wildcard -R '.gptab*' -R '.options' "$obj" "$obj" 2>/dev/null || true
      "$NM" "$obj" 2>/dev/null | awk -v f="$base.c" '/ T | t /{print $3"\t"f}' >> "$sym2file"
      rm -f "$obj.err"; ok=$((ok+1))
    else
      failed+=("$base"); fail=$((fail+1)); rm -f "$obj"
    fi
  done
  shopt -u nullglob

  local nsyms=0
  if [ "$ok" -gt 0 ]; then
    "$LD" -r --allow-multiple-definition "$out"/obj/*.o -o "$ref" 2>>"$out/ld.err" || \
      echo "build_audio_refs: $cell ld -r FAILED (see $out/ld.err)" >&2
    nsyms=$(wc -l < "$sym2file")
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$cell" "$lib" "$ver" "$compiler" "$opt" "$ok" "$fail" "$nsyms" >> "$SUMMARY"
  echo "build_audio_refs: $cell  ok=$ok fail=$fail nsyms=$nsyms" >&2
  if [ "$fail" -gt 0 ] && [ "${VERBOSE_FAIL:-0}" = 1 ]; then
    printf '  failed: %s\n' "${failed[*]}" >&2
    for b in "${failed[@]:0:1}"; do echo "  e.g. $b:"; head -2 "$out/obj/$b.o.err" 2>/dev/null | sed 's/^/    /'; done >&2
  fi
}

echo "build_audio_refs: COMPILERS='$COMPILERS' OPTS='$OPTS' FILTER='${FILTER:-}'" >&2
while IFS=$'\t' read -r lib ver srcdir defines incdirs; do
  [[ "$lib" =~ ^[[:space:]]*# ]] && continue
  [ -z "${lib// }" ] && continue
  for compiler in $COMPILERS; do
    for opt in $OPTS; do
      build_cell "$lib" "$ver" "$compiler" "$opt" "$srcdir" "$defines" "$incdirs"
    done
  done
done < "$MANIFEST"

echo "build_audio_refs: matrix summary -> $SUMMARY" >&2
column -t "$SUMMARY" >&2 || cat "$SUMMARY" >&2
