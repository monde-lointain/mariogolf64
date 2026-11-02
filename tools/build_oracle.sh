#!/bin/sh
# Build the oracle's compiler directory, build/oracle, for tools/oracle.py.
#
#     tools/build_oracle.sh [KMC_GCC_SOURCE]
#
# Builds cc1 from the KMC GCC 2.7.2 source (by default ~/src/kmc-gcc-src)
# without optimization and with debug information, so that gdb can read and
# set the register allocators' state, and puts it beside the toolchain's
# other programs. Its cc1 runs the debug build under tools/oracle_gdb.py when
# ORACLE_FUNCTION is set, and the toolchain's optimized cc1 otherwise, since
# gdb needs the debug build and nothing else does. The debug build gives the
# same code as the toolchain's cc1: the ROM builds with it unchanged.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
source=${1:-$HOME/src/kmc-gcc-src}
out=$root/build/oracle
work=$out/source

mkdir -p "$out"
if [ ! -x "$work/cc1" ]; then
    rm -rf "$work"
    cp -r "$source" "$work"
    cd "$work"
    ./configure --target=mips-mips-gnu --prefix=/opt/cross --with-gnu-as \
        --disable-gprof --disable-gdb --disable-werror \
        --host=i386-pc-linux --build=i386-pc-linux > configure.log
    # The parser and keyword table are prebuilt; keep make from remaking them
    touch c-parse.c c-gperf.h
    make cc1 CFLAGS="-static -std=gnu89 -m32 -O0 -g3 -w" > make.log 2>&1
fi

for program in "$root"/tools/gcc_kmc/*; do
    ln -sf "$program" "$out/$(basename "$program")"
done
rm -f "$out/cc1"
cp "$work/cc1" "$out/cc1.debug"
cat > "$out/cc1" <<SCRIPT
#!/bin/sh
if [ -n "\${ORACLE_FUNCTION:-}" ]; then
    exec gdb -batch -nx -q -x "$root/tools/oracle_gdb.py" --args "$out/cc1.debug" "\$@"
fi
exec "$root/tools/gcc_kmc/cc1" "\$@"
SCRIPT
chmod +x "$out/cc1"
echo "built $out"
