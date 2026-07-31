#!/usr/bin/env python3
"""Print a display-list emitter's packet stream in DISPLAY-LIST order.

A `main`-segment DL emitter precomputes one `Gfx *` per packet and spills most of
them, so the `.s` writes packets in an order that has nothing to do with the list
it builds: on `func_8002CDA8` (S303) program order and DL order diverge by up to
40 packets, which makes the raw `.s` unreadable as a display list.

This walks the splat `.s`, constant-propagates through registers AND `$sp` slots,
tracks the DL write pointer symbolically (`DL+n`), and re-sorts the packet stores
by their DL offset. Non-DL events (calls, branches, stores through an unresolved
base) follow in program order.

    venv/bin/python3 tools/dl_decode.py asm/nonmatchings/<seg>/<pack>/<fn>.s

The DL base is the `lw` that loads `*gfxp` into the walking pointer, auto-detected
as the first `lw <reg>, 0x0(<non-sp base>)`. Pass `--dl-init 0x<addr>` to name that
instruction when a function loads some other pointer first.

Decode the words it prints with `tools/rdp_word.py` and `tools/combine_word.py`.
"""

from __future__ import annotations

import argparse
import re

INSN = re.compile(r"/\* \w+ (\w{8}) (\w{8}) \*/\s+(\S+)\s*(.*)")
SP_MEM = re.compile(r"(-?0x[0-9A-Fa-f]+|0)\(\$sp\)$")
REG_MEM = re.compile(r"(-?0x[0-9A-Fa-f]+|0)\((\$\w+)\)$")
BRANCHES = ("beq", "bne", "blez", "bgez", "bgtz", "bltz", "beqz", "bnez", "j", "b")


class DL:
    """A value known to be `*gfxp + off`, i.e. a display-list write pointer."""

    __slots__ = ("off",)

    def __init__(self, off):
        self.off = off

    def __repr__(self):
        return "DL+%d" % self.off


def _imm(text):
    """Parse an immediate operand, or None when it is a relocation like %lo(sym)."""
    try:
        return int(text, 0)
    except ValueError:
        return None


def fmt(value):
    if value is None:
        return "?"
    if isinstance(value, (DL, str)):
        return str(value)
    return "0x%08X" % (value & 0xFFFFFFFF)


def decode(path, dl_init=None):
    """Return (packets, others) for one `.s`.

    packets is [(dl_offset, addr, value, offset_is_approx)] sorted by DL offset; others is
    [(addr_or_None, text)] in program order.
    """
    reg = {"$zero": 0}
    slot = {}
    packets = []
    others = []
    dl_found = dl_init is not None
    # The scan is linear over the `.s`, so once it crosses a label or a branch the cursor it is
    # tracking belongs to ONE path while the offsets keep accumulating across all of them. The
    # ORDER stays right (that is what the tool is for); the number does not, so mark it (S308).
    branched = False

    for line in open(path):
        text = line.strip()
        match = INSN.search(line)
        if not match:
            if text.startswith(".L") or text.startswith("glabel"):
                others.append((None, text))
                branched = branched or text.startswith(".L")
            continue
        addr = int(match.group(1), 16)
        op = match.group(3)
        args = [a.strip() for a in match.group(4).split(",")]

        if op == "lui":
            half = re.search(r"\(([^)]*) >> 16\)", args[1])
            reg[args[0]] = (int(half.group(1), 0) & 0xFFFF0000) if half else args[1]
        elif op == "ori":
            half = re.search(r"\(([^)]*) & 0xFFFF\)", args[2])
            base = reg.get(args[1])
            reg[args[0]] = (
                (base | (int(half.group(1), 0) & 0xFFFF)) & 0xFFFFFFFF
                if (half and isinstance(base, int))
                else "%s|%s" % (fmt(base), args[2])
            )
        elif op == "addiu":
            base = reg.get(args[1])
            addend = _imm(args[2])
            if addend is None:
                # `addiu $r, $r, %lo(sym)` and friends: a symbol, not a number.
                reg[args[0]] = "%s+%s" % (fmt(base), args[2])
                continue
            if args[1] == "$zero":
                reg[args[0]] = addend & 0xFFFFFFFF
            elif isinstance(base, DL):
                reg[args[0]] = DL(base.off + addend)
            elif isinstance(base, int):
                reg[args[0]] = (base + addend) & 0xFFFFFFFF
            else:
                reg[args[0]] = "%s+%s" % (fmt(base), args[2])
        elif op == "addu":
            left, right = reg.get(args[1]), reg.get(args[2])
            if isinstance(left, DL) and right == 0:
                reg[args[0]] = DL(left.off)
            elif isinstance(right, DL) and left == 0:
                reg[args[0]] = DL(right.off)
            elif isinstance(left, int) and isinstance(right, int):
                reg[args[0]] = (left + right) & 0xFFFFFFFF
            else:
                reg[args[0]] = "<addu %s %s>" % (fmt(left), fmt(right))
        elif op == "sw":
            source, dest = args[0], args[1]
            on_stack = SP_MEM.match(dest)
            if on_stack:
                slot[int(on_stack.group(1), 0)] = reg.get(source)
                continue
            through = REG_MEM.match(dest)
            base = reg.get(through.group(2)) if through else None
            if isinstance(base, DL):
                packets.append(
                    (base.off + int(through.group(1), 0), addr, reg.get(source), branched)
                )
            else:
                others.append((addr, "ST %s <- %s" % (dest, fmt(reg.get(source)))))
        elif op == "lw":
            on_stack = SP_MEM.match(args[1])
            if on_stack:
                reg[args[0]] = slot.get(int(on_stack.group(1), 0))
            else:
                reg[args[0]] = "<mem %s>" % args[1]
                through = REG_MEM.match(args[1])
                takes_base = dl_init == addr or (
                    not dl_found and through and int(through.group(1), 0) == 0
                )
                if takes_base:
                    reg[args[0]] = DL(0)
                    dl_found = True
        elif op == "jal":
            others.append((addr, "JAL %s (a0=%s)" % (args[0], fmt(reg.get("$a0")))))
        elif op in BRANCHES:
            others.append((addr, "BR %s %s" % (op, " ".join(args))))
            branched = True
        elif op in ("or", "and"):
            left = reg.get(args[1])
            right = reg.get(args[2]) if args[2].startswith("$") else _imm(args[2])
            if isinstance(left, int) and isinstance(right, int):
                reg[args[0]] = (left | right) if op == "or" else (left & right)
            else:
                shown = fmt(right) if not isinstance(right, int) else hex(right)
                reg[args[0]] = "(%s %s %s)" % (fmt(left), op, shown)
        elif args and args[0].startswith("$"):
            reg[args[0]] = "<%s %s>" % (op, ",".join(args[1:]))

    packets.sort(key=lambda entry: (entry[0], entry[1]))
    return packets, others


def main():
    parser = argparse.ArgumentParser(description="display-list order decoder")
    parser.add_argument("asm", help="path to a splat asm/nonmatchings/**/<fn>.s")
    parser.add_argument(
        "--dl-init",
        help="address (hex) of the instruction loading *gfxp, when auto-detection misses",
    )
    args = parser.parse_args()

    packets, others = decode(args.asm, int(args.dl_init, 16) if args.dl_init else None)
    print("=== display-list order ===")
    for offset, addr, value, approx in packets:
        print("%s%-5d (%08X) %s" % ("~" if approx else "+", offset, addr, fmt(value)))
    print()
    print("=== other events, program order ===")
    for addr, text in others:
        print("%s %s" % ("--------" if addr is None else "%08X" % addr, text))


if __name__ == "__main__":
    main()
