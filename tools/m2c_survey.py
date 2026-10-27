#!/usr/bin/env python3
"""Measure how many functions raw m2c output matches.

    m2c_survey.py OUT_DIR ASM_FILE ...

Decompiles each function of each ASM_FILE on its own with m2c, compiles it
with KMC GCC and compares it with the original. Writes OUT_DIR/<function>.c
and prints a tab-separated line per function: asm file, function, size in
words and score (-1 when m2c or the compiler failed).
"""

import multiprocessing
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import NamedTuple

import fncompare

ROOT = fncompare.ROOT
M2C = ROOT / ".venv" / "bin" / "m2c"
DATA_FILES = [p for p in os.environ.get("SURVEY_DATA", "").split(":") if p]
# SURVEY_HEADERS adds headers (colon-separated) that declare the globals
# m2c's output uses
PRELUDE = (
    "".join(
        f'#include "{h}"\n'
        for h in [
            "common.h",
            "m2c_macros.h",
            *[p for p in os.environ.get("SURVEY_HEADERS", "").split(":") if p],
        ]
    )
    + "\n"
)
# m2c guesses each callee's prototype from one call, which other calls then
# contradict; declaring the callees without parameter lists avoids that
GUESSED_PROTOTYPE_RE = re.compile(
    r"^(.*?\b(\w+))\(.*\);( +/\* (?:extern|static) \*/)$", re.MULTILINE
)
CALLEE_RE = re.compile(r"\b(func_[0-9A-F]{8}\w*)\b")
# The score of a function that m2c or the compiler failed on
FAILED_SCORE = -1.0


class Job(NamedTuple):
    """One function to decompile and score."""

    out_dir: Path
    asm_path: Path
    name: str
    target: list[int]


class Result(NamedTuple):
    """How a function's m2c output scored."""

    asm_path: Path
    name: str
    size: int
    value: float


def declare_callees(text: str, name: str) -> str:
    """Unprototype m2c's guessed declarations and declare callees it left out."""
    text = GUESSED_PROTOTYPE_RE.sub(r"\1();\3", text)
    declared: set[str] = set(re.findall(r"\b(\w+)\(\);", text)) | {name}
    called: set[str] = set(CALLEE_RE.findall(text))
    missing = sorted(called - declared)
    return "".join(f"s32 {callee}();\n" for callee in missing) + text


def survey_function(job: Job) -> Result:
    """Decompile, compile and score one function."""
    failed = Result(job.asm_path, job.name, len(job.target), FAILED_SCORE)
    result = subprocess.run(
        [
            str(M2C),
            "-t",
            "mips-gcc-c",
            "--valid-syntax",
            "--context",
            str(ROOT / "ctx.c"),
            "-f",
            job.name,
            str(job.asm_path),
            *DATA_FILES,
        ],
        capture_output=True,
        text=True,
        cwd=ROOT,
        check=False,
    )
    if result.returncode != 0:
        return failed
    c_path = job.out_dir / f"{job.name}.c"
    _ = c_path.write_text(PRELUDE + declare_callees(result.stdout, job.name))
    compiled, _ = fncompare.compile_functions(c_path)
    if not compiled or job.name not in compiled:
        return failed
    value = fncompare.score(job.target, compiled[job.name])
    return failed._replace(value=value)


def main() -> None:
    """Survey every function of the asm files named on the command line."""
    out_dir = Path(sys.argv[1]).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    jobs = [
        Job(out_dir, Path(asm_path).resolve(), name, words)
        for asm_path in sys.argv[2:]
        for name, words in fncompare.read_target_functions(asm_path).items()
    ]
    with multiprocessing.Pool() as pool:
        for result in pool.imap_unordered(survey_function, jobs):
            fields = [result.asm_path.name, result.name, str(result.size)]
            print("\t".join([*fields, f"{result.value:.3f}"]), flush=True)


if __name__ == "__main__":
    main()
