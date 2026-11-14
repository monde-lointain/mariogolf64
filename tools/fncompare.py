#!/usr/bin/env python3
"""Compare compiled functions with the original assembly, by instruction.

The instructions are paired by their words with the relocated fields, branch
offsets and jump targets masked out, and a pair is equal when its words are
and, as objdiff compares them, it also goes to and refers to the same places:
a branch or jump to its routine's own instructions must land on the paired
instruction, and a relocated field (a %hi or %lo half, or a call's target)
must take the value the matching build's linker gives it. A field's value is
known when the matching build's ELF names its symbol once; one whose symbol
it doesn't name, such as a compiler's anonymous constant, compares equal. A
field whose value can't be an address is compared as the constant it is.

    fncompare.py [-v | -s | -l] ASM_FILE C_FILE [FUNCTION ...]
    fncompare.py [-v | -s | -l] FUNCTION ...

ASM_FILE is a splat assembly file holding one or more functions and C_FILE
the C to compile, or an object file already compiled. Given only function
names, finds each one's assembly under asm/nonmatchings and the C file of
the same path under src, and compiles each C file once for all its names.
Prints one line per
function: its name, its score (the fraction of equal instructions) and either
OK for an exact match or the count of differing instructions, on either side,
which still moves when a change is too small to show in the score, and how
many of them differ in shape, with registers and constants ignored, then,
where the frames' sizes differ, by how many bytes the original's is larger
than ours. With -v,
also prints the instructions that differ, with each branch's destination as
@INDEX, and the calls one side makes more often than the other, which always
mean the sources differ. With -s, prints instead only the instructions
that differ in shape, which a change of registers can't explain. With -l,
prints instead every instruction of ours, as -v shows them, for reading the
compiled draft beside the original assembly.
"""

import atexit
import bisect
import collections
import ctypes
import enum
import functools
import itertools
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
from collections.abc import Callable, Container, Hashable, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast, final

import rabbitizer

ROOT = Path(__file__).resolve().parent.parent
LINKED_ELF = ROOT / "build" / "mariogolf64.elf"
# The compiler's directory; KMC_DIR overrides it to try another build of cc1
KMC_DIR = Path(os.environ.get("KMC_DIR", ROOT / "tools" / "gcc_kmc"))
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
    "-Ilib/libkmc/include",
    "-Ilib/ultralib/include",
    "-Ilib/libmus/include",
    "-Ibuild/lib/libnusys/include",
    "-Ilib/libnusys/include",
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
SYMBOL_BIND_SHIFT = 4
STT_NOTYPE = 0
STT_OBJECT = 1
STT_FUNC = 2
STT_SECTION = 3
# The section indices that name no section: undefined, absolute and common
SHN_UNDEF = 0
SHN_LORESERVE = 0xFF00
# What read_object reads with struct: the header's index of the section
# names, and each section header, symbol, relocation and word, whose
# relocation info holds the symbol's index above the type
E_SHSTRNDX = 0x32
SECTION_HEADER = struct.Struct(">10I")
SH_NAME_FIELD = 0
SH_TYPE_FIELD = 1
SH_OFFSET_FIELD = 4
SH_SIZE_FIELD = 5
SH_LINK_FIELD = 6
SYMBOL = struct.Struct(">3I2BH")
RELOCATION = struct.Struct(">2I")
RELOCATION_TYPE_MASK = 0xFF
RELOCATION_SYMBOL_SHIFT = 8
WORD = struct.Struct(">I")
# The symbols a relocated field can refer to: assembly labels, data and code
REFERENCED_TYPES = frozenset((STT_NOTYPE, STT_OBJECT, STT_FUNC))

MAX_DIFF_HUNKS = int(os.environ.get("MAX_DIFF_HUNKS", "4"))
# How many matching instructions -v prints around each run, as diff -U does
DIFF_CONTEXT = int(os.environ.get("DIFF_CONTEXT", "0"))
# The width an annotated instruction's text is padded to
ANNOTATION_COLUMN = 32

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


@dataclass
class Original:
    """A function's routine in the original, and those of its nested functions.

    NESTED maps each nested function's name to its routine.
    """

    routine: Routine
    nested: Functions


def read_nested_target(asm_dir: Path, name: str) -> Routine | None:
    """Return the original routine of the nested function NAME, if found.

    It is in its own file in ASM_DIR, named after it or, where splat had no
    name for it, after its address in the matching build.
    """
    labels = [name]
    address = read_linked_symbols().get(name)
    if address is not None:
        labels.append(f"func_{address:08X}")
    for label in labels:
        path = asm_dir / f"{label}.s"
        if path.exists():
            routine = read_target_functions(path).get(label)
            if routine is not None:
                return routine
    return None


def read_original(
    asm_path: StrPath, c_path: StrPath, function: str, optflags: list[str] | None = None
) -> Original:
    """Return FUNCTION's original routine in ASM_PATH, with its nested ones.

    The nested functions are those C_PATH defines in FUNCTION. One with no
    original routine, as an inline one never emitted alone, is left out.
    """
    routine = read_target_functions(asm_path)[function]
    if optflags is None:
        optflags = get_optflags(c_path)
    result = preprocess(c_path, optflags)
    nested: Functions = {}
    if result.returncode != 0:
        return Original(routine, nested)
    for name in find_nested_functions(result.stdout, function):
        found = read_nested_target(Path(asm_path).parent, name)
        if found is not None:
            nested[name] = found
    return Original(routine, nested)


# The one work directory, made on first use; a list so that it can be set
# without a global statement
_work_dirs: list[Path] = []
_work_dir_lock = threading.Lock()
WORK_DIR_PARENT = Path(tempfile.gettempdir())
WORK_DIR_PREFIX = "fncompare-"


def is_process_alive(pid: int) -> bool:
    """Return whether a process with this ID exists."""
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def remove_orphaned_work_dirs() -> None:
    """Remove the work directories of processes that have ended."""
    for work_dir in WORK_DIR_PARENT.glob(f"{WORK_DIR_PREFIX}*"):
        pid = work_dir.name.removeprefix(WORK_DIR_PREFIX)
        if not pid.isdigit() or not is_process_alive(int(pid)):
            shutil.rmtree(work_dir, ignore_errors=True)


def get_work_dir() -> Path:
    """Return a directory in /tmp that mirrors the repo through symlinks.

    The KMC assembler writes its temporary files to the current directory
    and fsyncs them. Concurrent fsyncs on the repo's disk wait on each
    other, which made parallel compiles slower than serial ones, so the
    compiler runs from here, where the files stay in memory.

    The directory is named after the process that made it. Pool workers
    exit without running atexit handlers, and sweeps get killed, so each
    new directory's maker also removes those whose process has ended.
    """
    with _work_dir_lock:
        if not _work_dirs:
            remove_orphaned_work_dirs()
            work_dir = WORK_DIR_PARENT / f"{WORK_DIR_PREFIX}{os.getpid()}"
            shutil.rmtree(work_dir, ignore_errors=True)
            work_dir.mkdir()
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


