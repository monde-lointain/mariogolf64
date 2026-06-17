#!/usr/bin/env bash
# Gated ROM verify: the stale-green guard (CLAUDE.md DoD rule).
#
# A failed `make` leaves the PREVIOUS build/mariogolf64.z64 in place, so running
# `sha1sum` unconditionally after `make` reports the stale ROM's hash and
# false-positives. Assert the `build/mariogolf64.z64: OK` line BEFORE trusting
# sha1sum.
#
# Usage: tools/verify-rom.sh [--extract]   (--extract runs `make extract` first)
# Exit 0 only when `make` printed OK AND the ROM SHA-1 equals the baserom.
set -uo pipefail
cd "$(dirname "$0")/.."

# Single-source the expected hash from splat's authoritative `sha1:` (top of mariogolf64.yaml),
# rather than a duplicated literal. `exit` stops at the first match (guards a future per-segment key).
BASEROM_SHA1=$(awk '/^sha1:/{print $2; exit}' mariogolf64.yaml)
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

if [ "${1:-}" = "--extract" ]; then
    if ! make extract >"$LOG" 2>&1; then
        echo "EXTRACT FAILED - sha1sum NOT trusted:"; tail -15 "$LOG"; exit 1
    fi
fi

if make >"$LOG" 2>&1 && tail -1 "$LOG" | grep -q "build/mariogolf64.z64: OK"; then
    s=$(sha1sum build/mariogolf64.z64 | cut -d' ' -f1)
    if [ "$s" = "$BASEROM_SHA1" ]; then
        echo "make OK + ROM SHA-1 MATCH ✓ ($s)"; exit 0
    fi
    echo "make OK but ROM SHA-1 MISMATCH ✗ (got $s, want $BASEROM_SHA1)"; exit 1
fi
echo "BUILD FAILED - sha1sum NOT trusted (stale-ROM guard). Errors:"
grep -iE "error|undefined reference|undeclared|Error [0-9]|: FAILED" "$LOG" | grep -v "^COMPILER_PATH" | head -15
exit 1
