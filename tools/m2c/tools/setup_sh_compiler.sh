#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${SH_CC_IMAGE:-m2c-sh-compiler:cc1-psx-26}

command -v docker >/dev/null 2>&1 || {
    echo "Docker is required to set up the SH compiler" >&2
    return 1 2>/dev/null || exit 1
}

docker build \
    --platform linux/amd64 \
    --tag "$image" \
    "$repo_root/tools/sh_compiler" >&2

SH_CC="$repo_root/tools/sh_compiler/sh-gcc"
SH_CC_IMAGE=$image
printf "export SH_CC='%s'\n" "$(printf %s "$SH_CC" | sed "s/'/'\\\\''/g")"
printf "export SH_CC_IMAGE='%s'\n" "$(printf %s "$SH_CC_IMAGE" | sed "s/'/'\\\\''/g")"
