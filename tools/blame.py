#!/usr/bin/env python3
"""Show where KMC GCC made each instruction that differs from the target.

    blame.py ASM_FILE C_FILE FUNCTION

Prints fncompare's diff of FUNCTION, noting after each of our differing
instructions the insn that made it, as -dp names it, the first pass whose
RTL dump holds that insn, and the passes before global-alloc that changed
its pattern. Then, for each pseudo register those insns used before
register allocation, prints what the allocators knew of it: its uses and
live length from local-alloc, the hard register it was given, and its
conflicts and preferences from global-alloc.
"""

import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import rabbitizer
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

import fncompare
from fncompare import StrPath

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
# The insn UID and pattern name -dp writes after each instruction
INSN_NAME_RE = re.compile(r"#\s+(\d+)\s+(\S+)$")
# What follows an insn's pattern: its recognized code and pattern name
PATTERN_END_RE = re.compile(r"\) (?:-1|\d+ \{\S+\}) ")
# The start of an insn's header in an RTL dump, before its UID
INSN_HEADER_START = r"^ *\((?:insn|jump_insn|call_insn)(?:/\w+)? "
PSEUDO_RE = re.compile(r"\(reg(?:/\w+)?:\w+ (\d+)\)")
# An insn's header in an RTL dump, with its UID
INSN_HEADER_RE = re.compile(INSN_HEADER_START + r"(\d+) ", re.MULTILINE)
# A line number note of a -g dump: its file and line
LINE_NOTE_RE = re.compile(r'^\(note \d+ \d+ \d+ \("([^"]*)"\) (\d+)\)$', re.MULTILINE)
DISPOSITION_RE = re.compile(r"(\d+) in (\d+)")


@dataclass
class Compilation:
    """FUNCTION's assembly, object and RTL dumps from one compile."""

    lines: list[str]
    # The .s line number that made each word of the function
    word_lines: list[int]
    routine: fncompare.Routine
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
    with obj_path.open("rb") as obj_file:
        elf = ELFFile(obj_file)
        symbols = elf.get_section_by_name(".symtab")
        if not isinstance(symbols, SymbolTableSection):
            sys.exit(f"{obj_path} has no symbols")
        text_index = elf.get_section_index(".text")
        values = {
            symbol.name: fncompare.read_entry_int(symbol.entry, "st_value")
            for symbol in symbols.iter_symbols()
            if fncompare.read_entry_field(symbol.entry, "st_shndx") == text_index
        }
    start = next(
        value for name, value in values.items() if name.split(".")[0] == function
    )
    ends = {
        int(name.removeprefix(MARK_PREFIX)): value
        for name, value in values.items()
        if name.startswith(MARK_PREFIX)
    }
    return start, ends


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


def run(c_path: StrPath, optflags: list[str], arguments: list[str], work: Path) -> None:
    """Run the compiler in WORK, or exit with its errors."""
    result = fncompare.run_compiler(c_path, optflags, arguments, cwd=work)
    if result.returncode != 0:
        sys.exit(result.stderr)


def read_dumps(work: Path, function: str) -> dict[str, str]:
    """Return {pass: FUNCTION's part of that pass's dump} from WORK."""
    return {
        path.suffix[1:]: split_dump(path.read_text(errors="replace"), function)
        for path in work.glob("unit.*")
        if path.suffix[1:] in PASSES
    }


def map_insn_lines(dump: str, noted_dump: str) -> dict[int, str]:
    """Return {insn UID of DUMP: its source line} from NOTED_DUMP.

    NOTED_DUMP is the same pass's dump from a -g compile. -g leaves the
    insns as they were but adds line notes, which take UIDs of their own, so
    the insns are paired by their order. Each takes the last line note
    before it.
    """
    uids = [int(uid) for uid in INSN_HEADER_RE.findall(dump)]
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
    c_path: str, optflags: list[str], dumps: dict[str, str], work: Path, function: str
) -> dict[str, dict[int, str]]:
    """Return {pass: {insn UID: source line}} for the insns of DUMPS."""
    noted_work = work / "noted"
    noted_work.mkdir()
    _ = (noted_work / "unit.i").write_text((work / "unit.i").read_text())
    run(
        noted_work / "unit.i", optflags, ["-S", "-g", "-da", "-o", "unit.s"], noted_work
    )
    noted_dumps = read_dumps(noted_work, function)
    return {
        name: map_insn_lines(dump, noted_dumps.get(name, ""))
        for name, dump in dumps.items()
    }


