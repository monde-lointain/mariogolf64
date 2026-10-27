#!/usr/bin/env python3
"""Show where a function's instruction sequence differs from the target's.

Only mnemonics are compared, so that register and offset changes are ignored.

usage: opdiff.py EXPECTED.s FILE.c FUNCTION
"""

import difflib
import sys

import rabbitizer

import fncompare


def get_mnemonics(words: list[int]) -> list[str]:
    """Return the mnemonic of each instruction."""
    return [rabbitizer.Instruction(word).getOpcodeName() for word in words]


def main() -> None:
    """Print each run of mnemonics that differs."""
    asm_path, c_path, function = sys.argv[1:4]
    target = fncompare.read_target_functions(asm_path)[function]
    compiled, error = fncompare.compile_functions(c_path)
    if compiled is None:
        sys.exit(error)
    expected_ops = get_mnemonics(target)
    compiled_ops = get_mnemonics(compiled[function])
    matcher = difflib.SequenceMatcher(None, expected_ops, compiled_ops, autojunk=False)
    for tag, start, end, compiled_start, compiled_end in matcher.get_opcodes():
        if tag == "equal":
            continue
        expected_text = " ".join(expected_ops[start:end])
        compiled_text = " ".join(compiled_ops[compiled_start:compiled_end])
        print(f"{tag:7} expected {start}-{end} {expected_text}")
        print(f"        compiled {compiled_start}-{compiled_end} {compiled_text}")


if __name__ == "__main__":
    main()
