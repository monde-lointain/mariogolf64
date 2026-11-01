#!/usr/bin/env python3
"""Measure the build's progress as objdiff's report.json.

    progress_report.py OUT_DIR DRAFTS_DIR OBJECT ...

Each OBJECT is one unit: an object of the matching build, compiled or
assembled from a source in src/ or lib/. As objdiff does, each routine of it
is compared with its build in the NON_MATCHING build in DRAFTS_DIR. Only the
routines the object marks NAME.NON_MATCHING, as INCLUDE_ASM does, are built
from other source there, so the rest match without comparing; so does the
data that INCLUDE_RODATA doesn't mark. A draft is scored the way objdiff
scores it: the instructions are paired by opcode and each difference costs
a penalty, out of 100 for each of the original's instructions. A draft that
compiles to the original bytes matches, even while its parent's draft keeps
it from the matching build. A routine without a draft has no fuzzy match,
which progress_map.py shows as not written.

Writes OUT_DIR/report.json and OUT_DIR/regs.json, {unit: {routine: size}}
of the drafts that differ from the original in register numbers only.
progress_map.py draws them; the Makefile's progress target runs both.
"""

import difflib
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import rabbitizer
from numpy.typing import NDArray

from fncompare import (
    STT_FUNC,
    ObjectFile,
    Pairing,
    Routine,
    pair_rows,
    read_linked_symbols,
    read_object,
    read_object_file,
    read_object_functions,
)

MARKER_SUFFIX = ".NON_MATCHING"
CODE_SECTION = ".text"
DATA_SECTIONS = (".data", ".rodata")
STB_GLOBAL = 1
PERCENT = 100.0

# objdiff's penalties (objdiff-core/src/diff/code.rs)
PENALTY_IMMEDIATE = 1
PENALTY_REGISTER = 5
PENALTY_REPLACE = 60
PENALTY_INSERT_DELETE = 100

# The base of the hashes LongestMatcher compares runs of opcodes by. It's
# odd, so that it has an inverse modulo 2**64, at which numpy's uint64 wraps.
HASH_BASE = 0x9E3779B97F4A7C15
HASH_MODULUS = 1 << 64
# The smallest window, in pairs of opcodes, that LongestMatcher searches by
# hashing; difflib's own search is faster in smaller ones.
MIN_HASHED_WINDOW = 50000

# An instruction's operands: registers and immediates. A branch's offset is
# masked, so the parts of its destination compare equal.
OPERAND = re.compile(r"[^\s,()]+")


@dataclass
class Comparison:
    """How a draft compares with the original routine."""

    match_percent: float
    is_registers_only: bool


def read_instruction(word: int) -> tuple[str, list[str]]:
    """Return the opcode and the operands of the instruction WORD."""
    instruction = rabbitizer.Instruction(word)
    _, _, operands = instruction.disassemble().partition(" ")
    return instruction.getOpcodeName(), OPERAND.findall(operands)


def get_pair_penalty(original: int, draft: int) -> tuple[int, bool]:
    """Return what a paired instruction costs, and if only registers differ."""
    if original == draft:
        return 0, True
    original_opcode, original_operands = read_instruction(original)
    draft_opcode, draft_operands = read_instruction(draft)
    if original_opcode != draft_opcode or len(original_operands) != len(draft_operands):
        return PENALTY_REPLACE, False
    penalty = 0
    is_registers_only = True
    for original_operand, draft_operand in zip(
        original_operands, draft_operands, strict=True
    ):
        if original_operand == draft_operand:
            continue
        if original_operand.startswith("$") and draft_operand.startswith("$"):
            penalty += PENALTY_REGISTER
        else:
            penalty += PENALTY_IMMEDIATE
            is_registers_only = False
    return penalty, is_registers_only


