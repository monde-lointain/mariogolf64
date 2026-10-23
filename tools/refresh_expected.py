#!/usr/bin/env python3
"""Re-split the assembly that a decompiled C file is compared against.
    refresh_expected.py NAME
After symbols are renamed in symbol_addrs.txt, expected/NAME.s still uses
the old names. This briefly replaces src/NAME.c with INCLUDE_ASM stubs for
the functions of expected/NAME.s, so that splat writes their assembly with
the current names again, saves it to expected/NAME.s and puts the C file
back.
"""
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUNCTION_RE = re.compile(r"^glabel (\w+)\n(?:.*\n)*?\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ", re.MULTILINE)
SYMBOL_RE = re.compile(r"^(\w+) = 0x([0-9A-Fa-f]{8});.*type:func(?:.*segment:(\w+))?", re.MULTILINE)


def main():
    name = sys.argv[1]
    expected_path = os.path.join(ROOT, "expected", f"{name}.s")
    c_path = os.path.join(ROOT, "src", f"{name}.c")
    with open(expected_path) as expected_file:
        addresses = [address for _, address in FUNCTION_RE.findall(expected_file.read())]
    with open(os.path.join(ROOT, "symbol_addrs.txt")) as symbol_file:
        symbols = SYMBOL_RE.findall(symbol_file.read())
    # Overlays share addresses, so a symbol of this file's segment wins
    segment = name.split("/")[0]
    names_by_address = {}
    for symbol, address, symbol_segment in symbols:
        if symbol_segment in ("", segment):
            names_by_address.setdefault(address.upper(), symbol)
    functions = [names_by_address.get(address, f"func_{address}") for address in addresses]

    backup_path = c_path + ".refresh_backup"
    shutil.copyfile(c_path, backup_path)
    try:
        with open(c_path, "w") as c_file:
            c_file.write('#include "common.h"\n\n')
            for function in functions:
                c_file.write(f'INCLUDE_ASM("asm/nonmatchings/{name}", {function});\n\n')
        subprocess.run(["rm", "-rf", "asm"], cwd=ROOT, check=True)
        subprocess.run(["make", "extract"], cwd=ROOT, check=True, capture_output=True)
        texts = []
        for function in functions:
            path = os.path.join(ROOT, "asm", "nonmatchings", name, f"{function}.s")
            with open(path) as function_file:
                texts.append(function_file.read())
        with open(expected_path, "w") as expected_file:
            expected_file.write("".join(texts))
    finally:
        shutil.move(backup_path, c_path)
    print(f"{len(functions)} functions")


if __name__ == "__main__":
    main()
