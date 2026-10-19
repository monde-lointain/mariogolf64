#!/usr/bin/env python3
"""Create ctx.c, a preprocessed context file for m2c / decomp.me, from a C file.

The include paths and defines come from the C file's ROM compiler profile
(`make print-cflags`), so the context matches what KMC GCC sees.
"""

import argparse
import os
import re
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


C_TOKEN = re.compile(
    r"#[^\n]*"  # preprocessor lines, removed before scanning
    r'|"(?:\\.|[^"\\\n])*"'  # string literal
    r"|'(?:\\.|[^'\\\n])*'"  # character literal
    r"|[A-Za-z_]\w*|\d\w*|\S"  # identifiers, numbers, punctuation
)
C_IDENTIFIER = re.compile(r"[A-Za-z_]\w*")
C_STATEMENT_KEYWORDS = frozenset(
    {"case", "do", "else", "for", "if", "return", "sizeof", "switch", "while"}
)


def strip_nested_functions(text: str) -> str:
    """Remove GCC nested function definitions, which m2c's parser (pycparser) rejects.

    A nested definition is `<specifiers> name(params) {` inside a function body. The removed span
    runs from its first specifier through the matching `}`. Control statements such as `if (x) {`
    are not definitions and stay.
    """
    tokens = [match for match in C_TOKEN.finditer(text) if not match.group().startswith("#")]
    spans = []
    depth = 0
    index = 0
    while index < len(tokens):
        token = tokens[index].group()
        if token == "{":
            start = _nested_definition_start(tokens, index) if depth > 0 else None
            if start is not None:
                end = _matching_brace(tokens, index)
                spans.append((tokens[start].start(), tokens[end].end()))
                index = end + 1
                continue
            depth += 1
        elif token == "}":
            depth -= 1
        index += 1
    pieces = []
    position = 0
    for start, end in spans:
        pieces.append(text[position:start])
        position = end
    pieces.append(text[position:])
    return "".join(pieces)


def _is_name(word: str) -> bool:
    return C_IDENTIFIER.fullmatch(word) is not None and word not in C_STATEMENT_KEYWORDS


def _is_specifier(word: str) -> bool:
    return word == "*" or _is_name(word)


def _nested_definition_start(tokens: list, brace: int) -> "int | None":
    """Index of the first specifier of the nested function whose body opens at `brace`."""
    if tokens[brace - 1].group() != ")":
        return None
    open_paren = _matching_open_paren(tokens, brace - 1)
    if open_paren is None or open_paren < 2:
        return None
    if not _is_name(tokens[open_paren - 1].group()):
        return None
    if not _is_specifier(tokens[open_paren - 2].group()):
        return None
    start = open_paren - 1
    while start > 0 and _is_specifier(tokens[start - 1].group()):
        start -= 1
    return start


def _matching_open_paren(tokens: list, close: int) -> "int | None":
    level = 0
    for index in range(close, -1, -1):
        token = tokens[index].group()
        if token == ")":
            level += 1
        elif token == "(":
            level -= 1
            if level == 0:
                return index
    return None


def _matching_brace(tokens: list, brace: int) -> int:
    level = 0
    for index in range(brace, len(tokens)):
        token = tokens[index].group()
        if token == "{":
            level += 1
        elif token == "}":
            level -= 1
            if level == 0:
                return index
    raise ValueError("unbalanced braces in preprocessed context")



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
    return strip_nested_functions(out_text)


def main():
    parser = argparse.ArgumentParser(description="Create a context file for m2c / decomp.me")
    parser.add_argument("c_file", help="C file from which to create context")
    args = parser.parse_args()

    output = import_c_file(args.c_file)

    with open(os.path.join(ROOT_DIR, "ctx.c"), "w", encoding="utf-8") as f:
        f.write(output)


if __name__ == "__main__":
    main()
