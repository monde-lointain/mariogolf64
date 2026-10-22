#!/usr/bin/env python3
"""Score variants of a C file, each made by one text replacement.

    try_variants.py ASM_FILE C_FILE FUNCTION VARIANTS_FILE

VARIANTS_FILE holds blocks separated by lines of "====". Each block is the
text to replace, a line of "----", and the replacement. Prints the score of
each variant, so that several guesses cost one compile each.

The target assembly is read once, and the variants compile in parallel,
one process per CPU, since parsing the objects holds the interpreter lock.
"""

import os
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor

import fncompare


def score_variant(source, c_dir, optflags, target, function):
    with tempfile.NamedTemporaryFile("w", suffix=".c", dir=c_dir, delete=False) as variant:
        variant.write(source)
    try:
        compiled, error = fncompare.compile_functions(variant.name, optflags)
    finally:
        os.remove(variant.name)
    if compiled is None:
        return error.strip()[-300:]
    if function not in compiled:
        return f"{function} missing"
    value = fncompare.score(target, compiled[function])
    return f"{function} {value:.3f} {'OK' if value == 1.0 else ''}".rstrip()


def main():
    asm_path, c_path, function, variants_path = sys.argv[1:5]
    with open(c_path) as c_file:
        source = c_file.read()
    with open(variants_path) as variants_file:
        blocks = variants_file.read().split("\n====\n")
    target = fncompare.read_target_functions(asm_path)[function]
    c_dir = os.path.dirname(os.path.abspath(c_path))
    # The variants have their own names, so they take the flags of the file they vary
    optflags = fncompare.get_optflags(c_path)

    with ProcessPoolExecutor(max_workers=os.cpu_count()) as pool:
        results = []
        for block in blocks:
            old, new = block.split("\n----\n")
            if old not in source:
                results.append(None)
            else:
                results.append(pool.submit(score_variant, source.replace(old, new, 1), c_dir, optflags, target, function))
        for number, result in enumerate(results):
            print(f"{number}: {result.result() if result else 'text not found'}")


if __name__ == "__main__":
    main()
