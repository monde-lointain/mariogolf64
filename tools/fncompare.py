#!/usr/bin/env python3
"""Compare compiled functions with the original assembly, word by word.

Relocated fields (the immediates of %hi/%lo pairs and the targets of j/jal)
are masked out on both sides, so a function matches when it would produce
the original bytes once linked.

    fncompare.py [-v] ASM_FILE C_FILE [FUNCTION ...]

ASM_FILE is a splat assembly file holding one or more functions and C_FILE
the C to compile, or an object file already compiled. Prints one line per function: its name, its score (the
fraction of equal words) and whether it matches exactly. With -v, also
prints the instructions that differ.
"""

import atexit
import difflib
import os
import re
import subprocess
import sys
import shutil
import tempfile
import threading

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KMC_DIR = os.path.join(ROOT, "tools", "gcc_kmc")
CFLAGS = ["-G0", "-mips3", "-mgp32", "-mfp32", "-mno-abicalls", "-fno-PIC", "-ffast-math",
          "-nostdinc", "-funsigned-char", "-w"]
# Optimization levels by file or directory, as the Makefile sets them
OPTFLAGS_BY_PATH = {os.path.join("src", "libkmc"): "-O1 -fno-fast-math"}
DEFAULT_OPTFLAGS = "-O2"
INCLUDES = ["-Iinclude", "-Iinclude/libc", "-Ilib/ultralib/include",
            "-Ilib/ultralib/include/PR", "-Ilib/libmus/include", "-Isrc",
            "-Ibuild"]
DEFINES = ["-D_LANGUAGE_C", "-D_MIPS_SZLONG=32", "-D_MIPS_SZINT=32", "-DF3DEX_GBI_2",
           "-DNDEBUG", "-D_FINALROM", "-DBUILD_VERSION=VERSION_J", "-D__IS_KMC__=1",
           "-DNON_MATCHING"]

INSN_RE = re.compile(r"^\s*/\* [0-9A-F]+ [0-9A-F]+ ([0-9A-F]{8}) \*/\s+(.*)$")
GLABEL_RE = re.compile(r"^glabel (\w+)")
ENDLABEL_RE = re.compile(r"^endlabel (\w+)")

R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 = 4, 5, 6
RELOC_MASKS = {R_MIPS_26: 0x03FFFFFF, R_MIPS_HI16: 0xFFFF, R_MIPS_LO16: 0xFFFF}


def mask_jump(word):
    """Masks the target of j and jal, which always carry a relocation."""
    if word >> 26 in (2, 3):
        return word & ~0x03FFFFFF
    return word


def read_target_functions(asm_path):
    """Returns {name: [masked words]} for each function in a splat asm file."""
    functions = {}
    current = None
    with open(asm_path) as asm_file:
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
                    word &= ~0xFFFF
                current.append(mask_jump(word))
    return functions


_work_dir = None
_work_dir_lock = threading.Lock()


def get_work_dir():
    """Returns a directory in /tmp that mirrors the repo through symlinks.

    The KMC assembler writes its temporary files to the current directory
    and fsyncs them. Concurrent fsyncs on the repo's disk wait on each
    other, which made parallel compiles slower than serial ones, so the
    compiler runs from here, where the files stay in memory.
    """
    global _work_dir
    with _work_dir_lock:
        if _work_dir is None:
            work_dir = tempfile.mkdtemp(prefix="fncompare-")
            atexit.register(shutil.rmtree, work_dir, True)
            for entry in os.listdir(ROOT):
                os.symlink(os.path.join(ROOT, entry), os.path.join(work_dir, entry))
            _work_dir = work_dir
    return _work_dir


def get_optflags(c_path):
    """Returns the optimization flags the Makefile builds C_PATH with."""
    path = os.path.relpath(os.path.abspath(c_path), ROOT)
    for key in (path, os.path.dirname(path)):
        if key in OPTFLAGS_BY_PATH:
            return OPTFLAGS_BY_PATH[key].split()
    return DEFAULT_OPTFLAGS.split()


