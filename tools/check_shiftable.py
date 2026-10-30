#!/usr/bin/env python3
"""List the references that hold only while the ROM keeps its original layout.

    check_shiftable.py LD CONFIG LD_SCRIPT SYMBOL_SCRIPT... [-- LD_FLAG...]

The build links with scripts that give symbols fixed addresses, such as
the ones splat writes for every address the asm refers to without a name.
An address inside a segment, in RAM or in ROM, only holds while the code
and data before it keep their sizes: those symbols are pins, which a ROM
whose code can move must do without. Addresses outside every segment, such
as the boot segment's variables in low memory, stay where they are.

The check links the build's objects again with LD, through LD_SCRIPT and
only the SYMBOL_SCRIPTs' fixed addresses. It prints each pinned symbol that
is still referenced, with the routines that refer to it, and fails while
any are. CONFIG, splat's configuration, gives the original's segments.
"""

import itertools
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path
from typing import cast

import yaml

# An assignment in a linker script: NAME = EXPRESSION;
ASSIGNMENT = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)\s*=\s*([^;]+);", re.MULTILINE)
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
NUMBER = re.compile(r"0[xX][0-9A-Fa-f]+|\d+")
# What ld says about a reference it can't resolve. A reference from code
# follows a line that names its routine; one from data names its object.
IN_FUNCTION = re.compile(r"(\S+\.o): in function `([^']+)':$")
UNDEFINED = re.compile(r"undefined reference to `([^']+)'$")
IN_DATA = re.compile(r"(\S+\.o):\(")

USAGE_EXIT = 2


def read_assignments(path: Path) -> dict[str, str]:
    """Return the symbol assignments of the linker script at PATH."""
    text = COMMENT.sub("", path.read_text())
    return {
        match.group(1): match.group(2).strip() for match in ASSIGNMENT.finditer(text)
    }


def read_int(entry: dict[object, object], key: str, default: int | None = None) -> int:
    """Return ENTRY's integer at KEY, or DEFAULT where it has none."""
    value = entry.get(key, default)
    if not isinstance(value, int):
        sys.exit(f"expected an integer {key} in {entry}")
    return value


def read_segment_ranges(config_path: Path) -> list[tuple[int, int]]:
    """Return the ROM and RAM ranges of the original's segments.

    CONFIG_PATH is splat's configuration. A segment's ROM ends where the next
    begins, and its RAM, where it has an address there, holds its ROM and
    then its bss.
    """
    config = cast("object", yaml.safe_load(config_path.read_text()))
    segments = (
        cast("dict[object, object]", config).get("segments")
        if isinstance(config, dict)
        else None
    )
    if not isinstance(segments, list):
        sys.exit(f"{config_path} lists no segments")
    entries: list[dict[object, object]] = []
    for segment in cast("list[object]", segments):
        if isinstance(segment, dict):
            entries.append(cast("dict[object, object]", segment))
        elif isinstance(segment, list):
            entries.append({"start": cast("list[object]", segment)[0]})
    ranges: list[tuple[int, int]] = []
    for entry, following in itertools.pairwise(entries):
        start, end = read_int(entry, "start"), read_int(following, "start")
        ranges.append((start, end))
        if "vram" in entry:
            vram = read_int(entry, "vram")
            ranges.append((vram, vram + end - start + read_int(entry, "bss_size", 0)))
    return ranges


def is_pin(value: str, segment_ranges: list[tuple[int, int]]) -> bool:
    """Return whether VALUE, an assignment's expression, is a pinned address.

    An expression with a symbol in it moves with that symbol.
    """
    if not NUMBER.fullmatch(value):
        return False
    address = int(value, 0)
    return any(start <= address < end for start, end in segment_ranges)


def find_unresolved(ld: str, ld_args: list[str]) -> dict[str, set[str]]:
    """Link with LD_ARGS and return each undefined symbol's referrers.

    Each referrer is an object, with the routine that refers to the symbol
    where ld names one.
    """
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "unpinned.elf"
        result = subprocess.run(
            [ld, *ld_args, "-o", str(output)],
            capture_output=True,
            text=True,
            check=False,
        )
    referrers: dict[str, set[str]] = defaultdict(set)
    routine = ""
    for line in result.stderr.splitlines():
        if match := IN_FUNCTION.search(line):
            routine = f"{object_name(match[1])}: {match[2]}"
        elif match := UNDEFINED.search(line):
            if data := IN_DATA.search(line):
                routine = object_name(data[1])
            referrers[match[1]].add(routine)
    return referrers


def object_name(path: str) -> str:
    """Return the source file that the object at PATH was built from."""
    return Path(path).name.removesuffix(".o")


def main(args: list[str]) -> int:
    """Run the check on ARGS, the command line, and return the exit status."""
    if "--" in args:
        split = args.index("--")
        args, ld_flags = args[:split], args[split + 1 :]
    else:
        ld_flags = []
    if len(args) < 3:  # noqa: PLR2004 - LD, CONFIG and the linker script
        print(__doc__, file=sys.stderr)
        return USAGE_EXIT
    ld, config, ld_script, *symbol_scripts = args

    segment_ranges = read_segment_ranges(Path(config))
    fixed: dict[str, str] = {}
    pins: dict[str, str] = {}
    for script in symbol_scripts:
        for name, value in read_assignments(Path(script)).items():
            (pins if is_pin(value, segment_ranges) else fixed)[name] = value

    with tempfile.NamedTemporaryFile("w", suffix=".ld") as fixed_script:
        fixed_script.writelines(f"{name} = {value};\n" for name, value in fixed.items())
        fixed_script.flush()
        unresolved = find_unresolved(
            ld, ["-T", ld_script, "-T", fixed_script.name, *ld_flags]
        )

    pinned = {name: referrers for name, referrers in unresolved.items() if name in pins}
    for name in sorted(pinned, key=lambda name: int(pins[name], 0)):
        print(f"{pins[name]:>10} {name}")
        for referrer in sorted(pinned[name]):
            print(f"{'':>11}{referrer}")
    others = sorted(set(unresolved) - set(pinned))
    for name in others:
        print(f"{name} is defined nowhere", file=sys.stderr)
    print(f"{len(pinned)} pinned symbols referenced", file=sys.stderr)
    return 1 if pinned or others else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
