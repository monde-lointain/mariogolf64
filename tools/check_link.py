#!/usr/bin/env python3
"""Check a linked ELF against what the code expects of the layout.

    check_link.py ELF SCRIPTS -- OBJECTS

ld's map of the link is read from beside the ELF, with the suffix .map.

Three mistakes link without a word from ld:

A linker script that assigns a symbol which an object defines overrides the
definition: ld gives the symbol the script's address, as an absolute
symbol in no section. The SCRIPTS, which give symbols fixed addresses,
must therefore assign none of the symbols that the OBJECTS define.

CanLoadOverlay (src/system/overlay_manager.c) takes two overlays for
alternatives, never loaded at once, unless one starts after the other's
bss ends, by an inclusive comparison. The overlays of a region of RAM
share its start, and those of other regions must stay clear of them, so
every two overlays that it takes for alternatives must start at the same
address.

The scripts end by discarding every input section that they don't place,
which drops the sections that the game never loads, and empty ones. A
section with contents that no pattern places, such as one that a new
attribute or compiler option names, would be left out of the ROM, so the
map must list no such section among those discarded. The map lists there
too the sections that --gc-sections collects, as nothing refers to them,
which a pattern does place: those pass.

Each mistake found is printed, and the exit status is 1 if there were any.
"""

import re
import subprocess
import sys
from fnmatch import fnmatchcase
from pathlib import Path
from struct import iter_unpack
from typing import final

OBJDUMP = "mips-linux-gnu-objdump"
NM = "mips-linux-gnu-nm"
USAGE = "usage: check_link.py ELF SCRIPTS -- OBJECTS"
SEPARATOR = "--"
MAP_SUFFIX = ".map"

# A symbol assignment in a linker script: NAME = EXPRESSION;
ASSIGNMENT = re.compile(r"^\s*([A-Za-z_.$][\w.$]*)\s*=", re.MULTILINE)
# A line of objdump -t: address, flags, section, size and name
SYMBOL_PATTERN = (
    r"^(?P<address>\w{8}) .{7} \S+\s+(?P<size>\w{8}) (?:\.hidden )?(?P<name>\S+)$"
)
SYMBOL_LINE = re.compile(SYMBOL_PATTERN, re.MULTILINE)
# A line of objdump -h: index, name, size, address, load address, offset
SECTION_PATTERN = (
    r"^\s*\d+ \S+\s+(?P<size>\w{8})\s+(?P<address>\w{8})\s+\w{8}\s+(?P<offset>\w{8})"
)
SECTION_LINE = re.compile(SECTION_PATTERN, re.MULTILINE)
# The map's list of the input sections that the link discarded, and a line
# of it: name, address, size and file. ld puts a long name on a line of its
# own, which the space after it spans.
DISCARDED_HEADING = "Discarded input sections"
DISCARDED_END = "\nMemory Configuration"
DISCARDED_LINE = re.compile(
    r"^ (?P<name>\S+)\s+0x\w+\s+0x(?P<size>\w+) (?P<file>.+)$", re.MULTILINE
)
# A statement of the scripts that places input sections, kept or not: the
# files' pattern, which names an archive's members as ARCHIVE:MEMBER, and
# the sections'. The final /DISCARD/ places every file's ("*") sections.
PLACEMENT = re.compile(
    r"^\s*(?:KEEP\()?(?P<files>[^\s()]+)\((?P<sections>[^()]*)\)\)?;$",
    re.MULTILINE,
)
EVERY_FILE = "*"
ARCHIVE_MEMBER = re.compile(r"^(?P<archive>.+)\((?P<member>[^()]+)\)$")
# The sections, by their names' starts, that hold what the game never loads,
# such as the bytecode that the modern build's objects carry for the
# segments' LTO links (see link_segments.py), which a link of their code
# leaves out
UNLOADED_SECTIONS = (
    ".MIPS.abiflags",
    ".comment",
    ".debug",
    ".gnu.attributes",
    ".gnu.lto_",
    ".mdebug",
    ".note",
    ".pdr",
    ".reginfo",
)
# nm's line for a defined symbol: value, type, name
DEFINED_LINE = re.compile(r"^[0-9a-f]+ [A-Za-z] (?P<name>\S+)$", re.MULTILINE)

OVERLAY_TABLE = "f_Overlays"
# struct Overlay in include/system/overlay_manager.h: isLoaded, moduleIds,
# romStart, romEnd, vramStart, textEnd, bssStart, bssEnd, onLoad, onUnload
OVERLAY_FORMAT = ">B3xIIIIIIIII"
ROM_START_SUFFIX = "_ROM_START"


def symbol_table(elf: Path) -> dict[str, tuple[int, int]]:
    """Map each named symbol of ELF to its address and size."""
    text = subprocess.run(
        [OBJDUMP, "-t", str(elf)], capture_output=True, text=True, check=True
    ).stdout
    return {
        match.group("name"): (
            int(match.group("address"), 16),
            int(match.group("size"), 16),
        )
        for match in SYMBOL_LINE.finditer(text)
    }


