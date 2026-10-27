#!/usr/bin/env python3
"""Decode the display-list writes in a function's assembly into GBI macros.

    asm_gfx.py FUNCTION.s

m2c loses track of display-list words that the compiler spills to the stack
and loads back later, so m2c_gfx.py can't decode them. This instead steps
through the assembly in order, ignoring branches, and tracks the constants
that each register and stack slot holds. The display list is taken to start
at the pointer loaded through the first argument, and every word stored
inside it is recorded.

Because branches are ignored, a loop body is seen once and both arms of an
if are seen one after the other. Each display-list address is listed with
the words stored there, in program order. Words that aren't constants print
as the expressions that compute them, and a run of constant words is
decoded with gfxdis.
"""

import re
import subprocess
import sys

GFXDIS = "gfxdis.f3dex2"
MASK = 0xFFFFFFFF
# Where the display list and the stack are taken to be
GFX_BASE = 0x40000000
GFX_LIMIT = 0x50000000
STACK_BASE = 0x7F000000
LINE_RE = re.compile(r"^\s*/\* \w+ ([0-9A-F]{8}) [0-9A-F]+ \*/\s+(\S+)\s*(.*)$")
MEM_RE = re.compile(r"^(.*)\((\$\w+)\)$")
HI_RE = re.compile(r"^%hi\((.+)\)$")
LO_RE = re.compile(r"^%lo\((.+)\)$")
SHIFTED_HI_RE = re.compile(r"^\((0x[0-9A-Fa-f]+) >> 16\)$")
MASKED_LO_RE = re.compile(r"^\((0x[0-9A-Fa-f]+) & 0xFFFF\)$")


def parse_int(text):
    return int(text, 0) & MASK


class Machine:
    """Holds what is known about each register and memory word."""

    def __init__(self):
        self.regs = {"$zero": 0, "$sp": STACK_BASE}
        for index in range(4):
            self.regs[f"$a{index}"] = f"arg{index}"
        self.memory = {}
        self.stores = []
        self.call_count = 0

    def get(self, reg):
        return self.regs.get(reg, reg.lstrip("$"))

    def set(self, reg, value):
        if reg != "$zero":
            self.regs[reg] = value

    def address(self, operand):
        """Returns the address an `offset(base)` operand names."""
        offset_text, base = MEM_RE.match(operand).groups()
        base_value = self.get(base)
        lo = LO_RE.match(offset_text)
        if lo:
            return f"{lo.group(1)}"
        offset = int(offset_text, 0) if offset_text else 0
        if isinstance(base_value, int):
            return (base_value + offset) & MASK
        return f"*({base_value} + {offset:#x})"

    def load(self, operand):
        address = self.address(operand)
        if address == "*(arg0 + 0x0)":
            return GFX_BASE
        if isinstance(address, int):
            return self.memory.get(address, f"[{address:#x}]")
        return address

    def store(self, operand, value, pc):
        address = self.address(operand)
        if isinstance(address, int):
            self.memory[address] = value
            if GFX_BASE <= address < GFX_LIMIT:
                self.stores.append((address - GFX_BASE, value, pc))


def binary(op, left, right):
    """Combines two values, folding them when both are constants."""
    if isinstance(left, int) and isinstance(right, int):
        if op == "+":
            return (left + right) & MASK
        if op == "-":
            return (left - right) & MASK
        if op == "|":
            return left | right
        if op == "&":
            return left & right
        if op == "^":
            return left ^ right
        if op == "<<":
            return (left << right) & MASK
        if op == ">>":
            return left >> right
    if op == "+" and right == 0:
        return left
    if op == "|" and right == 0:
        return left
    if op in "+|" and left == 0:
        return right
    show = lambda value: hex(value) if isinstance(value, int) else value
    return f"({show(left)} {op} {show(right)})"


