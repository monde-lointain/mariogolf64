#!/usr/bin/env python3
"""Decode the display-list writes in m2c output into GBI macros.

    m2c_gfx.py M2C_OUTPUT.c

m2c writes display-list code as stores through chains of pointers, such as
`temp_fp_2 = temp_fp + 8;` and `temp_fp_2->unk4 = 0;`. This resolves each
pointer to an offset from the root of its chain and pairs the stored words
into commands.

gfxdis only recognises the macros that expand to several commands, such as
gDPLoadTextureTile, gDPLoadTLUT_pal256 and gSPTextureRectangle, when it is
given the whole run of commands, so each chain is decoded as one run. Words
computed at run time get placeholders: an opcode found in the expression,
with the constant bits OR'd into it for the operands. Each macro is printed
with the expressions of its computed words, and a texture rectangle whose
coordinates are clamped at zero is marked as gSPScisTextureRectangle.
"""

import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import NamedTuple

GFXDIS = "gfxdis.f3dex2"
STEP_RE = re.compile(r"^\s*(\w+) = (\w+)(?: ([+-]) (0x[0-9A-Fa-f]+|\d+))?;")
ASSIGN_RE = re.compile(r"^\s*(\w+) = (.+);")
STORE_RE = re.compile(r"^\s*(\w+)->unk(-?[0-9A-F]+) = (.+);")
DEREF_STORE_RE = re.compile(r"^\s*\*(\w+) = (.+);")
CONSTANT_RE = re.compile(r"^\(?(?:\(\w+\) )?(-?0x[0-9A-Fa-f]+|-?\d+|NULL)\)?U?$")
OR_RE = re.compile(r"\| 0x([0-9A-Fa-f]+)\)*$")
HEX_RE = re.compile(r"0x([0-9A-Fa-f]{7,8})\b")
OFFSET_RE = re.compile(r"^\s*/\* ([0-9A-Fa-f]+): \*/ (.*)$")
# MAX(x, 0) compiles to x & (~x >> 31), which m2c prints with this shift
CLAMP_RE = re.compile(r">> 0x1F")

WORD_MASK = 0xFFFFFFFF
WORD_SIZE = 4
COMMAND_SIZE = 8
COMMAND_ALIGNMENT_MASK = ~(COMMAND_SIZE - 1)
OPCODE_SHIFT = 24
# Opcodes from here up are the RDP's and the RSP's other commands; below are
# G_VTX to G_TRI2 and the other F3DEX2 geometry commands
FIRST_HIGH_OPCODE_WORD = 0xC0000000
GEOMETRY_OPCODES = range(0x01, 0x08)
# A root needs at least this many stores to be a display list
MIN_STORE_COUNT = 2


class Command(NamedTuple):
    """One display-list command, as its two words."""

    offset: int
    word0: int | None
    word1: int
    dynamic: list[str]


def parse_constant(text: str) -> int | None:
    """Return the value of a constant expression, or None if it isn't one."""
    match = CONSTANT_RE.match(text.strip())
    if not match:
        return None
    value = match.group(1)
    return 0 if value == "NULL" else int(value, 0) & WORD_MASK


def find_opcode(text: str) -> int | None:
    """Return the constant holding a command's opcode in a computed word."""
    for match in HEX_RE.finditer(text):
        value = int(match.group(1), 16)
        if (
            value >= FIRST_HIGH_OPCODE_WORD
            or (value >> OPCODE_SHIFT) in GEOMETRY_OPCODES
        ):
            return value
    return None


def find_or_constant(text: str) -> int:
    """Return the constant bits OR'd into a computed word, or 0."""
    match = OR_RE.search(text)
    return int(match.group(1), 16) if match else 0