def overrides(scripts: list[Path], objects: list[str]) -> list[str]:
    """List the symbols that SCRIPTS assign and OBJECTS define."""
    text = subprocess.run(
        [NM, "-g", "--defined-only", *objects],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    defined = {match.group("name") for match in DEFINED_LINE.finditer(text)}
    return [
        f"{script}: assigns {match.group(1)}, which an object defines"
        for script in scripts
        for match in ASSIGNMENT.finditer(script.read_text())
        if match.group(1) in defined
    ]


def read_data(elf: Path, address: int, size: int) -> bytes:
    """Read SIZE bytes of ELF's loaded contents at ADDRESS."""
    text = subprocess.run(
        [OBJDUMP, "-h", str(elf)], capture_output=True, text=True, check=True
    ).stdout
    for match in SECTION_LINE.finditer(text):
        start = int(match.group("address"), 16)
        if start <= address < start + int(match.group("size"), 16):
            offset = int(match.group("offset"), 16) + address - start
            with elf.open("rb") as file:
                _ = file.seek(offset)
                return file.read(size)
    message = f"{elf}: no section holds 0x{address:08x}"
    raise SystemExit(message)


def overlay_conflicts(elf: Path, symbols: dict[str, tuple[int, int]]) -> list[str]:
    """List the overlays of two regions that CanLoadOverlay takes for alternatives."""
    address, size = symbols[OVERLAY_TABLE]
    names = {
        value: name.removesuffix(ROM_START_SUFFIX)
        for name, (value, _) in symbols.items()
        if name.endswith(ROM_START_SUFFIX)
    }
    # Each overlay's name, by its ROM start, and its range in RAM
    overlays = [
        (names.get(entry[2], f"0x{entry[2]:x}"), entry[4], entry[7])
        for entry in iter_unpack(OVERLAY_FORMAT, read_data(elf, address, size))
    ]
    conflicts: list[str] = []
    for index, (name, start, end) in enumerate(overlays):
        for other, other_start, other_end in overlays[index + 1 :]:
            if start != other_start and other_start <= end and other_end >= start:
                first = f"{name} (0x{start:08x}-0x{end:08x})"
                second = f"{other} (0x{other_start:08x}-0x{other_end:08x})"
                conflicts.append(f"{first} and {second} touch")
    return conflicts


@final
class Placements:
    """The input sections that the scripts place, as their patterns say."""

    def __init__(self, scripts: list[Path]) -> None:
        """Read the statements of SCRIPTS that place input sections."""
        self.by_file: dict[str, list[str]] = {}
        self.wildcards: list[tuple[str, list[str]]] = []
        for script in scripts:
            for match in PLACEMENT.finditer(script.read_text()):
                files, sections = match.group("files"), match.group("sections")
                if files == EVERY_FILE:
                    continue
                if any(character in files for character in "*?[:"):
                    self.wildcards.append((files, sections.split()))
                else:
                    self.by_file.setdefault(files, []).extend(sections.split())

    def places(self, section: str, file: str) -> bool:
        """Whether a statement places SECTION of FILE, as the map names it."""
        patterns = list(self.by_file.get(file, []))
        for files, sections in self.wildcards:
            if file_matches(files, file):
                patterns += sections
        return any(fnmatchcase(section, pattern) for pattern in patterns)


def file_matches(pattern: str, file: str) -> bool:
    """Whether a script's file PATTERN matches FILE, as the map names it."""
    archive_pattern, separator, member_pattern = pattern.partition(":")
    if not separator:
        return fnmatchcase(file, pattern)
    match = ARCHIVE_MEMBER.match(file)
    return bool(
        match
        and fnmatchcase(match.group("archive"), archive_pattern)
        and fnmatchcase(match.group("member"), member_pattern)
    )


def discarded_contents(map_file: Path, placements: Placements) -> list[str]:
    """List the sections with contents that the link of MAP_FILE left out.

    A section that a statement places was collected, not left out.
    """
    _, _, rest = map_file.read_text().partition(DISCARDED_HEADING)
    discarded, _, _ = rest.partition(DISCARDED_END)
    mistakes: list[str] = []
    for match in DISCARDED_LINE.finditer(discarded):
        name, size = match.group("name"), int(match.group("size"), 16)
        file = match.group("file")
        if (
            size
            and not name.startswith(UNLOADED_SECTIONS)
            and not placements.places(name, file)
        ):
            section = f"{name} of {match.group('file')}"
            mistakes.append(f"discards {section}, 0x{size:x} bytes that it holds")
    return mistakes


def main() -> int:
    """Check the ELF that the command line names; return the exit status."""
    arguments = sys.argv[1:]
    if SEPARATOR not in arguments or arguments.index(SEPARATOR) < 1:
        print(USAGE, file=sys.stderr)
        return 2
    split = arguments.index(SEPARATOR)
    elf = Path(arguments[0])
    scripts = [Path(script) for script in arguments[1:split]]
    objects = arguments[split + 1 :]
    symbols = symbol_table(elf)
    mistakes = (
        overrides(scripts, objects)
        + overlay_conflicts(elf, symbols)
        + discarded_contents(elf.with_suffix(MAP_SUFFIX), Placements(scripts))
    )
    for mistake in mistakes:
        print(f"{elf}: {mistake}", file=sys.stderr)
    return 1 if mistakes else 0


if __name__ == "__main__":
    sys.exit(main())