class LongestMatcher(difflib.SequenceMatcher[str]):
    """A SequenceMatcher without junk that finds long matches by hashing.

    difflib's search for the longest match visits each pair of equal opcodes
    in the window, millions in a routine of thousands of instructions, and
    searches each window it splits off again. This one binary-searches the
    match's length, comparing the hashes of the runs of that length on each
    side, and takes the match difflib's search takes: the earliest in A, then
    in B. A match whose hashes are equal but whose opcodes aren't, which is
    vanishingly rare, falls back on difflib's search.
    """

    def __init__(self, a: list[str], b: list[str]) -> None:
        """Match A with B."""
        super().__init__(None, a, b, autojunk=False)
        # SequenceMatcher's stubs don't type its own a and b
        self.sides: tuple[list[str], list[str]] = (a, b)
        codes: dict[str, int] = {}
        length = max(len(a), len(b))
        # B**i, and the sums of each side's codes times B**-i up to each index,
        # so that a run's hash is the difference of two sums times B**start
        self.powers: NDArray[np.uint64] = np.ones(length + 1, dtype=np.uint64)
        self.powers[1:] = np.cumprod(np.full(length, HASH_BASE, dtype=np.uint64))
        inverse_powers = np.cumprod(
            np.full(length, pow(HASH_BASE, -1, HASH_MODULUS), dtype=np.uint64)
        )
        self.sums: list[NDArray[np.uint64]] = []
        for sequence in (a, b):
            sequence_codes = np.array(
                [codes.setdefault(opcode, len(codes) + 1) for opcode in sequence],
                dtype=np.uint64,
            )
            sums = np.zeros(len(sequence) + 1, dtype=np.uint64)
            sums[1:] = (sequence_codes * inverse_powers[: len(sequence)]).cumsum()
            self.sums.append(sums)

    def hash_runs(
        self, side: int, start: int, end: int, length: int
    ) -> NDArray[np.uint64]:
        """Return the hash of each run of LENGTH in [START, END) of SIDE."""
        sums = self.sums[side]
        starts = slice(start, end - length + 1)
        return (sums[start + length : end + 1] - sums[starts]) * self.powers[starts]

    def find_longest_match(  # pyright: ignore[reportImplicitOverride]
        self, alo: int = 0, ahi: int | None = None, blo: int = 0, bhi: int | None = None
    ) -> difflib.Match:
        """Return the longest match in the window, as difflib's search does.

        typing.override is newer than the tools' Python 3.10.
        """
        a, b = self.sides
        ahi = len(a) if ahi is None else ahi
        bhi = len(b) if bhi is None else bhi
        if (ahi - alo) * (bhi - blo) < MIN_HASHED_WINDOW:
            return super().find_longest_match(alo, ahi, blo, bhi)
        # A run as long as SHORTEST is in both, as far as the hashes tell, and
        # none as long as LONGEST is.
        shortest, longest = 0, min(ahi - alo, bhi - blo) + 1
        while longest - shortest > 1:
            length = (shortest + longest) // 2
            in_both = find_shared(
                self.hash_runs(0, alo, ahi, length), self.hash_runs(1, blo, bhi, length)
            )
            if in_both.any():
                shortest = length
            else:
                longest = length
        if not shortest:
            return difflib.Match(alo, blo, 0)
        a_hashes = self.hash_runs(0, alo, ahi, shortest)
        b_hashes = self.hash_runs(1, blo, bhi, shortest)
        i = alo + int(find_shared(a_hashes, b_hashes).argmax())
        j = blo + int(find_shared(b_hashes, a_hashes[i - alo : i - alo + 1]).argmax())
        if a[i : i + shortest] != b[j : j + shortest]:
            return super().find_longest_match(alo, ahi, blo, bhi)
        return difflib.Match(i, j, shortest)


def find_shared(
    hashes: NDArray[np.uint64], others: NDArray[np.uint64]
) -> NDArray[np.bool_]:
    """Return whether each of HASHES is among OTHERS."""
    # numpy's stubs leave part of isin's signature untyped.
    return np.isin(hashes, others)  # pyright: ignore[reportUnknownMemberType]


