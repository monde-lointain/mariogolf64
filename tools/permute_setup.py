#!/usr/bin/env python3
"""Set up a decomp-permuter directory for one function.

    permute_setup.py C_FILE TARGET_ASM FUNCTION [OUT_DIR]

C_FILE holds the current attempt at FUNCTION and TARGET_ASM its original
assembly (a single-function splat file). Writes base.c, target.o,
compile.sh, objdump.sh and settings.toml into OUT_DIR (build/permuter/FUNCTION by
default), ready for:

    permuter.py OUT_DIR -j 16

decomp-permuter is expected at $PERMUTER_DIR (default
~/projects/tools/decomp-permuter-nested), on its nested-functions branch:
pycparser rejects nested functions, and the display-list macros that base.c
keeps as calls need the branch's importer, which keeps braced macro bodies
and attributes. Any other branch is refused, since its importer writes a
base.c that compiles to different code.
"""

import contextlib
import importlib.util
import os
import re
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path
from types import ModuleType
from typing import Protocol, cast

import fncompare

ROOT = fncompare.ROOT
PERMUTER_DIR = Path(
    os.environ.get(
        "PERMUTER_DIR", Path("~/projects/tools/decomp-permuter-nested").expanduser()
    )
)
PERMUTER_BRANCH = "nested-functions"
OUT_DIR_ARGUMENT_COUNT = 5
# compile.sh has to be executable for the permuter to run it
SCRIPT_MODE = 0o755

# KMC's as writes a scratch file into the working directory and waits for the
# filesystem journal to commit it. On disk, that serialises the permuter's
# workers and leaves the CPUs idle, so compile.sh works in the temp directory
# and takes absolute paths.
COMPILE_SH = """#!/bin/bash
# Invoked by the permuter as: compile.sh INPUT.c -o OUTPUT.o
cd "${{TMPDIR:-/tmp}}" || exit
COMPILER_PATH={kmc} exec {kmc}/gcc -c {flags} -x c "$@"
"""

# Disassembles the nested functions the function holds, which KMC GCC names
# NAME.N and so are looked up, and then the function, in a fixed order. The
# permuter runs this for every candidate, and the lookup's four extra programs
# took longer than objdump itself.
OBJDUMP_SH = """#!/bin/bash
# Invoked by the permuter as: objdump.sh OBJECT.o
for name in {nested}; do
  symbol=$(mips-linux-gnu-nm "$1" | grep -m1 -E " $name(\\.[0-9]+)?$" | cut -d" " -f3)
  mips-linux-gnu-objdump -drz -m mips:4300 --disassemble=$symbol "$1"
done
exec mips-linux-gnu-objdump -drz -m mips:4300 --disassemble={function} "$1"
"""

# A function defined inside another, indented as the formatter leaves it: its
# name follows a return type and precedes a parameter list holding no ";" or
# braces
NESTED_DEFINITION = re.compile(
    r"^[ \t]+[A-Za-z_][\w \t*]*?[ \t*](\w+)\s*\([^;{}]*\)\s*\{", re.MULTILINE
)
# The permuter's settings beyond the function. natural_only leaves out the
# passes and forms that this project rejects as fakes: no-op blocks, dead
# stores, identities, volatile locals and the like. Of the weights, each pass
# was applied alone to 15 routines, 450 times each. Statement reordering gave
# the most gain per trial in a form this project accepts; the type and cast
# passes mostly gained by truncating, which changes behaviour, and a kept
# candidate seeds the next ones.
NATURAL_SETTINGS = """natural_only = true

[weight_overrides]
perm_reorder_stmts = 30
perm_cast_simple = 2
perm_randomize_internal_type = 2
"""
KEYWORDS = frozenset({"if", "for", "while", "switch", "return", "sizeof"})

# The importer's preprocess_c_with_macros, given the cpp command, its working
# directory, and the names of the macros to keep with the function naming the
# type each is declared as. Returns the source and the macros kept.
PreprocessWithMacros = Callable[
    [list[str], str, tuple[re.Pattern[str], Callable[[str], str]]],
    tuple[str, list[str]],
]

# The importer's prune_source, given the source, whether to prune it, and the
# function to keep. Returns the source and a compilable copy of it.
PruneSource = Callable[[str, bool, str], tuple[str, str | None]]


class Scorer(Protocol):
    """The permuter's scorer: score gives an object's score and a hash."""

    score: Callable[[str], tuple[int, str]]


# The display-list macros, kept as calls so that the permuter changes their
# arguments and not their expansions. Its changes within an expansion only
# split the command words into temporaries, which no readable source holds.
# The constants whose names the macros paste together, G_ and FR_ ones, are
# kept too, as the definitions of the others go with the expansion. So are
# _SHIFTL and _SHIFTR and their old name _SHIFT, which "siz##_SHIFT" holds.
DISPLAY_LIST_MACROS = re.compile(r"^(?:g[A-Za-z]\w*|G_\w+|FR_\w+|_SHIFT[LR]?)$")


# A one-parameter macro whose body is a GNU statement expression, as cpp -dM
# prints it: its name, parameter, the type of the body's first declaration, and
# the body
STATEMENT_EXPRESSION_MACRO = re.compile(
    r"^#define (\w+)\((\w+)\) (\(\{ (\w+) .*\}\))$", re.MULTILINE
)


