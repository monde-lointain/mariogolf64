#!/usr/bin/env python3
"""Score variants of a C file, each made by text replacements.

    try_variants.py [--apply NUMBER] ASM_FILE C_FILE FUNCTION VARIANTS_FILE
    try_variants.py --batch JOBS_FILE

VARIANTS_FILE holds blocks separated by lines of "====". Each block is the
text to replace, a line of "----", and the replacement, which is made in
FUNCTION's definition, or failing that in the text before it, which holds
the inline helpers and prototypes it uses. A block can make several
replacements, such as at a change's twin sites, joined by lines of "++++".
Prints the score of each variant, so that several guesses cost one compile
each. With --apply, writes variant NUMBER into C_FILE instead, made the same
way as the one that was scored.

With --batch, each line of JOBS_FILE names an ASM_FILE, C_FILE, FUNCTION
and VARIANTS_FILE, and each score is printed after its function's name.
A sweep holds a few variants for each of many functions, so scoring them
in one pool keeps every process busy, where a run per function waits on
its slowest variant and starts the interpreter again.

Each target assembly is read once, and the variants compile in parallel,
one process per CPU, since parsing the objects holds the interpreter lock.
Each compile cuts the file down to the function and its neighbour. The
variants that change only FUNCTION's body share one preprocessing of the
file around it, and their bodies are preprocessed together, so that each
costs only its compile; the others are preprocessed whole.
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


def find_definition(source: str, function: str) -> tuple[int, int, int] | None:
    """Return where FUNCTION's definition starts, its body starts and it ends.

    The body starts after the line of the header's opening brace.

    Routines that share code, such as the two copies of a loop, can hold the
    same text, so a variant's replacement is looked for in its function only.
    A nested function's definition is indented, and its closing brace alone
    starts a line at the same indent as its header.
    """
    header = re.compile(
        rf"^([ \t]*)[A-Za-z][\w* ]*[ *]{re.escape(function)}\([^;{{]*\) \{{\n",
        re.MULTILINE,
    )
    match = header.search(source)
    if match is None:
        return None
    closing_brace = f"\n{match.group(1)}}}\n"
    end = source.find(closing_brace, match.end())
    if end < 0:
        return None
    return match.start(), match.end(), end + len(closing_brace)


def describe_compiled(
    compiled: fncompare.Functions | None, error: str, function: str
) -> str:
    """Return the score of FUNCTION as compiled, or why it has none."""
    if compiled is None:
        # One line per variant, so that each score stays beside its number
        return " | ".join(error.strip().splitlines())[-ERROR_TAIL_LENGTH:]
    if function not in compiled:
        return f"{function} missing"
    return ""


def score_body(
    bodies: fncompare.BodyVariants, preprocessed: str, target: fncompare.Original
) -> str:
    """Compile one variant's preprocessed body and return its score."""
    compiled, error = bodies.compile(preprocessed)
    failure = describe_compiled(compiled, error, bodies.function)
    if failure or compiled is None:
        return failure
    function = bodies.function
    return f"{function} {fncompare.describe_original(target, compiled, function)}"


def get_variant_body(
    variant: str | None, source: str, start: int, end: int
) -> str | None:
    """Return VARIANT's text in place of SOURCE's body, from START to END.

    None when VARIANT changes SOURCE elsewhere, or is None.
    """
    after_length = len(source) - end
    if (
        variant is None
        or not variant.startswith(source[:start])
        or not variant.endswith(source[end:])
        or len(variant) < start + after_length
    ):
        return None
    return variant[start : len(variant) - after_length]


def prepare_bodies(
    c_path: str, function: str, optflags: list[str], bodies: list[str]
) -> tuple[fncompare.BodyVariants, list[str]] | None:
    """Return C_PATH's file ready for FUNCTION's BODIES, and them preprocessed.

    None when the bodies can't be preprocessed together, so that the
    variants compile whole instead.
    """
    source = Path(c_path).read_text()
    definition = find_definition(source, function)
    if definition is None or not bodies:
        return None
    _, start, end = definition
    try:
        variants = fncompare.BodyVariants(
            c_path, source, function, start, end, optflags
        )
        return variants, variants.preprocess_bodies(bodies)
    except ValueError:
        return None


