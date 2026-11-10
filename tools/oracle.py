#!/usr/bin/env python3
"""Find the register allocation that would make a draft match, and why.

    oracle.py [--allocation | --splits | --spills | --merges | --levers |
               --globals | --decisions] [ASM_FILE C_FILE] FUNCTION
              [PSEUDO=HARD ...]

Without ASM_FILE and C_FILE, finds them from FUNCTION as fncompare does.

Compiles FUNCTION once with the cc1 that tools/build_oracle.sh makes,
which traces the allocators (see oracle.patch). The instructions that
equal the target's but for their registers name, through the insns that
made them, the pseudo register each target register should hold. Those
pseudos are forced into those registers as reload starts, and the compile
repeated until it matches or stops improving. A match proves that the
draft's RTL is the original's up to register allocation, and the forced
pseudos are then cut down to the fewest that still match. PSEUDO=HARD
pairs, hard registers given by number or name, or "stack" for a pseudo the
original spilled, start the forcing off.

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

--levers finds the forcing as the search does, and then, replaying
global-alloc from the trace in editsearch (see EDITSEARCH), the refs or
live length of a single allocno that would give global-alloc the forced
registers unforced: the levers a source change can pull, each pseudo named
by its variable and lines. Pseudos local-alloc placed are out of its reach.

--merges searches for the draft's values that the original held in one
variable: as the RTL is first made (see ORACLE_MERGE in oracle.patch), each
pseudo of the closest forcing found is made one with every other pseudo of
its mode, and each variable with every other, then the closest of those two
at a time, and each is compiled unforced. A merge that matches names the
variable, or the expression, that the source would hold in another's
variable for the allocators to give the original's registers.

--globals searches for the pseudos that local-alloc placed and the
original left to global-alloc, as a pseudo living in more than one basic
block is: each is made global (see ORACLE_GLOBAL in oracle.patch) and
compiled unforced, then the closest two at a time. One that matches, or
comes closest, names a variable whose setting and uses the original had
in different blocks.

--decisions searches the choices the passes make between ways of
compiling the draft, which register allocation can't explain: each choice
is numbered (see ORACLE_FLIP in oracle.patch) and taken the other way
alone, compiled unforced, and the closest kept, round by round, until the
draft matches or nothing comes closer. Each flip found names the pass,
the insns and their lines, and why the pass chose as it did: for the
scheduler, by "order", the insns' order in the RTL, which the source's
order of statements sets, or by "priority" or "class", which the
dependences between the insns set.

Only FUNCTION's own routine is compared; the functions nested in it, which
fncompare counts with it, are named so that each can be given as FUNCTION.
"""

import collections
import functools
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Callable, Iterable, Iterator, Sequence
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, TypedDict, cast

import blame
import Levenshtein

import fncompare

# The oracle's compiler directory, which ORACLE_DIR can override
ORACLE_DIR = Path(os.environ.get("ORACLE_DIR", fncompare.ROOT / "build" / "oracle"))
MAX_ROUNDS = 8
# The dumps the search reads: the insns' pseudos as the RTL is first made
# and before reload, and the asm statements, which -dp doesn't name, from
# the last pass
SEARCH_PASSES = ("rtl", "lreg", blame.PASSES[-1])
MODE_OPTIONS = (
    "--allocation",
    "--splits",
    "--spills",
    "--merges",
    "--levers",
    "--globals",
    "--decisions",
)
# The editsearch that replays global-alloc for --levers
EDITSEARCH = Path(
    os.environ.get(
        "EDITSEARCH",
        Path.home()
        / "projects"
        / "tools"
        / "editsearch"
        / "target"
        / "release"
        / "editsearch",
    )
)
TRACE_NAME = "trace.jsonl"
LEVER_PSEUDO_RE = re.compile(r"pseudo (\d+)")
# The closest spill searches that --spills prints
SPILL_REPORT_COUNT = 10
# The closest merges that --merges prints
MERGE_REPORT_COUNT = 15
# The closest merges that --merges tries two at a time
MERGE_BEAM = 24
# The closest pseudos made global that --globals prints, and tries two at
# a time
GLOBAL_REPORT_COUNT = 15
GLOBAL_BEAM = 24
# The closest flips that --decisions prints in each round
DECISION_REPORT_COUNT = 10
# The dumps whose insns the scheduler's decisions name, by their site
DECISION_DUMPS = {"sched": "sched", "sched2": "sched2"}
# The children cc1's fork mode runs at once (see ORACLE_FORK_DIR in
# oracle.patch), and then the processes that score their assembly.
# Scoring them as they finish, in fewer processes beside fewer children,
# measured no faster.
FORK_JOBS = 19
# The assembly files a scoring process takes at a time
SCORE_CHUNK = 16
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