def compile_function(c_path: str, function: str, work: Path) -> Compilation:
    """Compile FUNCTION of C_PATH in WORK with -dp and -da."""
    optflags = fncompare.get_optflags(c_path)
    result = fncompare.run_compiler(c_path, optflags, ["-E"])
    isolated = fncompare.isolate_functions(result.stdout, {function})
    if not re.search(rf"\b{function}\(", isolated):
        isolated = result.stdout
    _ = (work / "unit.i").write_text(isolated)
    run(work / "unit.i", optflags, ["-S", "-dp", "-da", "-o", "unit.s"], work)
    lines = (work / "unit.s").read_text().splitlines()
    _ = (work / "marked.s").write_text(mark_lines(lines))
    assemble = ["-c", f"-Wa,-I{fncompare.ROOT}", "-o"]
    run(work / "unit.s", optflags, [*assemble, "unit.o"], work)
    run(work / "marked.s", optflags, [*assemble, "marked.o"], work)
    routine = fncompare.read_object_file(work / "unit.o")[function]
    if fncompare.read_object_file(work / "marked.o")[function].words != routine.words:
        sys.exit("marking the lines changed the code")
    start, ends = read_line_ends(work / "marked.o", function)
    dumps = read_dumps(work, function)
    return Compilation(
        lines,
        read_word_lines(lines, routine, start, ends),
        routine,
        dumps,
        read_insn_lines(c_path, optflags, dumps, work, function),
    )


def find_insn(dump: str, uid: int) -> str:
    """Return the RTL of insn UID in DUMP, or an empty string."""
    match = re.search(rf"{INSN_HEADER_START}{uid} ", dump, re.MULTILINE)
    if not match:
        return ""
    end = dump.find("\n\n", match.start())
    return dump[match.start() : end if end >= 0 else len(dump)]


def get_insn(compilation: Compilation, index: int) -> tuple[int, str] | None:
    """Return the UID and pattern of the insn that made word INDEX.

    An insn whose template takes several lines names itself on the first.
    """
    number = compilation.word_lines[index]
    while number > 0:
        line = compilation.lines[number - 1]
        match = INSN_NAME_RE.search(line)
        if match:
            return int(match.group(1)), match.group(2)
        if not INSTRUCTION_LINE_RE.match(line):
            return None
        number -= 1
    return None


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
    for pattern, dump in (
        (rf"^Register {pseudo} (used .*)$", lreg),
        (rf"^;; {pseudo} (conflicts: .*)$", greg),
        (rf"^;; {pseudo} (preferences: .*)$", greg),
    ):
        match = re.search(pattern, dump, re.MULTILINE)
        if match:
            parts.append(match.group(1))
    return "\n    ".join(parts)


def main() -> None:
    """Blame the differing instructions of the function named."""
    asm_path, c_path, function = sys.argv[1:]
    target = fncompare.read_target_functions(asm_path)[function]
    with tempfile.TemporaryDirectory() as work:
        compilation = compile_function(c_path, function, Path(work))
    compiled = compilation.routine
    print(f"{function} {fncompare.describe(target, compiled)}")
    fncompare.print_diff(target, compiled, lambda index: blame(compilation, index))
    runs = fncompare.find_diff_runs(target, compiled)[: fncompare.MAX_DIFF_HUNKS]
    indices = [index for run in runs for _, index in run if index is not None]
    for pseudo in find_pseudos(compilation, indices):
        print(f"  {describe_pseudo(compilation, pseudo)}")


if __name__ == "__main__":
    main()
