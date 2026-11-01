#!/usr/bin/env python3
"""Compare compiled functions with the original assembly, by instruction.

The instructions are paired by their words with the relocated fields, branch
offsets and jump targets masked out, and a pair is equal when its words are
and, as objdiff compares them, it also goes to and refers to the same places:
a branch or jump to its routine's own instructions must land on the paired
instruction, and a relocated field (a %hi or %lo half, or a call's target)
must take the value the matching build's linker gives it. A field's value is
known when the matching build's ELF names its symbol once; one whose symbol
it doesn't name, such as a compiler's anonymous constant, compares equal.

    fncompare.py [-v] ASM_FILE C_FILE [FUNCTION ...]

ASM_FILE is a splat assembly file holding one or more functions and C_FILE
the C to compile, or an object file already compiled. Prints one line per
function: its name, its score (the fraction of equal instructions) and either
OK for an exact match or the count of differing instructions, on either side,
which still moves when a change is too small to show in the score. With -v,
also prints the instructions that differ, with each branch's destination as
@INDEX.
"""

import atexit
import bisect
import difflib
import functools
import itertools
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

import rabbitizer
from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import Symbol, SymbolTableSection

ROOT = Path(__file__).resolve().parent.parent
LINKED_ELF = ROOT / "build" / "mariogolf64.elf"
KMC_DIR = ROOT / "tools" / "gcc_kmc"
CFLAGS = [
    "-G0",
    "-mips3",
    "-mgp32",
    "-mfp32",
    "-mno-abicalls",
    "-fno-PIC",
    "-ffast-math",
    "-nostdinc",
    "-funsigned-char",
    "-w",
]
# Optimization levels by file or directory, as the Makefile sets them
OPTFLAGS_BY_PATH = {Path("lib") / "libkmc": "-O1 -fno-fast-math"}
DEFAULT_OPTFLAGS = os.environ.get("OPTFLAGS", "-O2")
INCLUDES = [
    "-Iinclude",
    "-Iinclude/libc",
    "-Ilib/ultralib/include",
    "-Ilib/ultralib/include/PR",
    "-Ilib/libmus/include",
    "-Isrc",
    "-Ibuild",
]
DEFINES = [
    "-D_LANGUAGE_C",
    "-D_MIPS_SZLONG=32",
    "-D_MIPS_SZINT=32",
    "-DF3DEX_GBI_2",
    "-DNDEBUG",
    "-D_FINALROM",
    "-DBUILD_VERSION=VERSION_J",
    "-D__IS_KMC__=1",
    "-DNON_MATCHING",
]

# A splat listing's instruction: its address, its word and its text
INSN_RE = re.compile(r"^\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ([0-9A-F]{8}) \*/\s+(.*)$")
GLABEL_RE = re.compile(r"^glabel (\w+)")
ENDLABEL_RE = re.compile(r"^endlabel (\w+)")
# The label number GCC appends to a nested function's or static local's name
LABEL_NUMBER_RE = re.compile(r"\.\d+$")
# A %hi or %lo operand's symbol and addend
HALF_OPERAND_RE = re.compile(r"%(?:hi|lo)\((\w+)(?: \+ (0x[0-9A-Fa-f]+|\d+))?\)")

WORD_SIZE = 4
OPCODE_SHIFT = 26
J_OPCODE = 2
JAL_OPCODE = 3
JUMP_TARGET_MASK = 0x03FFFFFF
# The bits a jump's target keeps from the jump's own address, its 256 MB region
JUMP_REGION_MASK = 0xF0000000
IMMEDIATE_MASK = 0xFFFF
WORD_MASK = 0xFFFFFFFF
# Where KSEG0, through which the game reaches its code and data, starts and
# ends, and the ROM's size, above the physical addresses of RAM. A relocated
# field whose value can be none of these addresses, such as a KSEG1 address
# or a display list command, is compared by its value, as C may write it as a
# constant; the rest must be relocated, so that the game can be shifted.
K0BASE = 0x80000000
K1BASE = 0xA0000000
ROM_SIZE = 0x2000000
HALF_SHIFT = 16
# What %hi adds before it shifts, so that %lo's sign extension takes it away
HIGH_HALF_ROUNDING = 0x8000
R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6