def compare_routine(original: Routine, draft: Routine) -> Comparison:
    """Compare DRAFT with ORIGINAL, as objdiff's diff_code does.

    objdiff pairs the instructions with a patience diff of their opcodes;
    difflib's matcher pairs nearly all of them the same way. A paired branch
    that lands on another instruction, or a field that refers to another
    place, costs what a register does, as objdiff's relocations do.
    """
    opcodes = [
        [read_instruction(word)[0] for word in routine.words]
        for routine in (original, draft)
    ]
    matcher = LongestMatcher(*opcodes)
    pairing = Pairing(original, draft, pair_rows(matcher.get_opcodes()))
    penalty = 0
    is_registers_only = True
    for original_index, draft_index in pairing.rows:
        if original_index is None or draft_index is None:
            penalty += PENALTY_INSERT_DELETE
            is_registers_only = False
            continue
        pair_penalty, is_pair_registers_only = get_pair_penalty(
            original.words[original_index], draft.words[draft_index]
        )
        penalty += pair_penalty
        is_registers_only &= is_pair_registers_only
        is_same_opcode = opcodes[0][original_index] == opcodes[1][draft_index]
        if is_same_opcode and not pairing.has_same_places(original_index, draft_index):
            penalty += PENALTY_REGISTER
            is_registers_only = False
    most = len(original.words) * PENALTY_INSERT_DELETE
    if not most:
        return Comparison(PERCENT, is_registers_only)
    return Comparison(PERCENT * (1 - min(penalty, most) / most), is_registers_only)


@dataclass
class Symbol:
    """A symbol an object defines."""

    name: str
    section: str
    size: int
    is_function: bool
    is_global: bool


@dataclass
class Measures:
    """The sizes and counts that objdiff's report gives a unit."""

    total_code: int = 0
    matched_code: int = 0
    fuzzy_code: float = 0.0
    total_functions: int = 0
    matched_functions: int = 0
    total_data: int = 0
    matched_data: int = 0

    def add(self, other: "Measures") -> None:
        """Add OTHER's sizes and counts to these."""
        self.total_code += other.total_code
        self.matched_code += other.matched_code
        self.fuzzy_code += other.fuzzy_code
        self.total_functions += other.total_functions
        self.matched_functions += other.matched_functions
        self.total_data += other.total_data
        self.matched_data += other.matched_data

    def to_report(self) -> dict[str, float]:
        """Return these as objdiff's measures, with its percentages."""

        def get_percent(part: float, whole: int) -> float:
            return PERCENT * part / whole if whole else PERCENT

        return {
            "total_code": self.total_code,
            "matched_code": self.matched_code,
            "matched_code_percent": get_percent(self.matched_code, self.total_code),
            "fuzzy_match_percent": get_percent(self.fuzzy_code, self.total_code),
            "total_functions": self.total_functions,
            "matched_functions": self.matched_functions,
            "matched_functions_percent": get_percent(
                self.matched_functions, self.total_functions
            ),
            "total_data": self.total_data,
            "matched_data": self.matched_data,
            "matched_data_percent": get_percent(self.matched_data, self.total_data),
        }


def read_symbols(obj: ObjectFile) -> list[Symbol]:
    """Return the symbols OBJ defines in its sections."""
    return [
        Symbol(
            name=symbol.name,
            section=obj.section_names[symbol.section],
            size=symbol.size,
            is_function=symbol.type == STT_FUNC,
            is_global=symbol.bind == STB_GLOBAL,
        )
        for symbol in obj.symbols
        if symbol.section is not None
    ]


def get_section_size(obj: ObjectFile, names: tuple[str, ...]) -> int:
    """Return the total size of OBJ's sections named NAMES."""
    return sum(
        obj.get_section_size(index)
        for index, name in enumerate(obj.section_names)
        if name in names
    )


