#!/usr/bin/env python3
"""Measure the build's progress as objdiff's report.json, on standard output.

    progress_report.py DRAFTS_DIR OBJECT ...

Each OBJECT is one unit: an object of the matching build, compiled or
assembled from a source in src/ or lib/. A routine of it matches unless the
object marks it NAME.NON_MATCHING, as INCLUDE_ASM and INCLUDE_RODATA do, and
so does its data. Assembly sources assemble to the original bytes, so they
match whole. The fuzzy match of a routine that doesn't match is how much of
its draft, compiled in the NON_MATCHING build in DRAFTS_DIR, is the same as
the original, word for word.

progress_map.py draws the report; the Makefile's progress target runs both.
"""

import json
import sys
from dataclasses import dataclass
from pathlib import Path

from elftools.construct.lib.container import Container
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

from fncompare import (
    read_entry_field,
    read_entry_int,
    read_object_file,
    score,
)

MARKER_SUFFIX = ".NON_MATCHING"
CODE_SECTION = ".text"
DATA_SECTIONS = (".data", ".rodata")
PERCENT = 100.0


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


def measure_unit(obj_path: Path, draft_path: Path) -> Measures:
    """Measure the unit built as OBJ_PATH, whose drafts are in DRAFT_PATH."""
    with obj_path.open("rb") as obj_file:
        elf = ELFFile(obj_file)
        symbols = read_symbols(elf)
        measures = Measures(
            total_code=get_section_size(elf, (CODE_SECTION,)),
            total_data=get_section_size(elf, DATA_SECTIONS),
        )
    unmatched = {
        symbol.name.removesuffix(MARKER_SUFFIX)
        for symbol in symbols
        if symbol.name.endswith(MARKER_SUFFIX)
    }
    # Assembly sources label their routines without typing them as functions
    functions = [
        symbol
        for symbol in symbols
        if symbol.section == CODE_SECTION
        and (symbol.is_function or symbol.is_global)
        and not symbol.name.endswith(MARKER_SUFFIX)
    ]
    unmatched_functions = [symbol for symbol in functions if symbol.name in unmatched]
    unmatched_code = sum(symbol.size for symbol in unmatched_functions)
    measures.total_functions = len(functions)
    measures.matched_functions = len(functions) - len(unmatched_functions)
    measures.matched_code = measures.total_code - unmatched_code
    measures.fuzzy_code = measures.matched_code
    if unmatched_functions:
        originals = read_object_file(obj_path)
        drafts = read_object_file(draft_path)
        for symbol in unmatched_functions:
            draft = drafts.get(symbol.name, [])
            measures.fuzzy_code += symbol.size * score(originals[symbol.name], draft)
    measures.matched_data = measures.total_data - sum(
        symbol.size
        for symbol in symbols
        if symbol.name in unmatched and symbol.section in DATA_SECTIONS
    )
    return measures


def get_category(source: Path) -> str:
    """Return the progress category of the unit built from SOURCE."""
    top, *rest = source.parts
    if top == "lib":
        return rest[0]
    if rest[0] == "libultra":
        return "ultralib"
    return "overlays" if len(rest) > 1 else "game"


def main() -> None:
    """Print the report of the objects named on the command line."""
    drafts_dir, *obj_paths = map(Path, sys.argv[1:])
    total = Measures()
    units: list[dict[str, object]] = []
    for obj_path in obj_paths:
        built = obj_path.relative_to(obj_path.parts[0])
        source = built.with_suffix("")
        measures = measure_unit(obj_path, drafts_dir / built)
        total.add(measures)
        units.append(
            {
                "name": str(source.with_suffix("")),
                "measures": measures.to_report(),
                "metadata": {
                    "progress_categories": [get_category(source)],
                    "source_path": str(source),
                },
            }
        )
    report = {"measures": total.to_report(), "units": units}
    json.dump(report, sys.stdout, indent=1)
    print()


if __name__ == "__main__":
    main()