# A function defined inside another, indented as the formatter leaves it: its
# name follows a return type and precedes a parameter list holding no ";" or
# braces
NESTED_DEFINITION_RE = re.compile(
    r"^[ \t]+[A-Za-z_][\w \t*]*?[ \t*](\w+)\s*\([^;{}]*\)\s*\{", re.MULTILINE
)
# The statements NESTED_DEFINITION_RE also takes for definitions
NESTED_DEFINITION_KEYWORDS = frozenset(
    {"if", "for", "while", "switch", "return", "sizeof"}
)
# A C identifier, or a keyword
IDENTIFIER_RE = re.compile(r"\b[A-Za-z_]\w*")
# The functions that KMC GCC expands itself when it sees no definition of
# them, from c-decl.c
BUILTIN_NAMES = frozenset(
    (
        "abort", "abs", "alloca", "cos", "cosf", "cosl", "_exit", "exit",
        "fabs", "fabsf", "fabsl", "ffs", "labs", "memcmp", "memcpy", "sin",
        "sinf", "sinl", "sqrt", "sqrtf", "sqrtl", "strcmp", "strcpy", "strlen",
    )
)  # fmt: skip


# The tokens of preprocessed C matched whole, so that the brackets and words
# in them are passed over: strings, characters and line markers
WHOLE_TOKENS = r""""(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|^#[^\n]*"""
# The tokens a declaration's words are read from, and each with any
# whitespace before it
WORD_TOKENS = rf"{WHOLE_TOKENS}|[A-Za-z_]\w*|\S"
WORD_TOKEN_RE = re.compile(WORD_TOKENS, re.MULTILINE)
TOKEN_RE = re.compile(rf"\s*({WORD_TOKENS})", re.MULTILINE)
# Each opening bracket's closing one, and the tokens that matching it reads:
# its own brackets and the whole tokens, which hold brackets of no account
BRACKET_PAIRS = {"(": ")", "[": "]", "{": "}"}
BRACKET_TOKEN_RES = {
    opening: re.compile(
        rf"{WHOLE_TOKENS}|[{re.escape(opening + closing)}]", re.MULTILINE
    )
    for opening, closing in BRACKET_PAIRS.items()
}
# The words of declaration specifiers that make the type, those that say
# where the declared thing lives, and those that only qualify it
TYPE_KEYWORDS = frozenset(
    (
        "char", "double", "float", "int", "long", "short", "signed",
        "unsigned", "void",
    )
)  # fmt: skip
STORAGE_KEYWORDS = frozenset(
    ("auto", "extern", "inline", "__inline__", "register", "static", "typedef")
)
QUALIFIER_KEYWORDS = frozenset(
    ("const", "volatile", "__extension__", "__restrict", "__restrict__")
)
# The words that take a parenthesized argument wherever specifiers or
# declarators may
PARENTHESIZED_KEYWORDS = frozenset(("__attribute__", "__asm__", "asm"))
TAG_KEYWORDS = frozenset(("struct", "union", "enum"))
# The words that name no declaration of the file's own
C_KEYWORDS = (
    TYPE_KEYWORDS
    | STORAGE_KEYWORDS
    | QUALIFIER_KEYWORDS
    | PARENTHESIZED_KEYWORDS
    | TAG_KEYWORDS
    | frozenset(
        (
            "break", "case", "continue", "default", "do", "else", "for",
            "goto", "if", "return", "sizeof", "switch", "__typeof__", "while",
        )
    )
)  # fmt: skip


class Derivation(enum.Enum):
    """What a declarator makes of the type its specifiers give."""

    POINTER = enum.auto()
    FUNCTION = enum.auto()
    ARRAY = enum.auto()


# How tightly the declarator's operators bind: a pointer's operand stops
# before a function's parameters or an array's size, which bind first
POINTER_PRECEDENCE = 1
POSTFIX_PRECEDENCE = 2


@dataclass
class Declarator:
    """The name a declarator declares, and what the type nearest it makes it.

    A declarator with no name, as in a cast, declares "". In "*f(void)" f
    is a function, and in "(*f)(void)" a pointer.
    """

    name: str
    derivation: Derivation | None = None

    def derive(self, derivation: Derivation) -> "Declarator":
        """Return this declarator with DERIVATION applied outside it."""
        if self.derivation is None:
            self.derivation = derivation
        return self


@dataclass
class Definition:
    """A function's definition: its name, its body's bounds and if it's inline."""

    name: str
    body_start: int
    body_end: int
    is_inline: bool


@dataclass
class Declaration:
    """One top-level declaration, definition or line marker of preprocessed C.

    It spans [START, END) of the source, from the end of the one before.
    """

    start: int
    end: int
    # The names it declares
    names: set[str]
    # Whether it defines an object, which takes room in the file's data
    is_object: bool = False
    definition: Definition | None = None


