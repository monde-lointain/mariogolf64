#!/usr/bin/env python3
"""Find the register allocation that would make a draft match, and why.

    oracle.py [--allocation | --splits] [ASM_FILE C_FILE] FUNCTION
              [PSEUDO=HARD ...]

Without ASM_FILE and C_FILE, finds them from FUNCTION as fncompare does.

Compiles FUNCTION with the debug build of cc1 that tools/build_oracle.sh
makes, under gdb (see oracle_gdb.py). The instructions that equal the
target's but for their registers name, through the insns that made them,
the pseudo register each target register should hold. Those pseudos are
forced into those registers as reload starts, and the compile repeated
until it matches or stops improving. A match proves that the draft's RTL is
the original's up to register allocation, and the forced pseudos are then
cut down to the fewest that still match. PSEUDO=HARD pairs, hard registers
given by number or name, start the forcing off.

For each forced pseudo, prints what the allocator knew of it: its quantity
in local-alloc or its allocno in global-alloc, with its priority, and the
pseudos that took the register it needed. A draft whose code differs
otherwise gets the closest forcing found, and the instructions that still
differ under it, which register allocation does not explain. Last, it
names each pseudo that lines up with two or more target registers, a sign
that the original used a variable for each place; --splits prints only
that, from one compile.

Only FUNCTION's own routine is compared; the functions nested in it, which
fncompare counts with it, are named so that each can be given as FUNCTION.
"""

import collections
import difflib
import json
import os
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, TypedDict, cast

import blame

import fncompare

ORACLE_DIR = fncompare.ROOT / "build" / "oracle"
MAX_ROUNDS = 8
# The instructions a register needs from a pseudo to count as holding it
MIN_SPLIT_VOTES = 2
FIRST_PSEUDO_REGISTER = blame.FIRST_PSEUDO_REGISTER
FPR_BASE = len(blame.GPR_NAMES)
FIELD_MASK = 0x1F
OPCODE_SHIFT = 26
# Opcodes whose register fields this reads differently from the I-type's
SPECIAL, REGIMM, JUMP, JUMP_AND_LINK, COP1 = 0, 1, 2, 3, 0x11
FPR_MEMORY_OPCODES = {0x31, 0x35, 0x39, 0x3D}  # lwc1 ldc1 swc1 sdc1
# The COP1 formats that move between a GPR and an FPR, and that branch
COP1_MOVE_FORMATS = {0, 2, 4, 6}
COP1_BRANCH_FORMAT = 8


class Quantity(TypedDict):
    """Pseudos local-alloc tied into one register in a block, as traced."""

    qty: int
    pseudos: list[int]
    n_refs: int
    birth: int
    death: int
    size: int
    phys_reg: int
    n_sugg: int
    n_copy_sugg: int
    priority: int


class Allocno(TypedDict):
    """A pseudo that global-alloc placed, as traced."""

    pseudo: int
    n_refs: int
    size: int
    live_length: int
    priority: int
    calls_crossed: int
    conflicts: list[int]
    hard_conflicts: list[int]
    preferences: list[int]
    copy_preferences: list[int]
    hard: int


class LocalRecord(TypedDict):
    """The quantities of one block, in the order local-alloc placed them."""

    stage: Literal["local"]
    block: int
    quantities: list[Quantity]


class GlobalRecord(TypedDict):
    """The allocnos, in the order global-alloc placed them."""

    stage: Literal["global"]
    allocnos: list[Allocno]


class DispositionsRecord(TypedDict):
    """{pseudo: the hard register it went into reload with}."""

    stage: Literal["dispositions"]
    registers: dict[str, int]


TraceRecord = LocalRecord | GlobalRecord | DispositionsRecord


@dataclass
class Compilation:
    """One compile of the function: its code, insns and allocators' trace."""

    blamed: blame.Compilation
    blocks: list[LocalRecord]
    allocnos: list[Allocno]
    registers: dict[int, int]

    @property
    def routine(self) -> fncompare.Routine:
        """The compiled function."""
        return self.blamed.routine


