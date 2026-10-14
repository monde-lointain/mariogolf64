#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
image=${SHC_CC_IMAGE:-m2c-shc-compiler:v5.1r03}

command -v docker >/dev/null 2>&1 || {
    echo "Docker is required to set up the SHC compiler" >&2
    return 1 2>/dev/null || exit 1
}

docker build \
    --platform linux/amd64 \
    --tag "$image" \
    "$repo_root/tools/shc_compiler" >&2

SHC_CC="$repo_root/tools/shc_compiler/shc"
SHC_CC_IMAGE=$image
printf "export SHC_CC='%s'\n" "$(printf %s "$SHC_CC" | sed "s/'/'\\\\''/g")"
printf "export SHC_CC_IMAGE='%s'\n" "$(printf %s "$SHC_CC_IMAGE" | sed "s/'/'\\\\''/g")"
