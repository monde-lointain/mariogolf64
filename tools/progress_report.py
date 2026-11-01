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

import rabbitizer
from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

from fncompare import (
    Pairing,
    Routine,
    pair_rows,
    read_entry_field,
    read_entry_int,
    read_linked_symbols,
    read_object_file,
)

MARKER_SUFFIX = ".NON_MATCHING"
CODE_SECTION = ".text"
DATA_SECTIONS = (".data", ".rodata")
PERCENT = 100.0

# objdiff's penalties (objdiff-core/src/diff/code.rs)
PENALTY_IMMEDIATE = 1
PENALTY_REGISTER = 5
PENALTY_REPLACE = 60
PENALTY_INSERT_DELETE = 100

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
    matcher = difflib.SequenceMatcher(None, *opcodes, autojunk=False)
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


def read_symbols(elf: ELFFile) -> list[Symbol]:
    """Return the symbols ELF defines in its sections."""
    table = elf.get_section_by_name(".symtab")
    if not isinstance(table, SymbolTableSection):
        return []
    symbols: list[Symbol] = []
    for symbol in table.iter_symbols():
        index = read_entry_field(symbol.entry, "st_shndx")
        info = read_entry_field(symbol.entry, "st_info")
        if not isinstance(index, int) or not isinstance(info, Container):
            continue
        symbols.append(
            Symbol(
                name=symbol.name,
                section=elf.get_section(index).name,
                size=read_entry_int(symbol.entry, "st_size"),
                is_function=read_entry_field(info, "type") == "STT_FUNC",
                is_global=read_entry_field(info, "bind") == "STB_GLOBAL",
            )
        )
    return symbols


def get_section_size(elf: ELFFile, names: tuple[str, ...]) -> int:
    """Return the total size of ELF's sections named NAMES."""
    return sum(
        section.data_size for section in elf.iter_sections() if section.name in names
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
    with obj_path.open("rb") as obj_file:
        elf = ELFFile(obj_file)
        symbols = read_symbols(elf)
        unit = Unit(Measures(total_data=get_section_size(elf, DATA_SECTIONS)))
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
    originals = read_object_file(obj_path) if unmatched else {}
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
