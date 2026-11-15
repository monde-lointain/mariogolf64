#!/usr/bin/env python3
"""Characterize the modern build against the matching build, frame by frame.

    equivalence.py [--frames N] [--harness PATH] [--reference DIR]
                   [--modern DIR] [--verbose]

Runs build/mariogolf64.z64 and build/modern/mariogolf64.z64 side by side in
ares's n64-harness and stops each where nuGfxTaskStart queues a frame's
display list. Every frame's game logic and drawing has run by then, so the
game's state there is a narrow place through which all of the frame's
behavior passes. Frames are paired by g_FrameCount, which counts frames of
logic, and not by when each build shows them, as the builds run at
different speeds.

The game's logic follows two things that the harness does not otherwise
control: the buttons, which neither run presses, and how many frames each
hole takes to load, which follows the speed of the build's code. The
harness controls the loads too. Each build's loader ends at once after it
sets g_HasLoaderFinished, which the game moves on in the first frame to
find set, so the harness clears it there, and sets it in both builds as
the first frame ends by which both loads have ended. Both then load in
the same frames, as a slower loader would have, and builds of any speed
can be compared. A loader also writes the buffers of the frame it
prepares the hole in, whose parity follows its speed, so there the
modern build is given the matching build's (see LoaderGate).

--reference and --modern name other builds' directories. With
build/nonmatching as the reference, only the compilers differ, which
compares source that no longer matches the original. Any modern build,
such as one with other optimizations, can stand for the modern one.

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
copies. The padding of some structures, which no one reads, is left out,
as is the build date that each build carries, and the copies of it.

The sun's glare grows and fades with what the z-buffer shows as each frame's
task ends, so its strength follows how far the RDP lags. The modern build
draws each frame's lens flare with the strength that the matching build
drew it with, so that the rest of the display list can still be compared.

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
import multiprocessing
import os
import re
import signal
import subprocess
import sys
import tempfile
from collections.abc import Iterator
from dataclasses import dataclass, field, replace
from multiprocessing.pool import AsyncResult
from pathlib import Path
from struct import iter_unpack
from typing import cast, final

from heap_region import read_main_region

ROOT = Path(__file__).resolve().parent.parent
REFERENCE_DIR = ROOT / "build"
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
MAIN_REGION = read_main_region()
MAIN_REGION_END = MAIN_REGION.end
MAIN_REGION_SIZE = MAIN_REGION.size
HEAP_START = MAIN_REGION.start
HEAP_NAME = "(main heap)"

# The hole loader thread, the flag it sets as it ends, and the routine it
# ends itself with, whose first argument is the thread
LOADER_THREAD = "f_LoaderThread"
LOADER_FINISHED = "g_HasLoaderFinished"
THREAD_END = "osDestroyThread"
FIRST_ARGUMENT_REGISTER = 4
ADDRESS_MASK = 0xFFFF_FFFF
RUNNING_THREAD = "__osRunningThread"
# What a hole loader leaves in the buffers of the frame it gets there in,
# which the graphics thread swaps each frame: the index of the matrices'
# buffer, which it sets to 0, and the routine with which it writes the
# characters' lights in the draw buffer's slot of each of these
MATRIX_BUFFER_INDEX = "g_MatrixBufferIndex"
DRAW_BUFFER_INDEX = "g_DrawBufferIndex"
LIGHTS_ROUTINE = "SetCharacterLights"
DRAW_BUFFERED_LIGHTS = ("f_DryLights", "f_RainLights")
DRAW_BUFFER_COUNT = 2

# The layout of an Overlay (include/system/overlay_manager.h)
OVERLAY_SIZE = 40
OVERLAY_ROM_START_OFFSET = 8

SUMMARY = "Characterize the modern build against the matching build."
DEFAULT_FRAMES = 3000
DEFAULT_HARNESS = os.environ.get("N64_HARNESS", "n64-harness")

# How many video frames the harness may run before a frame of logic must
# have been queued; a build that takes longer has hung
FRAME_TIMEOUT = 600

# The processes that compare frames, while the harnesses run the next, and
# how many frames' dumps may wait for them, each in its own pair of files
WORKER_COUNT = max(1, (os.cpu_count() or 1) - 4)
DUMP_SLOT_COUNT = 2 * WORKER_COUNT

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
# The glare's strength, which the modern build draws each lens flare with
# as the matching build drew it
GLARE_STRENGTH = "f_SunGlareStrength"
FRAME_RATE = "it counts retraces, so it follows speed"
# libmus starts a sound only on a channel that its audio thread has freed,
# and numbers each that it starts, so a start that comes a retrace sooner
# may fail, and the numbers after it shift.
AUDIO_HANDLE = "libmus's handles depend on when its audio thread frees channels"
# The original blends both copies of the sky's vertices, one of which the
# RSP may be reading, where AVOID_UB blends only the frame's own.
SKY_BLENDING = "AVOID_UB blends only the frame's copy (a9ac2e65)"
# Three of ovl_01's files have their own copies of this table, which the
# collection of unused sections leaves two of, so the LTO build, which
# names no source, has no way to say which (see attribute_sources)
UNATTRIBUTED_COPY = "LTO names no source, and the builds keep different copies"
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
    "f_DrawnImpactFlashLights": AVOID_UB_ONLY,
    "f_DrawnSunScreenX": AVOID_UB_ONLY,
    "f_DrawnSunScreenY": AVOID_UB_ONLY,
    "f_SkyVertices": SKY_BLENDING,
    ":f_CharacterChoices": UNATTRIBUTED_COPY,
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
    "f_DrawnImpactFlashLights": DrawnCopy("f_ImpactFlashLights", 0x50),
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


# A span of bytes in an object
@dataclass(frozen=True)
class Field:
    """Where a field lies in an object."""

    offset: int
    size: int

    def holds(self, offset: int) -> bool:
        """Return whether the word at OFFSET is in the field."""
        return self.offset <= offset < self.offset + self.size


# Each build has a build date of its own (see BUILD_DATE in the Makefile),
# which the game reads from ROM and stamps into each save, inverted. By
# object, the fields that hold it, as include/system/main.h and
# include/save/save.h lay them out.
ROM_BUILD_DATE_SIZE = 0x40
SAVE_BUILD_DATE = Field(0x8, 0x20)
BUILD_DATE_FIELDS = {
    "g_RomBuildDate": Field(0, ROM_BUILD_DATE_SIZE),
    "f_SaveData": SAVE_BUILD_DATE,
}

# An objdump -t line: address, binding (l or g), type (O for an object, F
# for a function, f for a source file), section, size and name, which a
# hidden symbol's visibility comes before. objdump lists the symbols in the
# table's order, in which each source file's local symbols follow its file
# symbol. The objects that LTO compiles have file symbols with no name.
OBJDUMP_SYMBOL = re.compile(
    r"^([0-9a-f]{8}) (.).{5}(.) (\S+)\s+([0-9a-f]{8})(?: \.hidden)? ?(\S*)$",
    re.MULTILINE,
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
# LTO renames a static that another file's static shares the name of, or
# that it makes a hidden global, to use it from another part of the
# program, with this suffix
LTO_PRIVATE = re.compile(r"\.lto_priv\.\d+$")
SWITCH_TABLE_PREFIX = "CSWTCH."
# The game's sources, by basename, which file symbols give. KMC's name the
# copy that the build converts to Shift JIS.
GAME_SOURCES = frozenset(path.name for path in (ROOT / "src").rglob("*.c"))
SHIFT_JIS_SUFFIX = ".sjis.c"
# The modern build's objects of the game's sources, whose globals and
# statics are the game's, an nm line of one of them: address or blanks,
# type, name, and the types of a static object, in data or in bss
GAME_OBJECT_FILES = "src/**/*.o"
NM_SYMBOL = re.compile(r"^\S* *(\S) (\S+)$", re.MULTILINE)
STATIC_OBJECT_TYPES = frozenset("bdgrs")
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
    # The places that each word seen can address in any frame
    fixed_place_cache: dict[int, frozenset[Place]] = field(
        default_factory=dict[int, frozenset[Place]]
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
            is_local = binding == LOCAL_BINDING or bool(LTO_PRIVATE.search(name))
            if is_local:
                name = STATIC_NUMBER.sub("", LTO_PRIVATE.sub("", name))
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

    def object_size(self, name: str) -> int:
        """Find the size of the build's one object named NAME."""
        sizes = [
            symbol.size
            for symbol in self.symbols
            if symbol.is_object and symbol.name == name
        ]
        if len(sizes) != 1:
            message = f"{self.directory}: {len(sizes)} objects named {name}"
            raise KeyError(message)
        return sizes[0]

    @property
    def rdram_extent(self) -> int:
        """How much of RDRAM, from its start, holds the build's sections and heap.

        Every place compared or read lies in one of them, so a dump of this
        much of RDRAM holds them all.
        """
        ends = [
            end
            for start, end in self.ram_sections.values()
            if KSEG0 <= start < KSEG0 + RDRAM_SIZE
        ]
        return max([*ends, MAIN_REGION_END]) - KSEG0

    def frame_state(self, rdram: bytes) -> FrameState:
        """Take a frame's RDRAM, with the overlays loaded in it."""
        return FrameState(rdram, self.resident_sections(rdram))

    def resident_sections(self, rdram: bytes) -> frozenset[str]:
        """Find the main segment's sections and the loaded overlays'."""
        resident = {".main", ".main_bss"}
        table = self.addresses["f_Overlays"] - KSEG0
        for entry in range(table, table + overlay_table_size(self), OVERLAY_SIZE):
            if not rdram[entry]:
                continue
            rom_start = big_endian_word(rdram, entry + OVERLAY_ROM_START_OFFSET)
            index = bisect.bisect_left(self.rom_starts, rom_start)
            if self.rom_starts[index : index + 1] == [rom_start]:
                section = self.rom_sections[index][2]
                resident.update({section, section + BSS_SUFFIX})
        return frozenset(resident)

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
        found = set(self.fixed_places(word))
        found.update(self.frame_places(word, state))
        return found

    def fixed_places(self, word: int) -> frozenset[Place]:
        """Name the places WORD can address in any frame; see places."""
        found = self.fixed_place_cache.get(word)
        if found is None:
            found = frozenset(self.find_fixed_places(word))
            self.fixed_place_cache[word] = found
        return found

    def find_fixed_places(self, word: int) -> Iterator[Place]:
        """Name the symbols, heap bytes, markers and ROM sections at WORD."""
        tag, physical = word & ~PHYSICAL_MASK, word & PHYSICAL_MASK
        address = physical | KSEG0
        for contents in self.spanning_sections(address):
            for member, offset in holders(contents, address):
                key = member.key
                yield (tag, key, offset)
                if key in PAGE_ALIGNED_CONTENTS:
                    skip = -(address - offset) % PAGE_SIZE
                    yield (tag, key + PAGE_SUFFIX, offset - skip)
                copy = DRAWN_COPIES.get(key)
                if copy is not None and not copy.through_pointer:
                    yield (tag, copy.source, offset % copy.size)
        heap_offset = address - HEAP_START
        if 0 <= heap_offset < MAIN_REGION_SIZE:
            yield (tag, HEAP_NAME, heap_offset)
        yield from ((MARKER_TAG, name, 0) for name in self.markers.get(word, ()))
        yield from self.rom_places(word)

    def frame_places(self, word: int, state: FrameState) -> Iterator[Place]:
        """Name the places WORD addresses by what the frame's RDRAM holds.

        These are the heap's bytes that a drawn copy copies through a
        pointer, and unnamed data, which is named by its bytes.
        """
        tag, physical = word & ~PHYSICAL_MASK, word & PHYSICAL_MASK
        address = physical | KSEG0
        for contents in self.spanning_sections(address):
            for member, offset in holders(contents, address):
                copy = DRAWN_COPIES.get(member.key)
                if copy is not None and copy.through_pointer:
                    yield self.pointed_place(copy, tag, offset, state)
        if tag == KSEG0:
            yield from self.unnamed_places(physical, state)

    def unnamed_places(self, physical: int, state: FrameState) -> Iterator[Place]:
        """Name the unnamed data at PHYSICAL in a resident section, by its bytes.

        Data that follows a named object begins where the object ends, so
        only an object that holds the address names it alone.
        """
        address = physical | KSEG0
        resident = [self.sections.get(name) for name in state.sections]
        if any(
            offset < member.size
            for contents in resident
            if contents
            for member, offset in holders(contents, address)
        ):
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

    def pointed_place(
        self, copy: DrawnCopy, tag: int, offset: int, state: FrameState
    ) -> Place:
        """Name the heap's byte that a byte of a copy through a pointer copies."""
        pointer = big_endian_word(state.rdram, self.addresses[copy.source] - KSEG0)
        return (tag, HEAP_NAME, pointer - HEAP_START + offset % copy.size)

    def in_resident_section(self, address: int, sections: frozenset[str]) -> bool:
        """Whether ADDRESS is in one of the resident SECTIONS."""
        return any(
            start <= address < end
            for start, end in (self.ram_sections.get(name, (0, 0)) for name in sections)
        )


