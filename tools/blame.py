#!/usr/bin/env python3
"""Show where KMC GCC made each instruction that differs from the target.

    blame.py [--keep DIR] [ASM_FILE C_FILE] FUNCTION
    blame.py --dump PASSES [--insn UIDS] [--pseudo PSEUDOS] [--keep DIR]
             [ASM_FILE C_FILE] FUNCTION

Without ASM_FILE and C_FILE, finds them from FUNCTION as fncompare does.

Prints fncompare's diff of FUNCTION, noting after each of our differing
instructions the insn that made it, as -dp names it, the first pass whose
RTL dump holds that insn, and the passes before global-alloc that changed
its pattern. Then, for each pseudo register those insns used before register
allocation, prints what the allocators knew of it: its uses and live length
from local-alloc, the hard register it was given, and its conflicts and
preferences from global-alloc. For each such insn, it prints how sched and
sched2 placed it: its priority, the stalls before it became ready and the
insns it was placed ahead of, and whether local-alloc tied the pseudo it
sets to one it uses, so that both take one register, or what kept it from
doing so. Each pseudo's report also gives where it lived in the first pass
and in each later pass before allocation that moved its insns, and the stack
slot of one given no hard register. Last, it checks the target's order of
our insns against the dependences sched2 kept, or failing that lists the
pairs it orders the other way and their priorities. Words are paired by
text, so a pair it reports may be a mispairing. --keep leaves the compiler's
work files, such as the pass dumps unit.sched and unit.sched2, in DIR.

--dump prints FUNCTION's part of the dump of each pass in PASSES, in place
of the report. PASSES is a comma-separated list of rtl, jump, cse, loop,
cse2, flow, combine, sched, lreg, greg, sched2, jump2 and dbr. --insn and
--pseudo, comma-separated numbers, cut each dump down to the insns with
those UIDs or using those pseudos, and the passes' notes on the pseudos.
"""

import contextlib
import functools
import re
import struct
import sys
import tempfile
from collections import Counter
from collections.abc import Generator, Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import cast

import rabbitizer

import fncompare
from fncompare import StrPath

# ASM_FILE, C_FILE and FUNCTION
EXPECTED_ARGUMENT_COUNT = 3
# The options, each taking a value
OPTIONS = ("--keep", "--dump", "--insn", "--pseudo")

# The passes -da dumps, in the order they run
PASSES = (
    "rtl",
    "jump",
    "cse",
    "loop",
    "cse2",
    "flow",
    "combine",
    "sched",
    "lreg",
    "greg",
    "sched2",
    "jump2",
    "dbr",
)
# The letter that asks -d for each pass's dump
DUMP_LETTERS: dict[str, str] = dict(zip(PASSES, "rjsLtfcSlgRJd", strict=True))
# The passes before global-alloc, which rewrites every insn's pseudos
PASSES_BEFORE_ALLOCATION = PASSES[: PASSES.index("greg")]
# The first register number that names a pseudo, from config/mips/mips.h
FIRST_PSEUDO_REGISTER = 68
GPR_NAMES = (
    *("zero", "at", "v0", "v1", "a0", "a1", "a2", "a3"),
    *("t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7"),
    *("s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7"),
    *("t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"),
)
FPR_COUNT = 32
OTHER_REGISTER_NAMES = ("hi", "lo", "accum", "fcc")