# The offsets of the fields read_symbol_table and read_linked_symbols read
# in a big-endian ELF32 file's header, its section headers and its symbols
HALF_SIZE = 2
E_SHOFF = 0x20
E_SHENTSIZE = 0x2E
E_SHNUM = 0x30
SH_TYPE = 0x04
SH_OFFSET = 0x10
SH_SIZE = 0x14
SH_LINK = 0x18
SHT_SYMTAB = 2
SYMBOL_SIZE = 16
ST_NAME = 0x0
ST_VALUE = 0x4
ST_INFO = 0xC
SYMBOL_TYPE_MASK = 0xF
STT_NOTYPE = 0
STT_OBJECT = 1
STT_FUNC = 2
# The symbols a relocated field can refer to: assembly labels, data and code
REFERENCED_TYPES = frozenset((STT_NOTYPE, STT_OBJECT, STT_FUNC))
REFERENCED_TYPE_NAMES = frozenset(("STT_NOTYPE", "STT_OBJECT", "STT_FUNC"))

MAX_DIFF_HUNKS = int(os.environ.get("MAX_DIFF_HUNKS", "4"))
# How many matching instructions -v prints around each run, as diff -U does
DIFF_CONTEXT = int(os.environ.get("DIFF_CONTEXT", "0"))

StrPath = str | os.PathLike[str]


@dataclass
class Routine:
    """A function's instructions, as compared with another build of it.

    WORDS have their relocated fields, branch offsets and jump targets masked.
    DESTINATIONS hold the index of the instruction each branch, or jump within
    the routine, goes to, and REFERENCES the value the matching build's linker
    gives each relocated field; both are None elsewhere, and a reference is
    None too where the matching build doesn't name its symbol.
    """

    words: list[int]
    destinations: list[int | None]
    references: list[int | None]


Functions = dict[str, Routine]


def is_jump(word: int) -> bool:
    """Whether WORD is a j or a jal, whose target is always relocated."""
    return word >> OPCODE_SHIFT in (J_OPCODE, JAL_OPCODE)


@functools.cache
def get_branch_offset(word: int) -> int | None:
    """Return how many instructions past a branch it goes to, or None."""
    instruction = rabbitizer.Instruction(word)
    if not instruction.isBranch():
        return None
    return instruction.getBranchOffset() // WORD_SIZE


def build_routine(
    words: list[int], jumps: dict[int, int], references: list[int | None]
) -> Routine:
    """Return the routine of WORDS, whose relocated halves are masked.

    JUMPS gives {index: destination index} of the jumps within the routine.
    """
    masked: list[int] = []
    destinations: list[int | None] = []
    for index, word in enumerate(words):
        offset = get_branch_offset(word)
        if offset is not None:
            masked.append(word & ~IMMEDIATE_MASK)
            destinations.append(index + offset)
        elif is_jump(word):
            masked.append(word & ~JUMP_TARGET_MASK)
            destinations.append(jumps.get(index))
        else:
            masked.append(word)
            destinations.append(None)
    return Routine(masked, destinations, references)


def read_field(data: bytes, offset: int, size: int) -> int:
    """Return the big-endian field of SIZE bytes at OFFSET in DATA."""
    return int.from_bytes(data[offset : offset + size], "big")


def read_symbol_table(elf: bytes) -> tuple[bytes, bytes]:
    """Return a big-endian ELF32 file's symbol table and its names.

    Both are empty when the file has no symbol table.
    """
    first_section = read_field(elf, E_SHOFF, WORD_SIZE)
    section_size = read_field(elf, E_SHENTSIZE, HALF_SIZE)
    section_count = read_field(elf, E_SHNUM, HALF_SIZE)
    sections = [
        elf[start : start + section_size]
        for start in range(
            first_section, first_section + section_count * section_size, section_size
        )
    ]
    for section in sections:
        if read_field(section, SH_TYPE, WORD_SIZE) != SHT_SYMTAB:
            continue
        names = sections[read_field(section, SH_LINK, WORD_SIZE)]
        names_start = read_field(names, SH_OFFSET, WORD_SIZE)
        names_end = names_start + read_field(names, SH_SIZE, WORD_SIZE)
        start = read_field(section, SH_OFFSET, WORD_SIZE)
        end = start + read_field(section, SH_SIZE, WORD_SIZE)
        return elf[start:end], elf[names_start:names_end]
    return b"", b""