def holders(contents: SectionContents, address: int) -> list[tuple[DataObject, int]]:
    """List the members that hold ADDRESS or end at it, with its offsets."""
    index = bisect.bisect_right(contents.starts, address) - 1
    if index < 0:
        return []
    found: list[tuple[DataObject, int]] = []
    member = contents.members[index]
    if address <= member.address + member.size:
        found.append((member, address - member.address))
    if address == member.address and index > 0:
        previous = contents.members[index - 1]
        if previous.address + previous.size == address:
            found.append((previous, previous.size))
    return found


def source_name(file_symbol: str) -> str:
    """Name a file symbol's source by its basename, as GCC 16 names it."""
    basename = file_symbol.rsplit("/", 1)[-1]
    if basename.endswith(SHIFT_JIS_SUFFIX):
        return basename.removesuffix(SHIFT_JIS_SUFFIX) + ".c"
    return basename


@final
class GameSymbols:
    """The names of the globals and statics that the game's objects define."""

    def __init__(self, directory: Path) -> None:
        """Read the game's objects in DIRECTORY."""
        object_files = sorted(str(path) for path in directory.glob(GAME_OBJECT_FILES))
        listing = run_tool("mips-linux-gnu-nm", "--defined-only", *object_files)
        self.globals: frozenset[str] = frozenset(
            match.group(2)
            for match in NM_SYMBOL.finditer(listing)
            if match.group(1).isupper()
        )
        self.statics: frozenset[str] = frozenset(
            STATIC_NUMBER.sub("", match.group(2))
            for match in NM_SYMBOL.finditer(listing)
            if match.group(1) in STATIC_OBJECT_TYPES
        )

    def has(self, symbol: Symbol) -> bool:
        """Whether SYMBOL is the game's.

        A static is named by its source, or, where LTO left it none, by
        whether a game object defines a static of its name.
        """
        if not symbol.is_local:
            return symbol.name in self.globals
        if symbol.source:
            return symbol.source in GAME_SOURCES
        return symbol.name in self.statics