# An instruction line of the assembly: its code and any comment
INSTRUCTION_LINE_RE = re.compile(r"^(\t[a-z][^#]*?)(\s*#.*)?$")
# The start of the symbols that mark where each line's words end
MARK_PREFIX = "blame_line_"
# Big-endian ELF32: the header from e_shoff to e_shstrndx, a section header,
# and a symbol, with the fields of a section header this reads
ELF_HEADER_OFFSET = 0x20
ELF_HEADER = struct.Struct(">IIHHHHHH")
SECTION_HEADER = struct.Struct(">10I")
SYMBOL = struct.Struct(">IIIBBH")
SECTION_NAME, SECTION_TYPE, SECTION_OFFSET, SECTION_SIZE, SECTION_LINK = 0, 1, 4, 5, 6
SYMBOL_TABLE_TYPE = 2
# The insn UID and pattern name -dp writes after each instruction
INSN_NAME_RE = re.compile(r"#\s+(\d+)\s+(\S+)$")
# The line before an asm statement's lines in the assembly
ASM_START = "#APP"
# An asm statement's insn in an RTL dump: one with operands, or a bare one
ASM_INSN_RE = re.compile(
    r"^\S+ \d+ \d+ \d+ \((?:set \(reg\S* \d+(?: \S+)?\)\s+)?\(asm_"
)
# What follows an insn's pattern: its recognized code and pattern name
PATTERN_END_RE = re.compile(r"\) (?:-1|\d+ \{\S+\}) ")
# The start of an insn's header in an RTL dump, before its UID
INSN_HEADER_START = r"^ *\((?:insn|jump_insn|call_insn)(?:[/:]\w+)* "
PSEUDO_RE = re.compile(r"\(reg(?:/\w+)?:\w+ (\d+)\)")
# An insn's header in an RTL dump, with its UID
INSN_HEADER_RE = re.compile(INSN_HEADER_START + r"(\d+) ", re.MULTILINE)
# A line number note of a -g dump: its file and line
LINE_NOTE_RE = re.compile(r'^\(note \d+ \d+ \d+ \("([^"]*)"\) (\d+)\)$', re.MULTILINE)
DISPOSITION_RE = re.compile(r"(\d+) in (\d+)")
# An insn's pattern that sets a register: the register's number and the source
SET_RE = re.compile(r"^\(set \(reg(?:/\w+)?:\w+ (\d+)\) (.*)\)$")
# A hard register in RTL: its number
HARD_REGISTER_RE = re.compile(r"\(reg(?:/\w+)?:\w+ (\d+) \$\w+\)")
# A note that a register dies in an insn: its number
DEATH_RE = re.compile(r"REG_DEAD \(reg(?:/\w+)?:\w+ (\d+)\)")
# An insn, label or other element of an RTL dump: its kind and UID
ELEMENT_HEADER_RE = re.compile(
    r"^\((insn|jump_insn|call_insn|code_label|barrier|note)(?:[/:]\w+)* (\d+) ",
    re.MULTILINE,
)
# How many times longer than in the last pass reported a pseudo's live range
# must grow for its pass to be reported
STRETCH_FACTOR = 2
# A stack slot in RTL: its offset from the stack pointer
STACK_SLOT_RE = re.compile(r"\(plus:SI \(reg:SI 29 sp\)\s+\(const_int (-?\d+)\)\)")
# A dependence in LOG_LINKS: its kind, absent for a true one, and the insn
LINK_RE = re.compile(r"\(insn_list(?::(\w+))? (\d+)")
# The priority sched gives an insn as it schedules the insn's block
PRIORITY_RE = re.compile(r"^;; insn\[ *(\d+)\]: priority = +(\d+)", re.MULTILINE)
# sched's note that an insn became ready: it, the insn placed after it, the
# stalls it cost and the clock, counted back from the block's end
LAUNCH_RE = re.compile(
    r"^;; launching (\d+) before (\d+) with (no|\d+) stalls? at T-(\d+)",
    re.MULTILINE,
)
# sched's ready list at a clock: the clock, then what it placed first
READY_RE = re.compile(r"^;; ready list at T-(\d+):.*, now (\d+)(.*)$", re.MULTILINE)
SCHEDULE_PASSES = ("sched", "sched2")
DEPENDENCE_KINDS = {"REG_DEP_ANTI": "anti", "REG_DEP_OUTPUT": "output"}
# What PATTERN_END_RE splits an insn with links into: its pattern and the rest
PATTERN_AND_REST = 2
# The most broken dependences or reordered pairs the order check lists
MAX_ORDER_LINES = 12


@dataclass
class Compilation:
    """FUNCTION's assembly, object and RTL dumps from one compile."""

    lines: list[str]
    # The .s line number that made each word of the function
    word_lines: list[int]
    routine: fncompare.Routine
    # Every routine in the object, the function's nested ones among them
    functions: fncompare.Functions
    # {pass: the function's part of that pass's dump}
    dumps: dict[str, str]
    # {pass: {insn UID: the source line that made it}}
    insn_lines: dict[str, dict[int, str]]


def register_name(number: int) -> str:
    """Return the assembler's name for hard register NUMBER."""
    if number < len(GPR_NAMES):
        return "$" + GPR_NAMES[number]
    if number < len(GPR_NAMES) + FPR_COUNT:
        return f"$f{number - len(GPR_NAMES)}"
    return "$" + OTHER_REGISTER_NAMES[number - len(GPR_NAMES) - FPR_COUNT]


def mark_lines(lines: list[str]) -> str:
    """Return the assembly of LINES with each instruction line's end marked.

    The mark is a symbol set to the address after the line's words. It goes
    on the line itself, since KMC as reads the line after each
    multiplication to decide on its VR4300 fix, and a line of its own would
    change what that read sees. A symbol assignment, unlike a label, leaves
    the delay slots filled as before.
    """
    marked: list[str] = []
    for number, line in enumerate(lines, 1):
        match = INSTRUCTION_LINE_RE.match(line)
        if match:
            code, comment = match.groups()
            marked.append(f"{code} ; {MARK_PREFIX}{number} = .{comment or ''}")
        else:
            marked.append(line)
    return "\n".join(marked) + "\n"


def read_line_ends(obj_path: Path, function: str) -> tuple[int, dict[int, int]]:
    """Return FUNCTION's start and {line number: end} of its marked lines.

    A nested function's symbol is FUNCTION.N, after its label number.
    """
    values = read_text_symbols(obj_path.read_bytes())
    start = next(
        value for name, value in values.items() if name.split(".")[0] == function
    )
    ends = {
        int(name.removeprefix(MARK_PREFIX)): value
        for name, value in values.items()
        if name.startswith(MARK_PREFIX)
    }
    return start, ends


def read_text_symbols(elf: bytes) -> dict[str, int]:
    """Return {name: value} of the symbols in .text of the object ELF.

    The object is the assembler's, big-endian ELF32. It is read here rather
    than with pyelftools, which took most of the oracle's time on the marks.
    """
    header = unpack_fields(ELF_HEADER, elf, ELF_HEADER_OFFSET)
    section_offset, section_count, names_index = header[0], header[6], header[7]
    sections = [
        unpack_fields(SECTION_HEADER, elf, section_offset + index * SECTION_HEADER.size)
        for index in range(section_count)
    ]
    section_names = sections[names_index][SECTION_OFFSET]

    def read_name(table: int, offset: int) -> str:
        end = elf.index(0, table + offset)
        return elf[table + offset : end].decode()

    text_index = next(
        index
        for index, section in enumerate(sections)
        if read_name(section_names, section[SECTION_NAME]) == ".text"
    )
    symbols = next(
        section for section in sections if section[SECTION_TYPE] == SYMBOL_TABLE_TYPE
    )
    strings = sections[symbols[SECTION_LINK]][SECTION_OFFSET]
    start = symbols[SECTION_OFFSET]
    values: dict[str, int] = {}
    for offset in range(start, start + symbols[SECTION_SIZE], SYMBOL.size):
        name, value, _, _, _, section = unpack_fields(SYMBOL, elf, offset)
        if section == text_index:
            values[read_name(strings, name)] = value
    return values


