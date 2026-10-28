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
import re
import subprocess
import sys
from pathlib import Path

import fncompare

ROOT = fncompare.ROOT
PERMUTER_DIR = Path(
    os.environ.get("PERMUTER_DIR", Path("~/src/decomp-permuter").expanduser())
)
OUT_DIR_ARGUMENT_COUNT = 5
# compile.sh has to be executable for the permuter to run it
SCRIPT_MODE = 0o755

COMPILE_SH = """#!/bin/bash
# Invoked by the permuter as: compile.sh INPUT.c -o OUTPUT.o
COMPILER_PATH={kmc} exec {kmc}/gcc -c {flags} -x c "$@"
"""


def write_base(c_path: str, function: str, base_path: Path) -> None:
    """Write C_PATH preprocessed, with every function but FUNCTION stripped."""
    # Preprocess without KMC's cpp quirks; INCLUDE_ASM expands to nothing
    # under PERMUTER
    preprocessed = subprocess.run(
        [
            "mips-linux-gnu-cpp",
            "-P",
            "-undef",
            "-nostdinc",
            "-DPERMUTER",
            *fncompare.INCLUDES,
            *fncompare.DEFINES,
            "-D__GNUC__=2",
            "-D__mips__",
            c_path,
        ],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    _ = base_path.write_text(preprocessed)
    _ = subprocess.run(
        [
            sys.executable,
            str(PERMUTER_DIR / "strip_other_fns.py"),
            str(base_path),
            function,
        ],
        check=True,
    )
    # Nothing calls the function once the others are stripped, so a static
    # one would not be emitted at all
    base = re.sub(
        rf"^static\s+(.*\b{re.escape(function)}\s*\()",
        r"\1",
        base_path.read_text(),
        flags=re.MULTILINE,
    )
    _ = base_path.write_text(base)


def write_target(asm_path: str, out_dir: Path) -> None:
    """Assemble the original function into OUT_DIR/target.o."""
    target_s = out_dir / "target.s"
    header = ".set noat\n.set noreorder\n.set gp=64\n"
    body = Path(asm_path).read_text()
    # Splat writes some low parts as %lo(SYMBOL + (OFFSET & 0xFFFF)), which
    # leaves the assembler unable to pair them with their %hi parts
    body = re.sub(r"(%lo\([\w.]+ \+ )\((0x[0-9A-Fa-f]+) & 0xFFFF\)\)", r"\1\2)", body)
    _ = target_s.write_text(header + '.include "macro.inc"\n' + body)
    _ = subprocess.run(
        [
            "mips-linux-gnu-as",
            "-march=vr4300",
            "-mabi=32",
            "-G0",
            "-EB",
            "-Iinclude",
            "-o",
            str(out_dir / "target.o"),
            str(target_s),
        ],
        cwd=ROOT,
        check=True,
    )


def main() -> None:
    """Fill the permuter directory for the function named."""
    c_path, asm_path, function = sys.argv[1:4]
    out_dir = (
        Path(sys.argv[4])
        if len(sys.argv) >= OUT_DIR_ARGUMENT_COUNT
        else ROOT / "build" / "permuter" / function
    )
    out_dir.mkdir(parents=True, exist_ok=True)

    write_base(c_path, function, out_dir / "base.c")
    write_target(asm_path, out_dir)

    compile_sh = out_dir / "compile.sh"
    flags = " ".join(fncompare.CFLAGS + fncompare.get_optflags(c_path))
    _ = compile_sh.write_text(COMPILE_SH.format(kmc=fncompare.KMC_DIR, flags=flags))
    compile_sh.chmod(SCRIPT_MODE)

    settings = f'func_name = "{function}"\ncompiler_type = "gcc"\n'
    _ = (out_dir / "settings.toml").write_text(settings)
    print(out_dir)


if __name__ == "__main__":
    main()