@final
class DeclarationParser:
    """Parses preprocessed C's top-level declarations.

    Declarations are parsed by recursive descent, and their declarators by
    Pratt's method, as Bob Nystrom's Bantam parses expressions: a pointer
    is a prefix operator, a function's parameters and an array's size are
    postfix ones, and parentheses group. Bodies, parameters, initializers
    and expressions declare nothing at the top level, so they are skipped
    by matching their brackets, which is most of the source.
    """

    def __init__(self, source: str) -> None:
        """Parse SOURCE, reading its first token."""
        self.source = source
        # The end of the last token consumed, and the next token's text,
        # or "" at the end, and its bounds
        self.position = 0
        self.upcoming = ""
        self.token_start = 0
        self.token_end = 0
        self.read_token()

    def read_token(self) -> None:
        """Read the token after the last one consumed."""
        match = TOKEN_RE.match(self.source, self.position)
        if match is None:
            self.upcoming = ""
            self.token_start = self.token_end = len(self.source)
            return
        self.upcoming = match.group(1)
        self.token_start, self.token_end = match.span(1)

    def consume(self) -> str:
        """Consume the next token and return it."""
        consumed = self.upcoming
        self.position = self.token_end
        self.read_token()
        return consumed

    def match(self, expected: str) -> bool:
        """Consume the next token if it's EXPECTED, and tell whether it was."""
        if self.upcoming != expected:
            return False
        _ = self.consume()
        return True

    def is_name(self) -> bool:
        """Whether the next token is a name other than C's keywords."""
        first = self.upcoming[:1]
        return (first.isalpha() or first == "_") and self.upcoming not in C_KEYWORDS

    def skip_group(self) -> None:
        """Consume the bracketed group the next token opens.

        A group left open runs to the end of the source.
        """
        opening = self.upcoming
        closing = BRACKET_PAIRS[opening]
        depth = 0
        for match in BRACKET_TOKEN_RES[opening].finditer(self.source, self.token_start):
            bracket = match.group()
            if bracket == opening:
                depth += 1
            elif bracket == closing:
                depth -= 1
                if depth == 0:
                    self.position = match.end()
                    self.read_token()
                    return
        self.position = len(self.source)
        self.read_token()

    def skip_parenthesized(self) -> None:
        """Consume an __attribute__ or __asm__ word and its argument, if any."""
        _ = self.consume()
        if self.upcoming == "(":
            self.skip_group()

    def skip_to(self, ends: frozenset[str]) -> None:
        """Consume tokens, groups whole, up to one of ENDS or the end."""
        while self.upcoming and self.upcoming not in ends:
            if self.upcoming in BRACKET_PAIRS:
                self.skip_group()
            else:
                _ = self.consume()

    def parse_unit(self) -> list[Declaration]:
        """Return the source's top-level declarations, in order.

        Whatever follows the last, such as whitespace, is one more that
        declares nothing.
        """
        declarations: list[Declaration] = []
        while self.upcoming:
            start = self.position
            if self.upcoming[0] == "#":
                _ = self.consume()
                declarations.append(Declaration(start, self.position, set()))
            else:
                declarations.append(self.parse_declaration(start))
        if self.position < len(self.source):
            declarations.append(Declaration(self.position, len(self.source), set()))
        return declarations

    def parse_declaration(self, start: int) -> Declaration:
        """Parse a declaration or a function's definition, from START.

        One that this can't make out runs to its semicolon.
        """
        names: set[str] = set()
        storage = self.parse_specifiers(names)
        is_object = False
        while self.upcoming not in DECLARATION_ENDS:
            declarator = self.parse_declarator(0)
            if declarator.name:
                names.add(declarator.name)
            while self.upcoming in PARENTHESIZED_KEYWORDS:
                self.skip_parenthesized()
            is_function = declarator.derivation is Derivation.FUNCTION
            if is_function and self.upcoming == "{":
                body_start = self.token_start
                self.skip_group()
                is_inline = not storage.isdisjoint(("inline", "__inline__"))
                definition = Definition(
                    declarator.name, body_start, self.position, is_inline
                )
                return Declaration(start, self.position, names, definition=definition)
            is_object = is_object or not (
                is_function or {"typedef", "extern"} & storage
            )
            if self.match("="):
                self.skip_to(DECLARATOR_ENDS)
            if not self.match(","):
                break
        self.skip_to(DECLARATION_ENDS)
        _ = self.match(";")
        return Declaration(start, self.position, names, is_object=is_object)

    def parse_specifiers(self, names: set[str]) -> set[str]:
        """Parse a declaration's specifiers, adding the tags they define to NAMES.

        Returns the storage words among them. A name is the type's when no
        other word has given the type.
        """
        storage: set[str] = set()
        has_type = False
        while True:
            upcoming = self.upcoming
            if upcoming in STORAGE_KEYWORDS:
                storage.add(self.consume())
            elif upcoming in TYPE_KEYWORDS:
                _ = self.consume()
                has_type = True
            elif upcoming in QUALIFIER_KEYWORDS:
                _ = self.consume()
            elif upcoming in PARENTHESIZED_KEYWORDS:
                self.skip_parenthesized()
            elif upcoming in TAG_KEYWORDS:
                self.parse_tagged_type(names)
                has_type = True
            elif upcoming == "__typeof__":
                self.skip_parenthesized()
                has_type = True
            elif not has_type and self.is_name():
                _ = self.consume()
                has_type = True
            else:
                return storage

    def parse_tagged_type(self, names: set[str]) -> None:
        """Parse a structure, union or enumeration type, adding to NAMES.

        Its tag counts as declared where its body is or where it's declared
        alone, and so does each of an enumeration's constants.
        """
        is_enumeration = self.consume() == "enum"
        while self.upcoming in PARENTHESIZED_KEYWORDS:
            self.skip_parenthesized()
        tag = self.consume() if self.is_name() else ""
        while self.upcoming in PARENTHESIZED_KEYWORDS:
            self.skip_parenthesized()
        if tag and self.upcoming in TAG_DECLARING_FOLLOWERS:
            names.add(tag)
        if self.upcoming != "{":
            return
        if not is_enumeration:
            self.skip_group()
            return
        _ = self.consume()
        while self.upcoming and not self.match("}"):
            if self.is_name():
                names.add(self.consume())
            if self.match("="):
                self.skip_to(ENUMERATOR_ENDS)
            if not self.match(","):
                self.skip_to(ENUMERATOR_ENDS)
                _ = self.match(",")

    def parse_declarator(self, precedence: int) -> Declarator:
        """Parse a declarator, taking only operators binding tighter than PRECEDENCE."""
        prefix = DECLARATOR_PREFIX_PARSELETS.get(self.upcoming)
        if prefix is not None:
            declarator = prefix(self)
        elif self.is_name():
            declarator = Declarator(self.consume())
        else:
            declarator = Declarator("")
        while precedence < DECLARATOR_PRECEDENCES.get(self.upcoming, 0):
            derivation = DECLARATOR_POSTFIX_DERIVATIONS[self.upcoming]
            self.skip_group()
            declarator = declarator.derive(derivation)
        return declarator

    def parse_pointer(self) -> Declarator:
        """Parse a pointer's "*", its qualifiers and what it points from."""
        _ = self.consume()
        while True:
            if self.upcoming in PARENTHESIZED_KEYWORDS:
                self.skip_parenthesized()
            elif self.upcoming in QUALIFIER_KEYWORDS:
                _ = self.consume()
            else:
                break
        return self.parse_declarator(POINTER_PRECEDENCE).derive(Derivation.POINTER)

    def parse_grouping(self) -> Declarator:
        """Parse a parenthesized declarator."""
        _ = self.consume()
        declarator = self.parse_declarator(0)
        self.skip_to(GROUPING_ENDS)
        _ = self.match(")")
        return declarator


