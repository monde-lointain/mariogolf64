#!/usr/bin/env python3
"""Compare compiled functions with the original assembly, word by word.

Relocated fields (the immediates of %hi/%lo pairs and the targets of j/jal)
are masked out on both sides, so a function matches when it would produce
the original bytes once linked. A jal keeps its target, though, so that
calls made in another order still differ: the compiled call's target is
looked up by name in the matching build's ELF. A call to a function the ELF
doesn't name, or names more than once, is masked like the rest.

    fncompare.py [-v] ASM_FILE C_FILE [FUNCTION ...]

ASM_FILE is a splat assembly file holding one or more functions and C_FILE
the C to compile, or an object file already compiled. Prints one line per
function: its name, its score (the fraction of equal words) and either OK
for an exact match or the count of differing words, which still moves when
a change is too small to show in the score. With -v, also prints the
instructions that differ.
"""

import atexit
import difflib
import functools
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

import rabbitizer
from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
from elftools.elf.sections import SymbolTableSection

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

INSN_RE = re.compile(r"^\s*/\* [0-9A-F]+ [0-9A-F]+ ([0-9A-F]{8}) \*/\s+(.*)$")
GLABEL_RE = re.compile(r"^glabel (\w+)")
ENDLABEL_RE = re.compile(r"^endlabel (\w+)")

WORD_SIZE = 4
OPCODE_SHIFT = 26
J_OPCODE = 2
JAL_OPCODE = 3
JUMP_TARGET_MASK = 0x03FFFFFF
# The 256 MB region that a jump's target field addresses words in, which
# holds all of the game's code
CODE_REGION = 0x80000000
IMMEDIATE_MASK = 0xFFFF
R_MIPS_26 = 4
R_MIPS_HI16 = 5
R_MIPS_LO16 = 6
RELOC_MASKS = {
    R_MIPS_26: JUMP_TARGET_MASK,
    R_MIPS_HI16: IMMEDIATE_MASK,
    R_MIPS_LO16: IMMEDIATE_MASK,
}

# The offsets of the fields read_symbol_table and read_linked_functions read
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
STT_FUNC = 2

MAX_DIFF_HUNKS = int(os.environ.get("MAX_DIFF_HUNKS", "4"))
# How many matching instructions -v prints around each run, as diff -U does
DIFF_CONTEXT = int(os.environ.get("DIFF_CONTEXT", "0"))

Functions = dict[str, list[int]]
StrPath = str | os.PathLike[str]


def mask_jump(word: int) -> int:
    """Mask the target of j and jal, which always carry a relocation."""
    if word >> OPCODE_SHIFT in (J_OPCODE, JAL_OPCODE):
        return word & ~JUMP_TARGET_MASK
    return word


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
def read_linked_functions() -> dict[str, int]:
    """Return {name: address} of the functions the matching build names once.

    Reads the symbol table with struct, since pyelftools takes half a second
    over the game's thousands of symbols.
    """
    if not LINKED_ELF.exists():
        return {}
    symbols, names = read_symbol_table(LINKED_ELF.read_bytes())
    addresses: dict[str, int] = {}
    repeated: set[str] = set()
    for start in range(0, len(symbols), SYMBOL_SIZE):
        symbol = symbols[start : start + SYMBOL_SIZE]
        if symbol[ST_INFO] & SYMBOL_TYPE_MASK != STT_FUNC:
            continue
        name_start = read_field(symbol, ST_NAME, WORD_SIZE)
        name = names[name_start : names.index(0, name_start)].decode()
        if name in addresses:
            repeated.add(name)
        addresses[name] = read_field(symbol, ST_VALUE, WORD_SIZE)
    return {
        name: address for name, address in addresses.items() if name not in repeated
    }


@functools.cache
def read_linked_addresses() -> frozenset[int]:
    """Return the addresses of the functions read_linked_functions names."""
    return frozenset(read_linked_functions().values())


def get_call_field(address: int) -> int:
    """Return a jal's target field for a call to ADDRESS."""
    return (address >> 2) & JUMP_TARGET_MASK


def mask_target_jump(word: int) -> int:
    """Mask a jump's target, unless it calls a function the ELF names.

    The functions are those read_linked_functions names.
    """
    if word >> OPCODE_SHIFT == JAL_OPCODE:
        address = CODE_REGION | (word & JUMP_TARGET_MASK) << 2
        if address in read_linked_addresses():
            return word
    return mask_jump(word)


