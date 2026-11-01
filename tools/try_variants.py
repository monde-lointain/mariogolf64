#!/usr/bin/env python3
"""Score variants of a C file, each made by one text replacement.

    try_variants.py ASM_FILE C_FILE FUNCTION VARIANTS_FILE

VARIANTS_FILE holds blocks separated by lines of "====". Each block is the
text to replace, a line of "----", and the replacement, which is made in
FUNCTION's definition alone. Prints the score of each variant, so that
several guesses cost one compile each.

The target assembly is read once, and the variants compile in parallel,
one process per CPU, since parsing the objects holds the interpreter lock.
Each compile cuts the file down to the function and its neighbour.
"""

import os
import re
import sys
import tempfile
from concurrent.futures import Future, ProcessPoolExecutor
from pathlib import Path

import fncompare

# Leaves room for a permuter running alongside
DEFAULT_JOB_COUNT = "8"
# How much of a compiler error to show for a variant
ERROR_TAIL_LENGTH = 300
# A function definition's closing brace, which alone starts its line
DEFINITION_END = "\n}\n"


def find_definition(source: str, function: str) -> tuple[int, int] | None:
    """Return where FUNCTION's definition starts and ends in SOURCE.

    Routines that share code, such as the two copies of a loop, can hold the
    same text, so a variant's replacement is looked for in its function only.
    """
    header = re.compile(
        rf"^[A-Za-z][\w* ]*[ *]{re.escape(function)}\([^;{{]*\) \{{\n", re.MULTILINE
    )
    match = header.search(source)
    if match is None:
        return None
    end = source.find(DEFINITION_END, match.end())
    if end < 0:
        return None
    return match.start(), end + len(DEFINITION_END)


def score_variant(
    source: str, c_dir: Path, optflags: list[str], target: list[int], function: str
) -> str:
    """Compile one variant and return its score, or why it has none."""
    with tempfile.NamedTemporaryFile(
        "w", suffix=".c", dir=c_dir, delete=False
    ) as variant:
        _ = variant.write(source)
    try:
        compiled, error = fncompare.compile_isolated(variant.name, {function}, optflags)
    finally:
        Path(variant.name).unlink()
    if compiled is None:
        return error.strip()[-ERROR_TAIL_LENGTH:]
    if function not in compiled:
        return f"{function} missing"
    return f"{function} {fncompare.describe(target, compiled[function])}"


def main() -> None:
    """Print the score of each variant, in order."""
    asm_path, c_path, function, variants_path = sys.argv[1:5]
    source = Path(c_path).read_text()
    blocks = Path(variants_path).read_text().split("\n====\n")
    target = fncompare.read_target_functions(asm_path)[function]
    definition = find_definition(source, function)
    if definition is None:
        sys.exit(f"{c_path} has no definition of {function}")
    start, end = definition
    before, body, after = source[:start], source[start:end], source[end:]
    c_dir = Path(c_path).resolve().parent
    # The variants have their own names, so they take the flags of the file
    # they vary
    optflags = fncompare.get_optflags(c_path)

    job_count = int(os.environ.get("VARIANT_JOBS", DEFAULT_JOB_COUNT))
    with ProcessPoolExecutor(max_workers=job_count) as pool:
        results: list[Future[str] | None] = []
        for block in blocks:
            old, new = block.split("\n----\n")
            if old not in body:
                results.append(None)
            else:
                results.append(
                    pool.submit(
                        score_variant,
                        before + body.replace(old, new, 1) + after,
                        c_dir,
                        optflags,
                        target,
                        function,
                    )
                )
        for number, result in enumerate(results):
            print(f"{number}: {result.result() if result else 'text not found'}")


if __name__ == "__main__":
    main()