def get_cpp_command(c_path: str, *options: str) -> list[str]:
    """Return the command that preprocesses C_PATH without KMC's cpp quirks.

    INCLUDE_ASM expands to nothing under PERMUTER.
    """
    return [
        "mips-linux-gnu-cpp",
        "-undef",
        "-nostdinc",
        "-DPERMUTER",
        *fncompare.INCLUDES,
        *fncompare.DEFINES,
        "-D__GNUC__=2",
        "-D__mips__",
        *options,
        c_path,
    ]


def preprocess(c_path: str, *options: str) -> str:
    """Return C_PATH preprocessed, with every macro expanded."""
    return subprocess.run(
        get_cpp_command(c_path, "-P", *options),
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout


def load_importer() -> ModuleType:
    """Load the permuter's importer, putting the permuter's modules in reach."""
    # The importer is a script named import.py, which no import statement names
    sys.path.insert(0, str(PERMUTER_DIR))
    importer_path = PERMUTER_DIR / "import.py"
    spec = importlib.util.spec_from_file_location("permuter_import", importer_path)
    if spec is None or spec.loader is None:
        message = f"{importer_path} is no Python module"
        raise ImportError(message)
    importer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(importer)
    return importer


def preprocess_keeping_display_list_macros(c_path: str) -> str:
    """Return C_PATH preprocessed, with the display-list macros kept as calls.

    The permuter's importer does this, declaring each macro for its parser and
    putting the definitions back in a latedefine block before compiling.
    """
    importer = load_importer()
    preprocess_with_macros = cast(
        PreprocessWithMacros,  # noqa: TC006 - unquoted, so vulture sees it used
        importer.preprocess_c_with_macros,
    )

    def get_macro_type(name: str) -> str:
        return "int" if name.startswith("G_") else "void"

    # It lists the macros it keeps, which stdout leaves for the directory made
    with contextlib.redirect_stdout(sys.stderr):
        source, _macros = preprocess_with_macros(
            get_cpp_command(c_path), str(ROOT), (DISPLAY_LIST_MACROS, get_macro_type)
        )
    return source


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
    source, late_defines = fold_statement_expressions(
        c_path, preprocess_keeping_display_list_macros(c_path)
    )
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


def score_base(source: str, out_dir: Path, scorers: list[Scorer]) -> list[int] | None:
    """Score SOURCE as base.c in OUT_DIR, or give None if it doesn't compile."""
    ast_util = importlib.import_module("src.ast_util")
    process_pragmas = cast("Callable[[str], str]", ast_util.process_pragmas)
    source_path = out_dir.resolve() / "prune_check.c"
    object_path = out_dir.resolve() / "prune_check.o"
    _ = source_path.write_text(process_pragmas(source))
    compiled = subprocess.run(
        [str(out_dir / "compile.sh"), str(source_path), "-o", str(object_path)],
        capture_output=True,
        check=False,
    )
    source_path.unlink()
    if compiled.returncode != 0:
        return None
    scores = [scorer.score(str(object_path))[0] for scorer in scorers]
    object_path.unlink()
    return scores


def prune_base(out_dir: Path, function: str) -> None:
    """Drop the declarations FUNCTION doesn't use from OUT_DIR/base.c.

    The compiler takes 10-12 ms per candidate to read them. The pruned file
    puts static data at other offsets in its sections, which the target names
    by symbol instead, so it is kept only when it scores the same, with and
    without stack differences.
    """
    importer = load_importer()
    prune_source = cast(
        PruneSource,  # noqa: TC006 - unquoted, so vulture sees it used
        importer.prune_source,
    )
    new_scorer = cast(
        Callable[..., Scorer],  # noqa: TC006 - as above
        importlib.import_module("src.scorer").Scorer,
    )
    scorers = [
        new_scorer(
            str(out_dir / "target.o"),
            stack_differences=stack_differences,
            algorithm="levenshtein",
            debug_mode=False,
            ign_branch_targets=False,
            objdump_command=str(out_dir / "objdump.sh"),
        )
        for stack_differences in (False, True)
    ]
    base_path = out_dir / "base.c"
    source = base_path.read_text()
    should_prune = True
    with contextlib.redirect_stdout(sys.stderr):
        pruned, _compilable = prune_source(source, should_prune, function)
    if pruned == source:
        return
    pruned_scores = score_base(pruned, out_dir, scorers)
    if pruned_scores is None or pruned_scores != score_base(source, out_dir, scorers):
        print(f"{function}: pruning changes the score; base.c is kept whole")
        return
    _ = base_path.write_text(pruned)


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


def check_permuter_branch() -> None:
    """Exit unless PERMUTER_DIR has the nested-functions branch checked out."""
    branch = subprocess.run(
        ["git", "-C", str(PERMUTER_DIR), "branch", "--show-current"],
        check=False,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if branch != PERMUTER_BRANCH:
        found = branch or "(none)"
        sys.exit(f"{PERMUTER_DIR} is on {found}, not {PERMUTER_BRANCH}")


def main() -> None:
    """Fill the permuter directory for the function named."""
    c_path, asm_path, function = sys.argv[1:4]
    check_permuter_branch()
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
    _ = objdump_sh.write_text(
        OBJDUMP_SH.format(nested=" ".join(nested), function=function)
    )
    objdump_sh.chmod(SCRIPT_MODE)
    prune_base(out_dir, function)
    objdump = str(objdump_sh.resolve())
    settings = f"""func_name = "{function}"
compiler_type = "gcc"
objdump_command = "{objdump}"
{NATURAL_SETTINGS}"""
    _ = (out_dir / "settings.toml").write_text(settings)
    print(out_dir)


if __name__ == "__main__":
    main()