def read_target_functions(asm_path: StrPath) -> Functions:
    """Return {name: [masked words]} for each function in a splat asm file."""
    functions: Functions = {}
    current: list[int] | None = None
    with Path(asm_path).open() as asm_file:
        for line in asm_file:
            match = GLABEL_RE.match(line)
            if match:
                current = functions.setdefault(match.group(1), [])
                continue
            if ENDLABEL_RE.match(line):
                current = None
                continue
            match = INSN_RE.match(line)
            if match and current is not None:
                word = int(match.group(1), 16)
                text = match.group(2)
                if "%hi(" in text or "%lo(" in text:
                    word &= ~IMMEDIATE_MASK
                current.append(mask_target_jump(word))
    return functions


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
        info = read_entry_field(symbol.entry, "st_info")
        if not isinstance(info, Container):
            continue
        is_function = read_entry_field(info, "type") == "STT_FUNC"
        section_index = read_entry_field(symbol.entry, "st_shndx")
        if is_function and section_index == text_index:
            start = read_entry_int(symbol.entry, "st_value") // WORD_SIZE
            size = read_entry_int(symbol.entry, "st_size") // WORD_SIZE
            spans[start] = (symbol.name, size)
    return spans


def mask_relocations(
    words: list[int], elf: ELFFile, symbols: SymbolTableSection, spans: Spans
) -> None:
    """Mask the relocated fields of WORDS.

    The calls to functions that read_linked_functions names keep their target.
    """
    relocations = elf.get_section_by_name(".rel.text")
    if not isinstance(relocations, RelocationSection):
        return
    linked = read_linked_functions()
    for reloc in relocations.iter_relocations():
        kind = read_entry_int(reloc.entry, "r_info_type")
        index = read_entry_int(reloc.entry, "r_offset") // WORD_SIZE
        callee = ""
        if kind == R_MIPS_26 and words[index] >> OPCODE_SHIFT == JAL_OPCODE:
            # A call to a static function is made relative to .text, with the
            # function's word in the field
            symbol = symbols.get_symbol(read_entry_int(reloc.entry, "r_info_sym"))
            local = spans.get(words[index] & JUMP_TARGET_MASK, ("", 0))
            callee = (symbol.name or local[0]).split(".")[0]
        words[index] = mask_jump(words[index] & ~RELOC_MASKS.get(kind, 0))
        if callee in linked:
            words[index] |= get_call_field(linked[callee])


def read_object_functions(elf: ELFFile) -> Functions:
    """Return {name: [masked words]} for each function in an object's .text."""
    text_index = elf.get_section_index(".text")
    symbols = elf.get_section_by_name(".symtab")
    if text_index is None or not isinstance(symbols, SymbolTableSection):
        return {}
    text = elf.get_section(text_index).data()
    words = [
        int.from_bytes(text[i : i + WORD_SIZE], "big")
        for i in range(0, len(text), WORD_SIZE)
    ]
    spans = read_function_spans(symbols, text_index)
    mask_relocations(words, elf, symbols, spans)
    # The jumps that carry no relocation, to local labels
    words = [
        word if word >> OPCODE_SHIFT == JAL_OPCODE else mask_jump(word)
        for word in words
    ]
    return {
        name.split(".")[0]: words[start : start + size]
        for start, (name, size) in spans.items()
    }


def score(target: list[int], compiled: list[int]) -> float:
    """Return the fraction of words the two functions share, in order."""
    if target == compiled:
        return 1.0
    matcher = difflib.SequenceMatcher(None, target, compiled, autojunk=False)
    return matcher.ratio()


def count_differences(target: list[int], compiled: list[int]) -> int:
    """Return how many words, on either side, fall outside the shared runs."""
    matcher = difflib.SequenceMatcher(None, target, compiled, autojunk=False)
    shared = sum(block.size for block in matcher.get_matching_blocks())
    return len(target) + len(compiled) - 2 * shared


def describe(target: list[int], compiled: list[int]) -> str:
    """Return the score and, short of a match, the count of differing words."""
    value = score(target, compiled)
    if value == 1.0:
        return f"{value:.3f} OK"
    return f"{value:.3f} ({count_differences(target, compiled)} differ)"


def disassemble(word: int) -> str:
    """Return the text of one instruction."""
    return rabbitizer.Instruction(word).disassemble()


def print_diff(target: list[int], compiled: list[int]) -> None:
    """Print the first few runs of instructions that differ."""
    matcher = difflib.SequenceMatcher(None, target, compiled, autojunk=False)
    hunks = [opcode for opcode in matcher.get_opcodes() if opcode[0] != "equal"]
    for _, t1, t2, c1, c2 in hunks[:MAX_DIFF_HUNKS]:
        for index in range(max(0, t1 - DIFF_CONTEXT), t1):
            print(f"   {index:4} {disassemble(target[index])}")
        for index in range(t1, t2):
            print(f"  -{index:4} {disassemble(target[index])}")
        for index in range(c1, c2):
            print(f"  +{index:4} {disassemble(compiled[index])}")
        for index in range(t2, min(len(target), t2 + DIFF_CONTEXT)):
            print(f"   {index:4} {disassemble(target[index])}")
        print("  --")
    if len(hunks) > MAX_DIFF_HUNKS:
        print(f"  ({len(hunks) - MAX_DIFF_HUNKS} more)")


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
        if verbose and targets[name] != compiled[name]:
            print_diff(targets[name], compiled[name])


if __name__ == "__main__":
    main()
