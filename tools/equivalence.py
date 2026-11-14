#!/usr/bin/env python3
"""Characterize the modern build against the matching build, frame by frame.

    equivalence.py [--frames N] [--harness PATH] [--verbose]

Runs build/mariogolf64.z64 and build/modern/mariogolf64.z64 side by side in
ares's n64-harness and stops each where nuGfxTaskStart queues a frame's
display list. Every frame's game logic and drawing has run by then, so the
game's state there is a narrow place through which all of the frame's
behavior passes. Frames are paired by g_FrameCount, which counts frames of
logic, and not by when each build shows them, as the builds run at
different speeds.

At each frame it compares every object that the game's own code defines in
the main segment and the loaded overlays, and the main heap, word by word.
The libraries' objects are left out: their state follows threads, retraces
and timers, which differ with speed, and what they hand the game reaches
the game's objects, which are compared.

The modern build has another layout, so two words that differ are still
equal when they can address the same place in each build:

- an object or function at the same offset, or the end of one, whether
  through KSEG0 or physically, as the RCP takes addresses;
- the same offset in the heap, or in an object whose contents the game
  places from a page boundary in it;
- the same bytes, for unnamed data such as a string literal;
- the same linker marker, or the same offset in a section of the ROM.

A copy that only AVOID_UB makes for the RCP to read stands for what it
copies. The padding of some structures, which no one reads, is left out.

Each difference listed in KNOWN_DIFFERENCES is reported with its reason but
passes, as does one seen only while a hole loader thread was at work, as
each build's runs at its own speed. Any other fails the run, and the first
frame and word of each is printed, along with how many frames it covers.
The harness is built from the project's fork of ares, on its
mariogolf-harness branch; --harness or the N64_HARNESS environment variable
gives its path.
"""

import argparse
import bisect
import collections
import contextlib
import os
import re
import signal
import subprocess
import sys
import tempfile
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path
from struct import iter_unpack
from typing import cast, final

ROOT = Path(__file__).resolve().parent.parent
MATCHING_DIR = ROOT / "build"
MODERN_DIR = ROOT / "build" / "modern"
ELF_NAME = "mariogolf64.elf"
ROM_NAME = "mariogolf64.z64"

# KSEG0, through which the game addresses RDRAM, and the RDRAM that the
# Expansion Pak gives the game
KSEG0 = 0x8000_0000
RDRAM_SIZE = 0x80_0000
WORD_SIZE = 4
# The bits of a word that address RDRAM; the RCP ignores the others, which
# the game uses for KSEG0 and for tags of its own
PHYSICAL_MASK = RDRAM_SIZE - 1
# The page that the game rounds the characters' buffers up to for the TLB,
# SMALLEST_PAGE_SIZE in include/characters/character_model.h
PAGE_SIZE = 0x1000
# How many bytes of unnamed data, such as a string literal, stand for it,
# and the prefix of its name
UNNAMED_DATA_LENGTH = 64
UNNAMED_PREFIX = "unnamed data "
# The suffix that names an object's contents from its first page boundary
PAGE_SUFFIX = " from its first page"

# The main heap's region (include/system/heap.h), the same in every build
MAIN_REGION_END = 0x802E_A000
MAIN_REGION_SIZE = 0x8_C800
HEAP_NAME = "(main heap)"

# The layout of an Overlay (include/system/overlay_manager.h)
OVERLAY_SIZE = 40
OVERLAY_ROM_START_OFFSET = 8

SUMMARY = "Characterize the modern build against the matching build."
DEFAULT_FRAMES = 3000
DEFAULT_HARNESS = os.environ.get("N64_HARNESS", "n64-harness")

# How many video frames the harness may run before a frame of logic must
# have been queued; a build that takes longer has hung
FRAME_TIMEOUT = 600