def address_of(symbol: Symbol) -> int:
    """Return SYMBOL's address, to sort by."""
    return symbol.address


def counterpart(symbol: Symbol, peers: list[Symbol], found: list[Symbol]) -> str:
    """Return the source of SYMBOL's counterpart among FOUND, or "" if unclear.

    PEERS are SYMBOL's build's statics of its name in its segment. Of
    several counterparts, the one of SYMBOL's size is taken, or, of copies
    of one size, the one in its place in their order.
    """
    if len(found) == 1:
        return found[0].source
    alike = sorted((peer for peer in peers if peer.size == symbol.size), key=address_of)
    same = sorted((each for each in found if each.size == symbol.size), key=address_of)
    return same[alike.index(symbol)].source if len(same) == len(alike) else ""


def segment_of(symbol: Symbol) -> tuple[str, str]:
    """Return SYMBOL's name and segment: its section without the bss suffix.

    KMC GCC puts a static that starts as 0 in data, where modern GCC puts
    it in bss.
    """
    return symbol.name, symbol.section.removesuffix(BSS_SUFFIX)


def attribute_sources(builds: tuple[Build, Build]) -> None:
    """Give each static that LTO compiled the source of its counterpart.

    LTO's objects name no source, so its statics take their counterpart's:
    the other build's static of the same name in the same segment, or of
    several, the one of the same size. Copies of one name and size, such
    as the menus' own tables of the characters, are taken in address
    order, which follows the files' order in the layout, as LTO keeps it
    (-fno-toplevel-reorder). A static that matches none, or matches
    copies of which the builds have different numbers, keeps no source,
    so it pairs with nothing and is reported.
    """
    for build, other in (builds, builds[::-1]):
        candidates: dict[tuple[str, str], list[Symbol]] = {}
        for symbol in other.symbols:
            if symbol.is_local and symbol.source:
                candidates.setdefault(segment_of(symbol), []).append(symbol)
        pending: dict[tuple[str, str], list[Symbol]] = {}
        for symbol in build.symbols:
            if symbol.is_local and not symbol.source:
                pending.setdefault(segment_of(symbol), []).append(symbol)
        sources = {
            symbol: counterpart(symbol, peers, candidates.get(key, []))
            for key, peers in pending.items()
            for symbol in peers
        }
        build.symbols = [
            replace(symbol, source=sources[symbol]) if sources.get(symbol) else symbol
            for symbol in build.symbols
        ]


