#!/usr/bin/env python3
"""Re-split the assembly that a decompiled C file is compared against.

    refresh_expected.py NAME

After symbols are renamed in symbol_addrs.txt, expected/NAME.s still uses
the old names. This briefly replaces src/NAME.c with INCLUDE_ASM stubs for
the functions of expected/NAME.s, so that splat writes their assembly with
the current names again, saves it to expected/NAME.s and puts the C file
back.
"""

import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FUNCTION_RE = re.compile(
    r"^glabel (\w+)\n(?:.*\n)*?\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ", re.MULTILINE
)
SYMBOL_RE = re.compile(
    r"^(\w+) = 0x([0-9A-Fa-f]{8});.*type:func(?:.*segment:(\w+))?", re.MULTILINE
)


def name_functions(name: str, addresses: list[str]) -> list[str]:
    """Return the current names of the functions at ADDRESSES in NAME."""
    symbols: list[tuple[str, str, str]] = SYMBOL_RE.findall(
        (ROOT / "symbol_addrs.txt").read_text()
    )
    # Overlays share addresses, so a symbol of this file's segment wins
    segment = name.split("/", maxsplit=1)[0]
    names_by_address: dict[str, str] = {}
    for symbol, address, symbol_segment in symbols:
        if symbol_segment in ("", segment):
            _ = names_by_address.setdefault(address.upper(), symbol)
    suffix = f"_{segment}" if segment.startswith("ovl_") else ""
    return [
        names_by_address.get(address, f"func_{address}{suffix}")
        for address in addresses
    ]


def main() -> None:
    """Refresh expected/NAME.s for the NAME on the command line."""
    name = sys.argv[1]
    expected_path = ROOT / "expected" / f"{name}.s"
    c_path = ROOT / "src" / f"{name}.c"
    found: list[tuple[str, str]] = FUNCTION_RE.findall(expected_path.read_text())
    functions = name_functions(name, [address for _, address in found])

    backup_path = c_path.with_name(c_path.name + ".refresh_backup")
    _ = shutil.copyfile(c_path, backup_path)
    try:
        stubs = "".join(
            f'INCLUDE_ASM("asm/nonmatchings/{name}", {function});\n\n'
            for function in functions
        )
        _ = c_path.write_text('#include "common.h"\n\n' + stubs)
        _ = subprocess.run(["rm", "-rf", "asm"], cwd=ROOT, check=True)
        _ = subprocess.run(
            ["make", "extract"], cwd=ROOT, check=True, capture_output=True
        )
        function_dir = ROOT / "asm" / "nonmatchings" / name
        texts = [(function_dir / f"{function}.s").read_text() for function in functions]
        _ = expected_path.write_text("".join(texts))
    finally:
        _ = shutil.move(backup_path, c_path)
    print(f"{len(functions)} functions")


if __name__ == "__main__":
    main()
