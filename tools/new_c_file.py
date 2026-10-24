#!/usr/bin/env python3
"""Turn an asm segment of the main code into a C file of INCLUDE_ASM stubs.

    new_c_file.py ROM_OFFSET NAME

Changes the segment `[ROM_OFFSET, asm]` of the splat yaml into
`[ROM_OFFSET, c, NAME]`, writes src/NAME.c with one INCLUDE_ASM per function
of asm/<ROM_OFFSET>.s, re-splits the ROM and saves the functions' assembly to
expected/NAME.s, where fncompare.py can compare against it after the
functions are decompiled and splat stops writing their assembly.
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YAML_PATH = os.path.join(ROOT, "mariogolf64.yaml")
GLABEL_RE = re.compile(r"^glabel (\w+)", re.MULTILINE)


def main():
    offset, name = sys.argv[1], sys.argv[2]
    rom_offset = int(offset, 16)
    asm_path = os.path.join(ROOT, "asm", f"{rom_offset:X}.s")
    with open(asm_path) as asm_file:
        functions = GLABEL_RE.findall(asm_file.read())

    with open(YAML_PATH) as yaml_file:
        yaml = yaml_file.read()
    segment = f"[0x{rom_offset:X}, asm]"
    if segment not in yaml:
        sys.exit(f"{segment} not found in the yaml")
    yaml = yaml.replace(segment, f"[0x{rom_offset:X}, c, {name}]")
    with open(YAML_PATH, "w") as yaml_file:
        yaml_file.write(yaml)

    c_path = os.path.join(ROOT, "src", f"{name}.c")
    os.makedirs(os.path.dirname(c_path), exist_ok=True)
    with open(c_path, "w") as c_file:
        c_file.write('#include "common.h"\n\n')
        for function in functions:
            c_file.write(f'INCLUDE_ASM("asm/nonmatchings/{name}", {function});\n\n')

    subprocess.run(["make", "extract"], cwd=ROOT, check=True, capture_output=True)
    os.remove(asm_path)

    expected_path = os.path.join(ROOT, "expected", f"{name}.s")
    os.makedirs(os.path.dirname(expected_path), exist_ok=True)
    with open(expected_path, "w") as expected:
        for function in functions:
            path = os.path.join(ROOT, "asm", "nonmatchings", name, f"{function}.s")
            with open(path) as function_file:
                expected.write(function_file.read())
    print(f"{len(functions)} functions")


if __name__ == "__main__":
    main()
