#!/usr/bin/env python3
"""Set up a decomp-permuter directory for one function.

    permute_setup.py C_FILE TARGET_ASM FUNCTION [OUT_DIR]

C_FILE holds the current attempt at FUNCTION and TARGET_ASM its original
assembly (a single-function splat file). Writes base.c, target.o,
compile.sh and settings.toml into OUT_DIR (build/permuter/FUNCTION by
default), ready for:

    permuter.py OUT_DIR -j 16

decomp-permuter is expected at $PERMUTER_DIR (default ~/src/decomp-permuter).
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PERMUTER_DIR = os.environ.get("PERMUTER_DIR", os.path.expanduser("~/src/decomp-permuter"))

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fncompare  # noqa: E402

COMPILE_SH = """#!/bin/bash
# Invoked by the permuter as: compile.sh INPUT.c -o OUTPUT.o
COMPILER_PATH={kmc} exec {kmc}/gcc -c {flags} -x c "$@"
"""


def main():
    c_path, asm_path, function = sys.argv[1:4]
    out_dir = sys.argv[4] if len(sys.argv) > 4 else os.path.join(ROOT, "build", "permuter", function)
    os.makedirs(out_dir, exist_ok=True)

    # Preprocess without KMC's cpp quirks; INCLUDE_ASM expands to nothing
    # under PERMUTER
    base_path = os.path.join(out_dir, "base.c")
    preprocessed = subprocess.run(
        ["mips-linux-gnu-cpp", "-P", "-undef", "-nostdinc", "-DPERMUTER", *fncompare.INCLUDES,
         *fncompare.DEFINES, "-D__GNUC__=2", "-D__mips__",
         c_path],
        cwd=ROOT, check=True, capture_output=True, text=True).stdout
    with open(base_path, "w") as base_file:
        base_file.write(preprocessed)
    subprocess.run([sys.executable, os.path.join(PERMUTER_DIR, "strip_other_fns.py"), base_path, function],
                   check=True)

    target_s = os.path.join(out_dir, "target.s")
    header = ".set noat\n.set noreorder\n.set gp=64\n"
    with open(asm_path) as asm:
        body = asm.read()
    with open(target_s, "w") as target:
        target.write(header + '.include "macro.inc"\n' + body)
    subprocess.run(["mips-linux-gnu-as", "-march=vr4300", "-mabi=32", "-G0", "-EB", "-Iinclude",
                    "-o", os.path.join(out_dir, "target.o"), target_s], cwd=ROOT, check=True)

    compile_sh = os.path.join(out_dir, "compile.sh")
    with open(compile_sh, "w") as script:
        flags = " ".join(fncompare.CFLAGS)
        script.write(COMPILE_SH.format(kmc=os.path.join(ROOT, "tools", "gcc_kmc"), flags=flags))
    os.chmod(compile_sh, 0o755)

    with open(os.path.join(out_dir, "settings.toml"), "w") as settings:
        settings.write(f'func_name = "{function}"\ncompiler_type = "gcc"\n')
    print(out_dir)


if __name__ == "__main__":
    main()
