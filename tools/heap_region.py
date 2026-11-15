#!/usr/bin/env python3
"""Read the main heap's region from include/system/heap.h.

    heap_region.py

The game defines the region there, and the tools that check the layout
against it read it from there, so that they follow it when it changes. Run
alone, it prints the region.
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import final

HEAP_HEADER = Path(__file__).resolve().parent.parent / "include" / "system" / "heap.h"
REGION_END = "MAIN_REGION_END"
REGION_SIZE = "MAIN_REGION_SIZE"
REGION_DEFINE = re.compile(
    r"""
    ^\#define \s+ (?P<name>MAIN_REGION_(?:END|SIZE))
    \s+ (?P<value>0x[0-9A-Fa-f]+) \s*$
    """,
    re.MULTILINE | re.VERBOSE,
)


@final
@dataclass(frozen=True)
class MainRegion:
    """The memory that the game gives its main heap."""

    end: int
    size: int

    @property
    def start(self) -> int:
        """Where the region begins."""
        return self.end - self.size


def read_main_region(header: Path = HEAP_HEADER) -> MainRegion:
    """Read the main heap's region from HEADER's definitions."""
    values = {
        match.group("name"): int(match.group("value"), 16)
        for match in REGION_DEFINE.finditer(header.read_text(encoding="utf-8"))
    }
    missing = [name for name in (REGION_END, REGION_SIZE) if name not in values]
    if missing:
        message = f"{header}: no hexadecimal #define of {', '.join(missing)}"
        raise ValueError(message)
    return MainRegion(end=values[REGION_END], size=values[REGION_SIZE])


def main() -> None:
    """Print the main heap's region."""
    region = read_main_region()
    bounds = f"0x{region.start:08X} to 0x{region.end:08X}"
    _ = sys.stdout.write(f"main heap: {bounds}, 0x{region.size:X} bytes\n")


if __name__ == "__main__":
    main()