# Differences that are expected, by object, with why. An object is named
# by its symbol, or as "FILE:SYMBOL" when the name alone is ambiguous, by
# the basename of the source file that defines it.
TIMING = "set from the OS timer or by another thread, so it follows speed"
# The original's main thread overflows its stack onto these at boot, before
# the game sets them; AVOID_UB gives the stack room. They are expected to
# differ only until they first agree.
MAIN_STACK_OVERFLOW = "the original's main stack overflows onto it (6786e84c)"
THREAD = "a thread's saved registers or stack, which follow the compiler"
AVOID_UB_ONLY = "only AVOID_UB defines it, to fix a race in the original"
LOADING = "only while a hole loader thread was at work"
# The scheduler's graphics thread runs UpdateSunGlare as each frame's task
# ends, so the main thread finds its state a frame early or late.
GRAPHICS_THREAD = "set as a frame's task ends, so when it changes follows speed"
FRAME_RATE = "it counts retraces, so it follows speed"
# libmus starts a sound only on a channel that its audio thread has freed,
# and numbers each that it starts, so a start that comes a retrace sooner
# may fail, and the numbers after it shift.
AUDIO_HANDLE = "libmus's handles depend on when its audio thread frees channels"
# The original blends both copies of the sky's vertices, one of which the
# RSP may be reading, where AVOID_UB blends only the frame's own.
SKY_BLENDING = "AVOID_UB blends only the frame's copy (a9ac2e65)"
KNOWN_DIFFERENCES = {
    "f_UpdateStartTime": TIMING,
    "f_UpdateEndTime": TIMING,
    "f_DrawStartTime": TIMING,
    "f_GfxTaskStartTime": TIMING,
    "f_LastAudioFrame": TIMING,
    "f_AudioTask": TIMING,
    "f_AudioClient": TIMING,
    "f_RetraceQueue": TIMING,
    "f_TaskDoneQueue": TIMING,
    "f_RetraceCount": FRAME_RATE,
    "f_FramesThisSecond": FRAME_RATE,
    "g_FramesPerSecond": FRAME_RATE,
    "f_SoundHandles": AUDIO_HANDLE,
    "f_SoundHandleCopies": AUDIO_HANDLE,
    "f_SongHandle": AUDIO_HANDLE,
    "f_SongHandleCopy": AUDIO_HANDLE,
    "f_SunGlareStrength": GRAPHICS_THREAD,
    "f_SunDepth": GRAPHICS_THREAD,
    "f_IsSunGlareSettled": GRAPHICS_THREAD,
    "f_WasMetalMarioUnlocked": MAIN_STACK_OVERFLOW,
    "f_NoiseGrids": MAIN_STACK_OVERFLOW,
    "nuMainStack": "AVOID_UB doubles it, so that it doesn't overflow (6786e84c)",
    "f_LoaderThread": THREAD,
    "g_LoaderThreadStack": THREAD,
    "f_DrawnBillboardVertices": AVOID_UB_ONLY,
    "f_DrawnNoiseTexels": AVOID_UB_ONLY,
    "f_SkyVertices": SKY_BLENDING,
}


# The copies that AVOID_UB has the RCP read in place of what the original
# shares with the CPU, by the copy's name: what each copies, how many bytes
# a copy has, and whether what it copies is the target of the named
# pointer. A copy's bytes stand for those that it copies.
@dataclass(frozen=True)
class DrawnCopy:
    """A copy that a frame draws with, of data that later frames change."""

    source: str
    size: int
    through_pointer: bool = False


DRAWN_COPIES = {
    "f_DrawnBillboardVertices": DrawnCopy("f_BillboardVertices", 0x40),
    "f_DrawnNoiseTexels": DrawnCopy("g_NoiseTexels", 0x1000, through_pointer=True),
}

# Objects whose contents the game places from a page boundary in them, so
# that they lie at other offsets in each build. Their words are compared at
# the same offset, and then from each build's first page boundary.
PAGE_ALIGNED_CONTENTS = frozenset({"g_WorkArea"})


# The padding of structures, which no one reads, and which a structure's
# copy takes from wherever its source was, such as a stack temporary. By
# object: the size of its elements, and the bits of padding in the words of
# each, by the words' offsets in it.
@dataclass(frozen=True)
class Padding:
    """Where the padding lies in an object's elements."""

    element_size: int
    masks: dict[int, int]


WHOLE_WORD = 0xFFFF_FFFF
LOW_HALF = 0x0000_FFFF
# A Lights2's lights each end in a word that aligns them for the microcode,
# which doesn't read it.
LIGHTS2_PADDING = Padding(0x28, {0x14: WHOLE_WORD, 0x24: WHOLE_WORD})
# A PlaneTriangle's nine corners end a halfword short of its alignment.
PLANE_TRIANGLE_PADDING = Padding(0x18, {0x14: LOW_HALF})
PADDING = {
    "f_DryLights": LIGHTS2_PADDING,
    "f_RainLights": LIGHTS2_PADDING,
    "f_OpeningContactTriangle": PLANE_TRIANGLE_PADDING,
}

