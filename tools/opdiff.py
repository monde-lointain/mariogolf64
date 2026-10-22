#!/usr/bin/env python3
"""Shows where a function's instruction sequence differs from the target's,
comparing mnemonics only so that register and offset changes are ignored.

usage: opdiff.py EXPECTED.s FILE.c FUNCTION
"""

import difflib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fncompare  # noqa: E402
import rabbitizer  # noqa: E402


def mnemonics(words):
    return [rabbitizer.Instruction(word).getOpcodeName() for word in words]


def main():
    asm_path, c_path, function = sys.argv[1:4]
    target = fncompare.read_target_functions(asm_path)[function]
    compiled, error = fncompare.compile_functions(c_path)
    if compiled is None:
        sys.exit(error)
    expected_ops = mnemonics(target)
    compiled_ops = mnemonics(compiled[function])
    matcher = difflib.SequenceMatcher(None, expected_ops, compiled_ops, autojunk=False)
    for tag, start, end, compiled_start, compiled_end in matcher.get_opcodes():
        if tag == "equal":
            continue
        print(f"{tag:7} expected {start}-{end} {' '.join(expected_ops[start:end])}")
        print(f"        compiled {compiled_start}-{compiled_end} "
              f"{' '.join(compiled_ops[compiled_start:compiled_end])}")


if __name__ == "__main__":
    main()
