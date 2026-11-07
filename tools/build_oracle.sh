#!/bin/sh
# Build the oracle's compiler directory, build/oracle, for tools/oracle.py.
#
#     tools/build_oracle.sh [KMC_GCC_SOURCE]
#
# Builds cc1 twice from the KMC GCC 2.7.2 source (by default
# ~/src/kmc-gcc-src), with tools/oracle_force.patch, which lets cc1 override
# the allocation of the pseudos ORACLE_FORCE names as reload starts. The
# debug build, without optimization and with debug information, lets gdb
# read the register allocators' state. The optimized build, with -O3, LTO and
# the profile of a build of the ROM, as the toolchain's cc1 is built, makes
# the forced compiles, which take a twentieth of the debug build's time under
# gdb. Both are put beside the toolchain's other programs. Its cc1 runs the
# debug build under tools/oracle_gdb.py when ORACLE_TRACE is set, and the
# optimized build otherwise. Without ORACLE_FORCE each gives the same code as
# the toolchain's cc1: the ROM builds with them unchanged.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
source=${1:-$HOME/src/kmc-gcc-src}
out=$root/build/oracle
cores=$(nproc)

# configure_source DIR: copy the patched, configured source into DIR
configure_source() {
    rm -rf "$1"
    cp -r "$source" "$1"
    cd "$1"
    patch -p1 < "$root/tools/oracle_force.patch" > /dev/null
    ./configure --target=mips-mips-gnu --prefix=/opt/cross --with-gnu-as \
        --disable-gprof --disable-gdb --disable-werror \
        --host=i386-pc-linux --build=i386-pc-linux > configure.log
    # The parser and keyword table are prebuilt; keep make from remaking them
    touch c-parse.c c-gperf.h
}

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

debug=$out/source
if [ ! -x "$debug/cc1" ]; then
    configure_source "$debug"
    make cc1 CFLAGS="-static -std=gnu89 -m32 -O0 -g3 -w" > make.log 2>&1
fi

optimized=$out/source-optimized
if [ ! -x "$optimized/cc1" ]; then
    configure_source "$optimized"
    flags="-static -std=gnu89 -m32 -O3 -flto=auto -w"
    make cc1 CFLAGS="$flags -fprofile-generate" > make.log 2>&1
    rm -rf "$out/training"
    build_rom "$optimized/cc1" "$out/training"
    rm -rf "$out/training"
    # Keep the profile, in the .gcda files, and build every object again
    rm -f ./*.o cc1
    make cc1 CFLAGS="$flags -fprofile-use -fprofile-partial-training" \
        > make-profiled.log 2>&1
fi

for program in "$root"/tools/gcc_kmc/*; do
    ln -sf "$program" "$out/$(basename "$program")"
done
rm -f "$out/cc1"
cp "$debug/cc1" "$out/cc1.debug"
cp "$optimized/cc1" "$out/cc1.optimized"
cat > "$out/cc1" <<SCRIPT
#!/bin/sh
if [ -n "\${ORACLE_TRACE:-}" ]; then
    exec gdb -batch -nx -q -x "$root/tools/oracle_gdb.py" --args "$out/cc1.debug" "\$@"
fi
exec "$out/cc1.optimized" "\$@"
SCRIPT
chmod +x "$out/cc1"

# Check that, without ORACLE_FORCE, the optimized build makes the ROM
rm -rf "$out/check"
build_rom "$out/cc1.optimized" "$out/check"
sed "s|build/|$out/check/build/|" "$root/mariogolf64.sha1" | sha1sum -c --quiet
rm -rf "$out/check"
echo "built $out"
