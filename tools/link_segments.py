#!/usr/bin/env python3
"""Link each segment's objects into one with LTO, and place those instead.

    link_segments.py SCRIPT OUT_DIR -- COMPILER FLAGS...

SCRIPT is a linker script of the shiftable layout (see shiftable_ld.py),
which places each object by name. The objects that a segment of code
places, compiled with -flto, are partially linked in the script's order,
by COMPILER with FLAGS, into OUT_DIR/SEGMENT.o. The link compiles the
segment's bytecode at once, so that routines inline and constants fold
across the segment's files, but not into another segment, which the game
loads at another time, so that each overlay still holds its own code.
A partial link keeps every global, which the other segments may use.

OUT_DIR/segments.ld is SCRIPT with each such segment's statements naming
its object in place of its files. The statements of a segment place each
kind of section of each file in turn, so that once each names the
segment's object, the first of each kind places them all, and the rest,
which would place nothing more, are left out, as are the markers that
splat writes of where each file's data starts, which the segment's object
has no such place for. The segment's own symbols, which splat names after
it, stay. The segments with no C objects, such as the assets', which
nothing compiles together, still name their files.

The objects that the partial link doesn't compile, such as the assembly,
come first in each of its sections, in the script's order, so that main's
entry point still starts its code.
"""

import re
import subprocess
import sys
from pathlib import Path

SUMMARY = "Link each segment's objects into one with LTO, and place those instead."

SEPARATOR = "--"
# The script and the output directory come before the separator
SEPARATOR_INDEX = 2
USAGE = f"usage: link_segments.py SCRIPT OUT_DIR {SEPARATOR} COMPILER FLAGS..."
SCRIPT_NAME = "segments.ld"
OBJECT_SUFFIX = ".o"
COMPILED_SUFFIX = ".c.o"
# A partial link's output is an ordinary object, compiled from the
# bytecode, rather than more bytecode
PARTIAL_LINK_FLAGS = ("-r", "-nostdlib", "-flinker-output=nolto-rel", "-flto=auto")

# Each segment's statements start with the assignment of its ROM address
SEGMENT_START = re.compile(r"^\s*(?P<name>\w+)_ROM_START = __romPos;$", re.MULTILINE)
# A symbol that a statement sets to where it is
MARKER = re.compile(r"^\s*(?P<name>\w+) = \.;$")
# A statement that places sections of an object by name, kept or not
PLACEMENT = re.compile(
    r"""
    ^(?P<indent>\s*)(?P<keep>KEEP\()?(?P<file>[^\s(*]+\.o)
    (?P<sections>\(.*\))(?P<end>\)?;)$
    """,
    re.VERBOSE,
)


def placed_files(block: str) -> list[str]:
    """Return the files that BLOCK's statements place, in their order."""
    files = (PLACEMENT.match(line) for line in block.splitlines())
    return list(dict.fromkeys(match.group("file") for match in files if match))


def segment_statement(line: str, segment_object: Path) -> str:
    """Return LINE placing SEGMENT_OBJECT, if LINE places a file."""
    match = PLACEMENT.match(line.rstrip("\n"))
    if not match:
        return line
    return "".join(
        (
            match.group("indent"),
            match.group("keep") or "",
            str(segment_object),
            match.group("sections"),
            match.group("end"),
            "\n",
        )
    )


def is_file_marker(line: str, segment: str) -> bool:
    """Whether LINE marks where a file of SEGMENT starts, rather than SEGMENT."""
    match = MARKER.match(line.rstrip("\n"))
    return bool(match) and not match.group("name").startswith(f"{segment}_")


def segment_statements(block: str, segment: str, segment_object: Path) -> str:
    """Return SEGMENT's BLOCK with its statements placing SEGMENT_OBJECT once each."""
    lines: list[str] = []
    for line in block.splitlines(keepends=True):
        if is_file_marker(line, segment):
            continue
        statement = segment_statement(line, segment_object)
        if statement == line or statement not in lines:
            lines.append(statement)
    return "".join(lines)


def split_segments(script: str) -> tuple[str, list[tuple[str, str]]]:
    """Return SCRIPT's text before its segments, and each segment's name and text."""
    starts = list(SEGMENT_START.finditer(script))
    ends = [match.start() for match in starts[1:]] + [len(script)]
    segments = [
        (match.group("name"), script[match.start() : end])
        for match, end in zip(starts, ends, strict=True)
    ]
    return script[: starts[0].start()] if starts else script, segments


def main() -> int:
    """Link the segments that the command line names; return the exit status."""
    arguments = sys.argv[1:]
    if SEPARATOR not in arguments or arguments.index(SEPARATOR) != SEPARATOR_INDEX:
        print(USAGE, file=sys.stderr)
        return 2
    script, out_dir = Path(arguments[0]), Path(arguments[1])
    compiler = arguments[3:]
    out_dir.mkdir(parents=True, exist_ok=True)
    for stale in out_dir.glob(f"*{OBJECT_SUFFIX}"):
        stale.unlink()
    head, segments = split_segments(script.read_text())
    pieces = [head]
    links: list[subprocess.Popen[bytes]] = []
    for name, block in segments:
        files = placed_files(block)
        if not any(file.endswith(COMPILED_SUFFIX) for file in files):
            pieces.append(block)
            continue
        segment_object = out_dir / f"{name}{OBJECT_SUFFIX}"
        command = [*compiler, *PARTIAL_LINK_FLAGS, "-o", str(segment_object), *files]
        links.append(subprocess.Popen(command))
        pieces.append(segment_statements(block, name, segment_object))
    failures = sum(1 for link in links if link.wait() != 0)
    if failures:
        return 1
    _ = (out_dir / SCRIPT_NAME).write_text("".join(pieces))
    return 0


if __name__ == "__main__":
    sys.exit(main())
