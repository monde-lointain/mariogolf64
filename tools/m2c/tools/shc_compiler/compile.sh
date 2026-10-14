#!/bin/sh
set -eu

test -f /work/input.c

rm -rf /tmp/shc-compile
mkdir -p /tmp/shc-compile
cp -a /opt/shc/bin/. /tmp/shc-compile/
sed -e 's/\r$//' -e 's/$/\r/' /work/input.c > /tmp/shc-compile/input.c

cd /tmp/shc-compile

SHC_LIB=. SHC_TMP=. wibo ./shc.exe input.c "$@"

test -s input.src || {
    echo "shc produced no assembly" >&2
    exit 1
}

sed 's/\r$//' input.src > /work/output.src
