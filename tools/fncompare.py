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

MAX_DIFF_HUNKS = int(os.environ.get("MAX_DIFF_HUNKS", "4"))

Functions = dict[str, list[int]]
StrPath = str | os.PathLike[str]


def mask_jump(word: int) -> int:
    """Mask the target of j and jal, which always carry a relocation."""
    if word >> OPCODE_SHIFT in (J_OPCODE, JAL_OPCODE):
        return word & ~JUMP_TARGET_MASK
    return word


@functools.cache
def read_linked_functions() -> dict[str, int]:
    """Return {name: address} of the functions the matching build names once."""
    if not LINKED_ELF.exists():
        return {}
    addresses: dict[str, int] = {}
    repeated: set[str] = set()
    with LINKED_ELF.open("rb") as elf_file:
        symbols = ELFFile(elf_file).get_section_by_name(".symtab")
        if not isinstance(symbols, SymbolTableSection):
            return {}
        for symbol in symbols.iter_symbols():
            info = read_entry_field(symbol.entry, "st_info")
            if not isinstance(info, Container):
                continue
            if read_entry_field(info, "type") != "STT_FUNC":
                continue
            if symbol.name in addresses:
                repeated.add(symbol.name)
            addresses[symbol.name] = read_entry_int(symbol.entry, "st_value")
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
        env = dict(os.environ, COMPILER_PATH=str(KMC_DIR))
        result = subprocess.run(
            [
                str(KMC_DIR / "gcc"),
                "-c",
                *CFLAGS,
                *optflags,
                *INCLUDES,
                *DEFINES,
                "-o",
                str(obj_path),
                str(Path(c_path).resolve()),
            ],
            cwd=get_work_dir(),
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
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
        for index in range(t1, t2):
            print(f"  -{index:4} {disassemble(target[index])}")
        for index in range(c1, c2):
            print(f"  +{index:4} {disassemble(compiled[index])}")
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
