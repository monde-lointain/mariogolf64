#!/usr/bin/env python3
"""Find library functions in the ROM by comparing relocation-masked code.

Usage: find_lib_functions.py ROM ARCHIVE_OR_OBJECT... [--asm-dir asm]

For every function symbol in the given archives/objects, the code bytes are
compared against the function starts that splat found in the ROM. Fields
covered by a relocation in the library object are ignored on both sides, so
references to other symbols do not prevent a match. Prints
"romOffset vram librarySymbol sourceObject romName" for each match.
"""

import argparse
import io
import re
import struct
import subprocess
import tempfile
from collections.abc import Iterator
from pathlib import Path
from typing import NamedTuple

from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import Symbol, SymbolTableSection

from fncompare import read_entry_field, read_entry_int

GLABEL_PATTERN = re.compile(r"^glabel (\S+)")
INSTRUCTION_PATTERN = re.compile(r"/\* ([0-9A-F]+) ([0-9A-F]{8}) [0-9A-F]{8} \*/")

WORD_SIZE = 4
WORD_MASK = 0xFFFFFFFF
R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
JUMP_RELOCATION_MASK = 0xFC000000
HALF_RELOCATION_MASK = 0xFFFF0000
RELOCATION_SECTIONS = (".rel.text", ".rela.text")
JR_RA = 0x03E00008
# A function needs at least this many words for a match to mean anything
MIN_WORD_COUNT = 2


class RomFunction(NamedTuple):
    """A function that splat found in the ROM."""

    name: str
    vram: int


class LibraryFunction(NamedTuple):
    """A function in a library object, with the bits that aren't relocated."""

    name: str
    words: tuple[int, ...]
    masks: list[int]


class Arguments(argparse.Namespace):
    """The command line, typed."""

    rom: str = ""
    libraries: list[str] = []  # noqa: RUF012
    asm_dir: str = "asm"


def read_rom_functions(asm_dir: str) -> dict[int, RomFunction]:
    """Return the functions of every glabel in the asm dump, by ROM offset."""
    functions: dict[int, RomFunction] = {}
    for path in Path(asm_dir).rglob("*.s"):
        pending_name = None
        for line in path.read_text().splitlines():
            label_match = GLABEL_PATTERN.match(line)
            if label_match:
                pending_name = label_match.group(1)
                continue
            if pending_name:
                instruction_match = INSTRUCTION_PATTERN.search(line)
                if instruction_match:
                    rom_offset = int(instruction_match.group(1), 16)
                    vram = int(instruction_match.group(2), 16)
                    functions[rom_offset] = RomFunction(pending_name, vram)
                    pending_name = None
    return functions


def mask_for_relocation(relocation_type: int) -> int:
    """Return the bits of a word that a relocation of this type leaves alone."""
    if relocation_type == R_MIPS_26:
        return JUMP_RELOCATION_MASK
    if relocation_type in (R_MIPS_HI16, R_MIPS_LO16):
        return HALF_RELOCATION_MASK
    return WORD_MASK


def library_objects(paths: list[str]) -> Iterator[tuple[str, bytes]]:
    """Yield (objectName, ELF bytes) from archives and loose objects."""
    for path in paths:
        if path.endswith(".a"):
            with tempfile.TemporaryDirectory() as extract_dir:
                _ = subprocess.run(
                    ["mips-linux-gnu-ar", "x", str(Path(path).resolve())],
                    cwd=extract_dir,
                    check=True,
                )
                for object_path in sorted(Path(extract_dir).iterdir()):
                    yield object_path.name, object_path.read_bytes()
        else:
            yield Path(path).name, Path(path).read_bytes()


def read_text_masks(elf: ELFFile, word_count: int) -> list[int]:
    """Return the bits of each .text word that no relocation covers."""
    masks = [WORD_MASK] * word_count
    for section in elf.iter_sections():
        if (
            isinstance(section, RelocationSection)
            and section.name in RELOCATION_SECTIONS
        ):
            for relocation in section.iter_relocations():
                word_index = read_entry_int(relocation.entry, "r_offset") // WORD_SIZE
                relocation_type = read_entry_int(relocation.entry, "r_info_type")
                masks[word_index] &= mask_for_relocation(relocation_type)
    return masks


def is_code_symbol(symbol: Symbol, text_index: int) -> bool:
    """Return whether a symbol starts a function in .text."""
    if read_entry_field(symbol.entry, "st_shndx") != text_index:
        return False
    info = read_entry_field(symbol.entry, "st_info")
    if not isinstance(info, Container):
        return False
    symbol_type = read_entry_field(info, "type")
    if symbol_type == "STT_FUNC":
        return True
    # Hand-written assembly often leaves global labels untyped
    return (
        symbol_type == "STT_NOTYPE" and read_entry_field(info, "bind") == "STB_GLOBAL"
    )


def library_functions(elf_bytes: bytes) -> Iterator[LibraryFunction]:
    """Yield each function in an object's .text."""
    elf = ELFFile(io.BytesIO(elf_bytes))
    text_index = None
    text_data = b""
    for index, section in enumerate(elf.iter_sections()):
        if section.name == ".text":
            text_index = index
            text_data = section.data()
    symbol_table = elf.get_section_by_name(".symtab")
    if (
        text_index is None
        or not text_data
        or not isinstance(symbol_table, SymbolTableSection)
    ):
        return
    masks = read_text_masks(elf, len(text_data) // WORD_SIZE)
    code_symbols = [
        symbol
        for symbol in symbol_table.iter_symbols()
        if is_code_symbol(symbol, text_index)
    ]
    starts = sorted(
        {read_entry_int(symbol.entry, "st_value") for symbol in code_symbols}
    )
    for symbol in code_symbols:
        start = read_entry_int(symbol.entry, "st_value")
        size = read_entry_int(symbol.entry, "st_size")
        if size == 0:
            later = [s for s in starts if s > start]
            size = (later[0] if later else len(text_data)) - start
        words: tuple[int, ...] = struct.unpack(
            f">{size // WORD_SIZE}I", text_data[start : start + size]
        )
        # Trailing nops are alignment padding, not part of the function proper
        while len(words) > 1 and words[-1] == 0 and words[-2] != JR_RA:
            words = words[:-1]
        first = start // WORD_SIZE
        yield LibraryFunction(symbol.name, words, masks[first : first + len(words)])


def main() -> None:
    """Print each library function found in the ROM."""
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    _ = parser.add_argument("rom")
    _ = parser.add_argument("libraries", nargs="+")
    _ = parser.add_argument("--asm-dir", default="asm")
    args = parser.parse_args(namespace=Arguments())

    rom = Path(args.rom).read_bytes()
    rom_functions = read_rom_functions(args.asm_dir)
    for object_name, elf_bytes in library_objects(args.libraries):
        for function in library_functions(elf_bytes):
            count = len(function.words)
            if count < MIN_WORD_COUNT:
                continue
            for rom_offset, rom_function in rom_functions.items():
                rom_words: tuple[int, ...] = struct.unpack(
                    f">{count}I", rom[rom_offset : rom_offset + WORD_SIZE * count]
                )
                if all(
                    (a ^ b) & m == 0
                    for a, b, m in zip(
                        function.words, rom_words, function.masks, strict=False
                    )
                ):
                    fields = (
                        f"{rom_offset:06X}",
                        f"{rom_function.vram:08X}",
                        function.name,
                        object_name,
                        rom_function.name,
                    )
                    print(" ".join(fields))


if __name__ == "__main__":
    main()
