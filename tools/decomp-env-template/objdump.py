#!/usr/bin/env python3

"""
objdump.py is a Python script that processes MIPS object files and displays their disassembled assembly code in a clean, normalized format.

Usage: `./objdump.py file.o [--symbol SYMBOL]`
"""

import functools
import re
import subprocess
import sys
import shutil
from typing import List, Optional
from pathlib import Path

# Configuration
SKIP_LINES = 1
RE_INT = re.compile(r"-?[0-9]+")
RE_COMMENT = re.compile(r"<.*?>")
RE_SPREL = re.compile(r"(?<=,)([0-9]+|0x[0-9a-f]+)\((sp|s8)\)")

# MIPS objdump settings
OBJDUMP_EXECUTABLES = [
    "mips-linux-gnu-objdump",
    "mips64-linux-gnu-objdump",
    "mips64-elf-objdump",
]
OBJDUMP_ARGS = ["-drz", "-m", "mips:4300"]

BRANCH_INSTRUCTIONS = {
    "b", "j", "beq", "bne", "beqz", "bnez", "bgez", "bgtz", "blez", "bltz",
    "bc1t", "bc1f", "beql", "bnel", "beqzl", "bnezl", "bgezl", "bgtzl", 
    "blezl", "bltzl", "bc1tl", "bc1fl"
}


def find_objdump_executable() -> str:
    for executable in OBJDUMP_EXECUTABLES:
        if shutil.which(executable):
            return executable
    raise Exception(f"Could not find any objdump executables: {OBJDUMP_EXECUTABLES}")


def parse_relocated_line(line: str) -> tuple[str, str, str]:
    for c in ",\t ":
        if c in line:
            split_pos = line.rindex(c)
            break
    else:
        raise Exception(f"failed to parse relocated line: {line}")
    
    before = line[:split_pos + 1]
    after = line[split_pos + 1:]
    
    paren_pos = after.find("(")
    if paren_pos == -1:
        imm, after = after, ""
    else:
        imm, after = after[:paren_pos], after[paren_pos:]
    
    return before, "0" if imm == "0x0" else imm, after


AUTO_NAMED_SYMBOL = re.compile(r"^(?:D|func)_(?:ovl\d+_)?([0-9A-Fa-f]{8})$")
SYMBOL_ADDR_LINE = re.compile(r"^([A-Za-z_]\w*)\s*=\s*0x([0-9A-Fa-f]+)\s*;")
# This file lives in tools/decomp-env-template/, so the project root is two levels up.
SYMBOL_ADDRS_PATH = Path(__file__).resolve().parents[2] / "symbol_addrs.txt"


@functools.lru_cache(maxsize=None)
def symbol_addrs() -> dict[str, Optional[int]]:
    """VRAM of each named symbol in symbol_addrs.txt; None for a name declared twice (ambiguous)."""
    addrs: dict[str, Optional[int]] = {}
    for line in SYMBOL_ADDRS_PATH.read_text().splitlines():
        match = SYMBOL_ADDR_LINE.match(line)
        if match is None:
            continue
        name = match.group(1)
        addrs[name] = None if name in addrs else int(match.group(2), 16)
    return addrs


def symbol_base_address(symbol: str) -> Optional[int]:
    """VRAM a symbol is bound to: encoded in an auto-named D_/func_ name, else looked up in symbol_addrs.txt."""
    match = AUTO_NAMED_SYMBOL.match(symbol)
    if match is not None:
        return int(match.group(1), 16)
    return symbol_addrs().get(symbol)


def resolved_address(symbol: str, imm: str) -> Optional[int]:
    """VRAM a relocation resolves to (symbol base plus addend), or None if the symbol has no known address."""
    base = symbol_base_address(symbol)
    if base is None:
        return None
    try:
        addend = 0 if imm in ("0", "imm", "addr") else int(imm, 0)
    except ValueError:
        return None
    return base + addend


