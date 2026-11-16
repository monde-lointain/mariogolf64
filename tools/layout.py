#!/usr/bin/env python3
"""Write the linker scripts and the list of objects from layout.txt.

    layout.py objects LAYOUT
    layout.py matching LAYOUT BUILD_DIR
    layout.py shiftable LAYOUT BUILD_DIR [SEGMENT_SCRIPT]

LAYOUT lists what the ROM holds, in order (see layout.txt for its form).
"objects" prints the Make variables that name every object it places.
"matching" prints the script of the original's layout, and "shiftable" that
of the layout that lets code and data grow, for the objects under
BUILD_DIR. SEGMENT_SCRIPT, such as the test ROM's segment, goes before the
script's end, which discards whatever no statement has placed.

The matching layout places each segment where the original has it. Every
segment of code has its code, data and read-only data in RAM at its address,
each kind of section of each object in turn, and then its bss, after which
the next segment's ROM starts on a 16-byte boundary. The files, such as the
assets, follow each other in ROM.

The shiftable layout lets the code and data grow. The overlays share three
regions of base RAM after main, at fixed addresses in the original. Here
each region starts after the largest overlay of the region before it, the
first after main, with a gap: the overlay manager takes overlays whose
ranges touch for overlays that overlap, and so never loads one while the
other is loaded. The regions must end by the start of the game's main heap,
which keeps the original's size and address, so that the allocator behaves
the same in every layout; it is read from include/system/heap.h. Segments
elsewhere, such as an overlay of the Expansion Pak's memory, keep their
addresses. In ROM, each segment follows the one before it, in the order of
the regions, as the code takes every ROM address from the script's symbols.
Each segment after main starts on a 16-byte boundary, as every one of the
original's does: a file that grows by other than a multiple of 16, such as a
custom character's model, would otherwise leave the files after it where the
cartridge's DMA, which reads from even addresses only, and the code that
assumes the original's alignment can't take them.

The shiftable layout leaves out the objects that only the matching one
links, such as main_bss.c, which defines main's common objects at the
original's addresses. Each object of a segment that doesn't list where its
commons go keeps them with its bss instead.

A build may have the linker collect the sections that nothing refers to
(--gc-sections). The code reaches many of the segments, such as the asset
files and the build date, only through their addresses in ROM, which the
collector can't follow. So in the shiftable layout every object's whole
sections, those that the assembler and binary objects hold, are kept (KEEP),
and only the ones that -ffunction-sections and -fdata-sections split off,
each named for one routine or object (.text.NAME), may be collected.

The routines that the build links from archives, such as the libgcc helpers
that a modern GCC calls, go at the end of main's code, where every segment
can call them, and their bss at the end of main's. The sanitized build's
checks (make SANITIZE=1) pass their handlers the address of GCC's
description of each, which only tools/sanitize.py reads. The descriptions go
in a section that the ELF keeps and the ROM leaves out, at an address where
no memory is, so that the checked code fits. In other builds, no section
matches, and the section is left out.

Each segment defines the symbols that the code reads its bounds by:
NAME_ROM_START and NAME_ROM_END, and for a segment of code, NAME_VRAM,
NAME_TEXT_END, NAME_BSS_START, NAME_BSS_END, NAME_BSS_SIZE and NAME_VRAM_END.
"""

import re
import sys
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import final

from heap_region import read_main_region

ALIGNMENT = 16
# Where the Expansion Pak's memory begins; regions below it are packed
BASE_RDRAM_END = 0x80400000
KSEG0_START = 0x80000000

# The kinds of section that an object's line may name, in the order in which
# a segment places them, and what each places of an object
KINDS = ("text", "data", "rodata", "bss", "common")
PLAIN_KINDS = frozenset(("text", "data", "rodata", "bss"))
PATTERNS = {
    "text": ".text*",
    "data": ".data*",
    "rodata": ".rodata*",
    "bss": ".bss*",
    "common": "COMMON .scommon*",
}
# The word that marks an object only the original's layout links
MATCHING_ONLY = "matching"
FILE_PATTERN = ".data*"

