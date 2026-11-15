#!/usr/bin/env python3
"""Lay out splat's linker script so that code and data can grow.

    shiftable_ld.py < SPLAT_SCRIPT > SHIFTABLE_SCRIPT

splat's script places every segment where the original has it. The overlays
share three regions of base RAM after main, at fixed addresses, and in ROM
they lie between main and the build date, which the asset files follow.

The shiftable script packs each region of base RAM after the largest
overlay of the region before it, the first after main, with a gap: the
overlay manager takes overlays whose ranges touch for overlays that
overlap, and so never loads one while the other is loaded. It defines
overlay_regions_VRAM_END after the last, which must not pass the start of
the game's main heap: the heap keeps the original's size and address, so
that the allocator behaves the same in every layout. In ROM,
each segment follows the one before it, in the original's order, as the
code takes every ROM address from the script's symbols. Other segments,
such as an overlay of the Expansion Pak's memory, keep their addresses in
RAM.

The routines that the build links from archives, such as the libgcc
helpers that a modern GCC calls, go at the end of main's code, where
every segment can call them, and their bss at the end of main's.

splat places an overlay's one object's bss as .bss, and every other bss
as .bss*. Here each is .bss*, so that a section named .bss.NAME, as from
an attribute or -fdata-sections, lands with the rest of its object's
bss rather than in the script's /DISCARD/.

The matching build defines main's common objects in one file at the
original's addresses, which this layout leaves out. Each object of main
keeps its commons with its bss instead, as the overlays' objects do.

A build may have the linker collect the sections that nothing refers to
(--gc-sections). The code reaches many of the segments, such as the asset
files and the build date, only through their addresses in ROM, which the
collector can't follow. So every object's whole sections, those that the
assembler and binary objects hold, are kept (KEEP), and only the ones that
-ffunction-sections and -fdata-sections split off, each named for one
routine or object (.text.NAME), may be collected. A pattern such as .text*
becomes .text and .text[!.]*, which also matches a section of its own such
as .text_messages, kept, and .text.*, which may be collected. Without
--gc-sections, KEEP collects nothing, but each object's dotted sections,
such as the merged constants of .rodata.cst4, now follow its whole ones.
"""

import re
import sys

# Where the Expansion Pak's memory begins; regions below it are packed
BASE_RDRAM_END = 0x80400000
KSEG0_START = 0x80000000
ALIGNMENT = 16
# Where the main heap begins: MAIN_REGION_START in include/system/heap.h
MAIN_REGION_START = 0x8025D800

MAIN = "main"
BUILD_DATE = "build_date"

MAIN_TEXT_END = f"        {MAIN}_TEXT_END = .;\n"
ARCHIVE_SECTIONS = "        *.a:*(.text* .rodata* .data*);\n"
ARCHIVE_BSS = "        *.a:*(.bss* COMMON .scommon*);\n"
# The end of main's bss, which splat aligns
MAIN_BSS_END = f"        . = ALIGN(., 16);\n        {MAIN}_BSS_END = .;\n"
OBJECT_BSS = re.compile(r"^(\s*\S+\.o)\(\.bss\*\);$", re.MULTILINE)
EXACT_BSS = "(.bss COMMON .scommon)"
ANY_BSS = "(.bss* COMMON .scommon*)"

# A statement that places an object's input sections
OBJECT_SECTIONS = re.compile(r"^(\s*)(\S+\.o)\(([^()]*)\);$", re.MULTILINE)

BLOCK_NAME = re.compile(r"^\s*(\w+)_ROM_START = __romPos;$", re.MULTILINE)
SECTION_VRAM = re.compile(r"^(\s*\.\w+) (0x[0-9A-Fa-f]+) :", re.MULTILINE)


def block_name(block: str) -> str | None:
    """Return the name of the segment that BLOCK places, if it places one."""
    match = BLOCK_NAME.search(block)
    return match.group(1) if match else None


def block_vram(block: str) -> int | None:
    """Return the address that BLOCK gives its segment, if it gives one."""
    match = SECTION_VRAM.search(block)
    return int(match.group(2), 0) if match else None


def max_of(symbols: list[str]) -> str:
    """Return a linker expression for the largest of SYMBOLS."""
    expression = symbols[-1]
    for symbol in reversed(symbols[:-1]):
        expression = f"MAX({symbol}, {expression})"
    return expression