# An objdump -t line: address, binding (l or g), type (O for an object, F
# for a function, f for a source file), section, size and name. objdump
# lists the symbols in the table's order, in which each source file's local
# symbols follow its file symbol.
OBJDUMP_SYMBOL = re.compile(
    r"^([0-9a-f]{8}) (.).{5}(.) (\S+)\s+([0-9a-f]{8}) (\S+)$", re.MULTILINE
)
LOCAL_BINDING = "l"
GLOBAL_BINDING = "g"
NO_TYPE = " "
OBJECT_TYPE = "O"
FUNCTION_TYPE = "F"
FILE_TYPE = "f"
# The suffix GCC numbers a function's static variables with, which differs
# between compilers, and the prefix of the switch tables that only GCC 16
# makes
STATIC_NUMBER = re.compile(r"\.\d+$")
SWITCH_TABLE_PREFIX = "CSWTCH."
# The game's sources, by basename, which file symbols give. KMC's name the
# copy that the build converts to Shift JIS.
GAME_SOURCES = frozenset(path.name for path in (ROOT / "src").rglob("*.c"))
SHIFT_JIS_SUFFIX = ".sjis.c"
# The modern build's objects of the game's sources, whose globals are the
# game's, and an nm line of one of them: address or blanks, type, name
GAME_OBJECT_FILES = "src/**/*.o"
NM_SYMBOL = re.compile(r"^\S* *(\S) (\S+)$", re.MULTILINE)
# The suffix of the sections of bss, whose load addresses aren't in the ROM
BSS_SUFFIX = "_bss"
# An objdump -h line: index, name, size, VMA, LMA
OBJDUMP_SECTION = re.compile(
    r"^\s*\d+ (\S+)\s+([0-9a-f]{8})\s+([0-9a-f]{8})\s+([0-9a-f]{8})", re.MULTILINE
)


class HarnessError(Exception):
    """The harness replied with an error or an unexpected reply."""


# A place a word addresses: its tag, the name of the object, function,
# marker or section there, and the offset in it
Place = tuple[int, str, int]
MARKER_TAG = -1
ROM_TAG = -2
# The offset that names a section's end, whose size differs with the code
END_OFFSET = -1


@dataclass(frozen=True)
class Symbol:
    """An object or function in one build's symbol table."""

    source: str
    name: str
    is_local: bool
    is_object: bool
    address: int
    size: int
    section: str


@dataclass(frozen=True)
class DataObject:
    """An object or function, by where it is in one build."""

    key: str
    address: int
    size: int
    section: str


@dataclass(frozen=True)
class WordDifference:
    """A word of an object that differs between the builds."""

    offset: int
    matching: int
    modern: int


@dataclass
class FrameState:
    """The RDRAM of one build at one frame, and its loaded overlays."""

    rdram: bytes
    sections: frozenset[str]


@dataclass
class SectionContents:
    """The objects and functions of one section, by address."""

    starts: list[int] = field(default_factory=list[int])
    members: list[DataObject] = field(default_factory=list[DataObject])