class Allocno(TypedDict):
    """A pseudo that global-alloc placed, as traced."""

    pseudo: int
    n_refs: int
    size: int
    live_length: int
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


class Variable(TypedDict):
    """A variable or parameter of the function held in a pseudo, as traced."""

    name: str
    pseudo: int
    line: int


class VariablesRecord(TypedDict):
    """The function's variables, and each pseudo's mode, as the RTL is made.

    pseudos is {pseudo: [its mode, whether it is a variable's]}.
    """

    stage: Literal["variables"]
    variables: list[Variable]
    pseudos: dict[str, tuple[str, int]]


class GlobalStartRecord(TypedDict):
    """What global-alloc starts from, which editsearch's replay reads."""

    stage: Literal["global_start"]


class DecisionRecord(TypedDict):
    """A choice a pass made, which ORACLE_FLIP can take the other way.

    site is the pass; insns the UIDs of the insn chosen and of the one
    that flipping it would take; gap how far apart their priorities were;
    and by what chose between them: "launch" is a priority raised to
    launch an insn whose register is born, whose gap is no distance.
    """

    stage: Literal["decision"]
    index: int
    site: str
    insns: list[int]
    gap: int
    by: str


TraceRecord = (
    LocalRecord
    | GlobalRecord
    | DispositionsRecord
    | VariablesRecord
    | GlobalStartRecord
    | DecisionRecord
)


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


class CompileError(Exception):
    """A unit that doesn't compile, with the compiler's errors."""


def isolate_file(c_path: str, function: str, optflags: list[str]) -> str:
    """Return C_PATH preprocessed and cut down to FUNCTION and its neighbour."""
    result = fncompare.run_compiler(c_path, optflags, ["-E"])
    if result.returncode != 0:
        raise CompileError(result.stderr)
    isolated = fncompare.isolate_functions(result.stdout, {function})
    if not re.search(rf"\b{function}\(", isolated):
        return result.stdout
    return isolated


@dataclass
class Draft:
    """The function being compiled, and its traced, unforced compile."""

    function: str
    optflags: list[str]
    # Holds the unit.i that trace_function left, and every compile's files
    work: Path
    traced: Compilation
    # {pseudo: its mode} as the RTL is first made, and the variables'
    modes: dict[int, str]
    variables: dict[int, Variable]


def trace_function(unit: str, function: str, optflags: list[str], work: Path) -> Draft:
    """Compile FUNCTION of the preprocessed UNIT unforced, tracing the allocators.

    Leaves in WORK the unit.i that every forced compile of the function
    then compiles. The allocators run before reload, where the forcing is
    applied, so the trace and the dumps up to lreg hold for every forcing.
    """
    _ = (work / "unit.i").write_text(unit)
    trace_path = work / TRACE_NAME
    trace_path.unlink(missing_ok=True)
    blamed = build_function(
        function, optflags, work, {"ORACLE_TRACE": str(trace_path)}, SEARCH_PASSES
    )
    compilation = Compilation(blamed, [], [], {})
    draft = Draft(function, optflags, work, compilation, {}, {})
    for line in trace_path.read_text().splitlines():
        record = cast("TraceRecord", json.loads(line))
        if record["stage"] == "variables":
            draft.modes = {
                int(key): mode for key, (mode, _) in record["pseudos"].items()
            }
            draft.variables = {
                variable["pseudo"]: variable for variable in record["variables"]
            }
        elif record["stage"] == "local":
            compilation.blocks.append(record)
        elif record["stage"] == "global":
            compilation.allocnos = record["allocnos"]
        elif record["stage"] == "dispositions":
            compilation.registers = {
                int(key): hard for key, hard in record["registers"].items()
            }
    return draft