@functools.cache
def read_linked_symbols() -> dict[str, int]:
    """Return {name: address} of the symbols the matching build names once.

    The symbols are those of REFERENCED_TYPES, named without the label number
    GCC appends to a nested function or static local, as objects number them
    differently. Reads the symbol table with
    struct, since pyelftools takes half a second over the game's thousands of
    symbols.
    """
    if not LINKED_ELF.exists():
        return {}
    symbols, names = read_symbol_table(LINKED_ELF.read_bytes())
    addresses: dict[str, int] = {}
    repeated: set[str] = set()
    for start in range(0, len(symbols), SYMBOL_SIZE):
        symbol = symbols[start : start + SYMBOL_SIZE]
        if symbol[ST_INFO] & SYMBOL_TYPE_MASK not in REFERENCED_TYPES:
            continue
        name_start = read_field(symbol, ST_NAME, WORD_SIZE)
        name = names[name_start : names.index(0, name_start)].decode()
        name = LABEL_NUMBER_RE.sub("", name)
        address = read_field(symbol, ST_VALUE, WORD_SIZE)
        if addresses.get(name, address) != address:
            repeated.add(name)
        addresses[name] = address
    return {
        name: address for name, address in addresses.items() if name not in repeated
    }


def is_address(value: int) -> bool:
    """Whether VALUE can be a KSEG0 or physical address, or a ROM offset."""
    return K0BASE <= value < K1BASE or value < ROM_SIZE


def is_compared_by_value(text: str) -> bool:
    """Whether the relocated field of an instruction's TEXT keeps its value."""
    operand = HALF_OPERAND_RE.search(text)
    if operand is None:
        return False
    address = read_linked_symbols().get(operand.group(1))
    if address is None:
        return False
    addend = int(operand.group(2), 0) if operand.group(2) else 0
    return not is_address((address + addend) & WORD_MASK)


def read_listing(listing: list[tuple[int, int, str]]) -> Routine:
    """Return the routine of a splat LISTING of (address, word, text)."""
    start = listing[0][0] if listing else 0
    words: list[int] = []
    references: list[int | None] = []
    jumps: dict[int, int] = {}
    for index, (address, word, text) in enumerate(listing):
        reference = None
        relocated_mask = 0
        if ("%hi(" in text or "%lo(" in text) and not is_compared_by_value(text):
            reference = word & IMMEDIATE_MASK
            relocated_mask = IMMEDIATE_MASK
        elif word >> OPCODE_SHIFT == JAL_OPCODE:
            reference = word & JUMP_TARGET_MASK
        elif word >> OPCODE_SHIFT == J_OPCODE:
            target = address & JUMP_REGION_MASK | (word & JUMP_TARGET_MASK) << 2
            jumps[index] = (target - start) // WORD_SIZE
        words.append(word & ~relocated_mask)
        references.append(reference)
    return build_routine(words, jumps, references)


def read_target_functions(asm_path: StrPath) -> Functions:
    """Return {name: routine} of each function in a splat asm file."""
    listings: dict[str, list[tuple[int, int, str]]] = {}
    current: list[tuple[int, int, str]] | None = None
    with Path(asm_path).open() as asm_file:
        for line in asm_file:
            match = GLABEL_RE.match(line)
            if match:
                current = listings.setdefault(match.group(1), [])
                continue
            if ENDLABEL_RE.match(line):
                current = None
                continue
            match = INSN_RE.match(line)
            if match and current is not None:
                address, word, text = match.groups()
                current.append((int(address, 16), int(word, 16), text))
    return {name: read_listing(listing) for name, listing in listings.items()}


# The one work directory, made on first use; a list so that it can be set
# without a global statement
_work_dirs: list[Path] = []
_work_dir_lock = threading.Lock()


