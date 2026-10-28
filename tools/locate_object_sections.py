#!/usr/bin/env python3
"""Find where a library object's data sections were linked in the ROM.

Usage: locate_object_sections.py ROM SEGMENT_ROM SEGMENT_VRAM OBJECT:TEXT_ROM...

SEGMENT_ROM and SEGMENT_VRAM give the start of the code segment that holds
the objects, so ROM offsets and addresses can be converted into each other.
Each OBJECT is a relocatable object built from library source, and TEXT_ROM
is the ROM offset where its .text was linked.

Every %hi/%lo pair in the object's .text that refers to one of its own
.data, .rodata, .sdata or .bss sections is read back from the ROM. The
address the ROM loads, minus the offset the object expects within the
section, is the address of that section. Prints one line per section:
"object section address romOffset size votes", where votes counts the
relocation pairs that agree. Sections that nothing in .text refers to
are reported as "?".

Common symbols (uninitialized globals that the compiler leaves to the
linker) have no section of their own, so each one is reported by name as
"object COMMON:symbol address - size votes".
"""

import argparse
import collections
import io
import sys
from pathlib import Path
from typing import NamedTuple

from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import Relocation, RelocationSection
from elftools.elf.sections import Symbol, SymbolTableSection

from fncompare import read_entry_field, read_entry_int

R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
DATA_SECTION_NAMES = (".data", ".rodata", ".sdata", ".bss")
SHN_COMMON = "SHN_COMMON"
SCOMMON_SECTION_NAME = ".scommon"
COMMON_PREFIX = "COMMON:"
# How far past the previous match to look for a function, in bytes
SEARCH_WINDOW = 0x800

WORD_SIZE = 4
WORD_MASK = 0xFFFFFFFF
JUMP_TARGET_MASK = 0x03FFFFFF
IMMEDIATE_MASK = 0xFFFF
IMMEDIATE_SIGN_BIT = 0x8000
IMMEDIATE_RANGE = 0x10000
HALF_SHIFT = 16
HEX_BASE = 16

# Each address that the relocation pairs give a section or common symbol,
# with how many pairs give it
Votes = collections.defaultdict[str, collections.Counter[int]]


class FunctionLocation(NamedTuple):
    """Where an object's function was found in the ROM."""

    rom_offset: int
    size: int


class Arguments(argparse.Namespace):
    """The command line, typed."""

    rom: str = ""
    segmentRom: int = 0  # noqa: N815
    segmentVram: int = 0  # noqa: N815
    objects: list[str] = []  # noqa: RUF012


def sign_extend_16(value: int) -> int:
    """Return a 16-bit immediate as the signed value the CPU adds."""
    return value - IMMEDIATE_RANGE if value & IMMEDIATE_SIGN_BIT else value


def mask_for_relocation(relocation_type: int) -> int:
    """Return the bits of a word that a relocation of this type fills in."""
    if relocation_type == R_MIPS_26:
        return JUMP_TARGET_MASK
    if relocation_type in (R_MIPS_HI16, R_MIPS_LO16):
        return IMMEDIATE_MASK
    return WORD_MASK


def read_word(data: bytes, offset: int) -> int:
    """Return the big-endian word at an offset."""
    return int.from_bytes(data[offset : offset + WORD_SIZE], "big")


def read_immediate(data: bytes, offset: int) -> int:
    """Return the 16-bit immediate of the instruction at an offset."""
    return read_word(data, offset) & IMMEDIATE_MASK


def masked_words(data: bytes, mask_by_offset: dict[int, int]) -> list[int]:
    """Return the words of DATA with their relocated fields cleared."""
    return [
        read_word(data, offset) & ~mask_by_offset.get(offset, 0) & WORD_MASK
        for offset in range(0, len(data) - (WORD_SIZE - 1), WORD_SIZE)
    ]


def text_section_index(elf: ELFFile) -> int | None:
    """Return the index of the object's .text section."""
    for index, section in enumerate(elf.iter_sections()):
        if section.name == ".text":
            return index
    return None


def read_symbols(elf: ELFFile) -> SymbolTableSection:
    """Return the object's symbol table."""
    symbol_table = elf.get_section_by_name(".symtab")
    if not isinstance(symbol_table, SymbolTableSection):
        message = "the object has no symbol table"
        raise TypeError(message)
    return symbol_table


def read_text(elf: ELFFile) -> bytes:
    """Return the contents of the object's .text section."""
    text = elf.get_section_by_name(".text")
    if text is None:
        message = "the object has no .text section"
        raise TypeError(message)
    return text.data()


