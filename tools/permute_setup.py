#!/usr/bin/env python3
"""Set up a decomp-permuter directory for one function.

    permute_setup.py C_FILE TARGET_ASM FUNCTION [OUT_DIR]

C_FILE holds the current attempt at FUNCTION and TARGET_ASM its original
assembly (a single-function splat file). Writes base.c, target.o,
compile.sh, objdump.sh and settings.toml into OUT_DIR (build/permuter/FUNCTION by
default), ready for:

    permuter.py OUT_DIR -j 16

decomp-permuter is expected at $PERMUTER_DIR (default ~/src/decomp-permuter).
Functions that hold a nested function need its nested-functions branch, as
pycparser rejects them.
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

# Disassembles the function and the nested functions it holds, which KMC GCC
# names NAME.N, in a fixed order
OBJDUMP_SH = """#!/bin/bash
# Invoked by the permuter as: objdump.sh OBJECT.o
for name in {names}; do
  symbol=$(mips-linux-gnu-nm "$1" | grep -m1 -E " $name(\\.[0-9]+)?$" | cut -d" " -f3)
  mips-linux-gnu-objdump -drz -m mips:4300 --disassemble=$symbol "$1"
done
"""

# A function defined inside another, indented as the formatter leaves it: its
# name follows a return type and precedes a parameter list holding no ";" or
# braces
NESTED_DEFINITION = re.compile(
    r"^[ \t]+[A-Za-z_][\w \t*]*?[ \t*](\w+)\s*\([^;{}]*\)\s*\{", re.MULTILINE
)
KEYWORDS = frozenset({"if", "for", "while", "switch", "return", "sizeof"})


# A one-parameter macro whose body is a GNU statement expression, as cpp -dM
# prints it: its name, parameter, the type of the body's first declaration, and
# the body
STATEMENT_EXPRESSION_MACRO = re.compile(
    r"^#define (\w+)\((\w+)\) (\(\{ (\w+) .*\}\))$", re.MULTILINE
)


def preprocess(c_path: str, *options: str) -> str:
    """Return C_PATH preprocessed without KMC's cpp quirks.

    INCLUDE_ASM expands to nothing under PERMUTER.
    """
    return subprocess.run(
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
            *options,
            c_path,
        ],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout


def find_closing_parenthesis(text: str, start: int) -> int:
    """Return the index of the ")" that closes the "(" before START."""
    depth = 1
    index = start
    while depth > 0:
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
        index += 1
    return index - 1


def fold_statement_expressions(c_path: str, source: str) -> tuple[str, str]:
    """Turn the expansions of statement-expression macros back into calls.

    pycparser can't read GNU statement expressions. The permuter keeps each
    such macro as a call that its parser sees declared as a function, and puts
    the definition back from a latedefine block before compiling. Returns the
    source and that block, which goes after the types it names.
    """
    late_defines: list[str] = []
    for macro in STATEMENT_EXPRESSION_MACRO.finditer(preprocess(c_path, "-dM")):
        name, parameter, body, result_type = macro.groups()
        prefix, separator, suffix = body.partition(f"({parameter})")
        if not separator or parameter in suffix:
            continue
        prefix += "("
        suffix = ")" + suffix
        # From the last expansion back, so that nested ones fold innermost first
        start = source.rfind(prefix)
        if start < 0:
            continue
        while start >= 0:
            argument_start = start + len(prefix)
            argument_end = find_closing_parenthesis(source, argument_start)
            if source.startswith(suffix, argument_end):
                argument = source[argument_start:argument_end]
                end = argument_end + len(suffix)
                source = f"{source[:start]}{name}({argument}){source[end:]}"
            start = source.rfind(prefix, 0, start)
        late_defines += [
            f"#pragma _permuter define {name}({parameter}) {body}",
            f"{result_type} {name}();",
        ]
    if not late_defines:
        return source, ""
    block = "\n".join(
        [
            "#pragma _permuter latedefine start",
            *late_defines,
            "#pragma _permuter latedefine end",
            "",
        ]
    )
    return source, block


def write_base(c_path: str, function: str, base_path: Path) -> None:
    """Write C_PATH preprocessed, with every function but FUNCTION stripped."""
    source, late_defines = fold_statement_expressions(c_path, preprocess(c_path))
    _ = base_path.write_text(source)
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
    # The definition is the last line naming the function, the others being
    # its prototypes
    definition = list(
        re.finditer(rf"^.*\b{re.escape(function)}\s*\(", base, re.MULTILINE)
    )[-1]
    base = base[: definition.start()] + late_defines + base[definition.start() :]
    _ = base_path.write_text(base)


def find_nested_functions(base_path: Path, asm_path: str) -> list[str]:
    """List the nested functions in BASE_PATH that ASM_PATH's directory holds."""
    names = [
        match.group(1)
        for match in NESTED_DEFINITION.finditer(base_path.read_text())
        if match.group(1) not in KEYWORDS
    ]
    # Inline ones that were never emitted on their own have no assembly
    return [name for name in names if (Path(asm_path).parent / f"{name}.s").exists()]


def write_target(asm_paths: list[str], out_dir: Path) -> None:
    """Assemble the original functions into OUT_DIR/target.o."""
    target_s = out_dir / "target.s"
    header = ".set noat\n.set noreorder\n.set gp=64\n"
    body = "".join(Path(path).read_text() for path in asm_paths)
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
    nested = find_nested_functions(out_dir / "base.c", asm_path)
    asm_dir = Path(asm_path).parent
    write_target([str(asm_dir / f"{name}.s") for name in nested] + [asm_path], out_dir)

    compile_sh = out_dir / "compile.sh"
    flags = " ".join(fncompare.CFLAGS + fncompare.get_optflags(c_path))
    _ = compile_sh.write_text(COMPILE_SH.format(kmc=fncompare.KMC_DIR, flags=flags))
    compile_sh.chmod(SCRIPT_MODE)

    # The score covers the function and its nested functions alone, leaving
    # out any inline helper emitted from base.c
    objdump_sh = out_dir / "objdump.sh"
    _ = objdump_sh.write_text(OBJDUMP_SH.format(names=" ".join([*nested, function])))
    objdump_sh.chmod(SCRIPT_MODE)
    objdump = str(objdump_sh.resolve())
    settings = (
        f'func_name = "{function}"\ncompiler_type = "gcc"\n'
        f'objdump_command = "{objdump}"\n'
    )
    _ = (out_dir / "settings.toml").write_text(settings)
    print(out_dir)


if __name__ == "__main__":
    main()
