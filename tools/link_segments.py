#!/usr/bin/env python3
"""Link each segment's objects into one with LTO, and lay those out instead.

    link_segments.py LAYOUT BUILD_DIR OUT_DIR -- COMPILER FLAGS...

The objects under BUILD_DIR that a segment of LAYOUT places (see layout.py),
compiled with -flto, are partially linked in the order in which the
shiftable layout places them, by COMPILER with FLAGS, into
OUT_DIR/SEGMENT.o. The link compiles the segment's bytecode at once, so
that routines inline and constants fold across the segment's files, but not
into another segment, which the game loads at another time, so that each
overlay still holds its own code. A partial link keeps every global, which
the other segments may use. The objects that it doesn't compile, such as
the assembly's, come first in each of its sections, in the layout's order,
so that main's entry point still starts its code.

OUT_DIR/segments.ld is the shiftable layout's script with each such
segment's object in place of its files. The segments with no C objects,
such as the assets', which nothing compiles together, still name their
files.
"""

import subprocess
import sys
from pathlib import Path

import layout

from heap_region import read_main_region

SEPARATOR = "--"
# The layout and the two directories come before the separator
SEPARATOR_INDEX = 3
USAGE = (
    f"usage: link_segments.py LAYOUT BUILD_DIR OUT_DIR {SEPARATOR} COMPILER FLAGS..."
)
SCRIPT_NAME = "segments.ld"
OBJECT_SUFFIX = ".o"
C_SUFFIX = ".c"
# A partial link's output is an ordinary object, compiled from the
# bytecode, rather than more bytecode
PARTIAL_LINK_FLAGS = ("-r", "-nostdlib", "-flinker-output=nolto-rel", "-flto=auto")


def main() -> int:
    """Link the segments that the command line names; return the exit status."""
    arguments = sys.argv[1:]
    if SEPARATOR not in arguments or arguments.index(SEPARATOR) != SEPARATOR_INDEX:
        print(USAGE, file=sys.stderr)
        return 2
    layout_path, build_dir, out_dir = arguments[:SEPARATOR_INDEX]
    compiler = arguments[SEPARATOR_INDEX + 1 :]
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for stale in out.glob(f"*{OBJECT_SUFFIX}"):
        stale.unlink()
    try:
        rom = layout.read(Path(layout_path))
    except layout.LayoutError as error:
        print(error, file=sys.stderr)
        return 1
    linked: dict[str, str] = {}
    links: list[subprocess.Popen[bytes]] = []
    for segment in rom:
        if not isinstance(segment, layout.Segment):
            continue
        sources = layout.segment_objects(segment)
        if not any(source.endswith(C_SUFFIX) for source in sources):
            continue
        segment_object = f"{out_dir}/{segment.name}{OBJECT_SUFFIX}"
        objects = [f"{build_dir}/{source}{OBJECT_SUFFIX}" for source in sources]
        command = [*compiler, *PARTIAL_LINK_FLAGS, "-o", segment_object, *objects]
        links.append(subprocess.Popen(command))
        linked[segment.name] = segment_object
    failures = sum(1 for link in links if link.wait() != 0)
    if failures:
        return 1
    script = layout.shiftable_script(
        rom, build_dir, read_main_region().start, linked=linked
    )
    _ = (out / SCRIPT_NAME).write_text(script)
    return 0


if __name__ == "__main__":
    sys.exit(main())