def get_work_dir() -> Path:
    """Return a directory in /tmp that mirrors the repo through symlinks.

    The KMC assembler writes its temporary files to the current directory
    and fsyncs them. Concurrent fsyncs on the repo's disk wait on each
    other, which made parallel compiles slower than serial ones, so the
    compiler runs from here, where the files stay in memory.
    """
    with _work_dir_lock:
        if not _work_dirs:
            work_dir = Path(tempfile.mkdtemp(prefix="fncompare-"))
            _ = atexit.register(shutil.rmtree, work_dir, ignore_errors=True)
            for entry in ROOT.iterdir():
                (work_dir / entry.name).symlink_to(entry)
            _work_dirs.append(work_dir)
    return _work_dirs[0]


def get_optflags(c_path: StrPath) -> list[str]:
    """Return the optimization flags the Makefile builds C_PATH with."""
    path = Path(c_path).resolve()
    if not path.is_relative_to(ROOT):
        return DEFAULT_OPTFLAGS.split()
    path = path.relative_to(ROOT)
    for key in (path, path.parent):
        if key in OPTFLAGS_BY_PATH:
            return OPTFLAGS_BY_PATH[key].split()
    return DEFAULT_OPTFLAGS.split()


# The text that a top-level function definition's body starts after
DEFINITION_END_RE = re.compile(r"\)\s*$")
# A function definition's name, the first identifier its header calls
DEFINITION_NAME_RE = re.compile(r"(\w+)\s*\(")
# The functions that KMC GCC expands itself when it sees no definition of
# them, from c-decl.c
BUILTIN_NAMES = frozenset(
    (
        "abort", "abs", "alloca", "cos", "cosf", "cosl", "_exit", "exit",
        "fabs", "fabsf", "fabsl", "ffs", "labs", "memcmp", "memcpy", "sin",
        "sinf", "sinl", "sqrt", "sqrtf", "sqrtl", "strcmp", "strcpy", "strlen",
    )
)  # fmt: skip


# The tokens of preprocessed C that bound declarations and bodies. Strings,
# characters and line markers are matched whole, so that the braces and
# semicolons in them are passed over.
STRUCTURE_RE = re.compile(
    r""""(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|^#[^\n]*|[{};]""", re.MULTILINE
)


def find_definitions(source: str) -> list[tuple[str, int, int, bool]]:
    """Return each top-level function definition in the preprocessed SOURCE.

    Each is (name, body start, body end, is inline), in order.
    """
    definitions: list[tuple[str, int, int, bool]] = []
    # Where the declaration being scanned starts
    header_start = 0
    body_start = 0
    depth = 0
    for token in STRUCTURE_RE.finditer(source):
        kind = token.group()
        if kind == "{":
            if depth == 0:
                body_start = token.start()
            depth += 1
        elif kind == "}":
            depth -= 1
            if depth == 0:
                header = source[header_start:body_start]
                name = DEFINITION_NAME_RE.search(header)
                is_function = DEFINITION_END_RE.search(header) is not None
                if name is not None and is_function:
                    is_inline = "inline" in header.split()
                    definitions.append(
                        (name.group(1), body_start, token.end(), is_inline)
                    )
                header_start = token.end()
        elif depth == 0 and kind[0] in ";#":
            header_start = token.end()
    return definitions


def isolate_functions(source: str, keep: set[str]) -> str:
    """Return the preprocessed SOURCE with the bodies it needn't compile cut.

    Each top-level function body becomes a prototype, except those that hold
    the functions in KEEP, those just before them, inline ones and those of
    builtins.

    GCC compiles each function on its own at -O2, so the kept functions'
    code stays the same, and the compile skips the rest. The assembler
    pads a function's first instruction after a hazard in the last one
    before it, so that one stays too. A builtin without its definition
    would be expanded instead of called.
    """
    definitions = find_definitions(source)
    names = "|".join(map(re.escape, keep))
    nested = re.compile(rf"\b({names})\s*\([^;{{}}]*\)\s*\{{")
    holds = [
        name in keep or nested.search(source, start + 1, end) is not None
        for name, start, end, _ in definitions
    ]
    pieces: list[str] = []
    copied = 0
    for number, (name, start, end, is_inline) in enumerate(definitions):
        is_before_kept = number + 1 < len(holds) and holds[number + 1]
        is_builtin = name in BUILTIN_NAMES
        if holds[number] or is_before_kept or is_inline or is_builtin:
            continue
        pieces.append(source[copied:start].rstrip() + ";")
        copied = end
    pieces.append(source[copied:])
    return "".join(pieces)