@dataclass
class Build:
    """One build's symbols and sections."""

    directory: Path
    symbols: list[Symbol] = field(default_factory=list[Symbol])
    objects: list[DataObject] = field(default_factory=list[DataObject])
    sections: dict[str, SectionContents] = field(
        default_factory=dict[str, SectionContents]
    )
    rom_starts: list[int] = field(default_factory=list[int])
    rom_sections: list[tuple[int, int, str]] = field(
        default_factory=list[tuple[int, int, str]]
    )
    ram_sections: dict[str, tuple[int, int]] = field(
        default_factory=dict[str, tuple[int, int]]
    )
    markers: dict[int, set[str]] = field(default_factory=dict[int, set[str]])
    addresses: dict[str, int] = field(default_factory=dict[str, int])
    # The addresses where the set of sections whose members span them
    # changes, and the sections from each up to the next
    cuts: list[int] = field(default_factory=list[int])
    covers: list[list[SectionContents]] = field(
        default_factory=list[list[SectionContents]]
    )

    def __post_init__(self) -> None:
        """Read the symbols and sections of the build."""
        elf = str(self.directory / ELF_NAME)
        headers = run_tool("mips-linux-gnu-objdump", "-h", elf)
        for match in OBJDUMP_SECTION.finditer(headers):
            name = match.group(1)
            size, vma, lma = (int(match.group(index), 16) for index in (2, 3, 4))
            self.ram_sections[name] = (vma, vma + size)
            if not name.endswith(BSS_SUFFIX) and size:
                self.rom_sections.append((lma, lma + size, name))
        self.rom_sections.sort()
        self.rom_starts = [start for start, _end, _name in self.rom_sections]
        source = ""
        table = run_tool("mips-linux-gnu-objdump", "-t", elf)
        for match in OBJDUMP_SYMBOL.finditer(table):
            binding, kind, section, name = match.group(2, 3, 4, 6)
            address, size = int(match.group(1), 16), int(match.group(5), 16)
            if kind == FILE_TYPE:
                source = source_name(name)
                continue
            _ = self.addresses.setdefault(name, address)
            if kind == NO_TYPE and binding == GLOBAL_BINDING and size == 0:
                self.markers.setdefault(address, set()).add(name)
            if kind not in {OBJECT_TYPE, FUNCTION_TYPE} or size == 0:
                continue
            if name.startswith(SWITCH_TABLE_PREFIX):
                continue
            is_local = binding == LOCAL_BINDING
            if is_local:
                name = STATIC_NUMBER.sub("", name)
            self.symbols.append(
                Symbol(
                    source if is_local else "",
                    name,
                    is_local,
                    kind == OBJECT_TYPE,
                    address,
                    size,
                    section,
                )
            )

    def name_symbols(self, keys: dict[Symbol, str]) -> None:
        """Name each of the build's symbols by KEYS, which pair the builds'."""
        self.objects = []
        self.sections = {}
        for symbol in sorted(self.symbols, key=lambda symbol: symbol.address):
            named = DataObject(
                keys[symbol], symbol.address, symbol.size, symbol.section
            )
            contents = self.sections.setdefault(symbol.section, SectionContents())
            contents.starts.append(symbol.address)
            contents.members.append(named)
            if symbol.is_object:
                self.objects.append(named)
        self.index_sections()

    def index_sections(self) -> None:
        """Index the sections by the addresses their members span.

        Overlays share addresses, so a word may address several sections'
        members; the index finds those sections with one search, rather than
        one for each section. A member's end is one of its places, so a
        section spans one byte past its last member's end.
        """
        spans = [
            (
                contents.starts[0],
                max(member.address + member.size for member in contents.members) + 1,
                contents,
            )
            for contents in self.sections.values()
        ]
        self.cuts = sorted(
            {address for start, end, _ in spans for address in (start, end)}
        )
        self.covers = [
            [contents for start, end, contents in spans if start <= cut < end]
            for cut in self.cuts
        ]

    def spanning_sections(self, address: int) -> list[SectionContents]:
        """Find the sections whose members span ADDRESS."""
        index = bisect.bisect_right(self.cuts, address) - 1
        return self.covers[index] if index >= 0 else []

    @property
    def rom(self) -> Path:
        """The build's ROM."""
        return self.directory / ROM_NAME

    def places(self, word: int, state: FrameState) -> set[Place]:
        """Name the places WORD can address, with its tag.

        The word's low bits address RDRAM, whether through KSEG0 or
        physically, as the RCP takes it, and name each object or function
        that holds the address or that it ends. Overlays share addresses, so
        those of each can, and a pair of words is alike if any of their
        places are. A KSEG0 address of no symbol's in a resident section
        names the unnamed data there, such as a string, by its bytes. A
        word that is a linker marker's address names it, as does one in the
        ROM its section.
        """
        tag, physical = word & ~PHYSICAL_MASK, word & PHYSICAL_MASK
        found = self.ram_places(tag, physical | KSEG0, state)
        if tag == KSEG0:
            found.update(self.unnamed_places(physical, state))
        found.update((MARKER_TAG, name, 0) for name in self.markers.get(word, ()))
        found.update(self.rom_places(word))
        return found

    def ram_places(self, tag: int, address: int, state: FrameState) -> set[Place]:
        """Name the symbols and the heap's bytes at ADDRESS."""
        found: set[Place] = set()
        for contents in self.spanning_sections(address):
            for key, offset in holders(contents, address):
                found.add((tag, key, offset))
                if key in PAGE_ALIGNED_CONTENTS:
                    skip = -(address - offset) % PAGE_SIZE
                    found.add((tag, key + PAGE_SUFFIX, offset - skip))
                if key in DRAWN_COPIES:
                    found.add(self.copied_place(DRAWN_COPIES[key], tag, offset, state))
        heap_offset = address - (MAIN_REGION_END - MAIN_REGION_SIZE)
        if 0 <= heap_offset < MAIN_REGION_SIZE:
            found.add((tag, HEAP_NAME, heap_offset))
        return found

    def unnamed_places(self, physical: int, state: FrameState) -> Iterator[Place]:
        """Name the unnamed data at PHYSICAL in a resident section, by its bytes."""
        address = physical | KSEG0
        resident = [self.sections.get(name) for name in state.sections]
        if any(holders(contents, address) for contents in resident if contents):
            return
        if self.in_resident_section(address, state.sections):
            data = state.rdram[physical : physical + UNNAMED_DATA_LENGTH]
            yield (KSEG0, UNNAMED_PREFIX + data.split(b"\0")[0].hex(), 0)

    def rom_places(self, word: int) -> Iterator[Place]:
        """Name the section of the ROM at WORD, and its offset or its end."""
        index = bisect.bisect_right(self.rom_starts, word) - 1
        for start, end, name in self.rom_sections[max(index - 1, 0) : index + 1]:
            if start <= word < end:
                yield (ROM_TAG, name, word - start)
            elif word == end:
                yield (ROM_TAG, name, END_OFFSET)

    def copied_place(
        self, copy: DrawnCopy, tag: int, offset: int, state: FrameState
    ) -> Place:
        """Name the place that a byte of a drawn copy copies."""
        if not copy.through_pointer:
            return (tag, copy.source, offset % copy.size)
        pointer = big_endian_word(state.rdram, self.addresses[copy.source] - KSEG0)
        heap_offset = pointer - (MAIN_REGION_END - MAIN_REGION_SIZE)
        return (tag, HEAP_NAME, heap_offset + offset % copy.size)

    def in_resident_section(self, address: int, sections: frozenset[str]) -> bool:
        """Whether ADDRESS is in one of the resident SECTIONS."""
        return any(
            start <= address < end
            for start, end in (self.ram_sections.get(name, (0, 0)) for name in sections)
        )