def compile_function(
    draft: Draft, force: dict[int, int], *, dumps: bool = False
) -> Compilation:
    """Compile DRAFT again, as it was traced, with FORCE applied.

    Takes the trace and the dumps from the traced compile, unless DUMPS asks
    for the compile's own, of every pass, which blaming its words on the
    passes after reload needs.
    """
    traced = draft.traced
    pairs = ",".join(f"{pseudo}={hard}" for pseudo, hard in force.items())
    blamed = build_function(
        draft.function,
        draft.optflags,
        draft.work,
        {"ORACLE_FORCE": pairs},
        blame.PASSES if dumps else (),
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
    passes: Sequence[str],
) -> blame.Compilation:
    """Compile and assemble WORK's unit.i with ENVIRONMENT, for FUNCTION.

    Reads the function's dumps of PASSES. Raises CompileError when the unit
    doesn't compile.
    """
    dump_flags = [blame.get_dump_flag(passes)] if passes else []
    arguments = ["-S", "-dp", *dump_flags, "-o", "unit.s"]
    result = fncompare.run_compiler(
        work / "unit.i",
        optflags,
        arguments,
        work,
        {"ORACLE_FUNCTION": function, **environment},
    )
    if result.returncode != 0:
        raise CompileError(result.stderr)
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
        blame.read_dumps(work, function, passes),
        {},
    )


def read_insn_lines(
    draft: Draft, compilation: Compilation, stage: str = "lreg"
) -> dict[int, int]:
    """Return {insn UID: its source line}, for the insns of STAGE's dump.

    By default those before reload. COMPILATION must be of DRAFT's unit.i.
    """
    dump = compilation.blamed.dumps.get(stage, "")
    insn_lines = blame.read_insn_lines(
        draft.optflags, {stage: dump}, draft.work, draft.function
    ).get(stage, {})
    return {uid: int(place.split(":")[1]) for uid, place in insn_lines.items()}


def read_pseudo_lines(
    compilation: Compilation, insn_lines: dict[int, int], stage: str = "lreg"
) -> dict[int, set[int]]:
    """Return {pseudo: the source lines of the insns using it}.

    The insns are those of STAGE's dump, by default before reload.
    """
    dump = compilation.blamed.dumps.get(stage, "")
    lines: dict[int, set[int]] = collections.defaultdict(set)
    for header in blame.INSN_HEADER_RE.finditer(dump):
        uid = int(header.group(1))
        rtl = blame.find_insn(dump, uid)
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
    for target_start, compiled_start, length in match_masked_words(
        list(map(mask_registers, target.words)),
        list(map(mask_registers, compiled.words)),
    ):
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


def match_masked_words(
    target_words: list[int], compiled_words: list[int]
) -> list[tuple[int, int, int]]:
    """Return (target start, compiled start, length) of each run of equal words.

    The words are given without their registers. The C Levenshtein module
    aligns them about thirty times as fast as difflib.
    """
    edits = Levenshtein.opcodes(target_words, compiled_words)
    return Levenshtein.matching_blocks(edits, target_words, compiled_words)


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
    last and still lead to a better one. A forcing that reload can't carry
    out, which crashes cc1, ends the search.
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
        try:
            compilation = compile_function(draft, proposal)
        except CompileError:
            break
        differences = count_differences(target, compilation)
        best.rounds.append((len(proposal), differences))
        if differences < best.differences:
            best.force, best.differences = proposal, differences
            best.compilation = compilation
    return best


def summarize_search(search: Search) -> str:
    """Return in a line the closest forcing SEARCH found, but not its pairs."""
    count = len(search.force)
    if search.differences:
        return f"closest: {search.differences} differ when {count} are forced"
    return f"matches when {count} are forced" if count else "matches unforced"


def minimize_forcing(
    target: fncompare.Routine, draft: Draft, force: dict[int, int]
) -> dict[int, int]:
    """Return the fewest of FORCE's pairs that still make the draft match."""
    kept = dict(force)
    for pseudo in list(force):
        trial = {key: hard for key, hard in kept.items() if key != pseudo}
        try:
            compilation = compile_function(draft, trial)
        except CompileError:
            continue
        if count_differences(target, compilation) == 0:
            kept = trial
    return kept


