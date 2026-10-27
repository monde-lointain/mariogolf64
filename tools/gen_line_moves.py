#!/usr/bin/env python3
"""Write try_variants.py variants that each move one line of a function.

    gen_line_moves.py C_FILE FUNCTION OUT_FILE

Every variant moves one statement line of FUNCTION's body to another
position in the body. Statement order decides GCC's scheduling, so this
finds the order of stores and calls that the original used.
"""

import sys
from pathlib import Path


def main() -> None:
    """Write every one-line move of FUNCTION's statements to OUT_FILE."""
    c_path, function, out_path = sys.argv[1:4]
    source = Path(c_path).read_text()
    start = source.index("\n", source.index(f" {function}(")) + 1
    end = source.index("\n}\n", start) + 1
    body = source[start:end]
    lines = body.rstrip("\n").split("\n")
    variants: list[str] = []
    for from_index, line in enumerate(lines):
        if not line.strip().endswith(";"):
            continue
        rest = lines[:from_index] + lines[from_index + 1 :]
        for to_index in range(len(rest) + 1):
            if to_index == from_index:
                continue
            moved = [*rest[:to_index], line, *rest[to_index:]]
            variants.append(body + "----\n" + "\n".join(moved) + "\n")
    _ = Path(out_path).write_text("\n====\n".join(variants))
    print(f"{len(variants)} variants")


if __name__ == "__main__":
    main()
