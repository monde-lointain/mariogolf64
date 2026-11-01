#!/usr/bin/env python3
"""Score variants of a C file, each made by one text replacement.

    try_variants.py [--apply NUMBER] ASM_FILE C_FILE FUNCTION VARIANTS_FILE
    try_variants.py --batch JOBS_FILE

VARIANTS_FILE holds blocks separated by lines of "====". Each block is the
text to replace, a line of "----", and the replacement, which is made in
FUNCTION's definition alone. Prints the score of each variant, so that
several guesses cost one compile each. With --apply, writes variant NUMBER
into C_FILE instead, made the same way as the one that was scored.

With --batch, each line of JOBS_FILE names an ASM_FILE, C_FILE, FUNCTION
and VARIANTS_FILE, and each score is printed after its function's name.
A sweep holds a few variants for each of many functions, so scoring them
in one pool keeps every process busy, where a run per function waits on
its slowest variant and starts the interpreter again.

Each target assembly is read once, and the variants compile in parallel,
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
    source: str,
    c_dir: Path,
    optflags: list[str],
    target: fncompare.Routine,
    function: str,
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


def read_variants(c_path: str, function: str, variants_path: str) -> list[str | None]:
    """Return C_PATH's text as each variant makes it.

    A variant whose text to replace is absent gives None. Raises
    LookupError when C_PATH doesn't define FUNCTION.
    """
    source = Path(c_path).read_text()
    definition = find_definition(source, function)
    if definition is None:
        msg = f"{c_path} has no definition of {function}"
        raise LookupError(msg)
    start, end = definition
    before, body, after = source[:start], source[start:end], source[end:]
    # The file's own last newline ends the last block, not its replacement
    text = Path(variants_path).read_text().removesuffix("\n")
    variants: list[str | None] = []
    for block in text.split("\n====\n"):
        old, new = block.split("\n----\n")
        if old in body:
            variants.append(before + body.replace(old, new, 1) + after)
        else:
            variants.append(None)
    return variants


def read_target(asm_path: str, function: str) -> fncompare.Routine:
    """Return FUNCTION's routine in the target assembly at ASM_PATH."""
    return fncompare.read_target_functions(asm_path)[function]


def score_jobs(jobs: list[list[str]], *, prefix_names: bool) -> None:
    """Print the score of each variant of each job, in order.

    A job is an ASM_FILE, C_FILE, FUNCTION and VARIANTS_FILE.
    """
    found: list[list[str]] = []
    variants: list[list[str | None]] = []
    for job in jobs:
        _, c_path, function, variants_path = job
        try:
            variants.append(read_variants(c_path, function, variants_path))
        except LookupError as error:
            print(error, file=sys.stderr)
            continue
        found.append(job)
    if not found:
        sys.exit(1)
    jobs = found
    job_count = int(os.environ.get("VARIANT_JOBS", DEFAULT_JOB_COUNT))
    with ProcessPoolExecutor(max_workers=job_count) as pool:
        targets = list(
            pool.map(read_target, [job[0] for job in jobs], [job[2] for job in jobs])
        )
        results: list[list[Future[str] | None]] = []
        for (_, c_path, function, _), target, sources in zip(
            jobs, targets, variants, strict=True
        ):
            c_dir = Path(c_path).resolve().parent
            # The variants have their own names, so they take the flags of
            # the file they vary
            optflags = fncompare.get_optflags(c_path)
            results.append(
                [
                    pool.submit(
                        score_variant, source, c_dir, optflags, target, function
                    )
                    if source is not None
                    else None
                    for source in sources
                ]
            )
        for (_, _, function, _), scores in zip(jobs, results, strict=True):
            name = f"{function} " if prefix_names else ""
            for number, result in enumerate(scores):
                score = result.result() if result else "text not found"
                print(f"{name}{number}: {score}")


def main() -> None:
    """Print the score of each variant, in order, or apply one."""
    arguments = sys.argv[1:]
    if arguments[0] == "--batch":
        lines = Path(arguments[1]).read_text().splitlines()
        score_jobs([line.split() for line in lines if line.strip()], prefix_names=True)
        return
    apply_number = None
    if arguments[0] == "--apply":
        apply_number = int(arguments[1])
        arguments = arguments[2:]
    if apply_number is None:
        score_jobs([arguments], prefix_names=False)
        return
    _, c_path, function, variants_path = arguments
    try:
        chosen = read_variants(c_path, function, variants_path)[apply_number]
    except LookupError as error:
        sys.exit(str(error))
    if chosen is None:
        sys.exit(f"variant {apply_number}: text not found")
    _ = Path(c_path).write_text(chosen)


if __name__ == "__main__":
    main()