def score_variant(
    source: str,
    c_dir: Path,
    optflags: list[str],
    target: fncompare.Original,
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
    failure = describe_compiled(compiled, error, function)
    if failure or compiled is None:
        return failure
    return f"{function} {fncompare.describe_original(target, compiled, function)}"


def replace_all(before: str, body: str, block: str) -> tuple[str, str] | None:
    """Return BEFORE and BODY with each of BLOCK's replacements made, in turn.

    Each replacement is made in BODY, or in BEFORE when BODY lacks its text.
    Returns None when any of the texts to replace is absent from both. A
    replacement can be empty, to delete its text, so the line of "----" can
    end it.
    """
    for replacement in block.split("\n++++\n"):
        old, _, new = replacement.partition("\n----")
        new = new.removeprefix("\n")
        if old in body:
            body = body.replace(old, new, 1)
        elif old in before:
            before = before.replace(old, new, 1)
        else:
            return None
    return before, body


def read_variants(c_path: str, function: str, variants_path: str) -> list[str | None]:
    """Return C_PATH's text as each variant makes it.

    A variant any of whose texts to replace is absent gives None. Raises
    LookupError when C_PATH doesn't define FUNCTION.
    """
    source = Path(c_path).read_text()
    definition = find_definition(source, function)
    if definition is None:
        msg = f"{c_path} has no definition of {function}"
        raise LookupError(msg)
    start, _, end = definition
    before, body, after = source[:start], source[start:end], source[end:]
    # The file's own last newline ends the last block, not its replacement
    text = Path(variants_path).read_text().removesuffix("\n")
    variants: list[str | None] = []
    for block in text.split("\n====\n"):
        variant = replace_all(before, body, block)
        variants.append(None if variant is None else "".join(variant) + after)
    return variants


def find_bodies(
    c_path: str, function: str, sources: list[str | None]
) -> list[str | None]:
    """Return the body each of SOURCES gives FUNCTION, or None where it changes more."""
    source = Path(c_path).read_text()
    definition = find_definition(source, function)
    if definition is None:
        return [None] * len(sources)
    _, start, end = definition
    return [get_variant_body(variant, source, start, end) for variant in sources]


def read_target(asm_path: str, c_path: str, function: str) -> fncompare.Original:
    """Return FUNCTION's routines in the target assembly at ASM_PATH."""
    return fncompare.read_original(asm_path, c_path, function)


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
            pool.map(
                read_target,
                [job[0] for job in jobs],
                [job[1] for job in jobs],
                [job[2] for job in jobs],
            )
        )
        # The variants have their own names, so they take the flags of the
        # file they vary
        optflags = [fncompare.get_optflags(job[1]) for job in jobs]
        bodies = [
            find_bodies(job[1], job[2], sources)
            for job, sources in zip(jobs, variants, strict=True)
        ]
        prepared = list(
            pool.map(
                prepare_bodies,
                [job[1] for job in jobs],
                [job[2] for job in jobs],
                optflags,
                [
                    [body for body in found_bodies if body is not None]
                    for found_bodies in bodies
                ],
            )
        )
        results: list[list[Future[str] | None]] = []
        for number, job in enumerate(jobs):
            target, sources, flags = targets[number], variants[number], optflags[number]
            job_bodies, ready = bodies[number], prepared[number]
            c_dir = Path(job[1]).resolve().parent
            preprocessed = iter(ready[1]) if ready else iter(())
            scores: list[Future[str] | None] = []
            for source, body in zip(sources, job_bodies, strict=True):
                if source is None:
                    scores.append(None)
                elif ready and body is not None:
                    scores.append(
                        pool.submit(score_body, ready[0], next(preprocessed), target)
                    )
                else:
                    scores.append(
                        pool.submit(score_variant, source, c_dir, flags, target, job[2])
                    )
            results.append(scores)
        for (_, _, function, _), scores in zip(jobs, results, strict=True):
            name = f"{function} " if prefix_names else ""
            for number, result in enumerate(scores):
                score = result.result() if result else "text not found"
                print(f"{name}{number}: {score}")


def main() -> None:
    """Print the score of each variant, in order, or apply one."""
    arguments = sys.argv[1:]
    usage_lengths = {"--batch": 2, "--apply": 6}
    expected_length = usage_lengths.get(arguments[0] if arguments else "", 4)
    if len(arguments) != expected_length:
        sys.exit(__doc__)
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