def run_compiler(
    c_path: StrPath, optflags: list[str], arguments: list[str]
) -> subprocess.CompletedProcess[str]:
    """Run KMC GCC on C_PATH with the project's flags and ARGUMENTS."""
    return subprocess.run(
        [
            str(KMC_DIR / "gcc"),
            *CFLAGS,
            *optflags,
            *INCLUDES,
            *DEFINES,
            *arguments,
            str(Path(c_path).resolve()),
        ],
        cwd=get_work_dir(),
        env=dict(os.environ, COMPILER_PATH=str(KMC_DIR)),
        capture_output=True,
        text=True,
        check=False,
    )


def compile_isolated(
    c_path: StrPath, keep: set[str], optflags: list[str] | None = None
) -> tuple[Functions | None, str]:
    """Compile only the functions in KEEP of C_PATH, as compile_functions does.

    Falls back on compiling all of C_PATH when a function in KEEP is defined
    under another name, as through a macro, and so was cut.
    """
    if optflags is None:
        optflags = get_optflags(c_path)
    result = run_compiler(c_path, optflags, ["-E"])
    if result.returncode != 0:
        return None, result.stderr
    with tempfile.TemporaryDirectory() as tmp:
        isolated = Path(tmp) / "isolated.i"
        _ = isolated.write_text(isolate_functions(result.stdout, keep))
        compiled, error = compile_functions(isolated, optflags)
    if compiled is not None and not keep <= compiled.keys():
        return compile_functions(c_path, optflags)
    return compiled, error


def compile_functions(
    c_path: StrPath, optflags: list[str] | None = None
) -> tuple[Functions | None, str]:
    """Compile C_PATH with OPTFLAGS, by default its own.

    Returns the functions as read_object_functions does, or None and the
    compiler's errors.
    """
    if optflags is None:
        optflags = get_optflags(c_path)
    with tempfile.TemporaryDirectory() as tmp:
        obj_path = Path(tmp) / "out.o"
        result = run_compiler(c_path, optflags, ["-c", "-o", str(obj_path)])
        if result.returncode != 0:
            return None, result.stderr
        return read_object_file(obj_path), ""


def read_object_file(obj_path: StrPath) -> Functions:
    """Return the functions in an object file, as read_object_functions does."""
    with Path(obj_path).open("rb") as obj_file:
        return read_object_functions(ELFFile(obj_file))


def read_entry_field(entry: Container, field: str) -> object:
    """Return a field of an ELF entry, which pyelftools types as Any."""
    return entry[field]  # pyright: ignore[reportAny]


def read_entry_int(entry: Container, field: str) -> int:
    """Return an integer field of an ELF entry."""
    value = read_entry_field(entry, field)
    if not isinstance(value, int):
        message = f"{field} is {value!r}, not an integer"
        raise TypeError(message)
    return value


# {first word: (symbol name, size in words)} of each function in .text.
# GCC names a nested function NAME.N, after its label number.
Spans = dict[int, tuple[str, int]]


def read_function_spans(symbols: SymbolTableSection, text_index: int) -> Spans:
    """Return where each function in the .text at TEXT_INDEX starts and ends."""
    spans: Spans = {}
    for symbol in symbols.iter_symbols():
        if read_symbol_type(symbol) != "STT_FUNC":
            continue
        if read_entry_field(symbol.entry, "st_shndx") == text_index:
            start = read_entry_int(symbol.entry, "st_value") // WORD_SIZE
            size = read_entry_int(symbol.entry, "st_size") // WORD_SIZE
            spans[start] = (symbol.name, size)
    return spans


def read_symbol_type(symbol: Symbol) -> object:
    """Return a symbol's type, such as "STT_FUNC"."""
    info = read_entry_field(symbol.entry, "st_info")
    return read_entry_field(info, "type") if isinstance(info, Container) else None


# {section index: ([start of each symbol], [(start, size, name)])}, in order
Holders = dict[int, tuple[list[int], list[tuple[int, int, str]]]]