def holders(contents: SectionContents, address: int) -> list[tuple[str, int]]:
    """Name the members that hold ADDRESS or end at it, with its offsets."""
    index = bisect.bisect_right(contents.starts, address) - 1
    if index < 0:
        return []
    found: list[tuple[str, int]] = []
    member = contents.members[index]
    if address <= member.address + member.size:
        found.append((member.key, address - member.address))
    if address == member.address and index > 0:
        previous = contents.members[index - 1]
        if previous.address + previous.size == address:
            found.append((previous.key, previous.size))
    return found


def source_name(file_symbol: str) -> str:
    """Name a file symbol's source by its basename, as GCC 16 names it."""
    basename = file_symbol.rsplit("/", 1)[-1]
    if basename.endswith(SHIFT_JIS_SUFFIX):
        return basename.removesuffix(SHIFT_JIS_SUFFIX) + ".c"
    return basename


def game_globals() -> frozenset[str]:
    """Name the globals that the modern build's game objects define."""
    object_files = sorted(str(path) for path in MODERN_DIR.glob(GAME_OBJECT_FILES))
    listing = run_tool("mips-linux-gnu-nm", "--defined-only", "-g", *object_files)
    return frozenset(match.group(2) for match in NM_SYMBOL.finditer(listing))


def pair_symbols(builds: tuple[Build, Build]) -> frozenset[str]:
    """Name both builds' symbols alike; return the game's objects' names.

    A global is named by its symbol. So is a static, where no other symbol
    of either build has its name, and otherwise it is named by its source
    too. The matching build defines main's commons in main_bss.c, as
    globals, so the modern build, whose modules define their own, says
    which objects are the game's.
    """
    counts = [
        collections.Counter(symbol.name for symbol in build.symbols) for build in builds
    ]
    keys: dict[Symbol, str] = {}
    for build in builds:
        for symbol in build.symbols:
            unique = all(count[symbol.name] <= 1 for count in counts)
            if symbol.is_local and not unique:
                keys[symbol] = f"{symbol.source}:{symbol.name}"
            else:
                keys[symbol] = symbol.name
    for build in builds:
        build.name_symbols(keys)
    names = game_globals()
    return frozenset(
        keys[symbol]
        for symbol in builds[1].symbols
        if symbol.is_object
        and (symbol.source in GAME_SOURCES if symbol.is_local else symbol.name in names)
    )