def unpack_fields(layout: struct.Struct, data: bytes, offset: int) -> tuple[int, ...]:
    """Return the integer fields of LAYOUT at OFFSET of DATA."""
    return cast("tuple[int, ...]", layout.unpack_from(data, offset))


def is_branch_line(line: str) -> bool:
    """Whether LINE of the assembly holds a branch or a jump."""
    return line.split(maxsplit=1)[0][0] in "bj"


def has_delay_slot(routine: fncompare.Routine, index: int) -> bool:
    """Whether ROUTINE's word INDEX is a branch or a jump."""
    word = routine.words[index] | (routine.references[index] or 0)
    return rabbitizer.Instruction(word).hasDelaySlot()


def read_word_lines(
    lines: list[str], routine: fncompare.Routine, start: int, ends: dict[int, int]
) -> list[int]:
    """Return the line that made each of ROUTINE's words, which begin at START.

    Each line's words run from the previous line's end to its own. Where the
    assembler moved an instruction into the delay slot of the branch after
    it, the two words are swapped back.
    """
    word_lines = [0] * len(routine.words)
    previous_end = start
    for number, end in sorted(ends.items(), key=lambda item: (item[1], item[0])):
        for offset in range(previous_end, end, fncompare.WORD_SIZE):
            index = (offset - start) // fncompare.WORD_SIZE
            if index < len(word_lines):
                word_lines[index] = number
        previous_end = max(previous_end, end)
    for index in range(len(word_lines) - 1):
        first, second = word_lines[index], word_lines[index + 1]
        if (
            first < second
            and has_delay_slot(routine, index)
            and not is_branch_line(lines[first - 1])
            and is_branch_line(lines[second - 1])
        ):
            word_lines[index], word_lines[index + 1] = second, first
    return word_lines


def split_dump(dump: str, function: str) -> str:
    """Return FUNCTION's part of a pass's dump."""
    parts = re.split(r"^;; Function ", dump, flags=re.MULTILINE)
    return next((part for part in parts if part.startswith(f"{function}\n")), "")


def run(
    c_path: StrPath,
    optflags: list[str],
    arguments: list[str],
    work: Path,
    environment: dict[str, str] | None = None,
) -> None:
    """Run the compiler in WORK with ENVIRONMENT, or exit with its errors."""
    result = fncompare.run_compiler(c_path, optflags, arguments, work, environment)
    if result.returncode != 0:
        sys.exit(result.stderr)


def get_dump_flag(passes: Iterable[str]) -> str:
    """Return the -d option that asks for the dumps of PASSES."""
    return "-d" + "".join(DUMP_LETTERS[name] for name in passes)


def read_dumps(
    work: Path, function: str, passes: Iterable[str] = PASSES
) -> dict[str, str]:
    """Return {pass: FUNCTION's part of that pass's dump} from WORK, for PASSES."""
    return {
        name: split_dump(
            (work / f"unit.i.{name}").read_text(errors="replace"), function
        )
        for name in passes
    }


def map_insn_lines(dump: str, noted_dump: str) -> dict[int, str]:
    """Return {insn UID of DUMP: its source line} from NOTED_DUMP.

    NOTED_DUMP is the same pass's dump from a -g compile. -g leaves the
    insns as they were but adds line notes, which take UIDs of their own, so
    the insns are paired by their order. Each takes the last line note
    before it.
    """
    uids = [int(match.group(1)) for match in INSN_HEADER_RE.finditer(dump)]
    noted = list(INSN_HEADER_RE.finditer(noted_dump))
    if len(uids) != len(noted):
        return {}
    notes = list(LINE_NOTE_RE.finditer(noted_dump))
    lines: dict[int, str] = {}
    note_index = -1
    for uid, header in zip(uids, noted, strict=True):
        while (
            note_index + 1 < len(notes)
            and notes[note_index + 1].start() < header.start()
        ):
            note_index += 1
        if note_index >= 0:
            file, line = notes[note_index].groups()
            lines[uid] = f"{Path(file).name}:{line}"
    return lines


def read_insn_lines(
    optflags: list[str],
    dumps: dict[str, str],
    work: Path,
    function: str,
    environment: dict[str, str] | None = None,
) -> dict[str, dict[int, str]]:
    """Return {pass: {insn UID: source line}} for the insns of DUMPS.

    DUMPS must come from a compile with ENVIRONMENT, which the -g compile
    that notes the lines then has too.
    """
    noted_work = work / "noted"
    noted_work.mkdir(exist_ok=True)
    _ = (noted_work / "unit.i").write_text((work / "unit.i").read_text())
    arguments = ["-S", "-g", get_dump_flag(dumps), "-o", "unit.s"]
    run(noted_work / "unit.i", optflags, arguments, noted_work, environment)
    noted_dumps = read_dumps(noted_work, function, dumps)
    return {
        name: map_insn_lines(dump, noted_dumps.get(name, ""))
        for name, dump in dumps.items()
    }


