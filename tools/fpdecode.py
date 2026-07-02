#!/usr/bin/env python3
"""Decode a hex rodata constant to its IEEE-754 float/double value.

Before writing a C literal for an `ldc1`/`lwc1`-loaded rodata constant, run the
raw hex through this so you don't conflate a 32-bit float with a 64-bit double.
The trap (S160): the DOUBLE 0x3FE999999999999A is 0.8, but the FLOAT 0.3f is
0x3E99999A -- both carry the 0x999...A mantissa nibble pattern, so a glance at
"...9999999A" invites writing 0.3 when the rodata is a 0.8 double.

Usage:
  tools/fpdecode.py 0x3FE999999999999A          # 16 hex digits -> double
  tools/fpdecode.py 0x3E99999A                  # 8 hex digits  -> float
  tools/fpdecode.py 3FE999999999999A 41E0000000000000
  tools/fpdecode.py --f32 0x40000000            # force 32-bit read
  tools/fpdecode.py --f64 0x4004000000000000    # force 64-bit read

With no width flag: 8 hex digits -> float, 16 -> double; other widths print both
the float (low 32 bits) and, when >= 16 digits, the double reading.
"""

from __future__ import annotations

import argparse
import struct
import sys


def as_float(word: int) -> float:
    return struct.unpack(">f", struct.pack(">I", word & 0xFFFFFFFF))[0]


def as_double(dword: int) -> float:
    return struct.unpack(">d", struct.pack(">Q", dword & 0xFFFFFFFFFFFFFFFF))[0]


def decode(token: str, force: str | None) -> str:
    raw = token.lower().removeprefix("0x")
    try:
        val = int(raw, 16)
    except ValueError:
        return f"{token}: not a hex value"
    ndig = len(raw)

    if force == "f32":
        return f"0x{val & 0xFFFFFFFF:08X}  float  = {as_float(val)!r}"
    if force == "f64":
        return f"0x{val & 0xFFFFFFFFFFFFFFFF:016X}  double = {as_double(val)!r}"

    if ndig <= 8:
        return f"0x{val:08X}  float  = {as_float(val)!r}"
    if ndig <= 16:
        out = f"0x{val:016X}  double = {as_double(val)!r}"
        out += f"\n           (low32 float = {as_float(val)!r})"
        return out
    return f"{token}: {ndig} hex digits > 16 (not a single float/double)"


def main() -> None:
    ap = argparse.ArgumentParser(description="Decode hex rodata to IEEE-754 float/double.")
    ap.add_argument("values", nargs="+", help="hex word(s), 0x-prefixed or bare")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--f32", action="store_const", dest="force", const="f32", help="force 32-bit float")
    g.add_argument("--f64", action="store_const", dest="force", const="f64", help="force 64-bit double")
    args = ap.parse_args()
    for tok in args.values:
        print(decode(tok, args.force))


if __name__ == "__main__":
    main()