def read_holders(symbols: SymbolTableSection) -> Holders:
    """Return the named symbols an object defines, by section."""
    found: dict[int, list[tuple[int, int, str]]] = {}
    for symbol in symbols.iter_symbols():
        section_index = read_entry_field(symbol.entry, "st_shndx")
        if not symbol.name or not isinstance(section_index, int):
            continue
        if read_symbol_type(symbol) in REFERENCED_TYPE_NAMES:
            start = read_entry_int(symbol.entry, "st_value")
            size = read_entry_int(symbol.entry, "st_size")
            found.setdefault(section_index, []).append((start, size, symbol.name))
    holders: Holders = {}
    for section_index, held in found.items():
        held.sort()
        holders[section_index] = ([start for start, _, _ in held], held)
    return holders


def find_holder(
    held_symbols: tuple[list[int], list[tuple[int, int, str]]], addend: int
) -> tuple[str, int] | None:
    """Return the name of the symbol holding ADDEND and the offset into it."""
    starts, held = held_symbols
    position = bisect.bisect_right(starts, addend) - 1
    if position < 0:
        return None
    start, size, name = held[position]
    offset = addend - start
    return (name, offset) if offset < max(size, 1) else None


def get_linked_address(symbol: Symbol, addend: int, holders: Holders) -> int | None:
    """Return the matching build's address of SYMBOL plus ADDEND, if it has one.

    A relocation against a section, as one to a static variable is, takes the
    address from the object's symbol that holds it. One whose addend has had
    K0BASE taken off, as PhysicalAddress takes it, gives the physical address.
    """
    name, offset = symbol.name, addend
    if read_symbol_type(symbol) == "STT_SECTION":
        section_index = read_entry_field(symbol.entry, "st_shndx")
        if not isinstance(section_index, int) or section_index not in holders:
            return None
        shift = 0
        found = find_holder(holders[section_index], addend)
        if found is None:
            shift = -K0BASE
            found = find_holder(holders[section_index], (addend - shift) & WORD_MASK)
        if found is None:
            return None
        name, offset = found[0], found[1] + shift
    address = read_linked_symbols().get(LABEL_NUMBER_RE.sub("", name))
    return None if address is None else (address + offset) & WORD_MASK


def sign_extend_half(half: int) -> int:
    """Return the 16-bit HALF as a signed number."""
    return half - ((half & HIGH_HALF_ROUNDING) << 1)


@dataclass
class Relocated:
    """What relocate finds in an object's .text."""

    # The value each relocated field takes in the matching build, or None
    references: list[int | None]
    # {word: destination word} of the jumps within .text
    jumps: dict[int, int]
    # {word: the full address its half makes} of the resolved %hi and %lo halves
    addresses: dict[int, int] = field(default_factory=dict)


def relocate(words: list[int], elf: ELFFile, symbols: SymbolTableSection) -> Relocated:
    """Mask the relocated halves of WORDS and find what their fields refer to.

    A %lo half is paired with the %hi halves before it against the same
    symbol, as the linker pairs them, to give the address they make.
    """
    relocated = Relocated([None] * len(words), {})
    relocations = elf.get_section_by_name(".rel.text")
    if not isinstance(relocations, RelocationSection):
        return relocated
    holders = read_holders(symbols)
    text_index = elf.get_section_index(".text")
    references = relocated.references
    # {symbol index: the %hi halves that wait for their %lo}
    waiting: dict[int, list[int]] = {}
    # {symbol index: the latest %hi half's field}
    high_fields: dict[int, int] = {}
    for reloc in relocations.iter_relocations():
        kind = read_entry_int(reloc.entry, "r_info_type")
        index = read_entry_int(reloc.entry, "r_offset") // WORD_SIZE
        symbol_index = read_entry_int(reloc.entry, "r_info_sym")
        symbol = symbols.get_symbol(symbol_index)
        word = words[index]
        if kind == R_MIPS_HI16:
            waiting.setdefault(symbol_index, []).append(index)
            high_fields[symbol_index] = word & IMMEDIATE_MASK
            words[index] = word & ~IMMEDIATE_MASK
        elif kind == R_MIPS_LO16:
            addend = (high_fields.get(symbol_index, 0) << HALF_SHIFT) + (
                sign_extend_half(word & IMMEDIATE_MASK)
            )
            address = get_linked_address(symbol, addend, holders)
            if address is not None:
                references[index] = address & IMMEDIATE_MASK
                relocated.addresses[index] = address
                high = (address + HIGH_HALF_ROUNDING) >> HALF_SHIFT & IMMEDIATE_MASK
                for high_index in waiting.get(symbol_index, []):
                    references[high_index] = high
                    relocated.addresses[high_index] = address
            _ = waiting.pop(symbol_index, None)
            words[index] = word & ~IMMEDIATE_MASK
        elif kind == R_MIPS_26:
            addend = (word & JUMP_TARGET_MASK) * WORD_SIZE
            is_local = read_entry_field(symbol.entry, "st_shndx") == text_index
            if word >> OPCODE_SHIFT == J_OPCODE and is_local:
                target = read_entry_int(symbol.entry, "st_value") + addend
                relocated.jumps[index] = target // WORD_SIZE
                continue
            address = get_linked_address(symbol, addend, holders)
            if address is not None:
                references[index] = address // WORD_SIZE & JUMP_TARGET_MASK
    return relocated


