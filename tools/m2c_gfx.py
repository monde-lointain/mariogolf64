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
with the constant bits OR'd into it for the operands. Each macro is printed with the expressions of
its computed words, and a texture rectangle whose coordinates are clamped
at zero is marked as gSPScisTextureRectangle.
"""

import re
import subprocess
import sys
from collections import defaultdict

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


def parse_constant(text):
    match = CONSTANT_RE.match(text.strip())
    if not match:
        return None
    value = match.group(1)
    return 0 if value == "NULL" else int(value, 0) & 0xFFFFFFFF


def find_opcode(text):
    """Returns the constant holding a command's opcode in a computed word."""
    for digits in HEX_RE.findall(text):
        value = int(digits, 16)
        if value >= 0xC0000000 or (value >> 24) in (
            0x01,
            0x02,
            0x03,
            0x04,
            0x05,
            0x06,
            0x07,
        ):
            return value & 0xFF000000 | value & 0x00FFFFFF
    return None


def or_constant(text):
    """Returns the constant bits OR'd into a computed word, or 0."""
    match = OR_RE.search(text)
    return int(match.group(1), 16) if match else 0


def collect_stores(lines):
    root_of = {}
    offset_of = {}
    value_of = {}
    stores = defaultdict(dict)
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
            name, field, value = match.group(1), int(match.group(2), 16), match.group(3)
        else:
            match = DEREF_STORE_RE.match(line)
            if not match:
                continue
            name, field, value = match.group(1), 0, match.group(2)
        root = root_of.get(name, name)
        stores[root][offset_of.get(name, 0) + field] = value_of.get(value, value)
    return stores


def build_commands(stores):
    """Returns [(offset, word0, word1, dynamic expressions)] in offset order."""
    commands = []
    for slot in sorted({offset & ~7 for offset in stores}):
        first, second = stores.get(slot), stores.get(slot + 4)
        word0 = parse_constant(first) if first else None
        word1 = parse_constant(second) if second else None
        dynamic = []
        if word0 is None:
            word0 = find_opcode(first) if first else None
            dynamic.append(f"w0={first}")
        if word1 is None:
            word1 = or_constant(second) if second else 0
            dynamic.append(f"w1={second}")
        commands.append((slot, word0, word1, dynamic))
    return commands


def decode_run(run):
    words = []
    for _, word0, word1, _ in run:
        words += [f"{word0:08X}", f"{word1:08X}"]
    result = subprocess.run(
        [GFXDIS, "-i", "-p", "-w", *words], capture_output=True, text=True
    )
    decoded = []
    for line in result.stdout.splitlines():
        match = OFFSET_RE.match(line)
        if match:
            decoded.append((int(match.group(1), 16), match.group(2).rstrip(",")))
    return decoded


def print_run(run):
    decoded = decode_run(run)
    for index, (start, text) in enumerate(decoded):
        end = decoded[index + 1][0] if index + 1 < len(decoded) else len(run) * 8
        dynamic = [
            expr for command in run[start // 8 : end // 8] for expr in command[3]
        ]
        if "TextureRectangle" in text and any(
            CLAMP_RE.search(expr) for expr in dynamic
        ):
            text = text.replace("gsSPTextureRectangle", "gsSPScisTextureRectangle")
        print(f"  {run[0][0] + start:#05x} {text}")
        for expr in dynamic:
            print(f"          {expr}")


def print_root(root, commands):
    print(f"== {root}")
    run = []
    for command in commands:
        if run and command[0] != run[-1][0] + 8:
            print_run(run)
            run = []
        if command[1] is None:
            if run:
                print_run(run)
                run = []
            print(f"  {command[0]:#05x} ? {' '.join(command[3])}")
        else:
            run.append(command)
    if run:
        print_run(run)


def main():
    with open(sys.argv[1]) as m2c_file:
        stores = collect_stores(m2c_file.read().splitlines())
    for root, root_stores in stores.items():
        if len(root_stores) >= 2:
            print_root(root, build_commands(root_stores))


if __name__ == "__main__":
    main()
