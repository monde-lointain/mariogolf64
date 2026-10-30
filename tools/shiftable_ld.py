#!/usr/bin/env python3
"""Lay out splat's linker script so that code and data can grow.

    shiftable_ld.py < SPLAT_SCRIPT > SHIFTABLE_SCRIPT

splat's script places every segment where the original has it. The overlays
share three regions of base RAM after main, at fixed addresses, and in ROM
they lie between main and the assets.

The shiftable script packs each region of base RAM after the largest
overlay of the region before it, the first after main, and defines
overlay_regions_VRAM_END, where the heap can begin, after the last. In ROM,
the overlays follow the assets, which stay at their original address, as
data holds the addresses of files in them, and the padding after the
assets is dropped. Other segments, such as an overlay of the Expansion
Pak's memory, keep their addresses.

The routines that the build links from archives, such as the libgcc
helpers that a modern GCC calls, go at the end of main's code, where
every segment can call them.

The matching build defines main's common objects in one file at the
original's addresses, which this layout leaves out. Each object of main
keeps its commons with its bss instead, as the overlays' objects do.
"""

import re
import sys

# The build date's address in the original's ROM, just before the assets
BUILD_DATE_ROM_START = 0x171770
# Where the Expansion Pak's memory begins; regions below it are packed
BASE_RDRAM_END = 0x80400000
KSEG0_START = 0x80000000
ALIGNMENT = 16

MAIN = "main"
BUILD_DATE = "build_date"
PADDING = "padding"

MAIN_TEXT_END = f"        {MAIN}_TEXT_END = .;\n"
ARCHIVE_SECTIONS = "        *.a:*(.text* .rodata* .data*);\n"
MAIN_BSS_END = f"        {MAIN}_BSS_END = .;\n"
OBJECT_BSS = re.compile(r"^(\s*\S+\.o)\(\.bss\*\);$", re.MULTILINE)

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
    )


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
    build_date = blocks[names.index(BUILD_DATE)]
    # Segments without an address of their own, such as the assets, follow
    # the build date in ROM
    rom_only: list[str] = []
    regions: dict[int, list[str]] = {}
    expansion: list[str] = []
    commands: list[str] = []
    for block, name in zip(
        blocks[main_index + 1 :], names[main_index + 1 :], strict=True
    ):
        vram = block_vram(block)
        if name in {BUILD_DATE, PADDING}:
            continue
        if name is None:
            commands.append(block)
        elif vram is None:
            rom_only.append(block)
        elif KSEG0_START <= vram < BASE_RDRAM_END:
            regions.setdefault(vram, []).append(block)
        else:
            expansion.append(block)

    rom_start = f"0x{BUILD_DATE_ROM_START:X}"
    fixed_rom = (
        f'    ASSERT(__romPos <= {rom_start}, "main overlaps the assets")\n'
        f"    __romPos = {rom_start};\n"
    )
    out = [*blocks[: main_index + 1], fixed_rom + build_date, *rom_only]
    previous_ends = [f"{MAIN}_VRAM_END"]
    for index, vram in enumerate(sorted(regions), start=1):
        symbol = f"overlay_region_{index}_VRAM"
        moved = [
            SECTION_VRAM.sub(rf"\1 {symbol} :", block, count=1)
            for block in regions[vram]
        ]
        moved[0] = (
            f"    {symbol} = ALIGN({max_of(previous_ends)}, {ALIGNMENT});\n" + moved[0]
        )
        out.extend(moved)
        previous_ends = [f"{block_name(block)}_VRAM_END" for block in moved]
    out.append(
        f"    overlay_regions_VRAM_END = ALIGN({max_of(previous_ends)}, {ALIGNMENT});"
    )
    out.extend(expansion)
    out.extend(commands)
    return head + separator + "\n\n".join(out) + end_separator + tail


def main() -> None:
    """Lay out the script read from standard input."""
    _ = sys.stdout.write(lay_out(sys.stdin.read()))


if __name__ == "__main__":
    main()