def collect_stores(lines: list[str]) -> dict[str, dict[int, str]]:
    """Return each pointer chain's stores, by offset from its root."""
    root_of: dict[str, str] = {}
    offset_of: dict[str, int] = {}
    value_of: dict[str, str] = {}
    stores: defaultdict[str, dict[int, str]] = defaultdict(dict)
    for line in lines:
        match = ASSIGN_RE.match(line)
        if match and parse_constant(match.group(2)) is not None:
            value_of[match.group(1)] = match.group(2)
            continue
        match = STEP_RE.match(line)
        if match and match.group(2) in value_of:
            value_of[match.group(1)] = value_of[match.group(2)]
            continue
        if match:
            name, base = match.group(1), match.group(2)
            step = int(match.group(4), 0) if match.group(4) else 0
            if match.group(3) == "-":
                step = -step
            root_of[name] = root_of.get(base, base)
            offset_of[name] = offset_of.get(base, 0) + step
            continue
        match = STORE_RE.match(line)
        if match:
            name = match.group(1)
            field = int(match.group(2), 16)
            value = match.group(3)
        else:
            match = DEREF_STORE_RE.match(line)
            if not match:
                continue
            name, field, value = match.group(1), 0, match.group(2)
        root = root_of.get(name, name)
        stores[root][offset_of.get(name, 0) + field] = value_of.get(value, value)
    return stores


def build_commands(stores: dict[int, str]) -> list[Command]:
    """Return the commands of a chain's stores, in offset order."""
    commands: list[Command] = []
    for slot in sorted({offset & COMMAND_ALIGNMENT_MASK for offset in stores}):
        first = stores.get(slot)
        second = stores.get(slot + WORD_SIZE)
        word0 = parse_constant(first) if first else None
        word1 = parse_constant(second) if second else None
        dynamic: list[str] = []
        if word0 is None:
            word0 = find_opcode(first) if first else None
            dynamic.append(f"w0={first}")
        if word1 is None:
            word1 = find_or_constant(second) if second else 0
            dynamic.append(f"w1={second}")
        commands.append(Command(slot, word0, word1, dynamic))
    return commands


def decode_run(run: list[Command]) -> list[tuple[int, str]]:
    """Return gfxdis's macros for a run of commands, by offset in the run."""
    words: list[str] = []
    for command in run:
        words += [f"{command.word0:08X}", f"{command.word1:08X}"]
    result = subprocess.run(
        [GFXDIS, "-i", "-p", "-w", *words], capture_output=True, text=True, check=False
    )
    decoded: list[tuple[int, str]] = []
    for line in result.stdout.splitlines():
        match = OFFSET_RE.match(line)
        if match:
            decoded.append((int(match.group(1), 16), match.group(2).rstrip(",")))
    return decoded


def print_run(run: list[Command]) -> None:
    """Print a run's macros, each with the expressions of its computed words."""
    decoded = decode_run(run)
    for index, (start, text) in enumerate(decoded):
        end = (
            decoded[index + 1][0]
            if index + 1 < len(decoded)
            else len(run) * COMMAND_SIZE
        )
        dynamic = [
            expression
            for command in run[start // COMMAND_SIZE : end // COMMAND_SIZE]
            for expression in command.dynamic
        ]
        shown = text
        if "TextureRectangle" in text and any(
            CLAMP_RE.search(expression) for expression in dynamic
        ):
            shown = text.replace("gsSPTextureRectangle", "gsSPScisTextureRectangle")
        print(f"  {run[0].offset + start:#05x} {shown}")
        for expression in dynamic:
            print(f"          {expression}")


def print_root(root: str, commands: list[Command]) -> None:
    """Print a chain's commands, decoding each unbroken run together."""
    print(f"== {root}")
    run: list[Command] = []
    for command in commands:
        if run and command.offset != run[-1].offset + COMMAND_SIZE:
            print_run(run)
            run = []
        if command.word0 is None:
            if run:
                print_run(run)
                run = []
            print(f"  {command.offset:#05x} ? {' '.join(command.dynamic)}")
        else:
            run.append(command)
    if run:
        print_run(run)


def main() -> None:
    """Print the display lists of the m2c output named."""
    stores = collect_stores(Path(sys.argv[1]).read_text().splitlines())
    for root, root_stores in stores.items():
        if len(root_stores) >= MIN_STORE_COUNT:
            print_root(root, build_commands(root_stores))


if __name__ == "__main__":
    main()