def get_field_layout(word: int) -> list[tuple[int, int]]:
    """Return (shift, register number base) of each register field of WORD."""
    opcode = word >> OPCODE_SHIFT
    cop1_format = (word >> 21) & FIELD_MASK
    if opcode == SPECIAL:
        layout = [(21, 0), (16, 0), (11, 0)]
    elif opcode == REGIMM:
        layout = [(21, 0)]
    elif opcode in (JUMP, JUMP_AND_LINK) or (
        opcode == COP1 and cop1_format == COP1_BRANCH_FORMAT
    ):
        layout = []
    elif opcode == COP1 and cop1_format in COP1_MOVE_FORMATS:
        layout = [(16, 0), (11, FPR_BASE)]
    elif opcode == COP1:
        layout = [(16, FPR_BASE), (11, FPR_BASE), (6, FPR_BASE)]
    elif opcode in FPR_MEMORY_OPCODES:
        layout = [(21, 0), (16, FPR_BASE)]
    else:
        layout = [(21, 0), (16, 0)]
    return layout


def read_register_fields(word: int) -> list[tuple[int, int]]:
    """Return (shift, GCC register number) of each register field of WORD."""
    return [
        (shift, base + ((word >> shift) & FIELD_MASK))
        for shift, base in get_field_layout(word)
    ]


def mask_registers(word: int) -> int:
    """Return WORD with its register fields cleared."""
    for shift, _ in read_register_fields(word):
        word &= ~(FIELD_MASK << shift)
    return word


def parse_register(text: str) -> int:
    """Return the GCC number of a register given by number or by name."""
    if text.isdigit():
        return int(text)
    name = text.lstrip("$")
    if name.startswith("f") and name[1:].isdigit():
        return FPR_BASE + int(name[1:])
    return blame.GPR_NAMES.index(name)


def compile_function(
    c_path: str, function: str, force: dict[int, int], work: Path
) -> Compilation:
    """Compile FUNCTION with FORCE applied, tracing the allocators."""
    trace_path = work / "trace.jsonl"
    trace_path.unlink(missing_ok=True)
    os.environ.update(
        ORACLE_FUNCTION=function,
        ORACLE_FORCE=",".join(f"{pseudo}={hard}" for pseudo, hard in force.items()),
        ORACLE_TRACE=str(trace_path),
    )
    optflags = fncompare.get_optflags(c_path)
    result = fncompare.run_compiler(c_path, optflags, ["-E"])
    isolated = fncompare.isolate_functions(result.stdout, {function})
    if not re.search(rf"\b{function}\(", isolated):
        isolated = result.stdout
    _ = (work / "unit.i").write_text(isolated)
    blame.run(work / "unit.i", optflags, ["-S", "-dp", "-da", "-o", "unit.s"], work)
    del os.environ["ORACLE_FUNCTION"]
    lines = (work / "unit.s").read_text().splitlines()
    _ = (work / "marked.s").write_text(blame.mark_lines(lines))
    assemble = ["-c", f"-Wa,-I{fncompare.ROOT}", "-o"]
    blame.run(work / "unit.s", optflags, [*assemble, "unit.o"], work)
    blame.run(work / "marked.s", optflags, [*assemble, "marked.o"], work)
    routine = fncompare.read_object_file(work / "unit.o")[function]
    start, ends = blame.read_line_ends(work / "marked.o", function)
    blamed = blame.Compilation(
        lines,
        blame.read_word_lines(lines, routine, start, ends),
        routine,
        blame.read_dumps(work, function),
        {},
    )
    compilation = Compilation(blamed, [], [], {})
    for line in trace_path.read_text().splitlines():
        record = cast("TraceRecord", json.loads(line))
        if record["stage"] == "local":
            compilation.blocks.append(record)
        elif record["stage"] == "global":
            compilation.allocnos = record["allocnos"]
        else:
            compilation.registers = {
                int(key): hard for key, hard in record["registers"].items()
            }
    return compilation


def read_pseudo_lines(
    c_path: str, function: str, compilation: Compilation, work: Path
) -> dict[int, set[int]]:
    """Return {pseudo: the source lines of the insns using it before reload}.

    WORK must hold the unit.i that COMPILATION was compiled from.
    """
    lreg = compilation.blamed.dumps.get("lreg", "")
    insn_lines = blame.read_insn_lines(
        fncompare.get_optflags(c_path), {"lreg": lreg}, work, function
    ).get("lreg", {})
    lines: dict[int, set[int]] = collections.defaultdict(set)
    for header in blame.INSN_HEADER_RE.finditer(lreg):
        uid = int(header.group(1))
        rtl = blame.find_insn(lreg, uid)
        for match in blame.PSEUDO_RE.finditer(rtl):
            if uid in insn_lines:
                lines[int(match.group(1))].add(int(insn_lines[uid].split(":")[1]))
    return lines


