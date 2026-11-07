#!/usr/bin/env python3
"""Find the register allocation that would make a draft match, and why.

    oracle.py [--allocation | --splits | --spills] [ASM_FILE C_FILE] FUNCTION
              [PSEUDO=HARD ...]

Without ASM_FILE and C_FILE, finds them from FUNCTION as fncompare does.

Compiles FUNCTION once with the debug build of cc1 that
tools/build_oracle.sh makes, under gdb (see oracle_gdb.py), to trace the
allocators. The instructions that equal the target's but for their
registers name, through the insns that made them, the pseudo register each
target register should hold. Those pseudos are forced into those registers
as reload starts, by the optimized build, and the compile repeated until it
matches or stops improving. A match proves that the draft's RTL is
the original's up to register allocation, and the forced pseudos are then
cut down to the fewest that still match. PSEUDO=HARD pairs, hard registers
given by number or name, or "stack" for a pseudo the original spilled, start
the forcing off.

For each forced pseudo, prints what the allocator knew of it: its quantity
in local-alloc or its allocno in global-alloc, with its priority, and the
pseudos that took the register it needed. A draft whose code differs
otherwise gets the closest forcing found, and the instructions that still
differ under it, which register allocation does not explain. Last, it
names each pseudo that lines up with two or more target registers, a sign
that the original used a variable for each place; --splits prints only
that, from one compile.

--spills starts a search from each pseudo that global-alloc gave a
register forced onto the stack, for an original whose frame is larger, and
prints the closest.

Only FUNCTION's own routine is compared; the functions nested in it, which
fncompare counts with it, are named so that each can be given as FUNCTION.
"""

import collections
import difflib
import functools
import json
import os
import re
import shutil
import sys
import tempfile
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, TypedDict, cast

import blame

import fncompare

ORACLE_DIR = fncompare.ROOT / "build" / "oracle"
MAX_ROUNDS = 8
MODE_OPTIONS = ("--allocation", "--splits", "--spills")
# The closest spill searches that --spills prints
SPILL_REPORT_COUNT = 10
# The assembly, the C file and the function, before any forced pairs
MIN_ARGUMENT_COUNT = 3
# The instructions a register needs from a pseudo to count as holding it
MIN_SPLIT_VOTES = 2
FIRST_PSEUDO_REGISTER = blame.FIRST_PSEUDO_REGISTER
FPR_BASE = len(blame.GPR_NAMES)
# reg_renumber's value for a pseudo that has no hard register
NO_REGISTER = -1
FIELD_MASK = 0x1F
OPCODE_SHIFT = 26
# Opcodes whose register fields this reads differently from the I-type's
SPECIAL, REGIMM, JUMP, JUMP_AND_LINK, COP1 = 0, 1, 2, 3, 0x11
FPR_MEMORY_OPCODES = {0x31, 0x35, 0x39, 0x3D}  # lwc1 ldc1 swc1 sdc1
# The COP1 formats that move between a GPR and an FPR, and that branch
COP1_MOVE_FORMATS = {0, 2, 4, 6}
COP1_BRANCH_FORMAT = 8
# The modes whose values take two 32-bit registers
DOUBLE_MODES = frozenset(["DF", "DI", "DC"])
PSEUDO_MODE_RE = re.compile(r"\(reg(?:/\w+)?:(\w+) (\d+)\)")


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
    """Return the GCC number of a register given by number or by name.

    "stack" gives NO_REGISTER, which leaves the pseudo for reload to spill.
    """
    if text == "stack":
        return NO_REGISTER
    if text.isdigit():
        return int(text)
    name = text.lstrip("$")
    if name.startswith("f") and name[1:].isdigit():
        return FPR_BASE + int(name[1:])
    return blame.GPR_NAMES.index(name)


