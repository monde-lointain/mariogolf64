#!/bin/bash
# Set up a decomp-permuter work directory for one function, without the
# permuter's import.py (which on macOS needs Homebrew GCC's cpp and gmake).
#
# usage: tools/permuter-import.sh <segment> <function>
#   Run from the repo root, on a branch whose src/<segment>.c contains
#   <function> written in C (a near-match you want the permuter to improve).
#
# Creates permuter/<function>/ with:
#   base.c        src/<segment>.c preprocessed with -DPERMUTER (include_asm.h
#                 then drops every INCLUDE_ASM, so the file is self-contained)
#   target.o      the original function: a one-line INCLUDE_ASM stub built
#                 with the same KMC GCC + KMC as pipeline as the ROM build
#   compile.sh    that pipeline, as the permuter expects it (in.c -o out.o)
#   settings.toml func_name + compiler_type
#
# Then run: ./venv/bin/python tools/decomp-permuter/permuter.py permuter/<function> -j8
set -euo pipefail

seg=$1
func=$2
root=$(pwd)
cc="$root/tools/cc"
dir="permuter/$func"

[ -f "src/$seg.c" ] || { echo "src/$seg.c not found (are you on the right branch?)" >&2; exit 1; }
[ -f "asm/nonmatchings/$seg/$func.s" ] || { echo "asm/nonmatchings/$seg/$func.s not found (run make extract)" >&2; exit 1; }
if grep -q "INCLUDE_ASM(\"asm/nonmatchings/$seg\", $func)" "src/$seg.c"; then
    echo "$func is still INCLUDE_ASM in src/$seg.c; write a C version first" >&2
    exit 1
fi

# Same flags as the Makefile's CFLAGS for game code (C_PROFILE_CFLAGS default).
CFLAGS="-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 \
-I $root/include -I $root/include/libultra -I $root/include/libultra/internal \
-I $root/include/libkmc -I $root/include/libnusys -I $root/include/libmus -I $root/include/libnaudio \
-DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM"

mkdir -p "$dir"

cat > "$dir/compile.sh" <<EOF
#!/bin/bash
# Invoked by the permuter as: compile.sh input.c -o output.o
set -e
in="\$1"
out="\$3"
COMPILER_PATH=$cc $cc/gcc -S $CFLAGS -o "\$out.s" "\$in"
$cc/as -EB -mips2 -G 0 -I $root/include -o "\$out" "\$out.s"
rm -f "\$out.s"
EOF
chmod +x "$dir/compile.sh"

# base.c: the whole segment file, preprocessed so it stands alone.
COMPILER_PATH="$cc" "$cc/gcc" -E -P $CFLAGS -DPERMUTER "src/$seg.c" > "$dir/base.c"

# target.o: built from the original asm exactly as INCLUDE_ASM does in the ROM
# build. Compiled from the repo root so the .include paths resolve.
stub=$(mktemp -t permstub).c
printf '#include "common.h"\nINCLUDE_ASM("asm/nonmatchings/%s", %s);\n' "$seg" "$func" > "$stub"
COMPILER_PATH="$cc" "$cc/gcc" -S $CFLAGS -o "$stub.s" "$stub"
"$cc/as" -EB -mips2 -G 0 -I "$root/include" -o "$dir/target.o" "$stub.s"
rm -f "$stub" "$stub.s"

cat > "$dir/settings.toml" <<EOF
func_name = "$func"
compiler_type = "gcc"
EOF

# Sanity check: base.c must compile with compile.sh.
"$dir/compile.sh" "$dir/base.c" -o "$dir/base.o"
echo "set up $dir"