def pair_symbols(builds: tuple[Build, Build]) -> frozenset[str]:
    """Name both builds' symbols alike; return the game's objects' names.

    A global is named by its symbol. So is a static, where no other symbol
    of either build has its name, and otherwise it is named by its source
    too (see attribute_sources). The matching build defines main's commons
    in main_bss.c, as globals, so the modern build, whose modules define
    their own, says which objects are the game's.
    """
    attribute_sources(builds)
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
    game = GameSymbols(builds[1].directory)
    return frozenset(
        keys[symbol]
        for symbol in builds[1].symbols
        if symbol.is_object and game.has(symbol)
    )


def run_tool(*command: str) -> str:
    """Run a program and return its output."""
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


@final
class Harness:
    """A running n64-harness, which halts at the frame's queueing and lens flare."""

    def __init__(self, program: str, build: Build) -> None:
        """Start the harness on BUILD's ROM and break at nuGfxTaskStart.

        Only memory is compared, so the harness shows no video.
        """
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
        _ = self.command(f"break {build.addresses['DrawLensFlare']:x}")
        _ = self.command(f"break {build.addresses[THREAD_END]:x}")
        _ = self.command(f"break {build.addresses[LIGHTS_ROUTINE]:x}")
        _ = self.command("video off")

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

    def halt(self) -> int:
        """Read where a run halted."""
        reply = self.reply()
        if not reply.startswith("halt"):
            message = f"{self.build.directory}: no frame queued ({reply})"
            raise HarnessError(message)
        return int(reply.split()[1], 16)

    def write_word(self, address: int, value: int) -> None:
        """Write a word of memory."""
        _ = self.command(f"write {address:x} {value:08x}")

    def swap_halves(self, name: str) -> None:
        """Swap the two halves of the object NAME."""
        address = self.build.addresses[name]
        half = self.build.object_size(name) // DRAW_BUFFER_COUNT
        first = self.command(f"read {address:x} {half:x}")
        second = self.command(f"read {address + half:x} {half:x}")
        _ = self.command(f"write {address:x} {second}{first}")

    def is_loader_running(self) -> bool:
        """Whether the hole loader thread is the one running."""
        addresses = self.build.addresses
        return self.read_word(addresses[RUNNING_THREAD]) == addresses[LOADER_THREAD]

    def frame(self) -> int:
        """Read which frame of logic the game is on."""
        return self.read_word(self.build.addresses["g_FrameCount"])

    def is_loading(self) -> bool:
        """Whether the hole loader thread is at work, or its end is held."""
        return not self.read_word(self.build.addresses[LOADER_FINISHED])

    def start_dump(self, dump: Path, size: int) -> None:
        """Have the harness dump SIZE bytes of RDRAM to DUMP, without waiting."""
        self.send(f"dump {dump} {KSEG0:x} {size:x}")

    def finish_dump(self, size: int) -> None:
        """Wait for the dump that start_dump began."""
        reply = self.reply()
        if reply != f"ok {size}":
            raise HarnessError(reply)

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