def trace_function(c_path: str, function: str, work: Path) -> Compilation:
    """Compile FUNCTION unforced under gdb, tracing the allocators.

    Leaves in WORK the unit.i that every forced compile of the function
    then compiles. The allocators run before reload, where the forcing is
    applied, so the trace and the dumps up to lreg hold for every forcing.
    """
    optflags = fncompare.get_optflags(c_path)
    result = fncompare.run_compiler(c_path, optflags, ["-E"])
    isolated = fncompare.isolate_functions(result.stdout, {function})
    if not re.search(rf"\b{function}\(", isolated):
        isolated = result.stdout
    _ = (work / "unit.i").write_text(isolated)
    trace_path = work / "trace.jsonl"
    trace_path.unlink(missing_ok=True)
    blamed = build_function(
        function, optflags, work, {"ORACLE_TRACE": str(trace_path)}, dumps=True
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


@dataclass
class Draft:
    """The function being compiled, and its traced, unforced compile."""

    c_path: str
    function: str
    # Holds the unit.i that trace_function left, and every compile's files
    work: Path
    traced: Compilation


def compile_function(
    draft: Draft, force: dict[int, int], *, dumps: bool = False
) -> Compilation:
    """Compile DRAFT again, as it was traced, with FORCE applied.

    Takes the trace and the dumps from the traced compile, unless DUMPS asks
    for the compile's own, which blaming its words on the passes after
    reload needs.
    """
    traced = draft.traced
    pairs = ",".join(f"{pseudo}={hard}" for pseudo, hard in force.items())
    blamed = build_function(
        draft.function,
        fncompare.get_optflags(draft.c_path),
        draft.work,
        {"ORACLE_FORCE": pairs},
        dumps=dumps,
    )
    if not dumps:
        blamed.dumps = traced.blamed.dumps
    registers = traced.registers | {
        pseudo: hard for pseudo, hard in force.items() if pseudo in traced.registers
    }
    return Compilation(blamed, traced.blocks, traced.allocnos, registers)


def build_function(
    function: str,
    optflags: list[str],
    work: Path,
    environment: dict[str, str],
    *,
    dumps: bool,
) -> blame.Compilation:
    """Compile and assemble WORK's unit.i with ENVIRONMENT, for FUNCTION.

    DUMPS asks for every pass's dump of the function.
    """
    os.environ.update(ORACLE_FUNCTION=function, **environment)
    arguments = ["-S", "-dp", *(["-da"] if dumps else []), "-o", "unit.s"]
    try:
        blame.run(work / "unit.i", optflags, arguments, work)
    finally:
        for name in ["ORACLE_FUNCTION", *environment]:
            del os.environ[name]
    lines = (work / "unit.s").read_text().splitlines()
    _ = (work / "marked.s").write_text(blame.mark_lines(lines))
    # The marks leave the code as it was, so one object gives both
    assemble = ["-c", f"-Wa,-I{fncompare.ROOT}", "-o", "marked.o"]
    blame.run(work / "marked.s", optflags, assemble, work)
    functions = fncompare.read_object_file(work / "marked.o")
    routine = functions[function]
    start, ends = blame.read_line_ends(work / "marked.o", function)
    return blame.Compilation(
        lines,
        blame.read_word_lines(lines, routine, start, ends),
        routine,
        functions,
        blame.read_dumps(work, function) if dumps else {},
        {},
    )


def read_insn_lines(
    c_path: str, function: str, compilation: Compilation, work: Path
) -> dict[int, int]:
    """Return {insn UID: its source line}, for the insns before reload.

    WORK must hold the unit.i that COMPILATION was compiled from.
    """
    lreg = compilation.blamed.dumps.get("lreg", "")
    insn_lines = blame.read_insn_lines(
        fncompare.get_optflags(c_path), {"lreg": lreg}, work, function
    ).get("lreg", {})
    return {uid: int(place.split(":")[1]) for uid, place in insn_lines.items()}


def read_pseudo_lines(
    compilation: Compilation, insn_lines: dict[int, int]
) -> dict[int, set[int]]:
    """Return {pseudo: the source lines of the insns using it before reload}."""
    lreg = compilation.blamed.dumps.get("lreg", "")
    lines: dict[int, set[int]] = collections.defaultdict(set)
    for header in blame.INSN_HEADER_RE.finditer(lreg):
        uid = int(header.group(1))
        rtl = blame.find_insn(lreg, uid)
        for match in blame.PSEUDO_RE.finditer(rtl):
            if uid in insn_lines:
                lines[int(match.group(1))].add(insn_lines[uid])
    return lines


def read_insn_pseudos(compilation: Compilation, index: int) -> frozenset[int]:
    """Return the pseudos that the insn making word INDEX uses before reload."""
    insn = blame.get_insn(compilation.blamed, index)
    if insn is None:
        return frozenset()
    lreg = compilation.blamed.dumps.get("lreg", "")
    return index_insn_pseudos(lreg).get(insn[0], frozenset())


@functools.cache
def index_insn_pseudos(lreg: str) -> dict[int, frozenset[int]]:
    """Return {insn UID: the pseudos it uses} for the insns of the LREG dump.

    The dump is from before reload, and so the same for every forcing.
    """
    pseudos: dict[int, frozenset[int]] = {}
    for uid, rtl in blame.index_insns(lreg).items():
        numbers = {int(match.group(1)) for match in blame.PSEUDO_RE.finditer(rtl)}
        pseudos[uid] = frozenset(
            number for number in numbers if number >= FIRST_PSEUDO_REGISTER
        )
    return pseudos


@functools.cache
def index_double_pseudos(lreg: str) -> frozenset[int]:
    """Return the pseudos of the LREG dump whose mode takes two registers."""
    return frozenset(
        int(match.group(2))
        for match in PSEUDO_MODE_RE.finditer(lreg)
        if match.group(1) in DOUBLE_MODES
    )


def get_register_count(compilation: Compilation, pseudo: int) -> int:
    """Return how many hard registers PSEUDO takes, from its mode's name."""
    lreg = compilation.blamed.dumps.get("lreg", "")
    return 2 if pseudo in index_double_pseudos(lreg) else 1


def vote_registers(
    target: fncompare.Routine, compilation: Compilation
) -> dict[int, collections.Counter[int]]:
    """Return {pseudo: Counter of the target registers it lines up with}."""
    votes: dict[int, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    for pseudo, wanted, _ in iterate_votes(target, compilation):
        votes[pseudo][wanted] += 1
    return votes


def iterate_votes(
    target: fncompare.Routine, compilation: Compilation
) -> Iterator[tuple[int, int, int]]:
    """Yield (pseudo, target register, our word's index) for each vote.

    The instructions are paired by their words without their registers, and
    each pair's register fields vote for the pseudo, in the insn that made
    our instruction, that holds our register.
    """
    compiled = compilation.routine
    registers = compilation.registers
    # The hard registers each pseudo with one holds
    spans = {
        pseudo: range(hard, hard + get_register_count(compilation, pseudo))
        for pseudo, hard in registers.items()
        if hard >= 0
    }
    matcher = difflib.SequenceMatcher(
        None,
        [mask_registers(word) for word in target.words],
        [mask_registers(word) for word in compiled.words],
        autojunk=False,
    )
    for target_start, compiled_start, length in matcher.get_matching_blocks():
        for offset in range(length):
            target_index = target_start + offset
            compiled_index = compiled_start + offset
            held = [
                (pseudo, spans[pseudo])
                for pseudo in read_insn_pseudos(compilation, compiled_index)
                if pseudo in spans
            ]
            if not held:
                continue
            for (_, wanted), (_, ours) in zip(
                read_register_fields(target.words[target_index]),
                read_register_fields(compiled.words[compiled_index]),
                strict=True,
            ):
                holders = [pseudo for pseudo, span in held if ours in span]
                if len(holders) == 1:
                    pseudo = holders[0]
                    yield pseudo, wanted - (ours - registers[pseudo]), compiled_index


def count_differences(target: fncompare.Routine, compilation: Compilation) -> int:
    """Return how many instructions differ between TARGET and COMPILATION."""
    return fncompare.count_differences(target, compilation.routine)


def find_tied_pseudos(compilation: Compilation) -> dict[int, list[int]]:
    """Return {pseudo: the pseudos local-alloc tied into its quantity}.

    Forcing one pseudo of a quantity alone breaks the tie, so the others
    are forced with it unless they have votes of their own.
    """
    tied: dict[int, list[int]] = {}
    for block in compilation.blocks:
        for quantity in block["quantities"]:
            for pseudo in quantity["pseudos"]:
                tied[pseudo] = quantity["pseudos"]
    return tied


@dataclass
class Search:
    """The closest forcing that search_forcing found, and how it got there."""

    force: dict[int, int]
    differences: int
    compilation: Compilation
    # The pairs forced and the words that differed in each round, the first
    # the starting forcing's
    rounds: list[tuple[int, int]]


def search_forcing(
    target: fncompare.Routine, draft: Draft, force: dict[int, int]
) -> Search:
    """Return the forcing that brings the draft closest to TARGET.

    Each round adds to the last round's forcing every pseudo whose votes
    favour another register than the one it has, until a forcing repeats.
    Forcing one pseudo can move others, so a round may do worse than the
    last and still lead to a better one.
    """
    compilation = compile_function(draft, force) if force else draft.traced
    differences = count_differences(target, compilation)
    best = Search(dict(force), differences, compilation, [(len(force), differences)])
    tried = [dict(force)]
    for _ in range(MAX_ROUNDS):
        if best.differences == 0:
            break
        registers = compilation.registers
        tied = find_tied_pseudos(compilation)
        proposal = dict(tried[-1])
        for pseudo, counter in vote_registers(target, compilation).items():
            wanted, _ = counter.most_common(1)[0]
            if wanted != registers.get(pseudo):
                for partner in tied.get(pseudo, [pseudo]):
                    if partner not in proposal:
                        proposal[partner] = wanted
                proposal[pseudo] = wanted
        if proposal in tried:
            break
        tried.append(proposal)
        compilation = compile_function(draft, proposal)
        differences = count_differences(target, compilation)
        best.rounds.append((len(proposal), differences))
        if differences < best.differences:
            best.force, best.differences = proposal, differences
            best.compilation = compilation
    return best


def minimize_forcing(
    target: fncompare.Routine, draft: Draft, force: dict[int, int]
) -> dict[int, int]:
    """Return the fewest of FORCE's pairs that still make the draft match."""
    kept = dict(force)
    for pseudo in list(force):
        trial = {key: hard for key, hard in kept.items() if key != pseudo}
        if count_differences(target, compile_function(draft, trial)) == 0:
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


def get_word_line(
    compilation: Compilation, index: int, insn_lines: dict[int, int]
) -> int | None:
    """Return the source line of the insn that made word INDEX, if known."""
    insn = blame.get_insn(compilation.blamed, index)
    return insn_lines.get(insn[0]) if insn is not None else None


def print_splits(
    target: fncompare.Routine, compilation: Compilation, insn_lines: dict[int, int]
) -> None:
    """Print the pseudos whose instructions line up with more than one register.

    GCC gives a variable one pseudo, and global-alloc one register, so a
    pseudo the original holds in two registers in different places was
    likely two variables there, such as a flag declared in each of two loops.
    Each register is followed by the source lines of its votes.
    """
    votes: dict[int, dict[int, list[int]]] = collections.defaultdict(
        lambda: collections.defaultdict(list)
    )
    for pseudo, wanted, index in iterate_votes(target, compilation):
        votes[pseudo][wanted].append(index)
    for pseudo, registers in sorted(votes.items()):
        held = sorted(
            (
                (register, indices)
                for register, indices in registers.items()
                if len(indices) >= MIN_SPLIT_VOTES
            ),
            key=lambda pair: -len(pair[1]),
        )
        if len(held) <= 1:
            continue
        print(f"pseudo {pseudo} lines up with several registers; maybe two variables")
        for register, indices in held:
            lines = (get_word_line(compilation, index, insn_lines) for index in indices)
            used = sorted({line for line in lines if line is not None})
            print(f"  {register_name(register)} x{len(indices)}, lines {used}")


# The registers a call keeps: $s0-$s7 and $fp
SAVED_REGISTERS = frozenset([*range(16, 24), 30])


def print_merges(
    target: fncompare.Routine, compilation: Compilation, insn_lines: dict[int, int]
) -> None:
    """Print pseudos held in a saved register there and a clobbered one here.

    Each comes with the other pseudos the original holds in that register.
    A value live across no call takes a saved register only by sharing a
    variable with one that is, so the original likely read it into one of
    the others' variables, such as a grid mark read into the character.
    """
    votes: dict[int, collections.Counter[int]] = collections.defaultdict(
        collections.Counter
    )
    indices: dict[int, list[int]] = collections.defaultdict(list)
    for pseudo, wanted, index in iterate_votes(target, compilation):
        votes[pseudo][wanted] += 1
        indices[pseudo].append(index)
    held: dict[int, list[int]] = collections.defaultdict(list)
    for pseudo, counts in sorted(votes.items()):
        register, count = counts.most_common(1)[0]
        # One vote is enough when no other register gets any
        agreed = count >= MIN_SPLIT_VOTES or count == counts.total()
        if agreed and register in SAVED_REGISTERS:
            held[register].append(pseudo)

    def describe(pseudo: int) -> str:
        lines = (
            get_word_line(compilation, index, insn_lines) for index in indices[pseudo]
        )
        used = sorted({line for line in lines if line is not None})
        ours = register_name(compilation.registers.get(pseudo, -1))
        return f"pseudo {pseudo} (in {ours}, lines {used})"

    for register, pseudos in sorted(held.items()):
        for pseudo in pseudos:
            if compilation.registers.get(pseudo, -1) in SAVED_REGISTERS:
                continue
            others = [other for other in pseudos if other != pseudo]
            if not others:
                continue
            saved = register_name(register)
            print(f"{describe(pseudo)} lines up with {saved}; maybe one variable with")
            for other in others:
                print(f"  {describe(other)}")


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


@dataclass
class Request:
    """What the command line asks for."""

    # "search", or the option that names another mode
    mode: str
    asm_path: str
    c_path: str
    function: str
    force: dict[int, int]


def parse_arguments(arguments: list[str]) -> Request:
    """Read the command line, or exit with the usage if it doesn't fit."""
    modes = [argument[2:] for argument in arguments if argument in MODE_OPTIONS]
    arguments = [argument for argument in arguments if argument not in MODE_OPTIONS]
    arguments = fncompare.expand_function_name(arguments)
    if (
        len(modes) > 1
        or len(arguments) < MIN_ARGUMENT_COUNT
        or any(argument.startswith("-") for argument in arguments)
    ):
        sys.exit(__doc__)
    asm_path, c_path, function, *pairs = arguments
    force = {
        int(pseudo): parse_register(hard)
        for pseudo, hard in (pair.split("=") for pair in pairs)
    }
    return Request(modes[0] if modes else "search", asm_path, c_path, function, force)


def print_search(search: Search) -> None:
    """Print each round of SEARCH."""
    _, differences = search.rounds[0]
    print(f"  unforced: {differences} differ")
    for number, (forced, differences) in enumerate(search.rounds[1:], 1):
        print(f"  round {number}: {forced} forced, {differences} differ")


def scan_spills(target: fncompare.Routine, draft: Draft, force: dict[int, int]) -> None:
    """Print the closest searches that start with a pseudo forced onto the stack.

    Tries each pseudo that global-alloc gave a register, with FORCE, for an
    original whose larger frame holds a variable that the draft does not
    spill.
    """
    traced = draft.traced
    lines = read_pseudo_lines(
        traced, read_insn_lines(draft.c_path, draft.function, traced, draft.work)
    )
    start = search_forcing(target, draft, force)
    print(f"  without a spill: {start.differences} differ")
    results: list[tuple[int, int, int]] = []
    for allocno in traced.allocnos:
        pseudo = allocno["pseudo"]
        if allocno["hard"] < 0 or pseudo in force:
            continue
        search = search_forcing(target, draft, force | {pseudo: NO_REGISTER})
        results.append((search.differences, len(search.force), pseudo))
    for differences, forced, pseudo in sorted(results)[:SPILL_REPORT_COUNT]:
        register = register_name(traced.registers.get(pseudo, NO_REGISTER))
        used = join_lines([pseudo], lines)
        result = f"{differences} differ when {forced} are forced"
        print(f"pseudo {pseudo} ({register}, lines {used}) spilled: {result}")


def main() -> None:
    """Search for and explain the allocation that matches the function."""
    request = parse_arguments(sys.argv[1:])
    asm_path, c_path, function = request.asm_path, request.c_path, request.function
    force = request.force
    if not (ORACLE_DIR / "cc1.optimized").exists():
        sys.exit("run tools/build_oracle.sh first")
    os.environ["KMC_DIR"] = str(ORACLE_DIR)
    fncompare.KMC_DIR = ORACLE_DIR
    original = fncompare.read_original(asm_path, c_path, function)
    target = original.routine
    print(function)
    if original.nested:
        print(f"  nested, not compared: {' '.join(original.nested)}")
    with tempfile.TemporaryDirectory() as work_name:
        work = Path(work_name)
        traced = trace_function(c_path, function, work)
        draft = Draft(c_path, function, work, traced)
        if request.mode == "spills":
            scan_spills(target, draft, force)
            return
        if request.mode == "allocation":
            compilation = compile_function(draft, force)
            print(f"  {count_differences(target, compilation)} differ")
            print_allocation(
                compilation,
                read_pseudo_lines(
                    compilation, read_insn_lines(c_path, function, compilation, work)
                ),
            )
            return
        if request.mode == "splits":
            compilation = compile_function(draft, force)
            insn_lines = read_insn_lines(c_path, function, compilation, work)
            print_splits(target, compilation, insn_lines)
            print_merges(target, compilation, insn_lines)
            return
        search = search_forcing(target, draft, force)
        print_search(search)
        force = search.force
        if search.differences == 0 and force:
            force = minimize_forcing(target, draft, force)
            print(f"MATCHES when {len(force)} pseudos are forced:")
        elif search.differences == 0:
            print("MATCHES unforced")
            return
        else:
            print(f"closest: {search.differences} differ when {len(force)} are forced")
            closest = compile_function(draft, force, dumps=True).blamed
            fncompare.print_diff(
                target, closest.routine, lambda index: blame.blame(closest, index)
            )
        noted_work = work / "noted"
        if noted_work.exists():
            shutil.rmtree(noted_work)
        insn_lines = read_insn_lines(c_path, function, traced, work)
        explain(traced, force, read_pseudo_lines(traced, insn_lines))
        print_splits(target, traced, insn_lines)
        print_merges(target, traced, insn_lines)


if __name__ == "__main__":
    main()
