#!/usr/bin/env python3
"""Reduce a decomp-permuter result to the changes its score needs.

    permute_minimize.py PERMUTER_DIR OUTPUT_DIR

PERMUTER_DIR is the directory permute_setup.py made, holding base.c and
target.o, and OUTPUT_DIR one of its output-* directories. The permuter
prints its candidates from a parsed tree, so base.c is printed the same way
first, into printed_base.c; otherwise every line differs. Each differing
hunk is then put back as base.c had it, one at a time, and stays put back
when the compiled function differs from the original in no more words
than before. Prints the hunks that remain, and writes the reduced source
to minimized.c in OUTPUT_DIR.

decomp-permuter is expected where permute_setup.py expects it.
"""

import difflib
import subprocess
import sys
import tempfile
from pathlib import Path

import permute_setup

import fncompare

ARGUMENT_COUNT = 3
Hunk = tuple[int, int, int, int]


# Prints standard input as the permuter prints its candidates
PRINTER = """
import sys
from src import ast_util
sys.stdout.write(ast_util.to_c(ast_util.parse_c(sys.stdin.read())))
"""


def print_like_permuter(source: str) -> str:
    """Return SOURCE as the permuter prints its candidates."""
    return subprocess.run(
        [sys.executable, "-c", PRINTER],
        cwd=permute_setup.PERMUTER_DIR,
        input=source,
        capture_output=True,
        text=True,
        check=True,
    ).stdout


def read_printed_base(permuter_dir: Path) -> str:
    """Return base.c as the permuter prints it.

    The result is kept in printed_base.c, beside base.c, and reused while it
    is newer, since printing takes longer than the trials.
    """
    base_path = permuter_dir / "base.c"
    printed_path = permuter_dir / "printed_base.c"
    is_current = (
        printed_path.exists()
        and printed_path.stat().st_mtime >= base_path.stat().st_mtime
    )
    if not is_current:
        _ = printed_path.write_text(print_like_permuter(base_path.read_text()))
    return printed_path.read_text()


def splice(
    base: list[str], output: list[str], hunks: list[Hunk], kept: set[int]
) -> str:
    """Return BASE with the KEPT hunks taken from OUTPUT."""
    lines: list[str] = []
    position = 0
    for index, (base_start, base_end, output_start, output_end) in enumerate(hunks):
        lines += base[position:base_start]
        if index in kept:
            lines += output[output_start:output_end]
        else:
            lines += base[base_start:base_end]
        position = base_end
    lines += base[position:]
    return "".join(lines)


def count_differences(
    source: str, target: fncompare.Routine, permuter_dir: Path, work: Path
) -> int:
    """Return how many words of SOURCE's function differ from TARGET.

    SOURCE is compiled with the permuter's compile.sh, as the permuter does.
    """
    c_path, obj_path = work / "trial.c", work / "trial.o"
    _ = c_path.write_text(source)
    result = subprocess.run(
        [str(permuter_dir.resolve() / "compile.sh"), str(c_path), "-o", str(obj_path)],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        return sys.maxsize
    compiled = fncompare.read_object_file(obj_path).get(permuter_dir.resolve().name)
    if compiled is None:
        return sys.maxsize
    return fncompare.count_differences(target, compiled)


def main() -> None:
    """Reduce the output named on the command line."""
    if len(sys.argv) != ARGUMENT_COUNT:
        sys.exit(__doc__)
    permuter_dir, output_dir = Path(sys.argv[1]), Path(sys.argv[2])
    function = permuter_dir.resolve().name
    target = fncompare.read_object_file(permuter_dir / "target.o")[function]
    base = read_printed_base(permuter_dir)
    base_lines = base.splitlines(keepends=True)
    output_lines = (output_dir / "source.c").read_text().splitlines(keepends=True)
    matcher = difflib.SequenceMatcher(None, base_lines, output_lines, autojunk=False)
    hunks = [op[1:] for op in matcher.get_opcodes() if op[0] != "equal"]

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        kept = set(range(len(hunks)))
        best = count_differences(
            splice(base_lines, output_lines, hunks, kept), target, permuter_dir, work
        )
        for index in range(len(hunks)):
            trial = splice(base_lines, output_lines, hunks, kept - {index})
            if count_differences(trial, target, permuter_dir, work) <= best:
                kept.discard(index)

    reduced = splice(base_lines, output_lines, hunks, kept)
    _ = (output_dir / "minimized.c").write_text(reduced)
    print(f"{function}: {best} words differ")
    for index in sorted(kept):
        base_start, base_end, output_start, output_end = hunks[index]
        print(f"--- line {base_start + 1}")
        _ = sys.stdout.writelines(
            "- " + line for line in base_lines[base_start:base_end]
        )
        _ = sys.stdout.writelines(
            "+ " + line for line in output_lines[output_start:output_end]
        )


if __name__ == "__main__":
    main()