def compile_dumps(c_path: str, function: str, work: Path) -> dict[str, str]:
    """Compile FUNCTION of C_PATH to unit.s in WORK with -dp and -da.

    Returns {pass: FUNCTION's part of that pass's dump}.
    """
    optflags = fncompare.get_optflags(c_path)
    result = fncompare.run_compiler(c_path, optflags, ["-E"])
    isolated = fncompare.isolate_functions(result.stdout, {function})
    if not re.search(rf"\b{function}\(", isolated):
        isolated = result.stdout
    _ = (work / "unit.i").write_text(isolated)
    run(work / "unit.i", optflags, ["-S", "-dp", "-da", "-o", "unit.s"], work)
    return read_dumps(work, function)


def compile_function(c_path: str, function: str, work: Path) -> Compilation:
    """Compile FUNCTION of C_PATH in WORK with -dp and -da."""
    optflags = fncompare.get_optflags(c_path)
    dumps = compile_dumps(c_path, function, work)
    lines = (work / "unit.s").read_text().splitlines()
    _ = (work / "marked.s").write_text(mark_lines(lines))
    assemble = ["-c", f"-Wa,-I{fncompare.ROOT}", "-o"]
    run(work / "unit.s", optflags, [*assemble, "unit.o"], work)
    run(work / "marked.s", optflags, [*assemble, "marked.o"], work)
    functions = fncompare.read_object_file(work / "unit.o")
    routine = functions[function]
    if fncompare.read_object_file(work / "marked.o")[function].words != routine.words:
        sys.exit("marking the lines changed the code")
    start, ends = read_line_ends(work / "marked.o", function)
    return Compilation(
        lines,
        read_word_lines(lines, routine, start, ends),
        routine,
        functions,
        dumps,
        read_insn_lines(optflags, dumps, work, function),
    )


@functools.cache
def index_insns(dump: str) -> dict[int, str]:
    """Return {insn UID: its RTL} for the insns of DUMP, the first of each UID.

    Searching the dump for each insn took most of a large function's run.
    """
    insns: dict[int, str] = {}
    for match in INSN_HEADER_RE.finditer(dump):
        end = dump.find("\n\n", match.start())
        rtl = dump[match.start() : end if end >= 0 else len(dump)]
        _ = insns.setdefault(int(match.group(1)), rtl)
    return insns


def find_insn(dump: str, uid: int) -> str:
    """Return the RTL of insn UID in DUMP, or an empty string."""
    return index_insns(dump).get(uid, "")


def get_insn(compilation: Compilation, index: int) -> tuple[int, str] | None:
    """Return the UID and pattern of the insn that made word INDEX.

    An insn whose template takes several lines names itself on the first.
    -dp names no asm statement, whose lines final writes between #APP and
    #NO_APP, so those words take the asm insn in the same place among the
    function's asm insns in the last pass's dump.
    """
    number = compilation.word_lines[index]
    while number > 0:
        line = compilation.lines[number - 1]
        match = INSN_NAME_RE.search(line)
        if match:
            return int(match.group(1)), match.group(2)
        if line.strip() == ASM_START:
            return get_asm_insn(compilation, number)
        if not INSTRUCTION_LINE_RE.match(line):
            return None
        number -= 1
    return None


def get_asm_insn(compilation: Compilation, number: int) -> tuple[int, str] | None:
    """Return the UID and pattern of the asm insn whose #APP is line NUMBER."""
    start = min(line for line in compilation.word_lines if line > 0)
    earlier = compilation.lines[start - 1 : number - 1]
    place = sum(line.strip() == ASM_START for line in earlier)
    asm_uids = [
        uid
        for uid, rtl in index_insns(compilation.dumps.get(PASSES[-1], "")).items()
        if ASM_INSN_RE.search(rtl)
    ]
    if place >= len(asm_uids):
        return None
    return asm_uids[place], "asm_operands"


def read_pattern(insn: str) -> str:
    """Return the pattern of the RTL of INSN, without its links and notes."""
    body = insn.split(" ", 4)[-1]
    return " ".join(PATTERN_END_RE.split(body, maxsplit=1)[0].split())


def find_changes(compilation: Compilation, uid: int) -> list[str]:
    """Return the passes that made insn UID or changed its pattern.

    Global-alloc and the passes after it are left out, except as the maker
    of an insn, since they give every insn hard registers.
    """
    changes: list[str] = []
    pattern = None
    for name in PASSES:
        insn = find_insn(compilation.dumps.get(name, ""), uid)
        if not insn:
            continue
        is_changed = name in PASSES_BEFORE_ALLOCATION and read_pattern(insn) != pattern
        if pattern is None or is_changed:
            changes.append(name)
        pattern = read_pattern(insn)
    return changes or ["final"]


def blame(compilation: Compilation, index: int) -> str:
    """Return the insn that made word INDEX and the passes that shaped it."""
    insn = get_insn(compilation, index)
    is_padding = compilation.routine.words[index] == 0
    if insn is None or (is_padding and insn[1] != "nop"):
        return "made by the assembler"
    uid, pattern = insn
    made, *changed = find_changes(compilation, uid)
    shaped = f", changed by {' '.join(changed)}" if changed else ""
    line = compilation.insn_lines.get(made, {}).get(uid)
    at = f" at {line}" if line else ""
    return f"insn {uid} {pattern}{at}, from {made}{shaped}"


def find_blamed_lines(asm_path: str, c_path: str, function: str) -> Counter[int]:
    """Return how many of FUNCTION's differing words each line of C_PATH made.

    ASM_PATH holds the target's FUNCTION.
    Words that the target alone holds, or that no line of C_PATH made, such as
    those of inline functions from headers, are left out.
    """
    target = fncompare.read_target_functions(asm_path)[function]
    with tempfile.TemporaryDirectory() as work:
        compilation = compile_function(c_path, function, Path(work))
    file_name = Path(c_path).name
    counts: Counter[int] = Counter()
    for run in fncompare.find_diff_runs(target, compilation.routine):
        for _, index in run:
            insn = None if index is None else get_insn(compilation, index)
            if insn is None:
                continue
            made = find_changes(compilation, insn[0])[0]
            line = compilation.insn_lines.get(made, {}).get(insn[0], "")
            name, _, number = line.rpartition(":")
            if name == file_name:
                counts[int(number)] += 1
    return counts


