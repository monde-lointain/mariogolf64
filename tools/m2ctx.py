#!/usr/bin/env python3
"""Create ctx.c, a preprocessed context file for m2c / decomp.me, from a C file.

The include paths and defines come from the C file's ROM compiler profile
(`make print-cflags`), so the context matches what KMC GCC sees.
"""

import argparse
import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

EXTRA_FLAGS = [
    "-D_MIPS_SZLONG=32",
    "-D_MIPS_SZINT=32",
    "-ffreestanding",
    "-DM2CTX",
]


def profile_flags(rel_file: str) -> list[str]:
    output = subprocess.check_output(
        ["make", "-s", "--no-print-directory", "print-cflags", f"SRC={rel_file}"],
        cwd=ROOT_DIR,
        encoding="utf-8",
    )
    tokens = output.split()
    flags = []
    for index, token in enumerate(tokens):
        if token == "-I":
            flags += ["-I", tokens[index + 1]]
        elif token.startswith(("-I", "-D", "-U")):
            flags.append(token)
    return flags


def import_c_file(in_file: str) -> str:
    rel_file = os.path.relpath(in_file, ROOT_DIR)
    flags = profile_flags(rel_file) + EXTRA_FLAGS
    macro_cmd = ["gcc", "-E", "-P", "-dM", *flags, rel_file]
    preprocess_cmd = ["gcc", "-E", "-P", *flags, rel_file]

    with tempfile.NamedTemporaryFile(suffix=".c") as tmp:
        stock_macros = subprocess.check_output(
            ["gcc", "-E", "-P", "-dM", tmp.name],
            cwd=ROOT_DIR,
            encoding="utf-8",
        )
    stock_macros += "#define __STDC_HOSTED__ 0\n"

    try:
        out_text = subprocess.check_output(macro_cmd, cwd=ROOT_DIR, encoding="utf-8")
        out_text += subprocess.check_output(preprocess_cmd, cwd=ROOT_DIR, encoding="utf-8")
    except subprocess.CalledProcessError:
        print(
            "Failed to preprocess input file, when running command:\n" + " ".join(preprocess_cmd),
            file=sys.stderr,
        )
        sys.exit(1)

    if not out_text:
        print("Output is empty - aborting", file=sys.stderr)
        sys.exit(1)

    for line in stock_macros.strip().splitlines():
        out_text = out_text.replace(line + "\n", "")
    return out_text


def main():
    parser = argparse.ArgumentParser(description="Create a context file for m2c / decomp.me")
    parser.add_argument("c_file", help="C file from which to create context")
    args = parser.parse_args()

    output = import_c_file(args.c_file)

    with open(os.path.join(ROOT_DIR, "ctx.c"), "w", encoding="utf-8") as f:
        f.write(output)


if __name__ == "__main__":
    main()