def run_tool(*command: str) -> str:
    """Run a program and return its output."""
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


@final
class Harness:
    """A running n64-harness, which halts at the frame's queueing."""

    def __init__(self, program: str, build: Build) -> None:
        """Start the harness on BUILD's ROM and break at nuGfxTaskStart."""
        self.build = build
        self.process: subprocess.Popen[str] = subprocess.Popen(
            [program, str(build.rom)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            start_new_session=True,
        )
        _ = self.command(f"break {build.addresses['nuGfxTaskStart']:x}")

    def send(self, text: str) -> None:
        """Send a command without waiting for its reply."""
        stdin = self.process.stdin
        if stdin is None:
            raise HarnessError(text)
        _ = stdin.write(text + "\n")
        stdin.flush()

    def reply(self) -> str:
        """Read a command's reply."""
        stdout = self.process.stdout
        if stdout is None:
            raise HarnessError
        line = cast("str", stdout.readline()).strip()
        if not line or line.startswith("error"):
            raise HarnessError(line or "the harness exited")
        return line

    def command(self, text: str) -> str:
        """Send a command and return its reply."""
        self.send(text)
        return self.reply()

    def read_word(self, address: int) -> int:
        """Read a word of memory."""
        return int(self.command(f"read {address:x} {WORD_SIZE:x}"), 16)

    def frame(self) -> int:
        """Read which frame of logic the game is on."""
        return self.read_word(self.build.addresses["g_FrameCount"])

    def is_loading(self) -> bool:
        """Whether the hole loader thread is at work."""
        return not self.read_word(self.build.addresses["g_HasLoaderFinished"])

    def start_dump(self, dump: Path) -> None:
        """Have the harness dump RDRAM to DUMP, without waiting for it."""
        self.send(f"dump {dump} {KSEG0:x} {RDRAM_SIZE:x}")

    def state(self, dump: Path) -> FrameState:
        """Take the dump that start_dump began, and the overlays loaded in it."""
        reply = self.reply()
        if reply != f"ok {RDRAM_SIZE}":
            raise HarnessError(reply)
        rdram = dump.read_bytes()
        return FrameState(rdram, self.resident_sections(rdram))

    def resident_sections(self, rdram: bytes) -> frozenset[str]:
        """Find the main segment's sections and the loaded overlays'."""
        resident = {".main", ".main_bss"}
        table = self.build.addresses["f_Overlays"] - KSEG0
        for entry in range(table, table + overlay_table_size(self.build), OVERLAY_SIZE):
            if not rdram[entry]:
                continue
            rom_start = big_endian_word(rdram, entry + OVERLAY_ROM_START_OFFSET)
            index = bisect.bisect_left(self.build.rom_starts, rom_start)
            if self.build.rom_starts[index : index + 1] == [rom_start]:
                section = self.build.rom_sections[index][2]
                resident.update({section, section + BSS_SUFFIX})
        return frozenset(resident)

    def close(self) -> None:
        """Stop the harness and anything it started."""
        with contextlib.suppress(ProcessLookupError):
            os.killpg(self.process.pid, signal.SIGKILL)
        _ = self.process.wait()


def overlay_table_size(build: Build) -> int:
    """Find the size of the overlay table, f_Overlays."""
    for symbol in build.symbols:
        if symbol.name == "f_Overlays":
            return symbol.size
    message = "f_Overlays"
    raise KeyError(message)


def big_endian_word(rdram: bytes, offset: int) -> int:
    """Read a word from a dump of RDRAM."""
    return int.from_bytes(rdram[offset : offset + WORD_SIZE], "big")


def run_frames(harnesses: tuple[Harness, Harness], target: int) -> None:
    """Run both builds until each queues TARGET's display list."""
    pending = list(harnesses)
    while pending:
        for harness in pending:
            harness.send(f"run {FRAME_TIMEOUT}")
        still_behind: list[Harness] = []
        for harness in pending:
            reply = harness.reply()
            if not reply.startswith("halt"):
                message = f"{harness.build.directory}: no frame queued ({reply})"
                raise HarnessError(message)
            if harness.frame() < target:
                still_behind.append(harness)
        pending = still_behind


def game_objects(
    build: Build, game: frozenset[str], sections: frozenset[str]
) -> dict[str, DataObject]:
    """Select the game's objects that are resident in SECTIONS."""
    return {
        data_object.key: data_object
        for data_object in build.objects
        if data_object.key in game and data_object.section in sections
    }


def unpaired_objects(builds: tuple[Build, Build], game: frozenset[str]) -> list[str]:
    """List the game's objects that one build lacks or sizes differently."""
    sizes = [
        {data_object.key: data_object.size for data_object in build.objects}
        for build in builds
    ]
    return sorted(key for key in game if sizes[0].get(key) != sizes[1].get(key))


@dataclass
class Comparison:
    """Two builds' states at one frame, compared object by object."""

    builds: tuple[Build, Build]
    states: tuple[FrameState, FrameState]
    comparable: frozenset[str]

    def objects(self) -> Iterator[tuple[str, list[WordDifference]]]:
        """Each object that differs, with its differing words."""
        matching = game_objects(
            self.builds[0], self.comparable, self.states[0].sections
        )
        modern = game_objects(self.builds[1], self.comparable, self.states[1].sections)
        for key in sorted(matching.keys() | modern.keys()):
            if key not in matching or key not in modern:
                yield key, []
                continue
            words = self.object_words(key, matching[key], modern[key])
            if words:
                yield key, words
        heap_start = MAIN_REGION_END - MAIN_REGION_SIZE
        words = self.words(heap_start, heap_start, MAIN_REGION_SIZE)
        if words:
            yield HEAP_NAME, words

    def object_words(
        self, key: str, matching: DataObject, modern: DataObject
    ) -> list[WordDifference]:
        """Find the words of an object that differ, but for layout."""
        words = self.words(matching.address, modern.address, matching.size)
        if words and key in PAGE_ALIGNED_CONTENTS:
            skips = [
                -address % PAGE_SIZE for address in (matching.address, modern.address)
            ]
            size = matching.size - max(skips)
            aligned = self.words(
                matching.address + skips[0], modern.address + skips[1], size
            )
            still_differ = {word.offset + skips[0] for word in aligned}
            words = [word for word in words if word.offset in still_differ]
        padding = PADDING.get(key)
        if padding is not None:
            words = [
                word
                for word in words
                if (word.matching ^ word.modern)
                & ~padding.masks.get(word.offset % padding.element_size, 0)
            ]
        return words

    def words(self, matching: int, modern: int, size: int) -> list[WordDifference]:
        """Find the words that differ between two spans, but for layout."""
        first = self.states[0].rdram[matching - KSEG0 : matching - KSEG0 + size]
        second = self.states[1].rdram[modern - KSEG0 : modern - KSEG0 + size]
        if first == second:
            return []
        whole = size - size % WORD_SIZE
        differences: list[WordDifference] = []
        pairs = zip(
            iter_unpack(">I", first[:whole]),
            iter_unpack(">I", second[:whole]),
            strict=True,
        )
        for index, ((matching_word,), (modern_word,)) in enumerate(pairs):
            if matching_word != modern_word and not self.same_target(
                matching_word, modern_word
            ):
                differences.append(
                    WordDifference(index * WORD_SIZE, matching_word, modern_word)
                )
        if first[whole:] != second[whole:]:
            tails = (
                int.from_bytes(first[whole:], "big"),
                int.from_bytes(second[whole:], "big"),
            )
            differences.append(WordDifference(whole, *tails))
        return differences

    def same_target(self, matching: int, modern: int) -> bool:
        """Whether two words can address the same place in each build."""
        first = self.builds[0].places(matching, self.states[0])
        return bool(first) and not first.isdisjoint(
            self.builds[1].places(modern, self.states[1])
        )


@dataclass
class Finding:
    """An object that differed: when, and its words when it first did.

    The hole loader thread runs at each build's own speed, so while either
    build's is at work, the builds may differ in what it has written yet.
    The frames after both have finished must agree.
    """

    frame: int = 0
    words: list[WordDifference] = field(default_factory=list[WordDifference])
    frames: int = 0
    settled_frames: int = 0
    last_frame: int = 0

    def add(self, frame: int, words: list[WordDifference], *, loading: bool) -> None:
        """Count a frame in which the object differed."""
        if not self.frames or (not loading and not self.settled_frames):
            self.frame, self.words = frame, words
        self.frames += 1
        self.settled_frames += not loading
        self.last_frame = frame

    def ended_at_boot(self) -> bool:
        """Whether the object differed only from the first frame on, until it agreed."""
        return self.frames == self.last_frame


def characterize(program: str, frames: int, *, verbose: bool) -> dict[str, Finding]:
    """Compare the builds over FRAMES frames; return what differed."""
    builds = (Build(MATCHING_DIR), Build(MODERN_DIR))
    game = pair_symbols(builds)
    unpaired = unpaired_objects(builds, game)
    comparable = game - frozenset(unpaired)
    findings = {key: Finding() for key in unpaired}
    harnesses: list[Harness] = []
    with tempfile.TemporaryDirectory(dir="/dev/shm") as scratch:
        try:
            harnesses = [Harness(program, build) for build in builds]
            pair = (harnesses[0], harnesses[1])
            for frame in range(1, frames + 1):
                run_frames(pair, frame)
                dumps = (Path(scratch) / "matching.bin", Path(scratch) / "modern.bin")
                for harness, dump in zip(pair, dumps, strict=True):
                    harness.start_dump(dump)
                states = (pair[0].state(dumps[0]), pair[1].state(dumps[1]))
                loading = any(harness.is_loading() for harness in pair)
                comparison = Comparison(builds, states, comparable)
                for key, words in comparison.objects():
                    if verbose and key not in findings:
                        print(f"frame {frame}: {key} differs")
                    findings.setdefault(key, Finding()).add(
                        frame, words, loading=loading
                    )
        finally:
            for harness in harnesses:
                harness.close()
    return findings


def describe(finding: Finding) -> str:
    """Describe an object's first differing word."""
    if not finding.frames:
        return "in one build only, or of another size there"
    if not finding.words:
        return "loaded in one build only"
    words = finding.words
    first = words[0]
    values = f"0x{first.matching:x} vs 0x{first.modern:x}"
    return f"+0x{first.offset:x}: {values}, {len(words)} words"


def report(findings: dict[str, Finding]) -> bool:
    """Print the findings; return whether all were expected."""
    unexpected = 0
    for key, finding in sorted(findings.items(), key=lambda item: item[1].frame):
        reason = KNOWN_DIFFERENCES.get(key)
        if reason == MAIN_STACK_OVERFLOW and not finding.ended_at_boot():
            reason = None
        if reason is None and finding.frames and not finding.settled_frames:
            reason = LOADING
        verdict = "expected" if reason is not None else "DIFFERS"
        when = f"from frame {finding.frame}, in {finding.frames} frames"
        print(f"{verdict} {key} {when if finding.frames else 'in the layout'}")
        print(f"    {reason or describe(finding)}")
        unexpected += reason is None
    print(f"{len(findings)} objects differed, {unexpected} unexpectedly")
    return unexpected == 0


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    frames: int = DEFAULT_FRAMES
    harness: str = DEFAULT_HARNESS
    verbose: bool = False


def main() -> int:
    """Parse the arguments and run the comparison."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("--frames", type=int, help="frames of logic to compare")
    _ = parser.add_argument("--harness", help="the n64-harness program")
    _ = parser.add_argument(
        "--verbose", action="store_true", help="print each finding as found"
    )
    arguments = parser.parse_args(namespace=Arguments())
    findings = characterize(
        arguments.harness, arguments.frames, verbose=arguments.verbose
    )
    return 0 if report(findings) else 1


if __name__ == "__main__":
    sys.exit(main())
