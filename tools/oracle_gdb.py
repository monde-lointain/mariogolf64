"""A gdb script that watches KMC GCC's register allocators.

    gdb -batch -nx -x oracle_gdb.py --args cc1.debug ARGUMENTS...

Runs the debug build of cc1 that oracle.py's compiler directory puts in
cc1's place. For the function ORACLE_FUNCTION it writes to the file
ORACLE_TRACE one JSON object per line: each block's quantities from
local-alloc, then the allocnos of global-alloc and every pseudo's register
as reload starts, before the build applies any ORACLE_FORCE.

The other functions compile untouched. cc1's exit code is passed on.
"""

import json
import os
from pathlib import Path

import gdb  # pyright: ignore[reportMissingModuleSource]

FUNCTION = os.environ.get("ORACLE_FUNCTION", "")
TRACE_PATH = os.environ["ORACLE_TRACE"]
# From config/mips/mips.h
FIRST_PSEUDO_REGISTER = 68
# Where block_alloc has given every quantity its register
BLOCK_ALLOC_DONE = "local-alloc.c:1541"
HARD_REG_SET_BITS = 32


def value(expression: str) -> gdb.Value:
    """Evaluate EXPRESSION in cc1's current frame."""
    return gdb.parse_and_eval(expression)


def is_watched() -> bool:
    """Whether cc1 is compiling the function named by ORACLE_FUNCTION."""
    name = value("current_function_name")
    return bool(name) and name.string() == FUNCTION


def trace(record: dict[str, object]) -> None:
    """Append RECORD to the trace file."""
    with Path(TRACE_PATH).open("a") as file:
        _ = file.write(json.dumps(record) + "\n")


def read_ints(array: str, count: int) -> list[int]:
    """Return the first COUNT elements of the C array ARRAY."""
    pointer = value(array)
    return [int(pointer[i]) for i in range(count)]


def read_hard_reg_set(expression: str) -> list[int]:
    """Return the registers in the HARD_REG_SET EXPRESSION."""
    words = value(expression)
    registers: list[int] = []
    for word_index in range(words.type.sizeof // words[0].type.sizeof):
        word = int(words[word_index]) & 0xFFFFFFFF
        for bit in range(HARD_REG_SET_BITS):
            register = word_index * HARD_REG_SET_BITS + bit
            if word & (1 << bit) and register < FIRST_PSEUDO_REGISTER:
                registers.append(register)
    return registers


def get_priority(refs: int, size: int, length: int) -> int:
    """Return the priority qty_compare and allocno_compare give a register.

    That is from its REFS, SIZE and live LENGTH.
    """
    if length <= 0 or refs <= 0:
        return 0
    return int((refs.bit_length() - 1) * refs * size / length * 10000)


class BlockAllocDone(gdb.Breakpoint):
    """Records a block's quantities once local-alloc has placed them."""

    def stop(self) -> bool:  # pyright: ignore[reportImplicitOverride]
        """Record the watched function's state and carry on running."""
        if not is_watched():
            return False
        count = int(value("'local-alloc.c'::next_qty"))
        order = read_ints("qty_order", count)
        quantities: list[dict[str, object]] = []
        for quantity in order:
            pseudos: list[int] = []
            pseudo = int(value(f"'local-alloc.c'::qty_first_reg[{quantity}]"))
            while pseudo >= 0:
                pseudos.append(pseudo)
                pseudo = int(value(f"'local-alloc.c'::reg_next_in_qty[{pseudo}]"))
            fields = {
                name: int(value(f"'local-alloc.c'::qty_{name}[{quantity}]"))
                for name in ("n_refs", "birth", "death", "size", "phys_reg")
            }
            fields["n_sugg"] = int(
                value(f"'local-alloc.c'::qty_phys_num_sugg[{quantity}]")
            )
            fields["n_copy_sugg"] = int(
                value(f"'local-alloc.c'::qty_phys_num_copy_sugg[{quantity}]")
            )
            fields["priority"] = get_priority(
                fields["n_refs"], fields["size"], fields["death"] - fields["birth"]
            )
            quantities.append({"qty": quantity, "pseudos": pseudos, **fields})
        trace({"stage": "local", "block": int(value("b")), "quantities": quantities})
        return False


class ReloadStart(gdb.Breakpoint):
    """Records global-alloc's allocnos and every pseudo's register."""

    def stop(self) -> bool:  # pyright: ignore[reportImplicitOverride]
        """Record the watched function's state and carry on running."""
        if not is_watched():
            return False
        trace_global()
        trace_dispositions()
        return False


def trace_global() -> None:
    """Record each allocno of global-alloc, in the order it was allocated."""
    count = int(value("'global.c'::max_allocno"))
    if count == 0:
        return
    row_words = int(value("'global.c'::allocno_row_words"))
    conflict_words = read_ints("'global.c'::conflicts", count * row_words)
    allocnos: list[dict[str, object]] = []
    for allocno in read_ints("'global.c'::allocno_order", count):
        pseudo = int(value(f"'global.c'::allocno_reg[{allocno}]"))
        refs = int(value(f"'global.c'::allocno_n_refs[{allocno}]"))
        size = int(value(f"'global.c'::allocno_size[{allocno}]"))
        length = int(value(f"'global.c'::allocno_live_length[{allocno}]"))
        row = conflict_words[allocno * row_words : (allocno + 1) * row_words]
        conflicting = [
            int(value(f"'global.c'::allocno_reg[{other}]"))
            for other in range(count)
            if other != allocno
            and row[other // HARD_REG_SET_BITS] & (1 << (other % HARD_REG_SET_BITS))
        ]
        allocnos.append(
            {
                "pseudo": pseudo,
                "n_refs": refs,
                "size": size,
                "live_length": length,
                "priority": get_priority(refs, size, length),
                "calls_crossed": int(
                    value(f"'global.c'::allocno_calls_crossed[{allocno}]")
                ),
                "conflicts": conflicting,
                "hard_conflicts": read_hard_reg_set(
                    f"'global.c'::hard_reg_conflicts[{allocno}]"
                ),
                "preferences": read_hard_reg_set(
                    f"'global.c'::hard_reg_preferences[{allocno}]"
                ),
                "copy_preferences": read_hard_reg_set(
                    f"'global.c'::hard_reg_copy_preferences[{allocno}]"
                ),
                "hard": int(value(f"reg_renumber[{pseudo}]")),
            }
        )
    trace({"stage": "global", "allocnos": allocnos})


def trace_dispositions() -> None:
    """Record the register each pseudo goes into reload with."""
    pseudo_count = int(value("max_regno"))
    hard = read_ints("reg_renumber", pseudo_count)
    trace(
        {
            "stage": "dispositions",
            "registers": {
                str(pseudo): hard[pseudo]
                for pseudo in range(FIRST_PSEUDO_REGISTER, pseudo_count)
            },
        }
    )


def main() -> None:
    """Run cc1 under the breakpoints and exit with its code."""
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    _ = BlockAllocDone(BLOCK_ALLOC_DONE, internal=True)
    _ = ReloadStart("reload", internal=True)
    _ = gdb.execute("run", to_string=True)
    exit_code = value("$_exitcode")
    gdb.execute(
        f"quit {int(exit_code) if exit_code.type.code != gdb.TYPE_CODE_VOID else 1}"
    )


main()