def process_mips_reloc(reloc_row: str, repl: str, imm: str) -> str:
    if "R_MIPS_JALR" in reloc_row or "R_MIPS_NONE" in reloc_row:
        return repl
    if "R_MIPS_LO16" in reloc_row or "R_MIPS_HI16" in reloc_row:
        addr = resolved_address(repl, imm)
        if addr is not None:
            if "R_MIPS_LO16" in reloc_row:
                return f"%lo(0x{addr & 0xFFFF:x})"
            return f"%hi(0x{((addr + 0x8000) >> 16) & 0xFFFF:x})"
        
    if imm not in ("0", "imm", "addr"):
        try:
            imm_val = int(imm, 0)
            repl += f"+{imm}" if imm_val > 0 else imm
        except ValueError:
            # Handle cases where imm might be a hex string without 0x prefix
            try:
                imm_val = int(imm, 16)
                repl += f"+0x{imm}" if imm_val > 0 else f"-0x{imm}"
            except ValueError:
                # If still can't parse, just append as-is
                repl += f"+{imm}"
        
    if any(r in reloc_row for r in ["R_MIPS_LO16", "R_MIPS_LITERAL", "R_MIPS_GPREL16"]):
        return f"%lo({repl})"
    elif "R_MIPS_GOT16" in reloc_row:
        return f"%got({repl})"
    elif "R_MIPS_CALL16" in reloc_row:
        return f"%call16({repl})"
    elif "R_MIPS_HI16" in reloc_row:
        return f"%hi({repl})"
    elif "R_MIPS_26" in reloc_row:
        return repl
    else:
        raise Exception(f"unknown relocation type '{reloc_row}'")


def process_reloc(reloc_row: str, prev_line: str) -> Optional[str]:
    if prev_line == "<skipped>":
        return None
    
    before, imm, after = parse_relocated_line(prev_line)
    symbol = reloc_row.split()[-1]
    
    if "R_MIPS_" in reloc_row:
        new_symbol = process_mips_reloc(reloc_row, symbol, imm)
        return before + new_symbol + after
    else:
        raise Exception(f"unknown relocation type: {reloc_row}")


def normalize_jumptable_references(line: str) -> str:
    """Normalize jump table references to .rodata for consistent comparison"""
    # Pattern to match jump table references like jtbl_800CA268 or jtbl_ovlgap_8040CAA8
    jtbl_pattern = r'jtbl_\w+'
    
    # Replace jump table references with .rodata
    normalized = re.sub(jtbl_pattern, '.rodata', line)
    return normalized


def convert_numbers_to_hex(line: str) -> str:
    """Convert decimal numbers to hex, avoiding variable names and other text"""
    forbidden_chars = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_")
    
    def replace_func(match):
        full = match.group(0)
        if len(full) <= 1:
            return full
        start, end = match.span()
        if start > 0 and line[start - 1] in forbidden_chars:
            return full
        if end < len(line) and line[end] in forbidden_chars:
            return full
        return hex(int(full))
    
    return RE_INT.sub(replace_func, line)


# GCC nested functions are local text symbols that can sit before their parent in the object.
RE_SYMBOL_HEADER = re.compile(r"^([0-9a-f]+) <(.+)>:$")
RE_SECTION_OFFSET = re.compile(r"\.text(?:\+0x([0-9a-f]+))?")
# GCC gives a nested function the label `name.N`; the target object names the same callee `name`.
RE_NESTED_SUFFIX = re.compile(r"\.\d+$")
RE_HEX_ADDRESS = re.compile(r"[0-9a-f]+")