def find_pseudos(compilation: Compilation, indices: list[int]) -> list[int]:
    """Return the pseudos that the insns of words INDICES use in lreg."""
    pseudos: set[int] = set()
    for index in indices:
        insn = get_insn(compilation, index)
        if insn is None:
            continue
        rtl = find_insn(compilation.dumps.get("lreg", ""), insn[0])
        numbers = (int(match.group(1)) for match in PSEUDO_RE.finditer(rtl))
        pseudos |= {number for number in numbers if number >= FIRST_PSEUDO_REGISTER}
    return sorted(pseudos)


@functools.cache
def index_matches(
    pattern: re.Pattern[str], group: int, dump: str
) -> dict[int, re.Match[str]]:
    """Return {UID: the first match of PATTERN in DUMP whose GROUP is UID}."""
    matches: dict[int, re.Match[str]] = {}
    for match in pattern.finditer(dump):
        _ = matches.setdefault(int(match.group(group)), match)
    return matches


def describe_schedule(dump: str, uid: int) -> str:
    """Return how the scheduler that wrote DUMP placed insn UID.

    The scheduler fills a block from its end, so an insn becomes ready once
    every insn that waits for it is placed, after the stalls its latency
    needs, and is placed at the clock of the ready list it heads, ahead of
    the others still ready then.
    """
    priority = index_matches(PRIORITY_RE, 1, dump).get(uid)
    if priority is None:
        return ""
    parts = [f"priority {priority.group(2)}"]
    launch = index_matches(LAUNCH_RE, 1, dump).get(uid)
    if launch:
        after, stalls, clock = launch.group(2, 3, 4)
        parts.append(f"ready before insn {after} after {stalls} stalls at T-{clock}")
    placed = index_matches(READY_RE, 2, dump).get(uid)
    if placed:
        others = placed.group(3).split()
        rivals = f" ahead of {' '.join(others)}" if others else " alone"
        parts.append(f"placed at T-{placed.group(1)}{rivals}")
    return ", ".join(parts)


def describe_pseudo(compilation: Compilation, pseudo: int) -> str:
    """Return what the register allocators knew of PSEUDO."""
    lreg = compilation.dumps.get("lreg", "")
    greg = compilation.dumps.get("greg", "")
    dispositions = greg.partition(";; Register dispositions:")[2]
    hard = {
        int(match.group(1)): int(match.group(2))
        for match in DISPOSITION_RE.finditer(dispositions.partition("\n\n")[0])
    }
    given = register_name(hard[pseudo]) if pseudo in hard else "no hard register"
    local = re.search(rf"^;; Register {pseudo} in ", lreg, re.MULTILINE)
    parts = [f"pseudo {pseudo} in {given} ({'local' if local else 'global'})"]
    if pseudo not in hard:
        slot = find_stack_slot(compilation, pseudo)
        if slot is not None:
            parts[0] += f", on the stack at 0x{slot:X}($sp)"
    live = describe_live_range(compilation, pseudo)
    if live:
        parts.append(f"live in {live}")
    for pattern, dump in (
        (rf"^Register {pseudo} (used .*)$", lreg),
        (rf"^;; {pseudo} (conflicts: .*)$", greg),
        (rf"^;; {pseudo} (preferences: .*)$", greg),
    ):
        match = re.search(pattern, dump, re.MULTILINE)
        if match:
            parts.append(match.group(1))
    return "\n    ".join(parts)


def find_pseudo_insns(dump: str, pseudo: int) -> list[tuple[int, int]]:
    """Return the place and UID of each insn of DUMP that uses PSEUDO."""
    pattern = re.compile(rf"\(reg(?:/\w+)?:\w+ {pseudo}\)")
    return [
        (place, uid)
        for place, (uid, rtl) in enumerate(index_insns(dump).items())
        if pattern.search(rtl)
    ]


def describe_live_range(compilation: Compilation, pseudo: int) -> str:
    """Return where PSEUDO lives in each pass before allocation that moved it.

    A scheduler that moves a pseudo's uses away from its setting stretches
    its life, which can cost it a hard register. Each range gives the places
    of the first and last insns that use PSEUDO, counted in the pass's insn
    order, and the source lines of their insns. A pass is left out unless
    it changes the range's first or last insn or stretches it to at least
    STRETCH_FACTOR times its length, since deleting insns shortens ranges
    in every pass.
    """
    parts: list[str] = []
    previous: tuple[tuple[int, int], int] | None = None
    for name in PASSES_BEFORE_ALLOCATION:
        found = find_pseudo_insns(compilation.dumps.get(name, ""), pseudo)
        if not found:
            continue
        (first, first_uid), (last, last_uid) = found[0], found[-1]
        if previous:
            ends, length = previous
            is_stretched = last - first >= length * STRETCH_FACTOR
            if (first_uid, last_uid) == ends and not is_stretched:
                continue
        previous = (first_uid, last_uid), max(last - first, 1)
        lines = compilation.insn_lines.get(name, {})
        ends = " to ".join(
            f"insn {uid}" + (f" ({lines[uid]})" if uid in lines else "")
            for uid in dict.fromkeys((first_uid, last_uid))
        )
        parts.append(f"{name} places {first}-{last}, {ends}")
    return "; ".join(parts)


