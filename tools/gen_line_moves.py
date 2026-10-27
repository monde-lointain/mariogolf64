#!/usr/bin/env python3
"""Write try_variants.py variants that each move one line of a function.

    gen_line_moves.py C_FILE FUNCTION OUT_FILE

Every variant moves one statement line of FUNCTION's body to another
position in the body. Statement order decides GCC's scheduling, so this
finds the order of stores and calls that the original used.
"""

import sys


def main():
    c_path, function, out_path = sys.argv[1:4]
    with open(c_path) as c_file:
        source = c_file.read()
    start = source.index("\n", source.index(f" {function}(")) + 1
    end = source.index("\n}\n", start) + 1
    body = source[start:end]
    lines = body.rstrip("\n").split("\n")
    variants = []
    for from_index, line in enumerate(lines):
        if not line.strip().endswith(";"):
            continue
        rest = lines[:from_index] + lines[from_index + 1 :]
        for to_index in range(len(rest) + 1):
            if to_index == from_index:
                continue
            moved = rest[:to_index] + [line] + rest[to_index:]
            variants.append(body + "----\n" + "\n".join(moved) + "\n")
    with open(out_path, "w") as out_file:
        out_file.write("\n====\n".join(variants))
    print(f"{len(variants)} variants")


if __name__ == "__main__":
    main()