ARCHIVE_SECTIONS = "*.a:*(.text* .rodata* .data*);"
ARCHIVE_BSS = "*.a:*(.bss* COMMON .scommon*);"
# KSEG0's view of the cartridge's domain, which holds no RDRAM, so that no
# address of a sanitizer's description is taken for the game's
SANITIZER_DESCRIPTORS = """\
    HIDDEN(sanitizer_descriptors_LOCATION = .);
    .sanitizer_descriptors 0x90000000 (INFO) :
    {
        *(.data.$Lubsan_* .bss.$Lubsan_*);
    }
    . = sanitizer_descriptors_LOCATION;"""
HEAP_MESSAGE = "The overlays reach into the main heap: see include/system/heap.h"

USAGE = (__doc__ or "").split("\n\n")[1]
USAGE_EXIT = 2
NAME = re.compile(r"[A-Za-z_]\w*")


class LayoutError(Exception):
    """A line of the layout that can't be read."""


@final
@dataclass(frozen=True)
class Placement:
    """Where an object's sections of each of KINDS go in its segment."""

    path: str
    kinds: frozenset[str]
    matching_only: bool = False


@final
@dataclass(frozen=True)
class Pad:
    """A gap of SIZE bytes among a segment's sections of KIND."""

    size: int
    kind: str


@final
@dataclass(frozen=True)
class Segment:
    """Code and data loaded at ADDRESS in RAM, placed as ENTRIES list."""

    name: str
    address: int
    entries: tuple[Placement | Pad, ...]


@final
@dataclass(frozen=True)
class File:
    """A file that the ROM holds as it is, such as an asset."""

    path: str

    @property
    def name(self) -> str:
        """Return the name of the file's symbols: its path below the top folder."""
        below = self.path.split("/", 1)[-1]
        return below.split(".", 1)[0].replace("/", "_")


Layout = list[Segment | File]


def _parse_kinds(words: list[str], where: str) -> tuple[frozenset[str], bool]:
    matching_only = MATCHING_ONLY in words
    kinds = [word for word in words if word != MATCHING_ONLY]
    unknown = [kind for kind in kinds if kind not in KINDS]
    if unknown:
        msg = f"{where}: {unknown[0]!r} isn't a kind of section: {', '.join(KINDS)}"
        raise LayoutError(msg)
    return (frozenset(kinds) if kinds else PLAIN_KINDS), matching_only


def _parse_number(word: str, where: str) -> int:
    try:
        return int(word, 0)
    except ValueError:
        msg = f"{where}: {word!r} isn't a number"
        raise LayoutError(msg) from None


def _parse_segment(arguments: list[str], where: str) -> tuple[str, int]:
    """Return the name and address of a segment line's ARGUMENTS."""
    if len(arguments) != 2 or not NAME.fullmatch(arguments[0]):  # noqa: PLR2004 - a name and an address
        msg = f"{where}: expected segment NAME ADDRESS"
        raise LayoutError(msg)
    return arguments[0], _parse_number(arguments[1], where)


def _parse_file(arguments: list[str], where: str) -> File:
    """Return the file of a file line's ARGUMENTS."""
    if len(arguments) != 1:
        msg = f"{where}: expected file PATH"
        raise LayoutError(msg)
    return File(arguments[0])


def _parse_entry(keyword: str, arguments: list[str], where: str) -> Placement | Pad:
    """Return what a line of a segment, starting with KEYWORD, places."""
    if keyword != "pad":
        kinds, matching_only = _parse_kinds(arguments, where)
        return Placement(keyword, kinds, matching_only)
    if len(arguments) != 2 or arguments[1] not in KINDS:  # noqa: PLR2004 - a size and a kind
        msg = f"{where}: expected pad SIZE KIND, KIND one of {', '.join(KINDS)}"
        raise LayoutError(msg)
    return Pad(_parse_number(arguments[0], where), arguments[1])


