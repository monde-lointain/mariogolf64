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
from pathlib import Path

GFXDIS = "gfxdis.f3dex2"
MASK = 0xFFFFFFFF
HALF_SHIFT = 16
UPPER_HALF_MASK = 0xFFFF0000
LOWER_HALF_MASK = 0xFFFF
WORD_SIZE = 4
COMMAND_SIZE = 8
COMMAND_ALIGNMENT_MASK = ~(COMMAND_SIZE - 1)
ARGUMENT_REGISTER_COUNT = 4
# Where the display list and the stack are taken to be
GFX_BASE = 0x40000000
GFX_LIMIT = 0x50000000
STACK_BASE = 0x7F000000
# The operand through which the display list's pointer is loaded
GFX_POINTER_ADDRESS = "*(arg0 + 0x0)"
LINE_RE = re.compile(r"^\s*/\* \w+ ([0-9A-F]{8}) [0-9A-F]+ \*/\s+(\S+)\s*(.*)$")
MEM_RE = re.compile(r"^(.*)\((\$\w+)\)$")
HI_RE = re.compile(r"^%hi\((.+)\)$")
LO_RE = re.compile(r"^%lo\((.+)\)$")
SHIFTED_HI_RE = re.compile(r"^\((0x[0-9A-Fa-f]+) >> 16\)$")
MASKED_LO_RE = re.compile(r"^\((0x[0-9A-Fa-f]+) & 0xFFFF\)$")
IMMEDIATE_SYMBOLS = {"addiu": "+", "ori": "|", "andi": "&", "xori": "^"}
REGISTER_SYMBOLS = {"addu": "+", "subu": "-", "or": "|", "and": "&", "xor": "^"}
SHIFT_OPS = ("sll", "srl", "sra")
ARITHMETIC_OPS = {*IMMEDIATE_SYMBOLS, *REGISTER_SYMBOLS, "nor", *SHIFT_OPS}

# A constant, or the expression that computes a value
Value = int | str
# A word stored in the display list, its offset there and the store's address
Store = tuple[int, Value, str]


def parse_int(text: str) -> int:
    """Return a number as a 32-bit word."""
    return int(text, 0) & MASK


def show(value: Value) -> str:
    """Return a value as text, constants in hex."""
    return hex(value) if isinstance(value, int) else value


class Machine:
    """Holds what is known about each register and memory word."""

    def __init__(self) -> None:
        """Start with only the stack and argument registers known."""
        self.regs: dict[str, Value] = {"$zero": 0, "$sp": STACK_BASE}
        for index in range(ARGUMENT_REGISTER_COUNT):
            self.regs[f"$a{index}"] = f"arg{index}"
        self.memory: dict[int, Value] = {}
        self.stores: list[Store] = []
        self.call_count: int = 0

    def get(self, reg: str) -> Value:
        """Return what a register holds."""
        return self.regs.get(reg, reg.lstrip("$"))

    def set(self, reg: str, value: Value) -> None:
        """Record what a register holds; $zero stays zero."""
        if reg != "$zero":
            self.regs[reg] = value

    def address(self, operand: str) -> Value:
        """Return the address an `offset(base)` operand names."""
        match = MEM_RE.match(operand)
        if not match:
            message = f"{operand} isn't a memory operand"
            raise ValueError(message)
        offset_text, base = match.group(1), match.group(2)
        base_value = self.get(base)
        lo = LO_RE.match(offset_text)
        if lo:
            return f"{lo.group(1)}"
        offset = int(offset_text, 0) if offset_text else 0
        if isinstance(base_value, int):
            return (base_value + offset) & MASK
        return f"*({base_value} + {offset:#x})"

    def load(self, operand: str) -> Value:
        """Return what a load from an operand gives."""
        address = self.address(operand)
        if address == GFX_POINTER_ADDRESS:
            return GFX_BASE
        if isinstance(address, int):
            return self.memory.get(address, f"[{address:#x}]")
        return address

    def store(self, operand: str, value: Value, pc: str) -> None:
        """Record a store, and any word it puts in the display list."""
        address = self.address(operand)
        if isinstance(address, int):
            self.memory[address] = value
            if GFX_BASE <= address < GFX_LIMIT:
                self.stores.append((address - GFX_BASE, value, pc))


def fold(op: str, left: int, right: int) -> int:
    """Return two constants combined."""
    results = {
        "+": lambda: (left + right) & MASK,
        "-": lambda: (left - right) & MASK,
        "|": lambda: left | right,
        "&": lambda: left & right,
        "^": lambda: left ^ right,
        "<<": lambda: (left << right) & MASK,
        ">>": lambda: left >> right,
    }
    return results[op]()