def is_sized_function(symbol: Symbol, text_index: int | None) -> bool:
    """Return whether a symbol is a function in .text with a known size."""
    info = read_entry_field(symbol.entry, "st_info")
    return (
        isinstance(info, Container)
        and read_entry_field(info, "type") == "STT_FUNC"
        and read_entry_int(symbol.entry, "st_size") > 0
        and read_entry_field(symbol.entry, "st_shndx") == text_index
    )


def function_rom_offsets(
    elf: ELFFile, rom_bytes: bytes, text_rom: int, relocations: list[Relocation]
) -> dict[int, FunctionLocation]:
    """Map each function in .text to the ROM offset where its code appears.

    A game may link a slightly different revision of a library, in which
    functions are added, removed or resized. Each function is searched for
    from the end of the previous match, comparing with relocated fields
    masked out. Functions that are not found are left out, so their
    relocations do not cast misleading votes.
    """
    text_data = read_text(elf)
    mask_by_offset = {
        read_entry_int(relocation.entry, "r_offset"): mask_for_relocation(
            read_entry_int(relocation.entry, "r_info_type")
        )
        for relocation in relocations
    }
    text_index = text_section_index(elf)
    functions = sorted(
        (
            read_entry_int(symbol.entry, "st_value"),
            read_entry_int(symbol.entry, "st_size"),
        )
        for symbol in read_symbols(elf).iter_symbols()
        if is_sized_function(symbol, text_index)
    )
    offsets: dict[int, FunctionLocation] = {}
    search_start = text_rom
    for start, size in functions:
        function_mask = {
            offset - start: mask
            for offset, mask in mask_by_offset.items()
            if start <= offset < start + size
        }
        object_words = masked_words(text_data[start : start + size], function_mask)
        for candidate in range(search_start, search_start + SEARCH_WINDOW, WORD_SIZE):
            rom_words = masked_words(
                rom_bytes[candidate : candidate + size], function_mask
            )
            if rom_words == object_words:
                offsets[start] = FunctionLocation(candidate, size)
                search_start = candidate + size
                break
    return offsets


def read_section_names(elf: ELFFile) -> dict[int, str]:
    """Return the name of each section, by index."""
    return {index: section.name for index, section in enumerate(elf.iter_sections())}


def is_common_symbol(symbol: Symbol, section_names: dict[int, str]) -> bool:
    """Return whether a symbol is a common one, small or not."""
    section_index = read_entry_field(symbol.entry, "st_shndx")
    if section_index == SHN_COMMON:
        return True
    return (
        isinstance(section_index, int)
        and section_names.get(section_index) == SCOMMON_SECTION_NAME
    )


def find_lo_relocation(
    relocations: list[Relocation], position: int, symbol_index: int
) -> Relocation | None:
    """Return the first %lo relocation after POSITION for the same symbol."""
    for later in relocations[position + 1 :]:
        if (
            read_entry_int(later.entry, "r_info_type") == R_MIPS_LO16
            and read_entry_int(later.entry, "r_info_sym") == symbol_index
        ):
            return later
    return None


def find_rom_offset(
    function_offsets: dict[int, FunctionLocation], object_offset: int
) -> int | None:
    """Return the ROM offset of an offset in .text, if its function was found."""
    for start, location in function_offsets.items():
        if start <= object_offset < start + location.size:
            return location.rom_offset + object_offset - start
    return None


def read_pair_addresses(
    text_data: bytes,
    rom_bytes: bytes,
    function_offsets: dict[int, FunctionLocation],
    relocations: tuple[Relocation, Relocation],
) -> tuple[int, int] | None:
    """Return the address a %hi/%lo pair loads in the ROM and its addend.

    Returns None if either instruction's function wasn't found in the ROM.
    """
    hi_offset = read_entry_int(relocations[0].entry, "r_offset")
    lo_offset = read_entry_int(relocations[1].entry, "r_offset")
    rom_hi_offset = find_rom_offset(function_offsets, hi_offset)
    rom_lo_offset = find_rom_offset(function_offsets, lo_offset)
    if rom_hi_offset is None or rom_lo_offset is None:
        return None
    object_hi = read_immediate(text_data, hi_offset)
    object_lo = read_immediate(text_data, lo_offset)
    rom_hi = read_immediate(rom_bytes, rom_hi_offset)
    rom_lo = read_immediate(rom_bytes, rom_lo_offset)
    rom_address = ((rom_hi << HALF_SHIFT) + sign_extend_16(rom_lo)) & WORD_MASK
    addend = (object_hi << HALF_SHIFT) + sign_extend_16(object_lo)
    return rom_address, addend