@dataclass
class Unit:
    """One unit's measures and routines, as objdiff's report gives them."""

    measures: Measures = field(default_factory=Measures)
    functions: list[dict[str, object]] = field(default_factory=list)
    # {routine: size} of the drafts that differ in register numbers only
    registers_only: dict[str, int] = field(default_factory=dict)


def measure_unit(obj_path: Path, draft_path: Path) -> Unit:
    """Measure the unit built as OBJ_PATH, whose drafts are in DRAFT_PATH."""
    obj = read_object(obj_path)
    symbols = read_symbols(obj)
    unit = Unit(Measures(total_data=get_section_size(obj, DATA_SECTIONS)))
    unmatched = {
        symbol.name.removesuffix(MARKER_SUFFIX)
        for symbol in symbols
        if symbol.name.endswith(MARKER_SUFFIX)
    }
    # Assembly sources label their routines without typing them as functions.
    # objdiff skips symbols without a size, such as their entry labels.
    functions = [
        symbol
        for symbol in symbols
        if symbol.section == CODE_SECTION
        and (symbol.is_function or symbol.is_global)
        and symbol.size
        and not symbol.name.endswith(MARKER_SUFFIX)
    ]
    originals = read_object_functions(obj) if unmatched else {}
    drafts = read_object_file(draft_path) if unmatched else {}
    addresses = read_linked_symbols()
    measures = unit.measures
    for symbol in functions:
        report: dict[str, object] = {"name": symbol.name, "size": symbol.size}
        if symbol.name in addresses:
            report["metadata"] = {"virtual_address": addresses[symbol.name]}
        unit.functions.append(report)
        measures.total_code += symbol.size
        measures.total_functions += 1
        if symbol.name not in unmatched:
            comparison = Comparison(PERCENT, is_registers_only=False)
        elif symbol.name in drafts:
            comparison = compare_routine(originals[symbol.name], drafts[symbol.name])
        else:
            continue
        report["fuzzy_match_percent"] = comparison.match_percent
        measures.fuzzy_code += symbol.size * comparison.match_percent / PERCENT
        if comparison.match_percent == PERCENT:
            measures.matched_code += symbol.size
            measures.matched_functions += 1
        elif comparison.is_registers_only:
            unit.registers_only[symbol.name] = symbol.size
    measures.matched_data = measures.total_data - sum(
        symbol.size
        for symbol in symbols
        if symbol.name in unmatched and symbol.section in DATA_SECTIONS
    )
    return unit


def get_category(source: Path) -> str:
    """Return the progress category of the unit built from SOURCE."""
    top, *rest = source.parts
    if top == "lib":
        return rest[0]
    if rest[0] == "libultra":
        return "ultralib"
    return "overlays" if len(rest) > 1 else "game"


def main() -> None:
    """Write the report of the objects named on the command line."""
    out_dir, drafts_dir, *obj_paths = map(Path, sys.argv[1:])
    total = Measures()
    units: list[dict[str, object]] = []
    registers_only: dict[str, dict[str, int]] = {}
    for obj_path in obj_paths:
        built = obj_path.relative_to(obj_path.parts[0])
        source = built.with_suffix("")
        name = str(source.with_suffix(""))
        unit = measure_unit(obj_path, drafts_dir / built)
        total.add(unit.measures)
        units.append(
            {
                "name": name,
                "measures": unit.measures.to_report(),
                "functions": unit.functions,
                "metadata": {
                    "progress_categories": [get_category(source)],
                    "source_path": str(source),
                },
            }
        )
        if unit.registers_only:
            registers_only[name] = unit.registers_only
    report = {"measures": total.to_report(), "units": units}
    for file_name, value in (("report.json", report), ("regs.json", registers_only)):
        _ = (out_dir / file_name).write_text(json.dumps(value, indent=1) + "\n")


if __name__ == "__main__":
    main()