def parse(text: str, source: str = "layout") -> Layout:
    """Read a layout from TEXT, naming SOURCE in its errors."""
    layout: Layout = []
    segment: tuple[str, int] | None = None
    entries: list[Placement | Pad] = []

    def finish_segment() -> None:
        if segment is not None:
            layout.append(Segment(*segment, tuple(entries)))

    for number, line in enumerate(text.splitlines(), start=1):
        where = f"{source}:{number}"
        words = line.split("#", 1)[0].split()
        if not words:
            continue
        keyword, arguments = words[0], words[1:]
        if keyword == "segment":
            finish_segment()
            segment, entries = _parse_segment(arguments, where), []
        elif keyword == "file":
            finish_segment()
            segment = None
            layout.append(_parse_file(arguments, where))
        elif segment is None:
            msg = f"{where}: {keyword!r} comes before any segment"
            raise LayoutError(msg)
        else:
            entries.append(_parse_entry(keyword, arguments, where))
    finish_segment()
    return layout


def read(path: Path) -> Layout:
    """Read the layout at PATH."""
    return parse(path.read_text(encoding="utf-8"), str(path))


def objects(layout: Layout) -> tuple[list[str], list[str]]:
    """Return the paths of the objects that LAYOUT places, and the matching's own."""
    every: dict[str, None] = {}
    matching: dict[str, None] = {}
    for item in layout:
        if isinstance(item, File):
            every[item.path] = None
            continue
        for entry in item.entries:
            if isinstance(entry, Placement):
                every[entry.path] = None
                if entry.matching_only:
                    matching[entry.path] = None
    return list(every), list(matching)


def segment_objects(segment: Segment) -> list[str]:
    """Return the paths of the objects that the shiftable layout places in SEGMENT.

    They come in the order in which the segment first places each.
    """
    paths: dict[str, None] = {}
    for kind in KINDS:
        for entry in segment.entries:
            if (
                isinstance(entry, Placement)
                and not entry.matching_only
                and kind in entry.kinds
            ):
                paths[entry.path] = None
    return list(paths)


# A statement that places sections of one object, by its path and patterns
Statement = tuple[str, str]


def _keep(statement: Statement) -> list[str]:
    """Return the lines that keep STATEMENT's whole sections from collection.

    A pattern such as .text* becomes .text and .text[!.]*, which also matches a
    section of its own such as .text_messages, kept, and .text.*, which may be
    collected.
    """
    path, patterns = statement
    kept: list[str] = []
    split: list[str] = []
    for pattern in patterns.split():
        if pattern.startswith(".") and pattern.endswith("*"):
            name = pattern.removesuffix("*")
            kept += [name, f"{name}[!.]*"]
            split.append(f"{name}.*")
        else:
            kept.append(pattern)
    lines = [f"KEEP({path}({' '.join(kept)}));"]
    if split:
        lines.append(f"{path}({' '.join(split)});")
    return lines


@final
@dataclass(frozen=True)
class _Style:
    """How a script names and places the objects."""

    build_dir: str
    shiftable: bool
    # The objects that each segment's LTO link makes, by segment
    linked: dict[str, str]

    def path(self, source: str) -> str:
        return f"{self.build_dir}/{source}.o"


def _segment_statements(
    segment: Segment, kind: str, style: _Style
) -> Iterator[Statement | Pad]:
    """Yield what SEGMENT places among its sections of KIND, in order."""
    lists_commons = any(
        isinstance(entry, Placement) and "common" in entry.kinds
        for entry in segment.entries
    )
    for entry in segment.entries:
        if isinstance(entry, Pad):
            if entry.kind == kind:
                yield entry
            continue
        if entry.matching_only and style.shiftable:
            continue
        if kind in entry.kinds:
            yield style.path(entry.path), PATTERNS[kind]
            if kind == "bss" and style.shiftable and not lists_commons:
                yield style.path(entry.path), PATTERNS["common"]


def _place(
    segment: Segment, kinds: tuple[str, ...], style: _Style, seen: set[Statement]
) -> list[str]:
    """Return the lines that place SEGMENT's sections of KINDS."""
    lines: list[str] = []
    linked = style.linked.get(segment.name)
    for kind in kinds:
        for statement in _segment_statements(segment, kind, style):
            if isinstance(statement, Pad):
                lines.append(f". += 0x{statement.size:X};")
                continue
            if linked is not None:
                statement = (linked, statement[1])  # noqa: PLW2901 - the segment's object places all of its objects'
                if statement in seen:
                    continue
                seen.add(statement)
            if style.shiftable:
                lines += _keep(statement)
            else:
                lines.append(f"{statement[0]}({statement[1]});")
    return lines


