#!/bin/sh
# Build the oracle's compiler directory, build/oracle, for tools/oracle.py.
#
#     tools/build_oracle.sh [KMC_GCC_SOURCE]
#
# Builds cc1 from the KMC GCC 2.7.2 source (by default ~/src/kmc-gcc-src),
# with tools/oracle.patch, which lets cc1 trace its register allocators to the
# file ORACLE_TRACE, merge the pseudos ORACLE_MERGE pairs as the RTL is made,
# leave the pseudos ORACLE_GLOBAL names to global-alloc, and override the
# allocation of the pseudos ORACLE_FORCE names as reload starts. In the
# function ORACLE_FUNCTION it also numbers the scheduler's choices, and cse's
# taking of cheaper equivalents, as decisions, takes the other way at those
# ORACLE_FLIP lists, and, when ORACLE_FORK_DIR names a directory, forks at
# each decision to compile its flip too. It is built with -O3, LTO and the
# profile of a build of the ROM, as the toolchain's cc1 is, and put beside the
# toolchain's other programs. Without the ORACLE_ variables it gives the same
# code as the toolchain's cc1: the ROM builds with it unchanged. A changed
# patch rebuilds it.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
source=${1:-$HOME/src/kmc-gcc-src}
out=$root/build/oracle
patch=$root/tools/oracle.patch
cores=$(nproc)

# build_rom CC1 DIR: build the ROM into DIR with CC1, the toolchain otherwise
build_rom() {
    mkdir -p "$2/toolchain"
    for program in "$root"/tools/gcc_kmc/*; do
        ln -sf "$program" "$2/toolchain/$(basename "$program")"
    done
    ln -sf "$1" "$2/toolchain/cc1"
    make -C "$root" -j"$cores" KMC_DIR="$2/toolchain" BUILD_DIR="$2/build" \
        > "$2/make.log" 2>&1
}

build=$out/source
if [ ! -x "$build/cc1" ] || [ "$patch" -nt "$build/cc1" ]; then
    rm -rf "$build"
    cp -r "$source" "$build"
    cd "$build"
    patch -p1 < "$patch" > /dev/null
    ./configure --target=mips-mips-gnu --prefix=/opt/cross --with-gnu-as \
        --disable-gprof --disable-gdb --disable-werror \
        --host=i386-pc-linux --build=i386-pc-linux > configure.log
    # The parser and keyword table are prebuilt; keep make from remaking them
    touch c-parse.c c-gperf.h
    flags="-static -std=gnu89 -m32 -O3 -flto=auto -w"
    make cc1 CFLAGS="$flags -fprofile-generate" > make.log 2>&1
    rm -rf "$out/training"
    build_rom "$build/cc1" "$out/training"
    rm -rf "$out/training"
    # Keep the profile, in the .gcda files, and build every object again
    rm -f ./*.o cc1
    make cc1 CFLAGS="$flags -fprofile-use -fprofile-partial-training" \
        > make-profiled.log 2>&1
fi

for program in "$root"/tools/gcc_kmc/*; do
    ln -sf "$program" "$out/$(basename "$program")"
done
rm -f "$out/cc1" "$out/cc1.debug" "$out/cc1.optimized"
rm -rf "$out/source-optimized"
cp "$build/cc1" "$out/cc1"

# Check that, without the ORACLE_ variables, it makes the ROM
rm -rf "$out/check"
build_rom "$out/cc1" "$out/check"
sed "s|build/|$out/check/build/|" "$root/mariogolf64.sha1" | sha1sum -c --quiet
rm -rf "$out/check"
echo "built $out"