def binary(op: str, left: Value, right: Value) -> Value:
    """Combine two values, folding them when both are constants."""
    if isinstance(left, int) and isinstance(right, int):
        return fold(op, left, right)
    if op in "+|" and right == 0:
        return left
    if op in "+|" and left == 0:
        return right
    return f"({show(left)} {op} {show(right)})"


def read_immediate(operand: str) -> int:
    """Return an immediate operand, unmasking splat's %lo-style constants."""
    masked = MASKED_LO_RE.match(operand)
    if masked:
        return int(masked.group(1), 16) & LOWER_HALF_MASK
    return int(operand, 0)


def step_lui(machine: Machine, operands: list[str]) -> None:
    """Apply a lui."""
    hi = HI_RE.match(operands[1])
    shifted = SHIFTED_HI_RE.match(operands[1])
    if hi:
        machine.set(operands[0], hi.group(1))
    elif shifted:
        machine.set(operands[0], parse_int(shifted.group(1)) & UPPER_HALF_MASK)
    else:
        machine.set(operands[0], (parse_int(operands[1]) << HALF_SHIFT) & MASK)


def step_arithmetic(machine: Machine, op: str, operands: list[str]) -> None:
    """Apply an arithmetic, logic or shift instruction."""
    get = machine.get
    if op in IMMEDIATE_SYMBOLS:
        lo = LO_RE.match(operands[2])
        if lo:
            machine.set(operands[0], f"&{lo.group(1)}")
            return
        immediate = read_immediate(operands[2])
        if op == "addiu":
            immediate &= MASK
        machine.set(
            operands[0], binary(IMMEDIATE_SYMBOLS[op], get(operands[1]), immediate)
        )
    elif op in REGISTER_SYMBOLS:
        machine.set(
            operands[0],
            binary(REGISTER_SYMBOLS[op], get(operands[1]), get(operands[2])),
        )
    elif op == "nor":
        left, right = get(operands[1]), get(operands[2])
        if isinstance(left, int) and isinstance(right, int):
            machine.set(operands[0], ~(left | right) & MASK)
        else:
            machine.set(operands[0], f"~({left} | {right})")
    else:
        symbol = "<<" if op == "sll" else ">>"
        machine.set(operands[0], binary(symbol, get(operands[1]), int(operands[2], 0)))


def step(machine: Machine, pc: str, op: str, args: str) -> None:
    """Apply one instruction to MACHINE."""
    operands = [arg.strip() for arg in args.split(",")] if args else []
    if op == "lui":
        step_lui(machine, operands)
    elif op in ARITHMETIC_OPS:
        step_arithmetic(machine, op, operands)
    elif op == "move":
        machine.set(operands[0], machine.get(operands[1]))
    elif op == "lw":
        machine.set(operands[0], machine.load(operands[1]))
    elif op == "sw":
        machine.store(operands[1], machine.get(operands[0]), pc)
    elif op in ("jal", "jalr"):
        machine.call_count += 1
        name = operands[0] if operands else "?"
        machine.set("$v0", f"{name}()#{machine.call_count}")
    elif op[0] in "lm" and operands:
        # Other loads and moves give values this doesn't follow
        machine.set(operands[0], f"{op}#{pc}")


def decode(words: list[int]) -> str | None:
    """Return gfxdis's macros for a list of 32-bit words, or None."""
    text = [f"{word:08X}" for word in words]
    try:
        result = subprocess.run(
            [GFXDIS, "-p", "-w", *text], capture_output=True, text=True, check=False
        )
    except FileNotFoundError:
        return None
    return result.stdout.strip()


def print_commands(stores: list[Store]) -> None:
    """Print each display-list command with the words stored in it."""
    by_offset: dict[int, list[tuple[Value, str]]] = {}
    for offset, value, pc in stores:
        by_offset.setdefault(offset, []).append((value, pc))
    for command in sorted({offset & COMMAND_ALIGNMENT_MASK for offset in by_offset}):
        words = [by_offset.get(command + part, [("?", "")]) for part in (0, WORD_SIZE)]
        constants = [
            value
            for word in words
            if len(word) == 1 and isinstance(value := word[0][0], int)
        ]
        if len(constants) == len(words):
            macro = decode(constants)
            print(f"{command:#06x} {macro}")
        else:
            print(f"{command:#06x}")
            for part, values in zip(("w0", "w1"), words, strict=True):
                for value, pc in values:
                    shown = f"{value:08X}" if isinstance(value, int) else value
                    print(f"         {part}={shown}  @{pc}")


def main() -> None:
    """Print the display list that the function named builds."""
    machine = Machine()
    for line in Path(sys.argv[1]).read_text().splitlines():
        match = LINE_RE.match(line)
        if match:
            step(machine, match.group(1), match.group(2), match.group(3))
    print_commands(machine.stores)


if __name__ == "__main__":
    main()
