#!/usr/bin/env bash
# tools/audio_sweep.sh — fingerprint MG64 against every built audio reference cell via coddog.
#
# The audio analog of tools/nusys_sweep.sh, run once per build/audio-ref/<cell>/ produced by
# tools/build_audio_refs.sh. Each cell -> tools/coddog/audio/<cell>_map.tsv (4-col: mgname, refname,
# srcfile, pct). tools/audio_pin.py then aggregates these to pin the version+compiler+flags and write
# the canonical per-lib maps pick_target.py consumes. Needs a fresh MG64 ELF (run `make`). See
# docs/hazards.md#coddog-cross-ref.
#
# Usage:  tools/audio_sweep.sh [FILTER]   (FILTER = substring on cell-id)
# Env:    CODDOG (binary), THRESH (similarity floor, default 0.95)
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
FILTER="${1:-}"
CODDOG="${CODDOG:-$HOME/development/repos/coddog/target/release/coddog}"
THRESH="${THRESH:-0.95}"

MG_ELF="$REPO/build/mariogolf64.elf"
REFROOT="$REPO/build/audio-ref"
OUTDIR="$REPO/tools/coddog/audio"

for f in "$CODDOG" "$MG_ELF"; do
  [ -e "$f" ] || { echo "audio_sweep: missing $f (run the project build / build coddog first)" >&2; exit 1; }
done
[ -d "$REFROOT" ] || { echo "audio_sweep: no $REFROOT (run tools/build_audio_refs.sh first)" >&2; exit 1; }
mkdir -p "$OUTDIR"

MGYAML="$REFROOT/mg64.decomp.yaml"
cat > "$MGYAML" <<EOF
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

swept=0
for celldir in "$REFROOT"/*/; do
  cell="$(basename "$celldir")"
  [[ "$cell" == _* ]] && continue                       # skip _sdkinc / _norm staging dirs
  local_ref="$celldir/ref.o"; sym2file="$celldir/sym2file.tsv"
  [ -e "$local_ref" ] && [ -e "$sym2file" ] || continue
  [ -n "$FILTER" ] && [[ "$cell" != *"$FILTER"* ]] && continue

  cat > "$celldir/ref.decomp.yaml" <<EOF
name: $cell
platform: n64
versions:
  - name: ref
    fullname: audio reference $cell
    paths:
      target: $local_ref
      build_dir: $celldir
      map: $local_ref
      compiled_target: $local_ref
      elf: $local_ref
EOF
  "$CODDOG" compare2 -t "$THRESH" --sort-by similarity \
      "$MGYAML" jp "$celldir/ref.decomp.yaml" ref 2>/dev/null \
    | python3 "$REPO/tools/coddog/parse_map.py" "$sym2file" "$OUTDIR/${cell}_map.tsv"
  n=$(wc -l < "$OUTDIR/${cell}_map.tsv" 2>/dev/null || echo 0)
  echo "audio_sweep: $cell -> $n matches" >&2
  swept=$((swept+1))
done
echo "audio_sweep: swept $swept cells -> $OUTDIR/*.tsv" >&2
