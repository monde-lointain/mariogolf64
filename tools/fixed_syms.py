#!/usr/bin/env python3
"""Drop the symbol assignments that pin addresses inside the original's segments.

    fixed_syms.py CONFIG SCRIPT... > FIXED_SCRIPT

The matching build links with scripts that give symbols fixed addresses,
such as undefined_syms.txt. An address inside a segment, in RAM or in ROM,
only holds while the code and data before it keep their sizes: such an
assignment is a pin, which a ROM whose code can move must do without.
Addresses outside every segment, such as the boot segment's variables in
low memory, stay where they are.

This writes the SCRIPTs again without their pins, for the shiftable build
to link with. A reference that still needs a pin is then left undefined,
and ld names the routine that makes it. A pin must also go where an object
defines the symbol, since ld takes a script's address over the object's.
CONFIG, splat's configuration, gives the original's segments.
"""

import itertools
import re
import sys
from pathlib import Path
from typing import cast

import yaml

# An assignment of a number in a linker script: NAME = NUMBER;
NUMBER_ASSIGNMENT = re.compile(
    r"^\s*[A-Za-z_.$][\w.$]*\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)\s*;\s*$"
)

USAGE_EXIT = 2


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


def is_pin(line: str, segment_ranges: list[tuple[int, int]]) -> bool:
    """Return whether LINE assigns a symbol an address inside a segment.

    An expression with a symbol in it moves with that symbol.
    """
    match = NUMBER_ASSIGNMENT.match(line)
    if not match:
        return False
    address = int(match.group(1), 0)
    return any(start <= address < end for start, end in segment_ranges)


def main(args: list[str]) -> int:
    """Write the scripts named in ARGS without their pins."""
    if len(args) < 2:  # noqa: PLR2004 - CONFIG and a script
        print(__doc__, file=sys.stderr)
        return USAGE_EXIT
    config, *scripts = args
    segment_ranges = read_segment_ranges(Path(config))
    for script in scripts:
        _ = sys.stdout.write(f"/* {script} */\n")
        _ = sys.stdout.writelines(
            line
            for line in Path(script).read_text().splitlines(keepends=True)
            if not is_pin(line, segment_ranges)
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
