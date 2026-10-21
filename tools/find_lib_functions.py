#!/usr/bin/env python3
"""Find library functions in the ROM by comparing relocation-masked code.

Usage: find_lib_functions.py ROM ARCHIVE_OR_OBJECT... [--asm-dir asm]

For every function symbol in the given archives/objects, the code bytes are
compared against the function starts that splat found in the ROM. Fields
covered by a relocation in the library object are ignored on both sides, so
references to other symbols do not prevent a match. Prints
"romOffset vram librarySymbol sourceObject" for each match.
"""
import argparse
import io
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

GLABEL_PATTERN = re.compile(r"^glabel (\S+)")
INSTRUCTION_PATTERN = re.compile(r"/\* ([0-9A-F]+) ([0-9A-F]{8}) [0-9A-F]{8} \*/")

R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6


def read_rom_functions(asm_dir):
    """Return {romOffset: (name, vram)} for every glabel in the asm dump."""
    functions = {}
    for path in Path(asm_dir).rglob("*.s"):
        pendingName = None
        for line in path.read_text().splitlines():
            labelMatch = GLABEL_PATTERN.match(line)
            if labelMatch:
                pendingName = labelMatch.group(1)
                continue
            if pendingName:
                instructionMatch = INSTRUCTION_PATTERN.search(line)
                if instructionMatch:
                    romOffset = int(instructionMatch.group(1), 16)
                    vram = int(instructionMatch.group(2), 16)
                    functions[romOffset] = (pendingName, vram)
                    pendingName = None
    return functions


def mask_for_relocation(relocationType):
    if relocationType == R_MIPS_26:
        return 0xFC000000
    if relocationType in (R_MIPS_HI16, R_MIPS_LO16):
        return 0xFFFF0000
    return 0xFFFFFFFF


def library_objects(paths):
    """Yield (objectName, ELF bytes) from archives and loose objects."""
    for path in paths:
        if path.endswith(".a"):
            with tempfile.TemporaryDirectory() as extractDir:
                subprocess.run(["mips-linux-gnu-ar", "x", str(Path(path).resolve())],
                               cwd=extractDir, check=True)
                for objectPath in sorted(Path(extractDir).iterdir()):
                    yield objectPath.name, objectPath.read_bytes()
        else:
            yield Path(path).name, Path(path).read_bytes()


def library_functions(objectName, elfBytes):
    """Yield (symbolName, words, masks) for each function in .text."""
    elf = ELFFile(io.BytesIO(elfBytes))
    textIndex = None
    for index, section in enumerate(elf.iter_sections()):
        if section.name == ".text":
            textIndex = index
            textData = section.data()
    if textIndex is None or not textData:
        return
    wordCount = len(textData) // 4
    masks = [0xFFFFFFFF] * wordCount
    for section in elf.iter_sections():
        if isinstance(section, RelocationSection) and section.name in (".rel.text", ".rela.text"):
            for relocation in section.iter_relocations():
                wordIndex = relocation["r_offset"] // 4
                masks[wordIndex] &= mask_for_relocation(relocation["r_info_type"])
    symbolTable = elf.get_section_by_name(".symtab")
    def is_code_symbol(symbol):
        if symbol["st_shndx"] != textIndex:
            return False
        if symbol["st_info"]["type"] == "STT_FUNC":
            return True
        # Hand-written assembly often leaves global labels untyped
        return symbol["st_info"]["type"] == "STT_NOTYPE" and symbol["st_info"]["bind"] == "STB_GLOBAL"

    starts = sorted({symbol["st_value"] for symbol in symbolTable.iter_symbols() if is_code_symbol(symbol)})
    for symbol in symbolTable.iter_symbols():
        if not is_code_symbol(symbol):
            continue
        start = symbol["st_value"]
        size = symbol["st_size"]
        if size == 0:
            later = [s for s in starts if s > start]
            size = (later[0] if later else len(textData)) - start
        words = struct.unpack(">%dI" % (size // 4), textData[start:start + size])
        # Trailing nops are alignment padding, not part of the function proper
        while len(words) > 1 and words[-1] == 0 and words[-2] != 0x03E00008:
            words = words[:-1]
        yield symbol.name, words, masks[start // 4:start // 4 + len(words)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("rom")
    parser.add_argument("libraries", nargs="+")
    parser.add_argument("--asm-dir", default="asm")
    args = parser.parse_args()

    rom = Path(args.rom).read_bytes()
    romFunctions = read_rom_functions(args.asm_dir)
    for objectName, elfBytes in library_objects(args.libraries):
        for symbolName, words, masks in library_functions(objectName, elfBytes):
            if len(words) < 2:
                continue
            for romOffset, (romName, vram) in romFunctions.items():
                romWords = struct.unpack(">%dI" % len(words), rom[romOffset:romOffset + 4 * len(words)])
                if all((a ^ b) & m == 0 for a, b, m in zip(words, romWords, masks)):
                    print("%06X %08X %s %s %s" % (romOffset, vram, symbolName, objectName, romName))


if __name__ == "__main__":
    sys.exit(main())