def find_stack_slot(compilation: Compilation, pseudo: int) -> int | None:
    """Return the stack offset global-alloc's reload gave PSEUDO, if any.

    Reload rewrites each insn that used a pseudo with no hard register to
    use its slot, so the slot is the stack offset every such insn names.
    """
    greg = index_insns(compilation.dumps.get("greg", ""))
    common: set[int] | None = None
    for _, uid in find_pseudo_insns(compilation.dumps.get("lreg", ""), pseudo):
        offsets = {
            int(match.group(1)) for match in STACK_SLOT_RE.finditer(greg.get(uid, ""))
        }
        if offsets:
            common = offsets if common is None else common & offsets
    return min(common) if common else None


def find_local_block(lreg: str, pseudo: int) -> str | None:
    """Return the block that local-alloc gives PSEUDO, if it lives in one."""
    match = re.search(
        rf"^Register {pseudo} used .* in block (\d+);", lreg, re.MULTILINE
    )
    return match.group(1) if match else None


def describe_tie(compilation: Compilation, uid: int) -> str:
    """Return whether local-alloc tied the pseudo insn UID sets to one it uses.

    block_alloc offers combine_regs the used pseudos in order and stops at
    the first it ties. A tie needs both pseudos local to the block, the used
    one to die in the insn and the set one to be born there; tied pseudos
    share a quantity and so a register. A used hard register is only
    suggested for the set pseudo.
    """
    lreg = compilation.dumps.get("lreg", "")
    insn = find_insn(lreg, uid)
    match = SET_RE.match(read_pattern(insn)) if insn else None
    if not match or int(match.group(1)) < FIRST_PSEUDO_REGISTER:
        return ""
    set_pseudo = int(match.group(1))
    used = dict.fromkeys(
        number
        for number in (
            int(found.group(1)) for found in PSEUDO_RE.finditer(match.group(2))
        )
        if number >= FIRST_PSEUDO_REGISTER and number != set_pseudo
    )
    if not used:
        hard = HARD_REGISTER_RE.search(match.group(2))
        if not hard:
            return ""
        # combine_regs suggests a used hard register rather than tying it
        name = register_name(int(hard.group(1)))
        return f"suggests {name}, which it uses, for pseudo {set_pseudo}"
    dying = {int(found.group(1)) for found in DEATH_RE.finditer(insn)}
    is_set_born_earlier = bool(
        re.search(rf":\w+ {set_pseudo}\)", lreg[: lreg.find(insn)])
    )
    refusals: list[str] = []
    for pseudo in used:
        reasons: list[str] = []
        if find_local_block(lreg, pseudo) is None:
            reasons.append(f"{pseudo} lives in more than one block")
        if pseudo not in dying:
            reasons.append(f"{pseudo} lives on after it")
        if find_local_block(lreg, set_pseudo) is None:
            reasons.append(f"{set_pseudo} lives in more than one block")
        elif is_set_born_earlier:
            reasons.append(f"{set_pseudo} is set before it")
        if not reasons:
            block = find_local_block(lreg, pseudo)
            return (
                f"ties pseudo {set_pseudo} to {pseudo}, which is local to "
                f"block {block} and dies there"
            )
        refusals.append(f"to {pseudo}: {', '.join(reasons)}")
    return f"can't tie pseudo {set_pseudo} {'; nor '.join(refusals)}"


def read_dependence_kind(note: str | None) -> str:
    """Return the kind of dependence a link's NOTE, if any, names."""
    if note is None:
        return "true"
    return DEPENDENCE_KINDS.get(note, note)


def find_links(insn: str) -> list[tuple[str, int]]:
    """Return the (kind, UID) of each insn that INSN's LOG_LINKS name.

    The links are the first list after the pattern's code and name.
    """
    body = insn.split(" ", 4)[-1]
    parts = PATTERN_END_RE.split(body, maxsplit=1)
    if len(parts) < PATTERN_AND_REST or not parts[-1].startswith("(insn_list"):
        return []
    depth = 0
    for end, character in enumerate(parts[-1]):
        depth += {"(": 1, ")": -1}.get(character, 0)
        if depth == 0:
            return [
                (read_dependence_kind(link.group(1)), int(link.group(2)))
                for link in LINK_RE.finditer(parts[-1][: end + 1])
            ]
    return []


def read_dependences(dump: str) -> dict[int, list[tuple[str, int]]]:
    """Return {insn UID: the (kind, UID) of each insn it waits for} of DUMP."""
    return {
        int(match.group(2)): find_links(find_insn(dump, int(match.group(2))))
        for match in ELEMENT_HEADER_RE.finditer(dump)
        if match.group(1).endswith("insn")
    }


def read_blocks(dump: str) -> dict[int, int]:
    """Return {insn UID: its basic block's number} of DUMP's insns.

    A label begins a block and a jump ends one, as sched sees them.
    """
    blocks: dict[int, int] = {}
    block = 0
    for match in ELEMENT_HEADER_RE.finditer(dump):
        kind, uid = match.group(1), int(match.group(2))
        if kind == "code_label":
            block += 1
        elif kind.endswith("insn"):
            blocks[uid] = block
            if kind == "jump_insn":
                block += 1
    return blocks


def get_position(routine: fncompare.Routine, index: int) -> float:
    """Return where word INDEX goes in insn order: a delay slot's goes first."""
    if index > 0 and has_delay_slot(routine, index - 1):
        return index - 1.5
    return index