@final
class LoaderGate:
    """Holds each build's finished hole load until both builds' have ended.

    The loader sets g_HasLoaderFinished and then ends its own thread, and
    the main thread, which reads the flag, has the lower priority and can't
    run between the two. So where the loader ends its thread, the flag is
    cleared, and the build is held until the end of a frame by which no
    build's loader is still at work.

    Each build's loader also gets to the end of preparing the hole in a
    frame of its own, in which it writes the lights in the draw buffer's
    slot and sets the matrices' buffer to 0, which the graphics thread
    swaps every frame from then on. Which buffer each frame then uses is the
    outcome of a race that the loader's speed decides, as on the console,
    so as the gate lets the loads end, it gives the modern build the
    matching build's outcome: the lights in the matching build's slot, and
    its matrices' buffer.
    """

    def __init__(self) -> None:
        """Hold no build."""
        self.held: list[Harness] = []
        # The draw buffer that each build's loader wrote the lights in
        self.light_slots: dict[Harness, int] = {}

    def take_thread_end(self, harness: Harness) -> None:
        """Hold HARNESS's build if its loader is ending its thread."""
        addresses = harness.build.addresses
        argument = harness.command(f"reg {FIRST_ARGUMENT_REGISTER}")
        if int(argument, 16) & ADDRESS_MASK != addresses[LOADER_THREAD]:
            return
        if harness.read_word(addresses[LOADER_FINISHED]):
            harness.write_word(addresses[LOADER_FINISHED], 0)
            self.held.append(harness)
        elif harness in self.held:
            message = f"{harness.build.directory}: the game stopped a held loader"
            raise HarnessError(message)

    def take_lights(self, harness: Harness) -> None:
        """Note the slot that HARNESS's loader is writing the lights in, if it is."""
        if harness.is_loader_running():
            address = harness.build.addresses[DRAW_BUFFER_INDEX]
            self.light_slots[harness] = harness.read_word(address)

    def release(self, harnesses: tuple["Harness", "Harness"]) -> None:
        """Set the held builds' flags once no build's loader is at work."""
        if any(h.is_loading() for h in harnesses if h not in self.held):
            return
        if self.held:
            self.align_buffers(harnesses)
        for harness in self.held:
            harness.write_word(harness.build.addresses[LOADER_FINISHED], 1)
        self.held = []

    def align_buffers(self, harnesses: tuple["Harness", "Harness"]) -> None:
        """Give the modern build the matching build's buffers of the loads."""
        matching, modern = harnesses
        slots = [self.light_slots.pop(harness, None) for harness in harnesses]
        if None not in slots and slots[0] != slots[1]:
            for name in DRAW_BUFFERED_LIGHTS:
                modern.swap_halves(name)
        index = matching.read_word(matching.build.addresses[MATRIX_BUFFER_INDEX])
        modern.write_word(modern.build.addresses[MATRIX_BUFFER_INDEX], index)