def keep_constant_values(words: list[int], relocated: Relocated) -> None:
    """Give the halves of values that can't be addresses their values in WORDS.

    Their references become None, as is_compared_by_value makes a listing's.
    """
    for index, address in relocated.addresses.items():
        reference = relocated.references[index]
        if reference is not None and not is_address(address):
            words[index] |= reference
            relocated.references[index] = None


def read_object_functions(elf: ELFFile) -> Functions:
    """Return {name: routine} of each function in an object's .text."""
    text_index = elf.get_section_index(".text")
    symbols = elf.get_section_by_name(".symtab")
    if text_index is None or not isinstance(symbols, SymbolTableSection):
        return {}
    text = elf.get_section(text_index).data()
    words = [
        int.from_bytes(text[i : i + WORD_SIZE], "big")
        for i in range(0, len(text), WORD_SIZE)
    ]
    relocated = relocate(words, elf, symbols)
    keep_constant_values(words, relocated)
    functions: Functions = {}
    for start, (name, size) in read_function_spans(symbols, text_index).items():
        end = start + size
        jumps = {
            index - start: target - start
            for index, target in relocated.jumps.items()
            if start <= index < end
        }
        functions[name.split(".")[0]] = build_routine(
            words[start:end], jumps, relocated.references[start:end]
        )
    return functions


# A row of two routines' instructions, as pair_rows pairs them: the index of
# each side's instruction, or None where the other side's has no pair
Row = tuple[int | None, int | None]


def pair_rows(opcodes: Sequence[tuple[str, int, int, int, int]]) -> list[Row]:
    """Return the rows of a matcher's OPCODES, pairing changed runs in order."""
    rows: list[Row] = []
    for _, start1, end1, start2, end2 in opcodes:
        rows += itertools.zip_longest(range(start1, end1), range(start2, end2))
    return rows


@dataclass
class Pairing:
    """How ROWS pair the instructions of two routines."""

    target: Routine
    compiled: Routine
    rows: list[Row]
    # {instruction index: row} of each side
    target_rows: dict[int | None, int] = field(init=False)
    compiled_rows: dict[int | None, int] = field(init=False)

    def __post_init__(self) -> None:
        """Index the rows by each side's instructions."""
        self.target_rows = {pair[0]: row for row, pair in enumerate(self.rows)}
        self.compiled_rows = {pair[1]: row for row, pair in enumerate(self.rows)}

    def get_row(self, rows: dict[int | None, int], index: int, length: int) -> int:
        """Return the row of instruction INDEX of a routine of LENGTH.

        An index past the routine takes the rows past the last.
        """
        return rows.get(index, len(self.rows) + index - length)

    def has_same_places(self, target_index: int, compiled_index: int) -> bool:
        """Whether the paired instructions go to and refer to the same places."""
        target_reference = self.target.references[target_index]
        compiled_reference = self.compiled.references[compiled_index]
        if None not in (target_reference, compiled_reference) and (
            target_reference != compiled_reference
        ):
            return False
        target_destination = self.target.destinations[target_index]
        compiled_destination = self.compiled.destinations[compiled_index]
        if target_destination is None or compiled_destination is None:
            return True
        return self.get_row(
            self.target_rows, target_destination, len(self.target.words)
        ) == self.get_row(
            self.compiled_rows, compiled_destination, len(self.compiled.words)
        )

    def is_equal(self, row: Row) -> bool:
        """Whether ROW pairs two equal instructions."""
        target_index, compiled_index = row
        if target_index is None or compiled_index is None:
            return False
        if self.target.words[target_index] != self.compiled.words[compiled_index]:
            return False
        return self.has_same_places(target_index, compiled_index)


