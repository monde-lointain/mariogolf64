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
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fncompare  # noqa: E402

ROOT = fncompare.ROOT
M2C = os.path.join(ROOT, ".venv", "bin", "m2c")
DATA_FILES = [p for p in os.environ.get("SURVEY_DATA", "").split(":") if p]
PRELUDE = '#include "common.h"\n#include "m2c_macros.h"\n\n'


def survey_function(job):
    out_dir, asm_path, name, target = job
    result = subprocess.run(
        [M2C, "-t", "mips-gcc-c", "--valid-syntax", "--context", os.path.join(ROOT, "ctx.c"), "-f", name, asm_path, *DATA_FILES],
        capture_output=True, text=True, cwd=ROOT)
    c_path = os.path.join(out_dir, name + ".c")
    if result.returncode != 0:
        return asm_path, name, len(target), -1.0
    with open(c_path, "w") as c_file:
        c_file.write(PRELUDE + result.stdout)
    compiled, _ = fncompare.compile_functions(c_path)
    if not compiled or name not in compiled:
        return asm_path, name, len(target), -1.0
    return asm_path, name, len(target), fncompare.score(target, compiled[name])


def main():
    out_dir, *asm_paths = sys.argv[1:]
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    jobs = []
    for asm_path in asm_paths:
        for name, words in fncompare.read_target_functions(asm_path).items():
            jobs.append((out_dir, os.path.abspath(asm_path), name, words))
    with multiprocessing.Pool() as pool:
        for asm_path, name, size, value in pool.imap_unordered(survey_function, jobs):
            print(f"{os.path.basename(asm_path)}\t{name}\t{size}\t{value:.3f}", flush=True)


if __name__ == "__main__":
    main()