@dataclass
class FrameRun:
    """Both builds' runs to a frame's queueing.

    The frame's g_FrameCount is the same from its drawing to its queueing.
    """

    target: int
    running: list[Harness]
    gate: LoaderGate
    # The matching build's glare strength at each frame's lens flare
    flare_strengths: dict[int, int] = field(default_factory=dict[int, int])
    # The last frame that the matching build queued
    matching_frame: int = -1
    # The frame whose lens flare the modern build waits to draw, if any
    held_flare_frame: int | None = None

    def take_halt(self, harness: Harness, address: int, *, is_matching: bool) -> None:
        """Note where HARNESS halted, and run it on unless it is done or waits."""
        addresses = harness.build.addresses
        if address == addresses[THREAD_END]:
            self.gate.take_thread_end(harness)
            self.running.append(harness)
            return
        if address == addresses[LIGHTS_ROUTINE]:
            self.gate.take_lights(harness)
            self.running.append(harness)
            return
        frame = harness.frame()
        if address == addresses["DrawLensFlare"]:
            if is_matching:
                strength = harness.read_word(addresses[GLARE_STRENGTH])
                self.flare_strengths[frame] = strength
                self.running.append(harness)
            else:
                self.held_flare_frame = frame
            return
        if is_matching:
            self.matching_frame = frame
        if frame < self.target:
            self.running.append(harness)

    def release_flare(self, modern: Harness, *, is_matching_running: bool) -> None:
        """Give the modern build the matching build's strength, once it is known.

        The matching build's strength is known once it has queued the frame,
        as an interrupt taken at DrawLensFlare can run the glare's update
        and halt there again.
        """
        frame = self.held_flare_frame
        if frame is None:
            return
        if self.matching_frame < frame and is_matching_running:
            return
        strength = self.flare_strengths.get(frame)
        if strength is not None:
            address = modern.build.addresses[GLARE_STRENGTH]
            _ = modern.command(f"write {address:x} {strength:08x}")
        self.held_flare_frame = None
        self.running.append(modern)