def section_addresses(elf: ELFFile, rom_bytes: bytes, text_rom: int) -> Votes:
    """Return the votes for each section's or common symbol's address."""
    text_data = read_text(elf)
    symbol_table = read_symbols(elf)
    section_names = read_section_names(elf)
    votes: Votes = collections.defaultdict(collections.Counter)

    relocation_section = elf.get_section_by_name(".rel.text")
    if not isinstance(relocation_section, RelocationSection):
        return votes
    relocations = list(relocation_section.iter_relocations())
    function_offsets = function_rom_offsets(elf, rom_bytes, text_rom, relocations)

    for position, hi_relocation in enumerate(relocations):
        if read_entry_int(hi_relocation.entry, "r_info_type") != R_MIPS_HI16:
            continue
        symbol_index = read_entry_int(hi_relocation.entry, "r_info_sym")
        symbol = symbol_table.get_symbol(symbol_index)
        section_index = read_entry_field(symbol.entry, "st_shndx")
        section_name = (
            section_names.get(section_index) if isinstance(section_index, int) else None
        )
        is_common = is_common_symbol(symbol, section_names)
        if section_name not in DATA_SECTION_NAMES and not is_common:
            continue
        lo_relocation = find_lo_relocation(relocations, position, symbol_index)
        if lo_relocation is None:
            continue
        pair = read_pair_addresses(
            text_data,
            rom_bytes,
            function_offsets,
            (hi_relocation, lo_relocation),
        )
        if pair is None:
            continue
        rom_address, addend = pair
        if is_common:
            # A common symbol's st_value is its alignment, not an offset
            votes[COMMON_PREFIX + symbol.name][rom_address - addend] += 1
            continue
        if section_name is not None:
            symbol_value = read_entry_int(symbol.entry, "st_value")
            votes[section_name][rom_address - symbol_value - addend] += 1
    return votes


def common_symbol_sizes(elf: ELFFile) -> dict[str, int]:
    """Return {name: size} for the object's common symbols."""
    section_names = read_section_names(elf)
    return {
        symbol.name: read_entry_int(symbol.entry, "st_size")
        for symbol in read_symbols(elf).iter_symbols()
        if is_common_symbol(symbol, section_names)
    }


def print_sections(
    object_path: str, elf: ELFFile, votes: Votes, args: Arguments
) -> None:
    """Print where each of an object's data sections was linked."""
    for section_name in DATA_SECTION_NAMES:
        section = elf.get_section_by_name(section_name)
        if section is None:
            continue
        size = read_entry_int(section.header, "sh_size")
        if size == 0:
            continue
        if not votes[section_name]:
            print(f"{object_path} {section_name} ? ? 0x{size:X} 0")
            continue
        address, count = votes[section_name].most_common(1)[0]
        if len(votes[section_name]) > 1:
            conflicts = {hex(a): c for a, c in votes[section_name].items()}
            warning = " ".join(
                (
                    f"warning: {object_path} {section_name}",
                    f"has conflicting addresses {conflicts}",
                )
            )
            print(warning, file=sys.stderr)
        rom_offset = (
            f"0x{address - args.segmentVram + args.segmentRom:X}"
            if section_name != ".bss"
            else "-"
        )
        fields = (
            object_path,
            section_name,
            f"0x{address:08X}",
            rom_offset,
            f"0x{size:X}",
            str(count),
        )
        print(" ".join(fields))


def print_common_symbols(object_path: str, elf: ELFFile, votes: Votes) -> None:
    """Print where each of an object's common symbols was linked."""
    for symbol_name, size in sorted(common_symbol_sizes(elf).items()):
        symbol_votes = votes.get(COMMON_PREFIX + symbol_name)
        if not symbol_votes:
            print(f"{object_path} COMMON:{symbol_name} ? - 0x{size:X} 0")
            continue
        address, count = symbol_votes.most_common(1)[0]
        print(
            f"{object_path} COMMON:{symbol_name} 0x{address:08X} - 0x{size:X} {count}"
        )


def main() -> None:
    """Print where each object's data sections were linked."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    _ = parser.add_argument("rom")
    _ = parser.add_argument("segmentRom", type=lambda text: int(text, HEX_BASE))
    _ = parser.add_argument("segmentVram", type=lambda text: int(text, HEX_BASE))
    _ = parser.add_argument("objects", nargs="+", help="OBJECT:TEXT_ROM pairs")
    args = parser.parse_args(namespace=Arguments())
    rom_bytes = Path(args.rom).read_bytes()

    for pair in args.objects:
        object_path, text_rom_text = pair.rsplit(":", 1)
        elf = ELFFile(io.BytesIO(Path(object_path).read_bytes()))
        votes = section_addresses(elf, rom_bytes, int(text_rom_text, HEX_BASE))
        print_sections(object_path, elf, votes, args)
        print_common_symbols(object_path, elf, votes)


if __name__ == "__main__":
    main()