def get_word_text(routine: fncompare.Routine, index: int) -> str:
    """Return word INDEX's text."""
    return fncompare.disassemble(routine, index)


def get_register_free_text(routine: fncompare.Routine, index: int) -> str:
    """Return word INDEX's text with its registers left out."""
    return re.sub(r"\$\w+", "$", fncompare.disassemble(routine, index))


def pair_moved_words(
    target: fncompare.Routine, compiled: fncompare.Routine
) -> dict[int, int]:
    """Return {our word index: its target word's index}.

    Words the diff pairs as equal come first, then equal words that moved,
    then words that differ only in registers, each nearest first.
    """
    pairing = fncompare.pair_words(target, compiled)
    pairs = {
        ours: theirs
        for theirs, ours in pairing.rows
        if theirs is not None and ours is not None and pairing.is_equal((theirs, ours))
    }
    for read_key in (get_word_text, get_register_free_text):
        taken = set(pairs.values())
        free = [index for index in range(len(target.words)) if index not in taken]
        for ours in range(len(compiled.words)):
            if ours in pairs:
                continue
            key = read_key(compiled, ours)
            candidates = [theirs for theirs in free if read_key(target, theirs) == key]
            if candidates:
                theirs = min(candidates, key=lambda theirs: abs(theirs - ours))
                pairs[ours] = theirs
                free.remove(theirs)
    return pairs


def place_insns(
    compilation: Compilation, target: fncompare.Routine
) -> tuple[dict[int, float], dict[int, float]]:
    """Return {insn UID: its place} in our order and in the target's.

    An insn takes the place of its first word. One with a word the target
    lacks has no place in the target's order.
    """
    compiled = compilation.routine
    pairs = pair_moved_words(target, compiled)
    ours: dict[int, float] = {}
    theirs: dict[int, float] = {}
    unpaired: set[int] = set()
    for index in range(len(compiled.words)):
        insn = get_insn(compilation, index)
        if insn is None:
            continue
        uid = insn[0]
        position = get_position(compiled, index)
        ours[uid] = min(ours.get(uid, position), position)
        if index not in pairs:
            unpaired.add(uid)
            continue
        target_position = get_position(target, pairs[index])
        theirs[uid] = min(theirs.get(uid, target_position), target_position)
    for uid in unpaired:
        _ = theirs.pop(uid, None)
    return ours, theirs


def describe_insn(compilation: Compilation, uid: int) -> str:
    """Return insn UID's pattern name and source line, from sched2."""
    insn = find_insn(compilation.dumps.get("sched2", ""), uid)
    name = re.search(r"\{(\S+)\}", insn)
    line = compilation.insn_lines.get("sched2", {}).get(uid)
    return f"insn {uid} {name.group(1) if name else '?'}" + (
        f" at {line}" if line else ""
    )


def check_order(compilation: Compilation, target: fncompare.Routine) -> list[str]:
    """Return what keeps sched2 from placing our insns in the target's order.

    Each dependence sched2 kept that the target's order breaks is listed,
    noting whether sched, before register allocation, had it too: one it
    had comes from the code itself, one it lacked from the hard registers.
    With none broken, the pairs of a block the target orders the other way
    are listed with their sched2 priorities, the first thing that decides
    between two ready insns; source order breaks a tie.
    """
    sched2 = compilation.dumps.get("sched2", "")
    if not sched2:
        return []
    dependences = read_dependences(sched2)
    early = read_dependences(compilation.dumps.get("sched", ""))
    ours, theirs = place_insns(compilation, target)
    broken: list[str] = []
    for uid, links in dependences.items():
        for kind, before in links:
            if uid in theirs and before in theirs and theirs[before] > theirs[uid]:
                origin = (
                    "sched too"
                    if (kind, before) in early.get(uid, [])
                    else "sched2 only"
                )
                waiting = describe_insn(compilation, uid)
                awaited = describe_insn(compilation, before)
                broken.append(f"{waiting} waits for {awaited}: {kind}, {origin}")
    if broken:
        return [f"{len(broken)} dependences break the target's order:", *broken]
    blocks = read_blocks(sched2)
    priorities = {
        int(match.group(1)): int(match.group(2))
        for match in PRIORITY_RE.finditer(sched2)
    }
    placed = sorted(theirs, key=lambda uid: ours[uid])
    flipped = [
        (first, second)
        for position, first in enumerate(placed)
        for second in placed[position + 1 :]
        if blocks.get(first) == blocks.get(second) and theirs[first] > theirs[second]
    ]
    if not flipped:
        return []
    lines = [f"no dependence breaks the target's order; {len(flipped)} pairs flip:"]
    rtl_order = {
        int(match.group(2)): position
        for position, match in enumerate(
            ELEMENT_HEADER_RE.finditer(compilation.dumps.get("greg", ""))
        )
    }
    lines += [
        explain_flip(compilation, (first, second), priorities, rtl_order)
        for first, second in flipped
    ]
    return lines