# The prefix parselets of declarators, and the postfix ones' precedences and
# what they derive
DECLARATOR_PREFIX_PARSELETS: dict[str, Callable[[DeclarationParser], Declarator]] = {
    "*": DeclarationParser.parse_pointer,
    "(": DeclarationParser.parse_grouping,
}
DECLARATOR_PRECEDENCES = {"(": POSTFIX_PRECEDENCE, "[": POSTFIX_PRECEDENCE}
DECLARATOR_POSTFIX_DERIVATIONS = {"(": Derivation.FUNCTION, "[": Derivation.ARRAY}
# The tokens that end a declaration, a declarator with its initializer, an
# enumeration's constant and a parenthesized declarator, and those that
# follow a tag that is declared
DECLARATION_ENDS = frozenset((";", ""))
DECLARATOR_ENDS = frozenset((",", ";", ""))
ENUMERATOR_ENDS = frozenset((",", "}", ""))
GROUPING_ENDS = frozenset((")", ";", ""))
TAG_DECLARING_FOLLOWERS = frozenset(("{", ";"))


@functools.lru_cache(maxsize=4)
def parse_declarations(source: str) -> list[Declaration]:
    """Return the top-level declarations of the preprocessed SOURCE.

    Each tool parses the same source more than once, so the last few
    parses are kept.
    """
    return DeclarationParser(source).parse_unit()


def find_definitions(source: str) -> list[tuple[str, int, int, bool]]:
    """Return each top-level function definition in the preprocessed SOURCE.

    Each is (name, body start, body end, is inline), in order.
    """
    return [
        (
            definition.name,
            definition.body_start,
            definition.body_end,
            definition.is_inline,
        )
        for declaration in parse_declarations(source)
        if (definition := declaration.definition) is not None
    ]


def find_nested_functions(source: str, function: str) -> list[str]:
    """Return the functions nested in FUNCTION in the preprocessed SOURCE."""
    for name, start, end, _ in find_definitions(source):
        if name == function:
            return [
                match.group(1)
                for match in NESTED_DEFINITION_RE.finditer(source, start + 1, end)
                if match.group(1) not in NESTED_DEFINITION_KEYWORDS
            ]
    return []


# The text of a line of preprocessed C that isn't a line marker
NOT_LINE_MARKER_RE = re.compile(r"^(?!#)[^\n]*", re.MULTILINE)


def isolate_functions(source: str, keep: set[str], *, prune: bool = True) -> str:
    """Return the preprocessed SOURCE with the bodies it needn't compile cut.

    Each top-level function body becomes a prototype, except those that hold
    the functions in KEEP, those just before them, inline ones and those of
    builtins.

    GCC compiles each function on its own at -O2, so the kept functions'
    code stays the same, and the compile skips the rest. The assembler
    pads a function's first instruction after a hazard in the last one
    before it, so that one stays too. A builtin without its definition
    would be expanded instead of called. A cut body leaves its newlines
    and its line markers, which the preprocessor writes in place of runs
    of blank lines, so the lines after it keep their numbers. With PRUNE,
    the declarations the kept functions can't use go too.
    """
    definitions = find_definitions(source)
    names = "|".join(map(re.escape, keep))
    nested = re.compile(rf"\b({names})\s*\([^;{{}}]*\)\s*\{{")
    holds = [
        name in keep or nested.search(source, start + 1, end) is not None
        for name, start, end, _ in definitions
    ]
    pieces: list[str] = []
    # The definitions compiled in full, which the pruning starts from
    roots: list[str] = []
    copied = 0
    for number, (name, start, end, is_inline) in enumerate(definitions):
        is_before_kept = number + 1 < len(holds) and holds[number + 1]
        is_builtin = name in BUILTIN_NAMES
        if holds[number] or is_before_kept:
            roots.append(name)
        if holds[number] or is_before_kept or is_inline or is_builtin:
            continue
        header = source[copied:start].rstrip()
        cut = source[copied + len(header) : end]
        pieces.append(header + ";" + NOT_LINE_MARKER_RE.sub("", cut))
        copied = end
    pieces.append(source[copied:])
    isolated = "".join(pieces)
    return prune_declarations(isolated, set(roots)) if prune else isolated


def prune_declarations(source: str, roots: set[str]) -> str:
    """Return SOURCE without the declarations that the definitions of ROOTS can't use.

    Each declaration that a kept one names is kept in turn, and so is each
    that declares nothing this can tell, such as a line marker, and each
    object's definition, since dropping one would move the others in the
    file's data, and with them the addresses the functions use. Parsing the
    rest took about half of each compile. A pruned declaration leaves its
    newlines, so the lines after it keep their numbers.
    """
    declarations = parse_declarations(source)
    declaring: dict[str, list[int]] = collections.defaultdict(list)
    for number, declaration in enumerate(declarations):
        for name in declaration.names:
            declaring[name].append(number)
    pending = [
        number
        for number, declaration in enumerate(declarations)
        if not declaration.names or declaration.names & roots or declaration.is_object
    ]
    kept = set(pending)
    while pending:
        declaration = declarations[pending.pop()]
        words: list[str] = WORD_TOKEN_RE.findall(
            source, declaration.start, declaration.end
        )
        for word in set(words):
            for number in declaring.get(word, ()):
                if number not in kept:
                    kept.add(number)
                    pending.append(number)
    return "".join(
        source[declaration.start : declaration.end]
        if number in kept
        else "\n" * source.count("\n", declaration.start, declaration.end)
        for number, declaration in enumerate(declarations)
    )