def _indent(lines: Iterable[str], depth: int) -> str:
    return "".join(f"{'    ' * depth}{line}\n" for line in lines)


def _code_segment(segment: Segment, address: str, style: _Style, *, main: bool) -> str:
    """Return the statements that place SEGMENT, at ADDRESS in RAM."""
    name = segment.name
    seen: set[Statement] = set()
    end = f". = ALIGN(., {ALIGNMENT});"
    archives = main and style.shiftable
    loaded = [
        *_place(segment, ("text",), style, seen),
        end,
        *([ARCHIVE_SECTIONS] if archives else []),
        f"{name}_TEXT_END = .;",
        *_place(segment, ("data",), style, seen),
        end,
        *_place(segment, ("rodata",), style, seen),
        end,
    ]
    bss = [
        f"{name}_BSS_START = .;",
        *_place(segment, ("bss", "common"), style, seen),
        *([ARCHIVE_BSS] if archives else []),
        end,
        f"{name}_BSS_END = .;",
        f"{name}_BSS_SIZE = ABSOLUTE({name}_BSS_END - {name}_BSS_START);",
    ]
    return (
        _indent(
            [
                f"{name}_ROM_START = __romPos;",
                f"{name}_VRAM = ADDR(.{name});",
                f".{name} {address} : AT({name}_ROM_START)",
                "{",
            ],
            1,
        )
        + _indent(loaded, 2)
        + _indent(["}", f".{name}_bss (NOLOAD) :", "{"], 1)
        + _indent(bss, 2)
        + _indent(
            [
                "}",
                f"__romPos += SIZEOF(.{name});",
                f"__romPos = ALIGN(__romPos, {ALIGNMENT});",
                f"{name}_ROM_END = __romPos;",
                f"{name}_VRAM_END = .;",
            ],
            1,
        )
    )


def _file_segment(file: File, style: _Style) -> str:
    """Return the statements that place FILE in ROM."""
    name = file.name
    statement = (style.path(file.path), FILE_PATTERN)
    placed = (
        _keep(statement) if style.shiftable else [f"{statement[0]}({FILE_PATTERN});"]
    )
    return (
        _indent(
            [f"{name}_ROM_START = __romPos;", f".{name} : AT({name}_ROM_START)", "{"], 1
        )
        + _indent(placed, 2)
        + _indent(
            ["}", f"__romPos += SIZEOF(.{name});", f"{name}_ROM_END = __romPos;"], 1
        )
    )


def _max_of(symbols: list[str]) -> str:
    """Return a linker expression for the largest of SYMBOLS."""
    expression = symbols[-1]
    for symbol in reversed(symbols[:-1]):
        expression = f"MAX({symbol}, {expression})"
    return expression


def _wrap(blocks: list[str], segment_script: str) -> str:
    end = "    /DISCARD/ :\n    {\n        *(*);\n    }\n"
    return "SECTIONS\n{\n" + "\n".join(blocks) + "\n" + segment_script + end + "}\n"


def _first_segment(layout: Layout) -> int:
    index = next(
        (i for i, item in enumerate(layout) if isinstance(item, Segment)), None
    )
    if index is None:
        msg = "the layout has no segment of code"
        raise LayoutError(msg)
    return index


def matching_script(layout: Layout, build_dir: str) -> str:
    """Return the script that places LAYOUT's objects as the original does."""
    style = _Style(build_dir, shiftable=False, linked={})
    main = _first_segment(layout)
    blocks = ["    HIDDEN(__romPos = 0);\n"]
    for index, item in enumerate(layout):
        if isinstance(item, File):
            blocks.append(_file_segment(item, style))
        else:
            address = f"0x{item.address:08X}"
            blocks.append(_code_segment(item, address, style, main=index == main))
    return _wrap(blocks, "")