def explain_flip(
    compilation: Compilation,
    pair: tuple[int, int],
    priorities: dict[int, int],
    rtl_order: dict[int, int],
) -> str:
    """Return why sched2 put PAIR's insns in our order, not the target's.

    sched2 fills a block from its end: of the ready insns, it places the one
    with the highest priority, the longest chain of dependences after it,
    latest. An insn is ready once every insn that waits for it is placed.
    rank_for_schedule breaks a tie of priority first by the last placed
    insn, preferring an insn it doesn't wait for with a latency over one,
    then by RTL_ORDER, the order sched2 was given them, placing the insn
    that comes later there latest.
    """
    first, second = pair
    first_priority = priorities.get(first, 0)
    second_priority = priorities.get(second, 0)
    earlier = f"{describe_insn(compilation, first)} (priority {first_priority})"
    later = f"{describe_insn(compilation, second)} (priority {second_priority})"
    order = f"{earlier} goes before {later}"
    if first_priority > second_priority:
        reason = "a dependence of ours held the first back; the original lacks it"
    elif first_priority < second_priority:
        reason = "the original gives the first a longer chain, or holds the second"
    elif rtl_order.get(first, 0) < rtl_order.get(second, 0):
        reason = "a tie, which their order in the RTL decides"
    else:
        reason = "a tie, which the latency of an insn placed after them decides"
    return f"{order}; {reason}"


def print_insn_reports(compilation: Compilation, uids: list[int]) -> None:
    """Print how the schedulers placed each insn of UIDS and how it was tied."""
    for uid in uids:
        for name in SCHEDULE_PASSES:
            placement = describe_schedule(compilation.dumps.get(name, ""), uid)
            if placement:
                print(f"  insn {uid} in {name}: {placement}")
        tie = describe_tie(compilation, uid)
        if tie:
            print(f"  insn {uid} in lreg {tie}")


def select_dump(dump: str, uids: set[int], pseudos: set[int]) -> str:
    """Return DUMP's insns with UIDS or using PSEUDOS, and its pseudos' notes.

    With neither, returns all of DUMP.
    """
    if not uids and not pseudos:
        return dump
    notes = [
        match.group(0)
        for pseudo in sorted(pseudos)
        for match in re.finditer(rf"^Register {pseudo} .*$", dump, re.MULTILINE)
    ]
    insns = [
        rtl
        for uid, rtl in index_insns(dump).items()
        if uid in uids
        or any(int(m.group(1)) in pseudos for m in PSEUDO_RE.finditer(rtl))
    ]
    return "\n\n".join([*notes, *insns])


def print_dumps(
    c_path: str, function: str, work: Path, options: dict[str, str]
) -> None:
    """Print FUNCTION's part of the dumps that OPTIONS name."""
    passes = options["--dump"].split(",")
    unknown = [name for name in passes if name not in PASSES]
    if unknown:
        sys.exit(f"unknown passes {', '.join(unknown)}; known: {', '.join(PASSES)}")
    try:
        uids = {int(n) for n in options.get("--insn", "").split(",") if n}
        pseudos = {int(n) for n in options.get("--pseudo", "").split(",") if n}
    except ValueError:
        sys.exit(__doc__)
    dumps = compile_dumps(c_path, function, work)
    for name in passes:
        print(f";; {name}\n{select_dump(dumps.get(name, ''), uids, pseudos)}\n")


@contextlib.contextmanager
def open_work_dir(keep: Path | None) -> Generator[Path]:
    """Yield KEEP, made if need be, or a temporary directory."""
    if keep:
        keep.mkdir(parents=True, exist_ok=True)
        yield keep
    else:
        with tempfile.TemporaryDirectory() as work:
            yield Path(work)


def read_options(arguments: list[str]) -> tuple[dict[str, str], list[str]]:
    """Split ARGUMENTS into {option: value} and the rest, or exit with usage."""
    options: dict[str, str] = {}
    while arguments[:1] and arguments[0] in OPTIONS:
        value = arguments[1:2]
        if not value:
            sys.exit(__doc__)
        options[arguments[0]] = value[0]
        arguments = arguments[2:]
    if ("--insn" in options or "--pseudo" in options) and "--dump" not in options:
        sys.exit(__doc__)
    return options, arguments


def main() -> None:
    """Blame the differing instructions of the function named."""
    options, arguments = read_options(sys.argv[1:])
    keep = Path(options["--keep"]) if "--keep" in options else None
    arguments = fncompare.expand_function_name(arguments)
    if len(arguments) != EXPECTED_ARGUMENT_COUNT:
        sys.exit(__doc__)
    asm_path, c_path, function = arguments
    if "--dump" in options:
        with open_work_dir(keep) as work:
            print_dumps(c_path, function, work, options)
        return
    original = fncompare.read_original(asm_path, c_path, function)
    target = original.routine
    with open_work_dir(keep) as work:
        compilation = compile_function(c_path, function, work)
    compiled = compilation.routine
    score = fncompare.describe_original(original, compilation.functions, function)
    print(f"{function} {score}")
    nested_pairs = {
        name: (routine, compilation.functions.get(name, fncompare.NOT_COMPILED))
        for name, routine in original.nested.items()
    }
    if nested_pairs and fncompare.count_differences(target, compiled):
        print(f" {function}:")
    fncompare.print_diff(target, compiled, lambda index: blame(compilation, index))
    # The nested routines' words are left unblamed: word_lines covers the
    # function's own
    for name, pair in nested_pairs.items():
        if fncompare.count_differences(*pair):
            print(f" {name}:")
            fncompare.print_diff(*pair)
    runs = fncompare.find_diff_runs(target, compiled)[: fncompare.MAX_DIFF_HUNKS]
    indices = [index for run in runs for _, index in run if index is not None]
    for pseudo in find_pseudos(compilation, indices):
        print(f"  {describe_pseudo(compilation, pseudo)}")
    print_insn_reports(
        compilation,
        sorted(
            {insn[0] for index in indices if (insn := get_insn(compilation, index))}
        ),
    )
    order = check_order(compilation, target)
    if order:
        print(f"order: {order[0]}")
        for line in order[1 : MAX_ORDER_LINES + 1]:
            print(f"  {line}")


if __name__ == "__main__":
    main()
