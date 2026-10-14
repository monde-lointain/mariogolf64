#!/usr/bin/env bash
# Adapt the permuter's <flags> <input> -o <output> protocol to the ROM pipeline.
# Profile flags are exposed to the importer for preprocessing and forwarded to
# compilation; KMC GAS always reads a file, never a pipe.
set -euo pipefail
if [[ "${1:-}" != "--profile-source" || $# -lt 5 ]]; then
    echo "usage: $0 --profile-source <parent.c> [flags...] <input.c> -o <output.o>" >&2
    exit 2
fi
PARENT="$2"
shift 2
INPUT="${@: -3:1}"
OUTPUT="${@: -1}"
if [[ "${@: -2:1}" != "-o" ]]; then
    echo "expected <input.c> -o <output.o>" >&2
    exit 2
fi
EXTRA=()
for flag in "${@:1:$#-3}"; do
    EXTRA+=("--extra-flag=$flag")
done
export UV_PROJECT_ENVIRONMENT="$(cd "$(dirname "$0")/.." && pwd)/.venv"
exec uv run --locked --project "$(dirname "$0")/.." python tools/rom_build.py compile --source "$INPUT" --output "$OUTPUT" \
    --profile-source "$PARENT" --nonmatching "${EXTRA[@]}"