def compile_functions(c_path):
    """Compiles C_PATH; returns ({name: [masked words]}, error text)."""
    c_path = os.path.abspath(c_path)
    with tempfile.TemporaryDirectory() as tmp:
        obj_path = os.path.join(tmp, "out.o")
        env = dict(os.environ, COMPILER_PATH=KMC_DIR)
        result = subprocess.run(
            [os.path.join(KMC_DIR, "gcc"), "-c", *CFLAGS, *get_optflags(c_path), *INCLUDES, *DEFINES,
             "-o", obj_path, c_path],
            cwd=get_work_dir(), env=env, capture_output=True, text=True)
        if result.returncode != 0:
            return None, result.stderr
        with open(obj_path, "rb") as obj_file:
            return read_object_functions(ELFFile(obj_file)), ""


def read_object_functions(elf):
    text_index = None
    for index, section in enumerate(elf.iter_sections()):
        if section.name == ".text":
            text_index = index
            text = section.data()
    words = [int.from_bytes(text[i:i + 4], "big") for i in range(0, len(text), 4)]
    for section in elf.iter_sections():
        if isinstance(section, RelocationSection) and section.name == ".rel.text":
            for reloc in section.iter_relocations():
                mask = RELOC_MASKS.get(reloc["r_info_type"])
                if mask:
                    words[reloc["r_offset"] // 4] &= ~mask
    words = [mask_jump(word) for word in words]
    functions = {}
    symbols = elf.get_section_by_name(".symtab")
    for symbol in symbols.iter_symbols():
        if symbol["st_info"]["type"] == "STT_FUNC" and symbol["st_shndx"] == text_index:
            start = symbol["st_value"] // 4
            # GCC names a nested function NAME.N, after its label number
            name = symbol.name.split(".")[0]
            functions[name] = words[start:start + symbol["st_size"] // 4]
    return functions


def score(target, compiled):
    if target == compiled:
        return 1.0
    matcher = difflib.SequenceMatcher(None, target, compiled, autojunk=False)
    return matcher.ratio()


def disassemble(word):
    import rabbitizer
    return rabbitizer.Instruction(word).disassemble()


MAX_DIFF_HUNKS = int(os.environ.get("MAX_DIFF_HUNKS", 4))


def print_diff(target, compiled):
    """Prints the first few runs of instructions that differ."""
    matcher = difflib.SequenceMatcher(None, target, compiled, autojunk=False)
    hunks = [opcode for opcode in matcher.get_opcodes() if opcode[0] != "equal"]
    for tag, t1, t2, c1, c2 in hunks[:MAX_DIFF_HUNKS]:
        for index in range(t1, t2):
            print(f"  -{index:4} {disassemble(target[index])}")
        for index in range(c1, c2):
            print(f"  +{index:4} {disassemble(compiled[index])}")
        print("  --")
    if len(hunks) > MAX_DIFF_HUNKS:
        print(f"  ({len(hunks) - MAX_DIFF_HUNKS} more)")


def main():
    verbose = "-v" in sys.argv
    args = [arg for arg in sys.argv[1:] if arg != "-v"]
    asm_path, c_path, *names = args
    targets = read_target_functions(asm_path)
    if c_path.endswith(".o"):
        with open(c_path, "rb") as obj_file:
            compiled, error = read_object_functions(ELFFile(obj_file)), ""
    else:
        compiled, error = compile_functions(c_path)
    if compiled is None:
        print(error, file=sys.stderr)
        sys.exit(2)
    for name in names or targets:
        if name not in compiled:
            print(f"{name} missing")
            continue
        value = score(targets[name], compiled[name])
        print(f"{name} {value:.3f} {'OK' if value == 1.0 else ''}")
        if verbose and value != 1.0:
            print_diff(targets[name], compiled[name])


if __name__ == "__main__":
    main()
