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
import struct
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
DATA_SECTION_NAMES = (".data", ".rodata", ".sdata", ".bss")
SHN_COMMON = "SHN_COMMON"
SCOMMON_SECTION_NAME = ".scommon"
# How far past the previous match to look for a function, in bytes
SEARCH_WINDOW = 0x800


def sign_extend_16(value):
    return value - 0x10000 if value & 0x8000 else value


def mask_for_relocation(relocationType):
    if relocationType == R_MIPS_26:
        return 0x03FFFFFF
    if relocationType in (R_MIPS_HI16, R_MIPS_LO16):
        return 0x0000FFFF
    return 0xFFFFFFFF


def masked_words(data, maskByOffset):
    words = []
    for offset in range(0, len(data) - 3, 4):
        word = struct.unpack(">I", data[offset:offset + 4])[0]
        words.append(word & ~maskByOffset.get(offset, 0) & 0xFFFFFFFF)
    return words


def function_rom_offsets(elf, romBytes, textRom, relocations):
    """Map each function in .text to the ROM offset where its code appears.

    A game may link a slightly different revision of a library, in which
    functions are added, removed or resized. Each function is searched for
    from the end of the previous match, comparing with relocated fields
    masked out. Functions that are not found are left out, so their
    relocations do not cast misleading votes.
    """
    text = elf.get_section_by_name(".text")
    textData = text.data()
    maskByOffset = {relocation["r_offset"]: mask_for_relocation(relocation["r_info_type"])
                    for relocation in relocations}
    textIndex = text_section_index(elf)
    functions = sorted((symbol["st_value"], symbol["st_size"])
                       for symbol in elf.get_section_by_name(".symtab").iter_symbols()
                       if symbol["st_info"]["type"] == "STT_FUNC" and symbol["st_size"] > 0
                       and symbol["st_shndx"] == textIndex)
    offsets = {}
    searchStart = textRom
    for start, size in functions:
        functionMask = {offset - start: mask for offset, mask in maskByOffset.items()
                        if start <= offset < start + size}
        objectWords = masked_words(textData[start:start + size], functionMask)
        for candidate in range(searchStart, searchStart + SEARCH_WINDOW, 4):
            romWords = masked_words(romBytes[candidate:candidate + size], functionMask)
            if romWords == objectWords:
                offsets[start] = (candidate, size)
                searchStart = candidate + size
                break
    return offsets


def text_section_index(elf):
    for index, section in enumerate(elf.iter_sections()):
        if section.name == ".text":
            return index
    return None


def section_addresses(elf, romBytes, textRom):
    """Return {sectionName or "COMMON:symbol": Counter(address)} for one object."""
    textData = elf.get_section_by_name(".text").data()
    symbolTable = elf.get_section_by_name(".symtab")
    sectionNames = {index: section.name for index, section in enumerate(elf.iter_sections())}
    votes = collections.defaultdict(collections.Counter)

    relocationSection = elf.get_section_by_name(".rel.text")
    if not isinstance(relocationSection, RelocationSection):
        return votes
    relocations = list(relocationSection.iter_relocations())
    functionOffsets = function_rom_offsets(elf, romBytes, textRom, relocations)

    def rom_offset_of(objectOffset):
        for start, (romStart, size) in functionOffsets.items():
            if start <= objectOffset < start + size:
                return romStart + objectOffset - start
        return None

    for position, hiRelocation in enumerate(relocations):
        if hiRelocation["r_info_type"] != R_MIPS_HI16:
            continue
        symbolIndex = hiRelocation["r_info_sym"]
        symbol = symbolTable.get_symbol(symbolIndex)
        sectionName = sectionNames.get(symbol["st_shndx"])
        isCommon = symbol["st_shndx"] == SHN_COMMON or sectionName == SCOMMON_SECTION_NAME
        if sectionName not in DATA_SECTION_NAMES and not isCommon:
            continue
        loRelocation = next((later for later in relocations[position + 1:]
                             if later["r_info_type"] == R_MIPS_LO16 and later["r_info_sym"] == symbolIndex), None)
        if loRelocation is None:
            continue
        hiOffset = hiRelocation["r_offset"]
        loOffset = loRelocation["r_offset"]
        romHiOffset = rom_offset_of(hiOffset)
        romLoOffset = rom_offset_of(loOffset)
        if romHiOffset is None or romLoOffset is None:
            continue
        objectHi = struct.unpack(">I", textData[hiOffset:hiOffset + 4])[0] & 0xFFFF
        objectLo = struct.unpack(">I", textData[loOffset:loOffset + 4])[0] & 0xFFFF
        romHi = struct.unpack(">I", romBytes[romHiOffset:romHiOffset + 4])[0] & 0xFFFF
        romLo = struct.unpack(">I", romBytes[romLoOffset:romLoOffset + 4])[0] & 0xFFFF
        romAddress = ((romHi << 16) + sign_extend_16(romLo)) & 0xFFFFFFFF
        addend = (objectHi << 16) + sign_extend_16(objectLo)
        if isCommon:
            # A common symbol's st_value is its alignment, not an offset
            votes["COMMON:" + symbol.name][romAddress - addend] += 1
            continue
        votes[sectionName][romAddress - symbol["st_value"] - addend] += 1
    return votes


def common_symbol_sizes(elf):
    """Return {name: size} for the object's common symbols."""
    sectionNames = {index: section.name for index, section in enumerate(elf.iter_sections())}
    sizes = {}
    for symbol in elf.get_section_by_name(".symtab").iter_symbols():
        if symbol["st_shndx"] == SHN_COMMON or sectionNames.get(symbol["st_shndx"]) == SCOMMON_SECTION_NAME:
            sizes[symbol.name] = symbol["st_size"]
    return sizes


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("rom")
    parser.add_argument("segmentRom", type=lambda text: int(text, 16))
    parser.add_argument("segmentVram", type=lambda text: int(text, 16))
    parser.add_argument("objects", nargs="+", help="OBJECT:TEXT_ROM pairs")
    args = parser.parse_args()
    romBytes = Path(args.rom).read_bytes()

    for pair in args.objects:
        objectPath, textRomText = pair.rsplit(":", 1)
        elf = ELFFile(io.BytesIO(Path(objectPath).read_bytes()))
        votes = section_addresses(elf, romBytes, int(textRomText, 16))
        for sectionName in DATA_SECTION_NAMES:
            section = elf.get_section_by_name(sectionName)
            if section is None or section["sh_size"] == 0:
                continue
            size = section["sh_size"]
            if not votes[sectionName]:
                print("%s %s ? ? 0x%X 0" % (objectPath, sectionName, size))
                continue
            address, count = votes[sectionName].most_common(1)[0]
            if len(votes[sectionName]) > 1:
                print("warning: %s %s has conflicting addresses %s" % (
                    objectPath, sectionName, {hex(a): c for a, c in votes[sectionName].items()}), file=sys.stderr)
            romOffset = address - args.segmentVram + args.segmentRom if sectionName != ".bss" else None
            print("%s %s 0x%08X %s 0x%X %d" % (objectPath, sectionName, address,
                                               "0x%X" % romOffset if romOffset is not None else "-", size, count))
        for symbolName, size in sorted(common_symbol_sizes(elf).items()):
            symbolVotes = votes.get("COMMON:" + symbolName)
            if not symbolVotes:
                print("%s COMMON:%s ? - 0x%X 0" % (objectPath, symbolName, size))
                continue
            address, count = symbolVotes.most_common(1)[0]
            print("%s COMMON:%s 0x%08X - 0x%X %d" % (objectPath, symbolName, address, size, count))


if __name__ == "__main__":
    main()