def pair_words(target: Routine, compiled: Routine) -> Pairing:
    """Pair two routines' instructions by their masked words."""
    matcher = difflib.SequenceMatcher(
        None, target.words, compiled.words, autojunk=False
    )
    return Pairing(target, compiled, pair_rows(matcher.get_opcodes()))


def count_differences(target: Routine, compiled: Routine) -> int:
    """Return how many instructions, on either side, have no equal pair."""
    pairing = pair_words(target, compiled)
    shared = sum(map(pairing.is_equal, pairing.rows))
    return len(target.words) + len(compiled.words) - 2 * shared


def describe(target: Routine, compiled: Routine) -> str:
    """Return the score and, short of a match, the count of differing words."""
    differences = count_differences(target, compiled)
    total = len(target.words) + len(compiled.words)
    score = 1 - differences / total if total else 1.0
    if not differences:
        return f"{score:.3f} OK"
    return f"{score:.3f} ({differences} differ)"


def disassemble(routine: Routine, index: int) -> str:
    """Return the text of one of ROUTINE's instructions.

    A relocated field shows its value in the matching build, and a branch
    or jump within the routine its destination as @INDEX.
    """
    word = routine.words[index]
    reference = routine.references[index]
    text = rabbitizer.Instruction(word | (reference or 0)).disassemble()
    destination = routine.destinations[index]
    if destination is None:
        return text
    operands = text.rsplit(maxsplit=1)[0] if is_jump(word) else text.split(". +")[0]
    return f"{operands.rstrip(', ')} @{destination}"


def print_diff(target: Routine, compiled: Routine) -> None:
    """Print the first few runs of instructions that differ."""
    pairing = pair_words(target, compiled)
    runs = [
        list(group)
        for is_equal, group in itertools.groupby(pairing.rows, pairing.is_equal)
        if not is_equal
    ]
    for run in runs[:MAX_DIFF_HUNKS]:
        target_indices = [index for index, _ in run if index is not None]
        compiled_indices = [index for _, index in run if index is not None]
        first = target_indices[0] if target_indices else None
        last = target_indices[-1] + 1 if target_indices else None
        if first is not None:
            for index in range(max(0, first - DIFF_CONTEXT), first):
                print(f"   {index:4} {disassemble(target, index)}")
        for index in target_indices:
            print(f"  -{index:4} {disassemble(target, index)}")
        for index in compiled_indices:
            print(f"  +{index:4} {disassemble(compiled, index)}")
        if last is not None:
            for index in range(last, min(len(target.words), last + DIFF_CONTEXT)):
                print(f"   {index:4} {disassemble(target, index)}")
        print("  --")
    if len(runs) > MAX_DIFF_HUNKS:
        print(f"  ({len(runs) - MAX_DIFF_HUNKS} more)")


def main() -> None:
    """Score the functions named on the command line, or all of them."""
    verbose = "-v" in sys.argv
    args = [arg for arg in sys.argv[1:] if arg != "-v"]
    asm_path, c_path, *names = args
    targets = read_target_functions(asm_path)
    if c_path.endswith(".o"):
        compiled, error = read_object_file(c_path), ""
    elif names:
        compiled, error = compile_isolated(c_path, set(names))
    else:
        compiled, error = compile_functions(c_path)
    if compiled is None:
        print(error, file=sys.stderr)
        sys.exit(2)
    for name in names or targets:
        if name not in compiled:
            print(f"{name} missing")
            continue
        print(f"{name} {describe(targets[name], compiled[name])}")
        if verbose and count_differences(targets[name], compiled[name]):
            print_diff(targets[name], compiled[name])


if __name__ == "__main__":
    main()