def run_compiler(
    c_path: StrPath,
    optflags: list[str],
    arguments: list[str],
    cwd: Path | None = None,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run KMC GCC on C_PATH with the project's flags and ARGUMENTS.

    Runs in CWD, by default the work directory, which also takes any dumps,
    with ENVIRONMENT's variables added to the process's.
    """
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
        cwd=cwd or get_work_dir(),
        env=dict(os.environ, COMPILER_PATH=str(KMC_DIR), **(environment or {})),
        capture_output=True,
        text=True,
        check=False,
    )


def preprocess(
    c_path: StrPath, optflags: list[str]
) -> subprocess.CompletedProcess[str]:
    """Run KMC GCC's preprocessor on C_PATH with the project's flags and OPTFLAGS.

    A score preprocesses its file twice, to isolate the function and to find
    its nested ones, so the output is kept while the file stays the same.
    """
    status = Path(c_path).resolve().stat()
    return preprocess_version(
        str(Path(c_path).resolve()),
        tuple(optflags),
        status.st_mtime_ns,
        status.st_size,
    )


@functools.lru_cache(maxsize=4)
def preprocess_version(
    c_path: str, optflags: tuple[str, ...], _mtime_ns: int, _size: int
) -> subprocess.CompletedProcess[str]:
    """Preprocess the version of C_PATH that its modification time and size tell."""
    return run_compiler(c_path, list(optflags), ["-E"])


def compile_isolated(
    c_path: StrPath, keep: set[str], optflags: list[str] | None = None
) -> tuple[Functions | None, str]:
    """Compile only the functions in KEEP of C_PATH, as compile_functions does.

    Falls back on compiling all of C_PATH when a function in KEEP is defined
    under another name, as through a macro, and so was cut, or when the cut
    file fails, as when the pruning misses a declaration.
    """
    if optflags is None:
        optflags = get_optflags(c_path)
    result = preprocess(c_path, optflags)
    if result.returncode != 0:
        return None, result.stderr
    with tempfile.TemporaryDirectory() as tmp:
        isolated = Path(tmp) / "isolated.i"
        _ = isolated.write_text(isolate_functions(result.stdout, keep))
        compiled, error = compile_functions(isolated, optflags)
    if compiled is None or not keep <= compiled.keys():
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


# Fences a function's body while its file is preprocessed, and with a
# number marks the start of each of the bodies preprocessed together
BODY_MARK = "__fncompare_body__;"
NUMBERED_BODY_MARK = "__fncompare_body_{}__;"
NUMBERED_BODY_MARK_RE = re.compile(r"__fncompare_body_(\d+)__;")
# The most bodies one run of cpp preprocesses
BODY_BATCH_SIZE = 200


@dataclass
class BodyVariants:
    """Compiles variants of one function's body without redoing the rest.

    SOURCE is the C file at C_PATH, and START and END bound the body of
    FUNCTION in it, from after the opening brace's line through the closing
    brace's. The rest of the file is preprocessed and isolated once; each
    variant's body is preprocessed in batches and put in the body's place,
    so that a variant costs one compile.
    """

    c_path: StrPath
    source: str
    function: str
    start: int
    end: int
    optflags: list[str]
    # The preprocessed, isolated file before and after the body, with only
    # the declarations the body uses and with all of them
    before: str = field(init=False)
    after: str = field(init=False)
    whole_before: str = field(init=False)
    whole_after: str = field(init=False)
    # The identifiers the preprocessed original body names
    body_names: set[str] = field(init=False)

    def __post_init__(self) -> None:
        """Preprocess and isolate the file around the fenced body."""
        body = self.source[self.start : self.end]
        fenced = "".join(
            (self.source[: self.start], BODY_MARK, "\n", body, BODY_MARK, "\n")
        )
        preprocessed = self.preprocess(fenced + self.source[self.end :])
        isolated = isolate_functions(preprocessed, {self.function})
        self.before, _, self.after = isolated.partition(BODY_MARK)
        self.after = self.after.partition(BODY_MARK)[2]
        whole = isolate_functions(preprocessed, {self.function}, prune=False)
        self.body_names = set(IDENTIFIER_RE.findall(preprocessed.split(BODY_MARK)[1]))
        self.whole_before, _, self.whole_after = whole.partition(BODY_MARK)
        self.whole_after = self.whole_after.partition(BODY_MARK)[2]

    def preprocess(self, text: str) -> str:
        """Return TEXT preprocessed as though it were the file at C_PATH.

        It is written beside the file, so that its includes resolve alike.
        """
        directory = Path(self.c_path).resolve().parent
        with tempfile.NamedTemporaryFile(
            "w", suffix=".c", dir=directory, delete=False
        ) as copy:
            _ = copy.write(text)
        try:
            result = run_compiler(copy.name, self.optflags, ["-E"])
        finally:
            Path(copy.name).unlink()
        if result.returncode != 0:
            raise ValueError(result.stderr)
        return result.stdout

    def preprocess_bodies(self, bodies: list[str]) -> list[str]:
        """Return each of BODIES preprocessed in the body's place.

        Each follows the file up to the body, so it sees the same macros, and
        the rest of the file follows the last, closing any conditional the
        function is in.
        """
        preprocessed: list[str] = []
        for first in range(0, len(bodies), BODY_BATCH_SIZE):
            batch = bodies[first : first + BODY_BATCH_SIZE]
            marked = [
                NUMBERED_BODY_MARK.format(number) + "\n" + body
                for number, body in enumerate([*batch, ""])
            ]
            text = self.source[: self.start] + "".join(marked)
            pieces = NUMBERED_BODY_MARK_RE.split(
                self.preprocess(text + self.source[self.end :])
            )
            if len(pieces) != 2 * len(batch) + 3:
                msg = "a variant's body lost its mark in cpp"
                raise ValueError(msg)
            preprocessed += pieces[2:-2:2]
        return preprocessed

    def make_units(self, preprocessed_body: str) -> list[str]:
        """Return the files to compile PREPROCESSED_BODY in, in turn.

        The declarations were pruned for the original body, so a variant
        that names others compiles with them all, as does one that fails:
        without a pruned static inline definition, a call to it would
        compile as a call to an undeclared function.
        """
        whole = self.whole_before + preprocessed_body + self.whole_after
        if set(IDENTIFIER_RE.findall(preprocessed_body)) <= self.body_names:
            return [self.before + preprocessed_body + self.after, whole]
        return [whole]

    def compile(self, preprocessed_body: str) -> tuple[Functions | None, str]:
        """Compile the file with PREPROCESSED_BODY, as compile_functions does.

        Tries each of make_units' files until one compiles.
        """
        compiled: Functions | None = None
        error = ""
        with tempfile.TemporaryDirectory() as tmp:
            unit = Path(tmp) / "variant.i"
            for text in self.make_units(preprocessed_body):
                _ = unit.write_text(text)
                compiled, error = compile_functions(unit, self.optflags)
                if compiled is not None:
                    break
        return compiled, error


@dataclass(frozen=True)
class ObjectSymbol:
    """A symbol in an object file's symbol table."""

    name: str
    value: int
    size: int
    # Its STT_ type and STB_ binding
    type: int
    bind: int
    # The index of the section that defines it, or None for an undefined,
    # absolute or common symbol
    section: int | None


@dataclass(frozen=True)
class ObjectFile:
    """A big-endian ELF32 object file, read with struct.

    pyelftools parses each symbol and relocation through construct, which
    takes seconds over the build's objects.
    """

    data: bytes
    # The name and header of each section, by index
    section_names: list[str]
    section_headers: list[tuple[int, ...]]
    symbols: list[ObjectSymbol]

    def find_section(self, name: str) -> int | None:
        """Return the index of the first section named NAME, if there is one."""
        return self.section_names.index(name) if name in self.section_names else None

    def get_section_size(self, index: int) -> int:
        """Return the size of the section at INDEX."""
        return self.section_headers[index][SH_SIZE_FIELD]

    def read_section(self, index: int) -> bytes:
        """Return the contents of the section at INDEX."""
        header = self.section_headers[index]
        start = header[SH_OFFSET_FIELD]
        return self.data[start : start + header[SH_SIZE_FIELD]]

    def read_relocations(self, name: str) -> list[tuple[int, int, int]]:
        """Return (offset, type, symbol index) of each relocation in NAME."""
        index = self.find_section(name)
        if index is None:
            return []
        return [
            (offset, info & RELOCATION_TYPE_MASK, info >> RELOCATION_SYMBOL_SHIFT)
            for offset, info in unpack_all(RELOCATION, self.read_section(index))
        ]


def unpack_all(layout: struct.Struct, data: bytes) -> list[tuple[int, ...]]:
    """Return the records of LAYOUT in DATA, whose fields are all integers."""
    return cast("list[tuple[int, ...]]", list(layout.iter_unpack(data)))


def read_name(names: bytes, start: int) -> str:
    """Return the name at START in the string table NAMES."""
    return names[start : names.index(0, start)].decode()


def read_object(obj_path: StrPath) -> ObjectFile:
    """Read the sections and symbols of the object file at OBJ_PATH."""
    data = Path(obj_path).read_bytes()
    first_section = read_field(data, E_SHOFF, WORD_SIZE)
    section_count = read_field(data, E_SHNUM, HALF_SIZE)
    headers = unpack_all(
        SECTION_HEADER,
        data[first_section : first_section + section_count * SECTION_HEADER.size],
    )
    names_header = headers[read_field(data, E_SHSTRNDX, HALF_SIZE)]
    names_start = names_header[SH_OFFSET_FIELD]
    section_names = data[names_start : names_start + names_header[SH_SIZE_FIELD]]
    obj = ObjectFile(
        data,
        [read_name(section_names, header[SH_NAME_FIELD]) for header in headers],
        headers,
        [],
    )
    for header in headers:
        if header[SH_TYPE_FIELD] != SHT_SYMTAB:
            continue
        names = obj.read_section(header[SH_LINK_FIELD])
        start = header[SH_OFFSET_FIELD]
        table = data[start : start + header[SH_SIZE_FIELD]]
        obj.symbols.extend(
            ObjectSymbol(
                name=read_name(names, name),
                value=value,
                size=size,
                type=info & SYMBOL_TYPE_MASK,
                bind=info >> SYMBOL_BIND_SHIFT,
                section=section if SHN_UNDEF < section < SHN_LORESERVE else None,
            )
            for name, value, size, info, _, section in unpack_all(SYMBOL, table)
        )
        break
    return obj


def read_object_file(
    obj_path: StrPath, names: Container[str] | None = None
) -> Functions:
    """Return the functions in an object file, as read_object_functions does."""
    return read_object_functions(read_object(obj_path), names)


# {first word: (symbol name, size in words)} of each function in .text.
# GCC names a nested function NAME.N, after its label number.
Spans = dict[int, tuple[str, int]]


def read_function_spans(symbols: list[ObjectSymbol], text_index: int) -> Spans:
    """Return where each function in the .text at TEXT_INDEX starts and ends."""
    spans: Spans = {}
    for symbol in symbols:
        if symbol.type == STT_FUNC and symbol.section == text_index:
            spans[symbol.value // WORD_SIZE] = (symbol.name, symbol.size // WORD_SIZE)
    return spans


# {section index: ([start of each symbol], [(start, size, name)])}, in order
Holders = dict[int, tuple[list[int], list[tuple[int, int, str]]]]


def read_holders(symbols: list[ObjectSymbol]) -> Holders:
    """Return the named symbols an object defines, by section."""
    found: dict[int, list[tuple[int, int, str]]] = {}
    for symbol in symbols:
        if not symbol.name or symbol.section is None:
            continue
        if symbol.type in REFERENCED_TYPES:
            found.setdefault(symbol.section, []).append(
                (symbol.value, symbol.size, symbol.name)
            )
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


def get_linked_address(
    symbol: ObjectSymbol, addend: int, holders: Holders
) -> int | None:
    """Return the matching build's address of SYMBOL plus ADDEND, if it has one.

    A relocation against a section, as one to a static variable is, takes the
    address from the object's symbol that holds it. One whose addend has had
    K0BASE taken off, as PhysicalAddress takes it, gives the physical address.
    """
    name, offset = symbol.name, addend
    if symbol.type == STT_SECTION:
        section_index = symbol.section
        if section_index is None or section_index not in holders:
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


def relocate(words: list[int], obj: ObjectFile, text_index: int) -> Relocated:
    """Mask the relocated halves of WORDS and find what their fields refer to.

    A %lo half is paired with the %hi halves before it against the same
    symbol, as the linker pairs them, to give the address they make.
    """
    relocated = Relocated([None] * len(words), {})
    symbols = obj.symbols
    holders = read_holders(symbols)
    references = relocated.references
    # {symbol index: the %hi halves that wait for their %lo}
    waiting: dict[int, list[int]] = {}
    # {symbol index: the latest %hi half's field}
    high_fields: dict[int, int] = {}
    for offset, kind, symbol_index in obj.read_relocations(".rel.text"):
        index = offset // WORD_SIZE
        symbol = symbols[symbol_index]
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
            is_local = symbol.section == text_index
            if word >> OPCODE_SHIFT == J_OPCODE and is_local:
                target = symbol.value + addend
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


def read_object_functions(
    obj: ObjectFile, names: Container[str] | None = None
) -> Functions:
    """Return {name: routine} of each function in OBJ's .text, or of NAMES."""
    text_index = obj.find_section(".text")
    if text_index is None or not obj.symbols:
        return {}
    text = obj.read_section(text_index)
    words = [word for (word,) in unpack_all(WORD, text)]
    relocated = relocate(words, obj, text_index)
    keep_constant_values(words, relocated)
    functions: Functions = {}
    for start, (name, size) in read_function_spans(obj.symbols, text_index).items():
        if names is not None and name.split(".")[0] not in names:
            continue
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


# match_blocks.c, which finds difflib's matching blocks, and where it's built
MATCH_BLOCKS_SOURCE = ROOT / "tools" / "match_blocks.c"
MATCH_BLOCKS_LIBRARY = ROOT / "build" / "tools" / "match_blocks.so"
INT32_POINTER = ctypes.POINTER(ctypes.c_int32)
# (start in the first sequence, start in the second, length) of a run of
# equal entries
Block = tuple[int, int, int]
# A difflib opcode: its tag and the two sequences' ranges it covers
Opcode = tuple[str, int, int, int, int]


@functools.cache
def load_match_blocks() -> Callable[..., int]:
    """Return match_blocks.c's function, building it first when it's stale.

    It's built under another name and renamed, so that runs in parallel
    never load half a library.
    """
    library = MATCH_BLOCKS_LIBRARY
    if (
        not library.exists()
        or library.stat().st_mtime < MATCH_BLOCKS_SOURCE.stat().st_mtime
    ):
        library.parent.mkdir(parents=True, exist_ok=True)
        building = library.with_name(f"{library.name}.{os.getpid()}")
        _ = subprocess.run(
            [
                "cc",
                "-O2",
                "-shared",
                "-fPIC",
                "-o",
                str(building),
                str(MATCH_BLOCKS_SOURCE),
            ],
            check=True,
        )
        _ = building.replace(library)
    function = ctypes.CDLL(str(library)).match_blocks
    function.restype = ctypes.c_int32
    function.argtypes = [
        INT32_POINTER,
        ctypes.c_int32,
        INT32_POINTER,
        ctypes.c_int32,
        ctypes.c_int32,
        INT32_POINTER,
    ]
    return function


def find_matching_blocks(a: Sequence[Hashable], b: Sequence[Hashable]) -> list[Block]:
    """Return A's and B's matching blocks, as difflib.SequenceMatcher's.

    These are the blocks of difflib.SequenceMatcher(None, a, b,
    autojunk=False).get_matching_blocks(), which match_blocks.c finds by
    difflib's search, in compiled code: difflib's own took most of a score.
    """
    codes: dict[Hashable, int] = {}
    a_codes = [codes.setdefault(entry, len(codes)) for entry in a]
    b_codes = [codes.setdefault(entry, len(codes)) for entry in b]
    capacity = min(len(a), len(b)) + 1
    blocks = (ctypes.c_int32 * (3 * capacity))()
    count = load_match_blocks()(
        (ctypes.c_int32 * len(a))(*a_codes),
        len(a),
        (ctypes.c_int32 * len(b))(*b_codes),
        len(b),
        len(codes),
        blocks,
    )
    if count < 0:
        sys.exit("match_blocks: out of memory")
    found = [
        (blocks[start], blocks[start + 1], blocks[start + 2])
        for start in range(0, 3 * count, 3)
    ]
    found.append((len(a), len(b), 0))
    return found


def find_opcodes(a: Sequence[Hashable], b: Sequence[Hashable]) -> list[Opcode]:
    """Return the opcodes that turn A into B, as difflib.SequenceMatcher's."""
    opcodes: list[Opcode] = []
    a_index = b_index = 0
    for a_start, b_start, size in find_matching_blocks(a, b):
        if a_index < a_start and b_index < b_start:
            opcodes.append(("replace", a_index, a_start, b_index, b_start))
        elif a_index < a_start:
            opcodes.append(("delete", a_index, a_start, b_index, b_start))
        elif b_index < b_start:
            opcodes.append(("insert", a_index, a_start, b_index, b_start))
        a_index, b_index = a_start + size, b_start + size
        if size:
            opcodes.append(("equal", a_start, a_index, b_start, b_index))
    return opcodes


# A row of two routines' instructions, as pair_rows pairs them: the index of
# each side's instruction, or None where the other side's has no pair
Row = tuple[int | None, int | None]


def pair_rows(opcodes: Sequence[Opcode]) -> list[Row]:
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
    opcodes = find_opcodes(target.words, compiled.words)
    return Pairing(target, compiled, pair_rows(opcodes))


def count_differences(target: Routine, compiled: Routine) -> int:
    """Return how many instructions, on either side, have no equal pair."""
    pairing = pair_words(target, compiled)
    shared = sum(map(pairing.is_equal, pairing.rows))
    return len(target.words) + len(compiled.words) - 2 * shared


def read_operations(routine: Routine) -> list[object]:
    """Return the operation of each of ROUTINE's instructions."""
    return [rabbitizer.Instruction(word).uniqueId for word in routine.words]


def count_shape_differences(target: Routine, compiled: Routine) -> int:
    """Return how many instructions, on either side, have no pair by operation.

    Registers, offsets and constants are ignored, so this counts only the
    instructions that one side has and the other lacks or replaces.
    """
    return count_operation_differences(
        tuple(read_operations(target)), tuple(read_operations(compiled))
    )


@functools.cache
def count_operation_differences(
    target_operations: tuple[object, ...], compiled_operations: tuple[object, ...]
) -> int:
    """Return how many of two operation sequences' entries have no pair.

    Variants that differ only in registers share their operations, and
    aligning these is the slowest part of a score, since a routine uses few
    kinds of operation, so each count is kept.
    """
    blocks = find_matching_blocks(target_operations, compiled_operations)
    shared = sum(size for _, _, size in blocks)
    return len(target_operations) + len(compiled_operations) - 2 * shared


# The upper half of addiu $sp, $sp, N, which sets up a routine's frame
ADDIU_SP_SP = 0x27BD


def describe(
    target: Routine,
    compiled: Routine,
    nested_pairs: list[tuple[Routine, Routine]] | None = None,
) -> str:
    """Return the score and, short of a match, the counts of differences.

    The count of differing words is followed by how many of them differ in
    shape, by operation. A change of registers alone moves every word that
    names them, which in a large routine hides how close its instruction
    sequence has come. The shape count leaves those out. NESTED_PAIRS, the
    original and compiled routines of the function's nested functions,
    count too.
    """
    pairs = [(target, compiled), *(nested_pairs or [])]
    differences = sum(count_differences(*pair) for pair in pairs)
    total = sum(len(original.words) + len(ours.words) for original, ours in pairs)
    score = 1 - differences / total if total else 1.0
    if not differences:
        return f"{score:.3f} OK"
    shape_differences = sum(count_shape_differences(*pair) for pair in pairs)
    description = f"{score:.3f} ({differences} differ, {shape_differences} in shape)"
    growth = frame_size(target) - frame_size(compiled)
    if growth:
        # A frame of another size shows a stack slot more or fewer, as a
        # local that the original reads once into a register it spills
        description += f" frame {growth:+d}"
    return description


def frame_size(routine: Routine) -> int:
    """Return the bytes the routine's first addiu $sp, $sp, -N takes, or 0."""
    for word in routine.words:
        if word >> 16 == ADDIU_SP_SP:
            immediate = word & 0xFFFF
            return 0x10000 - immediate if immediate & 0x8000 else 0
    return 0


# The routine of a nested function that was not compiled on its own, so that
# every word of its original differs
NOT_COMPILED = Routine([], [], [])


def pair_nested(
    original: Original, compiled: Functions
) -> list[tuple[Routine, Routine]]:
    """Pair ORIGINAL's nested routines with those COMPILED."""
    return [
        (routine, compiled.get(name, NOT_COMPILED))
        for name, routine in original.nested.items()
    ]


def describe_original(original: Original, compiled: Functions, function: str) -> str:
    """Return describe's score of FUNCTION in COMPILED and its nested ones."""
    nested_pairs = pair_nested(original, compiled)
    return describe(original.routine, compiled[function], nested_pairs)


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


@functools.cache
def read_symbol_names() -> dict[int, str]:
    """Return {address: name} of the symbols the matching build names once."""
    return {address: name for name, address in read_linked_symbols().items()}


def count_calls(routine: Routine) -> collections.Counter[str]:
    """Return how many times ROUTINE calls each function, by name.

    A call whose callee the matching build doesn't name once isn't counted,
    as an object's call to it can't be resolved.
    """
    calls: collections.Counter[str] = collections.Counter()
    for word, reference in zip(routine.words, routine.references, strict=True):
        if word >> OPCODE_SHIFT != JAL_OPCODE or reference is None:
            continue
        name = read_symbol_names().get(reference * WORD_SIZE | K0BASE)
        if name is not None:
            calls[name] += 1
    return calls


def print_call_differences(target: Routine, compiled: Routine) -> None:
    """Print the calls one routine makes more often than the other.

    A missing or extra call is always a difference in the source, where
    differing words may only be the compiler's choices.
    """
    target_calls, compiled_calls = count_calls(target), count_calls(compiled)
    for sign, calls in (
        ("-", target_calls - compiled_calls),
        ("+", compiled_calls - target_calls),
    ):
        for name, count in sorted(calls.items()):
            print(f"  {sign}call {name} x{count}")


def find_diff_runs(target: Routine, compiled: Routine) -> list[list[Row]]:
    """Return the runs of rows whose instructions differ."""
    pairing = pair_words(target, compiled)
    return [
        list(group)
        for is_equal, group in itertools.groupby(pairing.rows, pairing.is_equal)
        if not is_equal
    ]


def print_diff(
    target: Routine,
    compiled: Routine,
    annotate: Callable[[int], str] | None = None,
) -> None:
    """Print the first few runs of instructions that differ.

    ANNOTATE, given the index of one of our instructions, returns a note to
    print after it.
    """
    runs = find_diff_runs(target, compiled)
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
            text = disassemble(compiled, index)
            if annotate:
                text = f"{text:{ANNOTATION_COLUMN}} ; {annotate(index)}"
            print(f"  +{index:4} {text}")
        if last is not None:
            for index in range(last, min(len(target.words), last + DIFF_CONTEXT)):
                print(f"   {index:4} {disassemble(target, index)}")
        print("  --")
    if len(runs) > MAX_DIFF_HUNKS:
        print(f"  ({len(runs) - MAX_DIFF_HUNKS} more)")
    print_call_differences(target, compiled)


def print_shape_diff(target: Routine, compiled: Routine) -> None:
    """Print the instructions that have no pair by operation."""
    opcodes = find_opcodes(read_operations(target), read_operations(compiled))
    for tag, first, last, compiled_first, compiled_last in opcodes:
        if tag == "equal":
            continue
        for index in range(first, last):
            print(f"  -{index:4} {disassemble(target, index)}")
        for index in range(compiled_first, compiled_last):
            print(f"  +{index:4} {disassemble(compiled, index)}")
        print("  --")


def report(
    function: str, original: Original, compiled: Functions, *, verbosity: str
) -> None:
    """Print FUNCTION's score and, given -v, -s or -l, its diffs or listings.

    The nested functions' are printed too.
    """
    print(f"{function} {describe_original(original, compiled, function)}")
    if not verbosity:
        return
    pairs = {function: (original.routine, compiled[function])}
    for name, routine in original.nested.items():
        pairs[name] = (routine, compiled.get(name, NOT_COMPILED))
    for name, (original_routine, ours) in pairs.items():
        if count_differences(original_routine, ours):
            if len(pairs) > 1:
                print(f" {name}:")
            if verbosity == "-s":
                print_shape_diff(original_routine, ours)
            elif verbosity == "-l":
                for index in range(len(ours.words)):
                    print(f"  {index:4} {disassemble(ours, index)}")
            else:
                print_diff(original_routine, ours)


# The options that print more than the score
VERBOSITIES = {"-v", "-s", "-l"}


# What the tools' file arguments end with, which no function name does
PATH_SUFFIXES = (".s", ".c", ".o")


def locate_function(function: str) -> tuple[str, str]:
    """Find FUNCTION's original assembly and the C file that holds it.

    Drafts are under asm/nonmatchings. Matched functions are found under
    asm/matchings, which splat writes when told to disassemble everything,
    so that changes to them can be scored too.
    """
    for kind in ("nonmatchings", "matchings"):
        asm_root = ROOT / "asm" / kind
        asm_paths = list(asm_root.rglob(f"{function}.s"))
        if len(asm_paths) > 1:
            sys.exit(f"{function}: {len(asm_paths)} assembly files under {asm_root}")
        if asm_paths:
            unit = asm_paths[0].parent.relative_to(asm_root)
            return str(asm_paths[0]), str(ROOT / "src" / unit.with_suffix(".c"))
    sys.exit(f"{function}: no assembly file under {ROOT / 'asm'}")


def expand_function_name(arguments: list[str]) -> list[str]:
    """Replace a leading function name with its ASM_FILE, C_FILE and name.

    The tools take an assembly file, a C file and a function name first; the
    name alone stands for all three.
    """
    if not arguments or arguments[0].startswith("-"):
        return arguments
    if arguments[0].endswith(PATH_SUFFIXES):
        return arguments
    return [*locate_function(arguments[0]), *arguments]


def score(asm_path: str, c_path: str, names: list[str], verbosity: str) -> None:
    """Score NAMES, or all of ASM_PATH's functions, as compiled from C_PATH."""
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
        nested: Functions = {}
        if names and not c_path.endswith(".o"):
            nested = read_original(asm_path, c_path, name).nested
        report(name, Original(targets[name], nested), compiled, verbosity=verbosity)


def main() -> None:
    """Score the functions named on the command line, or all of them."""
    verbosity = next((arg for arg in sys.argv[1:] if arg in VERBOSITIES), "")
    args = [arg for arg in sys.argv[1:] if arg not in VERBOSITIES]
    if args and not args[0].endswith(PATH_SUFFIXES):
        units: dict[tuple[str, str], list[str]] = {}
        for name in args:
            units.setdefault(locate_function(name), []).append(name)
        for (asm_path, c_path), names in units.items():
            score(asm_path, c_path, names, verbosity)
        return
    asm_path, c_path, *names = args
    score(asm_path, c_path, names, verbosity)


if __name__ == "__main__":
    main()