def shiftable_script(
    layout: Layout,
    build_dir: str,
    heap_start: int,
    segment_script: str = "",
    linked: dict[str, str] | None = None,
) -> str:
    """Return the script that lets LAYOUT's objects under BUILD_DIR grow.

    The overlays' regions must end by HEAP_START, where the main heap begins.
    SEGMENT_SCRIPT goes before the end. LINKED names the object that each
    segment's LTO link makes, which places all of the segment's objects.
    """
    style = _Style(build_dir, shiftable=True, linked=linked or {})
    main_index = _first_segment(layout)
    main = layout[main_index]
    assert isinstance(main, Segment)  # noqa: S101 - _first_segment found a Segment

    def aligned(block: str) -> str:
        return f"    __romPos = ALIGN(__romPos, {ALIGNMENT});\n" + block

    blocks = [SANITIZER_DESCRIPTORS, "    HIDDEN(__romPos = 0);\n"]
    blocks += [
        _file_segment(item, style)
        for item in layout[:main_index]
        if isinstance(item, File)
    ]
    blocks.append(_code_segment(main, f"0x{main.address:08X}", style, main=True))

    later = layout[main_index + 1 :]
    regions: dict[int, list[Segment]] = {}
    elsewhere: list[Segment] = []
    for item in later:
        if isinstance(item, Segment):
            if KSEG0_START <= item.address < BASE_RDRAM_END:
                regions.setdefault(item.address, []).append(item)
            else:
                elsewhere.append(item)
    previous_ends = [f"{main.name}_VRAM_END"]
    for number, address in enumerate(sorted(regions), start=1):
        symbol = f"overlay_region_{number}_VRAM"
        # One byte past the end keeps CanLoadOverlay from counting the
        # regions as overlapping, as it compares their bounds inclusively
        start = f"ALIGN({_max_of(previous_ends)} + 1, {ALIGNMENT})"
        blocks.append(f"    {symbol} = {start};\n")
        blocks += [
            aligned(_code_segment(segment, symbol, style, main=False))
            for segment in regions[address]
        ]
        previous_ends = [f"{overlay.name}_VRAM_END" for overlay in regions[address]]
    # ld takes an ASSERT among the sections only without a semicolon
    end = f"ALIGN({_max_of(previous_ends)}, {ALIGNMENT})"
    check = f'ASSERT(overlay_regions_VRAM_END <= 0x{heap_start:X}, "{HEAP_MESSAGE}")'
    blocks.append(f"    overlay_regions_VRAM_END = {end};\n    {check}\n")
    # Segments outside base RAM keep their addresses
    blocks += [
        aligned(_code_segment(segment, f"0x{segment.address:08X}", style, main=False))
        for segment in elsewhere
    ]
    blocks += [
        aligned(_file_segment(item, style)) for item in later if isinstance(item, File)
    ]
    return _wrap(blocks, segment_script)


def main(arguments: list[str]) -> int:
    """Print what the command line asks for; return the exit status."""
    command, rest = (arguments[0], arguments[1:]) if arguments else ("", [])
    try:
        if command == "objects" and len(rest) == 1:
            every, matching = objects(read(Path(rest[0])))
            print(f"LAYOUT_OBJECTS := {' '.join(every)}")
            print(f"MATCHING_ONLY_OBJECTS := {' '.join(matching)}")
        elif command == "matching" and len(rest) == 2:  # noqa: PLR2004 - LAYOUT and BUILD_DIR
            print(matching_script(read(Path(rest[0])), rest[1]), end="")
        elif command == "shiftable" and len(rest) in {2, 3}:
            extra = Path(rest[2]).read_text(encoding="utf-8") if len(rest) == 3 else ""  # noqa: PLR2004 - with SEGMENT_SCRIPT
            script = shiftable_script(
                read(Path(rest[0])), rest[1], read_main_region().start, extra
            )
            print(script, end="")
        else:
            print(f"usage:\n{USAGE}", file=sys.stderr)
            return USAGE_EXIT
    except LayoutError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