def step(machine, pc, op, args):
    """Applies one instruction to MACHINE."""
    operands = [arg.strip() for arg in args.split(",")] if args else []
    get = machine.get
    if op == "lui":
        hi = HI_RE.match(operands[1])
        shifted = SHIFTED_HI_RE.match(operands[1])
        if hi:
            machine.set(operands[0], hi.group(1))
        elif shifted:
            machine.set(operands[0], parse_int(shifted.group(1)) & 0xFFFF0000)
        else:
            machine.set(operands[0], (parse_int(operands[1]) << 16) & MASK)
    elif op in ("addiu", "ori", "andi", "xori"):
        lo = LO_RE.match(operands[2])
        if lo:
            machine.set(operands[0], f"&{lo.group(1)}")
            return
        masked = MASKED_LO_RE.match(operands[2])
        if masked:
            immediate = int(masked.group(1), 16) & 0xFFFF
        else:
            immediate = int(operands[2], 0)
        if op == "addiu":
            machine.set(operands[0], binary("+", get(operands[1]), immediate & MASK))
        elif op == "ori":
            machine.set(operands[0], binary("|", get(operands[1]), immediate))
        elif op == "andi":
            machine.set(operands[0], binary("&", get(operands[1]), immediate))
        else:
            machine.set(operands[0], binary("^", get(operands[1]), immediate))
    elif op in ("addu", "subu", "or", "and", "xor"):
        symbol = {"addu": "+", "subu": "-", "or": "|", "and": "&", "xor": "^"}[op]
        machine.set(operands[0], binary(symbol, get(operands[1]), get(operands[2])))
    elif op == "nor":
        left, right = get(operands[1]), get(operands[2])
        if isinstance(left, int) and isinstance(right, int):
            machine.set(operands[0], ~(left | right) & MASK)
        else:
            machine.set(operands[0], f"~({left} | {right})")
    elif op in ("sll", "srl", "sra"):
        symbol = "<<" if op == "sll" else ">>"
        machine.set(operands[0], binary(symbol, get(operands[1]), int(operands[2], 0)))
    elif op in ("move",):
        machine.set(operands[0], get(operands[1]))
    elif op == "lw":
        machine.set(operands[0], machine.load(operands[1]))
    elif op == "sw":
        machine.store(operands[1], get(operands[0]), pc)
    elif op in ("jal", "jalr"):
        machine.call_count += 1
        name = operands[0] if operands else "?"
        machine.set("$v0", f"{name}()#{machine.call_count}")
    elif op[0] in "lm" and operands:
        # Other loads and moves give values this doesn't follow
        machine.set(operands[0], f"{op}#{pc}")


def decode(words):
    """Returns gfxdis's macros for a list of 32-bit words, or None."""
    text = [f"{word:08X}" for word in words]
    try:
        result = subprocess.run([GFXDIS, "-p", "-w", *text], capture_output=True, text=True)
    except FileNotFoundError:
        return None
    return result.stdout.strip()


def main():
    machine = Machine()
    with open(sys.argv[1]) as asm_file:
        for line in asm_file:
            match = LINE_RE.match(line)
            if match:
                step(machine, match.group(1), match.group(2), match.group(3))

    by_offset = {}
    for offset, value, pc in machine.stores:
        by_offset.setdefault(offset, []).append((value, pc))
    for command in sorted({offset & ~7 for offset in by_offset}):
        words = [by_offset.get(command + part, [("?", "")]) for part in (0, 4)]
        constant = all(len(w) == 1 and isinstance(w[0][0], int) for w in words)
        if constant:
            macro = decode([words[0][0][0], words[1][0][0]])
            print(f"{command:#06x} {macro}")
        else:
            print(f"{command:#06x}")
            for part, values in zip(("w0", "w1"), words):
                for value, pc in values:
                    shown = f"{value:08X}" if isinstance(value, int) else value
                    print(f"         {part}={shown}  @{pc}")


if __name__ == "__main__":
    main()