def get_priority(refs: int, size: int, length: int) -> int:
    """Return the priority qty_compare and allocno_compare give a register.

    That is from its REFS, SIZE and live LENGTH.
    """
    if length <= 0 or refs <= 0:
        return 0
    return int((refs.bit_length() - 1) * refs * size / length * 10000)


def describe_quantity(quantity: Quantity, block: int) -> str:
    """Return what local-alloc knew of QUANTITY of BLOCK."""
    tied = "+".join(map(str, quantity["pseudos"]))
    register = register_name(quantity["phys_reg"])
    heading = f"local qty {quantity['qty']} of block {block} [{tied}] in {register}"
    life = quantity["death"] - quantity["birth"]
    parts = [
        f"{heading}: {quantity['n_refs']} refs",
        f"life {quantity['birth']}-{quantity['death']}",
        f"priority {get_priority(quantity['n_refs'], quantity['size'], life)}",
    ]
    if quantity["n_sugg"] or quantity["n_copy_sugg"]:
        parts.append("suggested")
    return ", ".join(parts)


def describe_allocno(allocno: Allocno, order: int) -> str:
    """Return what global-alloc knew of ALLOCNO, the ORDERth allocated."""
    register = register_name(allocno["hard"])
    priority = get_priority(allocno["n_refs"], allocno["size"], allocno["live_length"])
    parts = [
        ", ".join(
            [
                f"global #{order} in {register}: {allocno['n_refs']} refs",
                f"live {allocno['live_length']}",
                f"priority {priority}",
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


def print_levers(
    target: fncompare.Routine, draft: Draft, force: dict[int, int]
) -> None:
    """Print the refs and live lengths that would place the forcing unforced.

    The forcing is the search's, cut to the fewest pairs where it matches,
    and editsearch replays global-alloc from the draft's trace for each.
    The forced variables make editsearch's --goal, which ranks the edits
    that move them toward their registers.
    """
    search = search_forcing(target, draft, force)
    print_search(search)
    print(summarize_search(search))
    force = search.force
    if search.differences == 0 and force:
        force = minimize_forcing(target, draft, force)
    pairs = [f"{pseudo}={hard}" for pseudo, hard in sorted(force.items())]
    command = [str(EDITSEARCH), "allocation", str(draft.work / TRACE_NAME), *pairs]
    report = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    print(report, end="")
    lines = read_pseudo_lines(draft.traced, read_insn_lines(draft, draft.traced))
    named = sorted(
        {int(match[1]) for match in LEVER_PSEUDO_RE.finditer(report)} | set(force)
    )
    for pseudo in named:
        print(f"  {describe_pseudo(draft, pseudo, lines)}")
    goal = [
        f"--goal {draft.variables[pseudo]['name']}={hard}"
        for pseudo, hard in sorted(force.items())
        if pseudo in draft.variables
    ]
    if goal:
        print(f"editsearch's goal: {' '.join(goal)}")


def scan_spills(target: fncompare.Routine, draft: Draft, force: dict[int, int]) -> None:
    """Print the closest searches that start with a pseudo forced onto the stack.

    Tries each pseudo that global-alloc gave a register, with FORCE, for an
    original whose larger frame holds a variable that the draft does not
    spill.
    """
    traced = draft.traced
    lines = read_pseudo_lines(traced, read_insn_lines(draft, traced))
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


Merge = tuple[int, int]


def compile_trial(
    target: fncompare.Routine, draft: Draft, environment: dict[str, str], work: Path
) -> int | None:
    """Return how many words differ when DRAFT is compiled with ENVIRONMENT.

    Compiles in WORK, which must hold the draft's unit.i, unforced. None is
    a compile that failed.
    """
    try:
        blamed = build_function(draft.function, draft.optflags, work, environment, ())
    except CompileError:
        return None
    return fncompare.count_differences(target, blamed.routine)


def run_trials(
    target: fncompare.Routine, draft: Draft, trials: list[dict[str, str]]
) -> list[int | None]:
    """Return the words that differ for each of TRIALS, compiled in parallel.

    Each trial is the environment of one compile.
    """
    count = os.cpu_count() or 1
    works = [draft.work / f"trial{index}" for index in range(count)]
    for work in works:
        work.mkdir(exist_ok=True)
        _ = shutil.copy(draft.work / "unit.i", work / "unit.i")
    results: list[int | None] = [None] * len(trials)

    def run_share(index: int) -> None:
        for trial in range(index, len(trials), count):
            results[trial] = compile_trial(target, draft, trials[trial], works[index])

    with ThreadPoolExecutor(count) as pool:
        for future in [pool.submit(run_share, index) for index in range(count)]:
            future.result()
    return results


def run_merges(
    target: fncompare.Routine, draft: Draft, trials: list[tuple[Merge, ...]]
) -> list[int | None]:
    """Return the words that differ for each of TRIALS, compiled in parallel.

    Each (kept, gone) pair of a trial makes the pseudo gone one with kept.
    """
    environments = [
        {"ORACLE_MERGE": ",".join(f"{kept}={gone}" for kept, gone in trial)}
        for trial in trials
    ]
    return run_trials(target, draft, environments)


def describe_pseudo(draft: Draft, pseudo: int, lines: dict[int, set[int]]) -> str:
    """Return PSEUDO's variable, or that it holds an expression, and its lines.

    LINES are those of the insns using it as the RTL is first made.
    """
    variable = draft.variables.get(pseudo)
    what = f"`{variable['name']}`" if variable else "an expression"
    used = " ".join(map(str, sorted(lines.get(pseudo, set()))))
    return f"{pseudo} ({what}, lines {used})"


def list_merges(draft: Draft, forced: Iterable[int]) -> list[Merge]:
    """Return the merges to try alone, each a (kept, gone) pair.

    Each FORCED pseudo is merged with every other pseudo of its mode, and
    each variable with every other of its mode, each way round, as the one
    kept gives the merged pseudo its number.
    """
    modes = draft.modes
    forced_merges = {
        merge
        for pseudo in forced
        if pseudo in modes
        for other, mode in modes.items()
        if other != pseudo and mode == modes[pseudo]
        for merge in ((pseudo, other), (other, pseudo))
    }
    variable_merges = {
        (kept, gone)
        for kept in draft.variables
        for gone in draft.variables
        if kept != gone and modes.get(kept) == modes.get(gone)
    }
    return sorted(forced_merges | variable_merges)


def scan_merges(target: fncompare.Routine, draft: Draft, force: dict[int, int]) -> None:
    """Print the merges of a forced pseudo with another that come closest.

    The forcing is search_forcing's from FORCE. list_merges's merges are
    tried alone, then the MERGE_BEAM closest two at a time.
    """
    search = search_forcing(target, draft, force)
    forced = search.force
    if search.differences == 0 and forced:
        forced = minimize_forcing(target, draft, forced)
    unforced = count_differences(target, draft.traced)
    print(f"  unforced: {unforced} differ; {summarize_search(search)}")
    trials: list[tuple[Merge, ...]] = [(merge,) for merge in list_merges(draft, forced)]
    results = run_merges(target, draft, trials)
    print(f"  {len(trials)} merges of {len(forced)} forced pseudos or variables tried")
    ranked = sorted(
        (differences, trial)
        for trial, differences in zip(trials, results, strict=True)
        if differences is not None and differences < unforced
    )
    beam = [trial[0] for _, trial in ranked[:MERGE_BEAM]]
    pairs: list[tuple[Merge, ...]] = [
        (first, second)
        for index, first in enumerate(beam)
        for second in beam[index + 1 :]
        if not {*first} & {*second}
    ]
    results = run_merges(target, draft, pairs)
    ranked += [
        (differences, trial)
        for trial, differences in zip(pairs, results, strict=True)
        if differences is not None and differences < unforced
    ]
    print(f"  {len(pairs)} pairs of the closest tried")
    traced = draft.traced
    lines = read_pseudo_lines(traced, read_insn_lines(draft, traced, "rtl"), "rtl")
    closest = sorted(ranked, key=lambda item: (item[0], len(item[1])))[
        :MERGE_REPORT_COUNT
    ]
    for differences, trial in closest:
        merges = "; ".join(
            describe_pseudo(draft, gone, lines)
            + " into "
            + describe_pseudo(draft, kept, lines)
            for kept, gone in trial
        )
        print(f"{differences} differ: {merges}")


def scan_globals(target: fncompare.Routine, draft: Draft) -> None:
    """Print the pseudos local-alloc placed whose making global comes closest.

    Each is tried alone, then the GLOBAL_BEAM closest two at a time, all
    unforced.
    """
    traced = draft.traced
    unforced = count_differences(target, traced)
    print(f"  unforced: {unforced} differ")
    local = sorted(
        {
            pseudo
            for block in traced.blocks
            for quantity in block["quantities"]
            for pseudo in quantity["pseudos"]
        }
    )

    def rank(trials: list[tuple[int, ...]]) -> list[tuple[int, tuple[int, ...]]]:
        environments = [
            {"ORACLE_GLOBAL": ",".join(map(str, trial))} for trial in trials
        ]
        results = run_trials(target, draft, environments)
        return sorted(
            (differences, trial)
            for trial, differences in zip(trials, results, strict=True)
            if differences is not None and differences < unforced
        )

    ranked = rank([(pseudo,) for pseudo in local])
    beam = [trial for _, trial in ranked[:GLOBAL_BEAM]]
    ranked += rank(
        [
            first + second
            for index, first in enumerate(beam)
            for second in beam[index + 1 :]
        ]
    )
    print(f"  {len(local)} local pseudos tried alone, then the closest in pairs")
    lines = read_pseudo_lines(traced, read_insn_lines(draft, traced, "rtl"), "rtl")
    closest = sorted(ranked, key=lambda item: (item[0], len(item[1])))
    for differences, trial in closest[:GLOBAL_REPORT_COUNT]:
        pseudos = "; ".join(describe_pseudo(draft, pseudo, lines) for pseudo in trial)
        print(f"{differences} differ: {pseudos}")


@functools.cache
def read_driver_commands(
    work: Path, optflags: tuple[str, ...]
) -> tuple[list[str], list[str]]:
    """Return the cc1 and the as command lines that the driver runs.

    Each is without its input and output files, for a trial to name its
    own: a trial runs both itself, which saves two runs of the driver.
    """
    probe = work / "probe"
    probe.mkdir(exist_ok=True)
    _ = shutil.copy(work / "unit.i", probe / "unit.i")
    commands: list[list[str]] = []
    for arguments, program, files in (
        (["-v", "-S", "-o", "unit.s"], "/cc1 ", ("unit.s",)),
        (["-v", "-c", f"-Wa,-I{fncompare.ROOT}", "-o", "unit.o"], "/as ", ("unit.o",)),
    ):
        source = probe / ("unit.i" if program == "/cc1 " else "unit.s")
        result = fncompare.run_compiler(source, list(optflags), arguments, probe)
        if result.returncode != 0:
            raise CompileError(result.stderr)
        line = next(line for line in result.stderr.splitlines() if program in line)
        words = line.split()
        dropped = {str(source.resolve()), "-version", "-v", "-o", *files}
        commands.append([word for word in words if word not in dropped])
    cc1, assemble = commands
    return cc1, assemble


@dataclass
class Scoring:
    """What each scoring process compares the trials' assembly with."""

    target: fncompare.Routine | None = None
    function: str = ""
    assemble: Sequence[str] = ()


# Set in each scoring process by start_scoring
SCORING = Scoring()


def start_scoring(
    target: fncompare.Routine, function: str, assemble: list[str]
) -> None:
    """Give a scoring process what it compares the trials with."""
    SCORING.target, SCORING.function, SCORING.assemble = target, function, assemble


def score_assembly(path: str) -> tuple[int, int | None]:
    """Return a trial's number, from its file's name, and how many words differ.

    None is assembly that didn't assemble or lacks the function.
    """
    object_path = path.removesuffix(".s") + ".o"
    result = subprocess.run(
        [*SCORING.assemble, "-o", object_path, path], capture_output=True, check=False
    )
    index = int(Path(path).stem)
    if result.returncode != 0 or SCORING.target is None:
        return index, None
    routine = fncompare.read_object_file(object_path, {SCORING.function}).get(
        SCORING.function
    )
    if routine is None:
        return index, None
    return index, fncompare.count_differences(SCORING.target, routine)


def read_function_text(path: Path, function: str) -> bytes:
    """Return FUNCTION's assembly in the file at PATH, or all of it if not found.

    Two trials whose function reads the same assemble to the same words.
    """
    text = path.read_bytes()
    start = text.find(b"\n" + function.encode() + b":\n")
    end = text.find(b"\t.end\t" + function.encode() + b"\n", start)
    return text[start:end] if 0 <= start < end else text


def fork_decisions(
    target: fncompare.Routine, draft: Draft, flips: tuple[int, ...]
) -> tuple[list[DecisionRecord], dict[int, int]]:
    """Return DRAFT's decisions with FLIPS, and the words each flip leaves.

    One compile in cc1's fork mode gives every decision, beside FLIPS,
    taken the other way; the words are {decision's number: words that
    differ}, without the flips whose assembly fails.
    """
    work = draft.work / "fork"
    shutil.rmtree(work, ignore_errors=True)
    children = work / "children"
    children.mkdir(parents=True)
    cc1, assemble = read_driver_commands(draft.work, tuple(draft.optflags))
    trace_path = work / TRACE_NAME
    environment = {
        "ORACLE_FUNCTION": draft.function,
        "ORACLE_FLIP": ",".join(map(str, flips)),
        "ORACLE_TRACE": str(trace_path),
        "ORACLE_FORK_DIR": str(children),
        "ORACLE_FORK_JOBS": str(FORK_JOBS),
    }
    result = subprocess.run(
        [*cc1, str(draft.work / "unit.i"), "-o", str(work / "unit.s")],
        cwd=work,
        env=dict(os.environ, **environment),
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise CompileError(result.stderr)
    records = (
        cast("TraceRecord", json.loads(line))
        for line in trace_path.read_text().splitlines()
    )
    decisions = [record for record in records if record["stage"] == "decision"]
    # The children whose function reads the same take one scoring: about
    # a third of them for DrawGame
    alike: dict[bytes, list[int]] = collections.defaultdict(list)
    for path in children.glob("*.s"):
        alike[read_function_text(path, draft.function)].append(int(path.stem))
    paths = [str(children / f"{indices[0]}.s") for indices in alike.values()]
    with ProcessPoolExecutor(
        FORK_JOBS,
        initializer=start_scoring,
        initargs=(target, draft.function, assemble),
    ) as pool:
        scores = dict(pool.map(score_assembly, paths, chunksize=SCORE_CHUNK))
    words = {
        index: count
        for indices in alike.values()
        if (count := scores[indices[0]]) is not None
        for index in indices
    }
    return decisions, words


def read_decision_lines(
    draft: Draft, flips: tuple[int, ...]
) -> dict[str, dict[int, int]]:
    """Return the source lines of the insns DRAFT's decisions name with FLIPS.

    They are {site: {insn UID: source line}}, from the dumps of the passes
    that decide.
    """
    work = draft.work / "decisions"
    work.mkdir(exist_ok=True)
    _ = shutil.copy(draft.work / "unit.i", work / "unit.i")
    environment = {"ORACLE_FLIP": ",".join(map(str, flips))}
    passes = sorted(set(DECISION_DUMPS.values()))
    blamed = build_function(draft.function, draft.optflags, work, environment, passes)
    noted = blame.read_insn_lines(
        draft.optflags,
        blamed.dumps,
        work,
        draft.function,
        {"ORACLE_FUNCTION": draft.function, **environment},
    )
    return {
        site: {uid: int(place.split(":")[1]) for uid, place in noted[dump].items()}
        for site, dump in DECISION_DUMPS.items()
    }


def describe_decision(
    decision: DecisionRecord, lines: dict[str, dict[int, int]]
) -> str:
    """Return DECISION's pass, its insns with their lines, and what chose."""
    site_lines = lines.get(decision["site"], {})
    chosen, other = (f"{uid}@{site_lines.get(uid, '?')}" for uid in decision["insns"])
    by = decision["by"]
    if by == "priority":
        by = f"priority by {decision['gap']}"
    return f"#{decision['index']} {decision['site']}: {other} before {chosen} ({by})"


def scan_decisions(target: fncompare.Routine, draft: Draft) -> None:
    """Print the flips of the passes' decisions that bring DRAFT closest.

    Each round takes each decision the other way alone, beside the flips
    kept so far, and keeps the closest, until the draft matches or no flip
    comes closer. The scheduler schedules each block from its end, so a
    flip puts the insn it names first before the other.
    """
    closest = count_differences(target, draft.traced)
    print(f"  unflipped: {closest} differ")
    flips: tuple[int, ...] = ()
    for number in range(1, MAX_ROUNDS + 1):
        decisions, words = fork_decisions(target, draft, flips)
        if number == 1:
            print(f"  {len(decisions)} decisions")
        by_index = {decision["index"]: decision for decision in decisions}
        ranked = sorted(
            (differences, (*flips, index))
            for index, differences in words.items()
            if differences < closest and index not in flips
        )
        if not ranked:
            print(f"  round {number}: no flip comes closer")
            return
        print(f"  round {number}:")
        lines = read_decision_lines(draft, flips)
        for differences, trial in ranked[:DECISION_REPORT_COUNT]:
            decision = describe_decision(by_index[trial[-1]], lines)
            print(f"    {differences} differ: {decision}")
        closest, flips = ranked[0]
        if closest == 0:
            print(f"MATCHES when {len(flips)} decisions are flipped: {flips}")
            return
    print(f"  closest: {closest} differ when {flips} are flipped")


def use_compiler() -> None:
    """Compile with the oracle's cc1 from now on, or exit if it isn't built."""
    if not (ORACLE_DIR / "source" / "cc1").exists():
        sys.exit("run tools/build_oracle.sh first")
    os.environ["KMC_DIR"] = str(ORACLE_DIR)
    fncompare.KMC_DIR = ORACLE_DIR


def main() -> None:
    """Search for and explain the allocation that matches the function."""
    request = parse_arguments(sys.argv[1:])
    asm_path, c_path, function = request.asm_path, request.c_path, request.function
    use_compiler()
    original = fncompare.read_original(asm_path, c_path, function)
    target = original.routine
    print(function)
    if original.nested:
        print(f"  nested, not compared: {' '.join(original.nested)}")
    with tempfile.TemporaryDirectory() as work_name:
        work = Path(work_name)
        try:
            optflags = fncompare.get_optflags(c_path)
            unit = isolate_file(c_path, function, optflags)
            draft = trace_function(unit, function, optflags, work)
            run_mode(request, target, draft)
        except CompileError as error:
            sys.exit(str(error))


def run_mode(request: Request, target: fncompare.Routine, draft: Draft) -> None:
    """Search for and explain the forcing as REQUEST asks, from DRAFT."""
    force, traced = request.force, draft.traced
    scans: dict[str, Callable[[fncompare.Routine, Draft, dict[int, int]], None]] = {
        "spills": scan_spills,
        "merges": scan_merges,
        "levers": print_levers,
        "globals": lambda target, draft, _: scan_globals(target, draft),
        "decisions": lambda target, draft, _: scan_decisions(target, draft),
    }
    if request.mode in scans:
        scans[request.mode](target, draft, force)
        return
    if request.mode == "allocation":
        compilation = compile_function(draft, force)
        print(f"  {count_differences(target, compilation)} differ")
        print_allocation(
            compilation,
            read_pseudo_lines(compilation, read_insn_lines(draft, compilation)),
        )
        return
    if request.mode == "splits":
        compilation = compile_function(draft, force)
        insn_lines = read_insn_lines(draft, compilation)
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
        print(summarize_search(search))
        closest = compile_function(draft, force, dumps=True).blamed
        fncompare.print_diff(
            target, closest.routine, lambda index: blame.blame(closest, index)
        )
    noted_work = draft.work / "noted"
    if noted_work.exists():
        shutil.rmtree(noted_work)
    insn_lines = read_insn_lines(draft, traced)
    explain(traced, force, read_pseudo_lines(traced, insn_lines))
    print_splits(target, traced, insn_lines)
    print_merges(target, traced, insn_lines)


if __name__ == "__main__":
    main()