def process_objdump_lines(lines: List[str], symbol: Optional[str] = None) -> List[str]:
    output_lines = []
    base = 0
    in_symbol = True
    found = False

    for i, line in enumerate(lines):
        if i < SKIP_LINES:
            continue

        line = line.rstrip()
        header = RE_SYMBOL_HEADER.match(line)
        if header:
            in_symbol = symbol is None or header.group(2) == symbol
            if in_symbol and symbol is not None:
                base = int(header.group(1), 16)
                found = True
            continue
        if ">:" in line or not line or not in_symbol:
            continue

        # Remove comments and extract instruction part
        line = RE_COMMENT.sub("", line).rstrip()
        line = "\t".join(line.split("\t")[2:])  # Remove address and hex bytes
        if not line:
            continue

        # Parse mnemonic and arguments
        if "\t" in line:
            mnemonic, args = line.split("\t", 1)
        else:
            parts = line.split(" ", 1)
            mnemonic = parts[0].strip()
            args = parts[1].strip() if len(parts) > 1 else ""

        line = f"{mnemonic}\t{args}".replace("\t", " " * 4) if args else mnemonic

        # Handle relocations
        if "R_MIPS_" in line:
            if output_lines:
                modified_line = process_reloc(line, output_lines[-1])
                if modified_line:
                    output_lines[-1] = modified_line
            continue

        # Convert decimal numbers to hex (but preserve stack/register differences)
        if mnemonic not in BRANCH_INSTRUCTIONS:
            line = convert_numbers_to_hex(line)

        # Normalize jump table references to .rodata for consistent comparison
        line = normalize_jumptable_references(line)

        output_lines.append(line)

    # Remove trailing nops
    while output_lines and output_lines[-1].startswith("nop"):
        output_lines.pop()

    if symbol is not None:
        if not found:
            raise ValueError(f"symbol '{symbol}' not found in object")
        nested_names = {}
        for line in lines[SKIP_LINES:]:
            header = RE_SYMBOL_HEADER.match(line.rstrip())
            if header and RE_NESTED_SUFFIX.search(header.group(2)):
                nested_names.setdefault(int(header.group(1), 16), RE_NESTED_SUFFIX.sub("", header.group(2)))
        output_lines = [rebase_to_function(line, base, nested_names) for line in output_lines]

    return output_lines


def rebase_to_function(line: str, base: int, nested_names: Dict[int, str]) -> str:
    """Make branch targets and .text offsets at or after `base` relative to the function start.

    Earlier offsets (a nested child emitted before the target, or another function) stay as they are.
    A `jal` to a nested child is written with the child's source name, so it matches the target object.
    """
    parts = line.split(maxsplit=1)
    if not parts:
        return line
    mnemonic = parts[0]
    args = parts[1] if len(parts) > 1 else ""
    if base and mnemonic in BRANCH_INSTRUCTIONS and args:
        head, sep, target = args.rpartition(",")
        if RE_HEX_ADDRESS.fullmatch(target) and int(target, 16) >= base:
            args = f"{head}{sep}{int(target, 16) - base:x}"

    def rebase_offset(match: re.Match) -> str:
        offset = int(match.group(1) or "0", 16)
        if mnemonic == "jal" and offset in nested_names:
            return nested_names[offset]
        if offset < base:
            return match.group(0)
        relative = offset - base
        return f".text+0x{relative:x}" if relative else ".text"

    args = RE_SECTION_OFFSET.sub(rebase_offset, args)
    return f"{mnemonic}    {args}" if args else mnemonic


def objdump(filename: str, symbol: Optional[str] = None) -> List[str]:
    executable = find_objdump_executable()
    result = subprocess.check_output([executable] + OBJDUMP_ARGS + [filename])
    lines = result.decode("utf-8").splitlines()
    return process_objdump_lines(lines, symbol)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Process MIPS object files and display disassembled assembly code"
    )
    parser.add_argument("file", help="Object file (.o)")

    parser.add_argument(
        "--symbol",
        help="Dump only this function symbol, with branch targets and .text offsets relative to its start"
    )

    args = parser.parse_args()

    try:
        output_lines = objdump(args.file, args.symbol)

        for line in output_lines:
            print(line)

    except FileNotFoundError:
        print(f"Error: File '{args.file}' not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)