def place_unlisted_sections(blocks: list[str], main_index: int) -> None:
    """Add to main's blocks the archives' sections and the common symbols."""
    if MAIN_TEXT_END not in blocks[main_index]:
        sys.exit(f"expected {MAIN} to have code")
    blocks[main_index] = blocks[main_index].replace(
        MAIN_TEXT_END, ARCHIVE_SECTIONS + MAIN_TEXT_END, 1
    )
    bss_index = next(
        (index for index, block in enumerate(blocks) if MAIN_BSS_END in block), None
    )
    if bss_index is None:
        sys.exit(f"expected {MAIN} to have bss")
    blocks[bss_index] = OBJECT_BSS.sub(
        r"\g<0>\n\1(COMMON .scommon*);", blocks[bss_index]
    ).replace(MAIN_BSS_END, ARCHIVE_BSS + MAIN_BSS_END, 1)


def keep_whole_sections(match: re.Match[str]) -> str:
    """Keep the whole sections that MATCH places; leave the split ones."""
    indent, object_file, patterns = match.group(1), match.group(2), match.group(3)
    kept: list[str] = []
    split: list[str] = []
    for pattern in patterns.split():
        if pattern.startswith(".") and pattern.endswith("*"):
            name = pattern.removesuffix("*")
            kept += [name, f"{name}[!.]*"]
            split.append(f"{name}.*")
        else:
            kept.append(pattern)
    statement = f"{indent}KEEP({object_file}({' '.join(kept)}));"
    if split:
        statement += f"\n{indent}{object_file}({' '.join(split)});"
    return statement


def lay_out(script: str) -> str:
    """Return SCRIPT, splat's linker script, laid out to let code grow."""
    head, separator, rest = script.partition("SECTIONS\n{\n")
    body, end_separator, tail = rest.rpartition("\n}")
    if not separator or not end_separator:
        sys.exit("expected one SECTIONS command")
    blocks = body.split("\n\n")
    names = [block_name(block) for block in blocks]
    if MAIN not in names or BUILD_DATE not in names:
        sys.exit(f"expected segments named {MAIN} and {BUILD_DATE}")

    main_index = names.index(MAIN)
    place_unlisted_sections(blocks, main_index)
    blocks = [block.replace(EXACT_BSS, ANY_BSS) for block in blocks]
    build_date = blocks[names.index(BUILD_DATE)]
    # Segments without an address of their own, such as the asset files,
    # follow the build date in ROM
    rom_only: list[str] = []
    regions: dict[int, list[str]] = {}
    expansion: list[str] = []
    commands: list[str] = []
    for block, name in zip(
        blocks[main_index + 1 :], names[main_index + 1 :], strict=True
    ):
        vram = block_vram(block)
        if name == BUILD_DATE:
            continue
        if name is None:
            commands.append(block)
        elif vram is None:
            rom_only.append(block)
        elif KSEG0_START <= vram < BASE_RDRAM_END:
            regions.setdefault(vram, []).append(block)
        else:
            expansion.append(block)

    out = blocks[: main_index + 1]
    previous_ends = [f"{MAIN}_VRAM_END"]
    for index, vram in enumerate(sorted(regions), start=1):
        symbol = f"overlay_region_{index}_VRAM"
        moved = [
            SECTION_VRAM.sub(rf"\1 {symbol} :", block, count=1)
            for block in regions[vram]
        ]
        # One byte past the end keeps CanLoadOverlay from counting the
        # regions as overlapping, as it compares their bounds inclusively
        start = f"ALIGN({max_of(previous_ends)} + 1, {ALIGNMENT})"
        moved[0] = f"    {symbol} = {start};\n" + moved[0]
        out.extend(moved)
        previous_ends = [f"{block_name(block)}_VRAM_END" for block in moved]
    # ld takes an ASSERT among the sections only without a semicolon
    message = '"The overlays reach into the main heap: see include/system/heap.h"'
    heap_check = (
        f"    ASSERT(overlay_regions_VRAM_END <= 0x{MAIN_REGION_START:X}, {message})"
    )
    out.append(
        f"    overlay_regions_VRAM_END = ALIGN({max_of(previous_ends)}, {ALIGNMENT});\n"
        + heap_check
    )
    out.extend(expansion)
    out.append(build_date)
    out.extend(rom_only)
    out.extend(commands)
    body = OBJECT_SECTIONS.sub(keep_whole_sections, "\n\n".join(out))
    return head + separator + body + end_separator + tail


def main() -> None:
    """Lay out the script read from standard input."""
    _ = sys.stdout.write(lay_out(sys.stdin.read()))


if __name__ == "__main__":
    main()