def read_insn_pseudos(compilation: Compilation, index: int) -> set[int]:
    """Return the pseudos that the insn making word INDEX uses before reload."""
    insn = blame.get_insn(compilation.blamed, index)
    if insn is None:
        return set()
    rtl = blame.find_insn(compilation.blamed.dumps.get("lreg", ""), insn[0])
    numbers = {int(match.group(1)) for match in blame.PSEUDO_RE.finditer(rtl)}
    return {number for number in numbers if number >= FIRST_PSEUDO_REGISTER}


def get_register_count(compilation: Compilation, pseudo: int) -> int:
    """Return how many hard registers PSEUDO takes, from its mode's name."""
    lreg = compilation.blamed.dumps.get("lreg", "")
    match = re.search(rf"\(reg(?:/\w+)?:(\w+) {pseudo}\)", lreg)
    return 2 if match and match.group(1) in ("DF", "DI", "DC") else 1


def vote_registers(
    target: fncompare.Routine, compilation: Compilation
) -> dict[int, collections.Counter[int]]:
    """Return {pseudo: Counter of the target registers it lines up with}.

    The instructions are paired by their words without their registers, and
    each pair's differing register fields vote for the pseudo, in the insn
    that made our instruction, that holds our register.
    """
    compiled = compilation.routine
    registers = compilation.registers
    matcher = difflib.SequenceMatcher(
        None,
        [mask_registers(word) for word in target.words],
        [mask_registers(word) for word in compiled.words],
        autojunk=False,
    )
    votes: dict[int, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    for target_start, compiled_start, length in matcher.get_matching_blocks():
        for offset in range(length):
            target_index = target_start + offset
            compiled_index = compiled_start + offset
            pseudos = read_insn_pseudos(compilation, compiled_index)
            for (_, wanted), (_, ours) in zip(
                read_register_fields(target.words[target_index]),
                read_register_fields(compiled.words[compiled_index]),
                strict=True,
            ):
                holders = [
                    pseudo
                    for pseudo in pseudos
                    if registers.get(pseudo, -1)
                    <= ours
                    < registers.get(pseudo, -1)
                    + get_register_count(compilation, pseudo)
                    and registers.get(pseudo, -1) >= 0
                ]
                if len(holders) == 1:
                    pseudo = holders[0]
                    votes[pseudo][wanted - (ours - registers[pseudo])] += 1
    return votes


def count_differences(target: fncompare.Routine, compilation: Compilation) -> int:
    """Return how many instructions differ between TARGET and COMPILATION."""
    return fncompare.count_differences(target, compilation.routine)


def search_forcing(
    target: fncompare.Routine,
    c_path: str,
    function: str,
    force: dict[int, int],
    work: Path,
) -> tuple[dict[int, int], int, Compilation]:
    """Return the forcing that brings the draft closest to TARGET.

    Each round adds to the last round's forcing every pseudo whose votes
    favour another register than the one it has, until a forcing repeats.
    Forcing one pseudo can move others, so a round may do worse than the
    last and still lead to a better one.
    """
    compilation = compile_function(c_path, function, force, work)
    best = (dict(force), count_differences(target, compilation), compilation)
    print(f"  unforced: {best[1]} differ")
    tried = [dict(force)]
    for round_number in range(1, MAX_ROUNDS + 1):
        if best[1] == 0:
            break
        registers = compilation.registers
        proposal = dict(tried[-1])
        for pseudo, counter in vote_registers(target, compilation).items():
            wanted, _ = counter.most_common(1)[0]
            if wanted != registers.get(pseudo):
                proposal[pseudo] = wanted
        if proposal in tried:
            break
        tried.append(proposal)
        compilation = compile_function(c_path, function, proposal, work)
        differences = count_differences(target, compilation)
        print(f"  round {round_number}: {len(proposal)} forced, {differences} differ")
        if differences < best[1]:
            best = (proposal, differences, compilation)
    return best


def minimize_forcing(
    target: fncompare.Routine,
    c_path: str,
    function: str,
    force: dict[int, int],
    work: Path,
) -> dict[int, int]:
    """Return the fewest of FORCE's pairs that still make the draft match."""
    kept = dict(force)
    for pseudo in list(force):
        trial = {key: hard for key, hard in kept.items() if key != pseudo}
        if (
            count_differences(target, compile_function(c_path, function, trial, work))
            == 0
        ):
            kept = trial
    return kept


def describe_quantity(quantity: Quantity, block: int) -> str:
    """Return what local-alloc knew of QUANTITY of BLOCK."""
    tied = "+".join(map(str, quantity["pseudos"]))
    register = register_name(quantity["phys_reg"])
    heading = f"local qty {quantity['qty']} of block {block} [{tied}] in {register}"
    parts = [
        f"{heading}: {quantity['n_refs']} refs",
        f"life {quantity['birth']}-{quantity['death']}",
        f"priority {quantity['priority']}",
    ]
    if quantity["n_sugg"] or quantity["n_copy_sugg"]:
        parts.append("suggested")
    return ", ".join(parts)


def describe_allocno(allocno: Allocno, order: int) -> str:
    """Return what global-alloc knew of ALLOCNO, the ORDERth allocated."""
    register = register_name(allocno["hard"])
    parts = [
        ", ".join(
            [
                f"global #{order} in {register}: {allocno['n_refs']} refs",
                f"live {allocno['live_length']}",
                f"priority {allocno['priority']}",
                f"crosses {allocno['calls_crossed']} calls",
            ]
        )
    ]
    if allocno["preferences"]:
        names = " ".join(map(register_name, allocno["preferences"]))
        parts.append(f"prefers {names}")
    if allocno["copy_preferences"]:
        names = " ".join(map(register_name, allocno["copy_preferences"]))
        parts.append(f"copy-prefers {names}")
    return "; ".join(parts)


def register_name(number: int) -> str:
    """Return the name of hard register NUMBER, or "memory" if none."""
    return blame.register_name(number) if number >= 0 else "memory"


QuantityIndex = dict[int, tuple[LocalRecord, Quantity]]
AllocnoIndex = dict[int, tuple[int, Allocno]]


def index_quantities(compilation: Compilation) -> QuantityIndex:
    """Return {pseudo: (its block's record, its local quantity)}."""
    return {
        pseudo: (block, quantity)
        for block in compilation.blocks
        for quantity in block["quantities"]
        for pseudo in quantity["pseudos"]
    }


def index_allocnos(compilation: Compilation) -> AllocnoIndex:
    """Return {pseudo: (its place in the allocation order, its allocno)}."""
    return {
        allocno["pseudo"]: (order, allocno)
        for order, allocno in enumerate(compilation.allocnos)
    }


def explain_quantity(pseudo: int, wanted: int, quantities: QuantityIndex) -> None:
    """Print PSEUDO's quantity and those that took WANTED while it lived."""
    block, quantity = quantities[pseudo]
    print(f"    {describe_quantity(quantity, block['block'])}")
    for other in block["quantities"]:
        overlaps = (
            other["birth"] < quantity["death"] and quantity["birth"] < other["death"]
        )
        if other is not quantity and other["phys_reg"] == wanted and overlaps:
            print(f"    taken by {describe_quantity(other, block['block'])}")


def explain_allocno(
    pseudo: int, wanted: int, compilation: Compilation, quantities: QuantityIndex
) -> None:
    """Print PSEUDO's allocno and the conflicting pseudos that took WANTED."""
    registers = compilation.registers
    allocnos = index_allocnos(compilation)
    order, allocno = allocnos[pseudo]
    print(f"    {describe_allocno(allocno, order)}")
    for other in allocno["conflicts"]:
        if registers.get(other) != wanted:
            continue
        if other in allocnos:
            other_order, other_allocno = allocnos[other]
            description = describe_allocno(other_allocno, other_order)
        elif other in quantities:
            block, quantity = quantities[other]
            description = describe_quantity(quantity, block["block"])
        else:
            continue
        print(f"    taken by pseudo {other}, {description}")
    if wanted in allocno["hard_conflicts"]:
        print(f"    conflicts with {register_name(wanted)} itself")


def explain(
    compilation: Compilation, force: dict[int, int], lines: dict[int, set[int]]
) -> None:
    """Print what the allocators knew of each forced pseudo.

    Also prints the pseudos that took its wanted register while it was live.
    """
    registers = compilation.registers
    quantities = index_quantities(compilation)
    allocnos = index_allocnos(compilation)
    for pseudo, wanted in sorted(force.items()):
        moved = f"{register_name(registers.get(pseudo, -1))} -> {register_name(wanted)}"
        used = join_lines([pseudo], lines)
        print(f"pseudo {pseudo}: {moved}, used at lines {used}")
        if pseudo in quantities:
            explain_quantity(pseudo, wanted, quantities)
        elif pseudo in allocnos:
            explain_allocno(pseudo, wanted, compilation, quantities)
        else:
            print("    not allocated by either pass")


def print_splits(
    target: fncompare.Routine, compilation: Compilation, lines: dict[int, set[int]]
) -> None:
    """Print the pseudos whose instructions line up with more than one register.

    GCC gives a variable one pseudo, and global-alloc one register, so a
    pseudo the original holds in two registers in different places was
    likely two variables there, such as a flag declared in each of two loops.
    """
    for pseudo, votes in sorted(vote_registers(target, compilation).items()):
        registers = [
            (register, count)
            for register, count in votes.most_common()
            if count >= MIN_SPLIT_VOTES
        ]
        if len(registers) <= 1:
            continue
        held = ", ".join(
            f"{register_name(register)} x{count}" for register, count in registers
        )
        used = join_lines([pseudo], lines)
        guess = f"maybe two variables, lines {used}"
        print(f"pseudo {pseudo} lines up with {held}; {guess}")


def join_lines(pseudos: list[int], lines: dict[int, set[int]]) -> str:
    """Return the source lines that use any of PSEUDOS, in order."""
    used: set[int] = set()
    for pseudo in pseudos:
        used |= lines.get(pseudo, set())
    return " ".join(map(str, sorted(used)))


def print_allocation(compilation: Compilation, lines: dict[int, set[int]]) -> None:
    """Print every quantity and allocno in the order they were allocated."""
    for block in compilation.blocks:
        for quantity in block["quantities"]:
            used = join_lines(quantity["pseudos"], lines)
            print(f"  {describe_quantity(quantity, block['block'])}; lines {used}")
    for order, allocno in enumerate(compilation.allocnos):
        used = join_lines([allocno["pseudo"]], lines)
        description = describe_allocno(allocno, order)
        print(f"  pseudo {allocno['pseudo']} {description}; lines {used}")


def main() -> None:
    """Search for and explain the allocation that matches the function."""
    arguments = sys.argv[1:]
    show_allocation = "--allocation" in arguments
    if show_allocation:
        arguments.remove("--allocation")
    show_splits = "--splits" in arguments
    if show_splits:
        arguments.remove("--splits")
    asm_path, c_path, function, *pairs = fncompare.expand_function_name(arguments)
    if not (ORACLE_DIR / "cc1.debug").exists():
        sys.exit("run tools/build_oracle.sh first")
    os.environ["KMC_DIR"] = str(ORACLE_DIR)
    fncompare.KMC_DIR = ORACLE_DIR
    force = {
        int(pseudo): parse_register(hard)
        for pseudo, hard in (pair.split("=") for pair in pairs)
    }
    original = fncompare.read_original(asm_path, c_path, function)
    target = original.routine
    print(function)
    if original.nested:
        print(f"  nested, not compared: {' '.join(original.nested)}")
    with tempfile.TemporaryDirectory() as work_name:
        work = Path(work_name)
        if show_allocation:
            compilation = compile_function(c_path, function, force, work)
            print(f"  {count_differences(target, compilation)} differ")
            print_allocation(
                compilation, read_pseudo_lines(c_path, function, compilation, work)
            )
            return
        if show_splits:
            compilation = compile_function(c_path, function, force, work)
            lines = read_pseudo_lines(c_path, function, compilation, work)
            print_splits(target, compilation, lines)
            return
        force, differences, compilation = search_forcing(
            target, c_path, function, force, work
        )
        if differences == 0 and force:
            force = minimize_forcing(target, c_path, function, force, work)
            compilation = compile_function(c_path, function, {}, work)
            print(f"MATCHES when {len(force)} pseudos are forced:")
        elif differences == 0:
            print("MATCHES unforced")
            return
        else:
            print(f"closest: {differences} differ when {len(force)} are forced")
            fncompare.print_diff(
                target,
                compilation.routine,
                lambda index: blame.blame(compilation.blamed, index),
            )
            compilation = compile_function(c_path, function, {}, work)
        noted_work = work / "noted"
        if noted_work.exists():
            shutil.rmtree(noted_work)
        lines = read_pseudo_lines(c_path, function, compilation, work)
        explain(compilation, force, lines)
        print_splits(target, compilation, lines)


if __name__ == "__main__":
    main()
