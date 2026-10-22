#!/usr/bin/env python3
"""Decode the display-list writes in m2c output into GBI macros.

    m2c_gfx.py M2C_OUTPUT.c

m2c writes display-list code as stores through chains of pointers, such as
`temp_fp_2 = temp_fp + 8;` and `temp_fp_2->unk4 = 0;`. This resolves each
pointer to an offset from the root of its chain, pairs the stored words
into commands and prints them in offset order: constant commands decoded
with gfxdis, the others as their two raw words.
"""

import re
import subprocess
import sys
from collections import defaultdict

GFXDIS = "gfxdis.f3dex2"
STEP_RE = re.compile(r"^\s*(\w+) = (\w+) \+ (0x[0-9A-Fa-f]+|\d+);")
STORE_RE = re.compile(r"^\s*(\w+)->unk([0-9A-F]+) = (.+);")
DEREF_STORE_RE = re.compile(r"^\s*\*(\w+) = (.+);")
CONSTANT_RE = re.compile(r"^\(?(?:\(\w+\) )?(-?0x[0-9A-Fa-f]+|-?\d+|NULL)\)?U?$")


def parse_constant(text):
    match = CONSTANT_RE.match(text.strip())
    if not match:
        return None
    value = match.group(1)
    return 0 if value == "NULL" else int(value, 0) & 0xFFFFFFFF


def decode(word0, word1):
    result = subprocess.run([GFXDIS, "-w", f"{word0:08X}", f"{word1:08X}"], capture_output=True, text=True)
    lines = [line.strip().rstrip(",") for line in result.stdout.splitlines() if line.strip() not in ("{", "}")]
    return "; ".join(lines) if lines else "?"


def main():
    with open(sys.argv[1]) as m2c_file:
        lines = m2c_file.read().splitlines()
    root_of = {}
    offset_of = {}
    words = defaultdict(dict)
    for line in lines:
        match = STEP_RE.match(line)
        if match:
            name, base, step = match.group(1), match.group(2), int(match.group(3), 0)
            root_of[name] = root_of.get(base, base)
            offset_of[name] = offset_of.get(base, 0) + step
            continue
        match = STORE_RE.match(line) or DEREF_STORE_RE.match(line)
        if not match:
            continue
        if len(match.groups()) == 3:
            name, field, value = match.group(1), int(match.group(2), 16), match.group(3)
        else:
            name, field, value = match.group(1), 0, match.group(2)
        root = root_of.get(name, name)
        offset = offset_of.get(name, 0) + field
        words[root][offset] = value
    for root, stores in words.items():
        if len(stores) < 2:
            continue
        print(f"== {root}")
        for slot in sorted({offset & ~7 for offset in stores}):
            first, second = stores.get(slot), stores.get(slot + 4)
            word0 = parse_constant(first) if first else None
            word1 = parse_constant(second) if second else None
            if word0 is not None and word1 is not None:
                text = decode(word0, word1)
            else:
                text = f"[{first}] [{second}]"
            print(f"  {slot:#05x} {text}")


if __name__ == "__main__":
    main()
