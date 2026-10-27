#!/usr/bin/env python3
"""Turn an asm segment of the main code into a C file of INCLUDE_ASM stubs.

    new_c_file.py ROM_OFFSET NAME

Changes the segment `[ROM_OFFSET, asm]` of the splat yaml into
`[ROM_OFFSET, c, NAME]`, writes src/NAME.c with one INCLUDE_ASM per function
of asm/<ROM_OFFSET>.s, re-splits the ROM and saves the functions' assembly to
expected/NAME.s, where fncompare.py can compare against it after the
functions are decompiled and splat stops writing their assembly.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
YAML_PATH = ROOT / "mariogolf64.yaml"
GLABEL_RE = re.compile(r"^glabel (\w+)", re.MULTILINE)


def main() -> None:
    """Make the C file and expected assembly for the segment given."""
    offset, name = sys.argv[1], sys.argv[2]
    rom_offset = int(offset, 16)
    asm_path = ROOT / "asm" / f"{rom_offset:X}.s"
    functions: list[str] = GLABEL_RE.findall(asm_path.read_text())

    yaml = YAML_PATH.read_text()
    segment = f"[0x{rom_offset:X}, asm]"
    if segment not in yaml:
        sys.exit(f"{segment} not found in the yaml")
    _ = YAML_PATH.write_text(yaml.replace(segment, f"[0x{rom_offset:X}, c, {name}]"))

    c_path = ROOT / "src" / f"{name}.c"
    c_path.parent.mkdir(parents=True, exist_ok=True)
    stubs = "".join(
        f'INCLUDE_ASM("asm/nonmatchings/{name}", {function});\n\n'
        for function in functions
    )
    _ = c_path.write_text('#include "common.h"\n\n' + stubs)

    _ = subprocess.run(["make", "extract"], cwd=ROOT, check=True, capture_output=True)
    asm_path.unlink()

    expected_path = ROOT / "expected" / f"{name}.s"
    expected_path.parent.mkdir(parents=True, exist_ok=True)
    function_dir = ROOT / "asm" / "nonmatchings" / name
    texts = [(function_dir / f"{function}.s").read_text() for function in functions]
    _ = expected_path.write_text("".join(texts))
    print(f"{len(functions)} functions")


if __name__ == "__main__":
    main()