def run_frames(
    harnesses: tuple[Harness, Harness], target: int, gate: LoaderGate
) -> None:
    """Run the matching and modern builds until each queues TARGET's display list."""
    matching, modern = harnesses
    run = FrameRun(target, [matching, modern], gate)
    while run.running:
        for harness in run.running:
            harness.send(f"run {FRAME_TIMEOUT}")
        halts = [(harness, harness.halt()) for harness in run.running]
        run.running = []
        for harness, address in halts:
            run.take_halt(harness, address, is_matching=harness is matching)
        run.release_flare(modern, is_matching_running=matching in run.running)


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
    # The pairs of words found to address the same place in any frame
    same_pairs: set[tuple[int, int]]

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
        words = self.words(HEAP_START, HEAP_START, MAIN_REGION_SIZE)
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
        date = BUILD_DATE_FIELDS.get(key)
        if date is not None:
            words = [word for word in words if not date.holds(word.offset)]
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
        """Whether two words can address the same place in each build.

        Most pairs do so in every frame, which is learned once for each.
        """
        pair = (matching, modern)
        if pair in self.same_pairs:
            return True
        fixed = self.builds[0].fixed_places(matching)
        if not fixed.isdisjoint(self.builds[1].fixed_places(modern)):
            self.same_pairs.add(pair)
            return True
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


# A frame's objects that differ, with their differing words
FrameDifferences = list[tuple[str, list[WordDifference]]]


