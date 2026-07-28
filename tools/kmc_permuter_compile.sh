#!/usr/bin/env bash
# Compile one permuter candidate with the KMC toolchain, assembling from a FILE rather than a pipe.
#
# Why a file (S300): KMC binutils-2.6 `as` inserts two hazard nops when it reads a given input from a
# file that it does NOT insert when it reads the identical input from a pipe. The piped form the
# permuter settings used before therefore assembled a 2-instruction-short object, and an imported
# base scored 508/510 on that artifact alone -- the run spent its whole budget searching for a
# codegen difference that did not exist. `tools/decomp-permuter/import.py finalize_compile_command`
# rewrites a settings command into `<cmd> "$INPUT" -o "$OUTPUT"`, so this wrapper takes that shape.
#
# Usage: kmc_permuter_compile.sh <input.c> -o <output.o>
set -eu

INPUT="$1"
OUTPUT="$3"

COMPILER_PATH=tools/cc tools/cc/gcc -S -nostdinc -G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 \
    -I include -I include/libultra -I include/libultra/internal -I include/libkmc \
    -I include/libnusys -I include/libmus -I include/libnualstl -I include/libnaudio \
    -DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM -DNONMATCHING -DF3DEX_GBI_2 \
    -o "$OUTPUT.s" "$INPUT"
tools/cc/as -EB -mips3 -G 0 -I include -o "$OUTPUT" "$OUTPUT.s"
rm -f "$OUTPUT.s"
