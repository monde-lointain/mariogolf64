#!/usr/bin/env python3
"""Score variants of a C file, each made by one text replacement.

    try_variants.py ASM_FILE C_FILE FUNCTION VARIANTS_FILE

VARIANTS_FILE holds blocks separated by lines of "====". Each block is the
text to replace, a line of "----", and the replacement. Prints the score of
each variant, so that several guesses cost one compile each.
"""

import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    asm_path, c_path, function, variants_path = sys.argv[1:5]
    with open(c_path) as c_file:
        source = c_file.read()
    with open(variants_path) as variants_file:
        blocks = variants_file.read().split("\n====\n")
    for number, block in enumerate(blocks):
        old, new = block.split("\n----\n")
        if old not in source:
            print(f"{number}: text not found")
            continue
        with tempfile.NamedTemporaryFile("w", suffix=".c", dir=os.path.dirname(c_path), delete=False) as variant:
            variant.write(source.replace(old, new, 1))
        result = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "fncompare.py"), asm_path, variant.name, function],
                                capture_output=True, text=True, cwd=ROOT)
        os.remove(variant.name)
        print(f"{number}: {result.stdout.strip() or result.stderr.strip()[-300:]}")


if __name__ == "__main__":
    main()