@final
class FrameComparer:
    """Compares frames' dumps, keeping what it learns between frames."""

    def __init__(self, builds: tuple[Build, Build], comparable: frozenset[str]) -> None:
        """Compare BUILDS' COMPARABLE objects."""
        self.builds = builds
        self.comparable = comparable
        self.same_pairs: set[tuple[int, int]] = set()

    def compare(self, dumps: tuple[Path, Path]) -> FrameDifferences:
        """Compare the builds' RDRAM in DUMPS."""
        states = (
            self.builds[0].frame_state(dumps[0].read_bytes()),
            self.builds[1].frame_state(dumps[1].read_bytes()),
        )
        comparison = Comparison(self.builds, states, self.comparable, self.same_pairs)
        return list(comparison.objects())


# The comparer that each worker process has from the main one, which forks
# them, so that the builds' symbols aren't sent to them
FORKED_COMPARER: list[FrameComparer] = []


def compare_frame(dumps: tuple[Path, Path]) -> FrameDifferences:
    """Compare a frame's DUMPS in a worker process."""
    return FORKED_COMPARER[0].compare(dumps)


@dataclass
class PendingFrame:
    """A frame that a worker process is comparing."""

    frame: int
    loading: bool
    differences: AsyncResult[FrameDifferences]


def characterize(arguments: "Arguments") -> dict[str, Finding]:
    """Compare the builds over the frames asked for; return what differed.

    Worker processes compare each frame while the harnesses run the next,
    and their findings are taken in the frames' order.
    """
    builds = (Build(Path(arguments.reference)), Build(Path(arguments.modern)))
    game = pair_symbols(builds)
    unpaired = unpaired_objects(builds, game)
    comparable = game - frozenset(unpaired)
    findings = {key: Finding() for key in unpaired}
    dump_size = max(build.rdram_extent for build in builds)

    def take(pending: PendingFrame) -> None:
        for key, words in pending.differences.get():
            if arguments.verbose and key not in findings:
                print(f"frame {pending.frame}: {key} differs")
            findings.setdefault(key, Finding()).add(
                pending.frame, words, loading=pending.loading
            )

    FORKED_COMPARER[:] = [FrameComparer(builds, comparable)]
    harnesses: list[Harness] = []
    with (
        tempfile.TemporaryDirectory(dir="/dev/shm") as scratch,
        multiprocessing.get_context("fork").Pool(WORKER_COUNT) as pool,
    ):
        try:
            harnesses = [Harness(arguments.harness, build) for build in builds]
            pair = (harnesses[0], harnesses[1])
            gate = LoaderGate()
            queued: collections.deque[PendingFrame] = collections.deque()
            for frame in range(1, arguments.frames + 1):
                run_frames(pair, frame, gate)
                if len(queued) == DUMP_SLOT_COUNT:
                    take(queued.popleft())
                slot = frame % DUMP_SLOT_COUNT
                dumps = (
                    Path(scratch) / f"matching{slot}.bin",
                    Path(scratch) / f"modern{slot}.bin",
                )
                for harness, dump in zip(pair, dumps, strict=True):
                    harness.start_dump(dump, dump_size)
                for harness in pair:
                    harness.finish_dump(dump_size)
                loading = any(harness.is_loading() for harness in pair)
                gate.release(pair)
                differences = pool.apply_async(compare_frame, (dumps,))
                queued.append(PendingFrame(frame, loading, differences))
            while queued:
                take(queued.popleft())
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
    reference: str = str(REFERENCE_DIR)
    modern: str = str(MODERN_DIR)
    verbose: bool = False


def main() -> int:
    """Parse the arguments and run the comparison."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("--frames", type=int, help="frames of logic to compare")
    _ = parser.add_argument("--harness", help="the n64-harness program")
    _ = parser.add_argument(
        "--reference", help="the directory of the build to compare with"
    )
    _ = parser.add_argument("--modern", help="the directory of the modern build")
    _ = parser.add_argument(
        "--verbose", action="store_true", help="print each finding as found"
    )
    arguments = parser.parse_args(namespace=Arguments())
    findings = characterize(arguments)
    return 0 if report(findings) else 1


if __name__ == "__main__":
    sys.exit(main())
