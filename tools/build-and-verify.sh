#!/usr/bin/env bash
set -euo pipefail

trap 'echo "BUILD HAS FAILED. Treat this as a build failure; warnings or a non-matching checksum still count as failures."' ERR

build_make="${MAKE:-make}"
if [[ $# -gt 0 ]]; then
    echo "Usage: $0" >&2
    exit 2
fi

QUIET=1 "$build_make" clean
QUIET=1 "$build_make" extract >/dev/null
BUILD_AND_VERIFY=1 QUIET=1 "$build_make" -j"${JOBS:-$(nproc)}" all
printf "\033[0;32mBUILD SUCCEEDED\033[0m. The rebuilt ROM matches mariogolf64.sha1.\n"
