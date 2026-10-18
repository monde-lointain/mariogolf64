#!/usr/bin/env python3
"""Find data symbols whose linked size exceeds their recorded symbol size."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SYMBOL_FILES = ("symbol_addrs.txt", "ghidra_symbols.txt")
DEFAULT_ELF = ROOT / "build/mariogolf64.elf"
SYMBOL_LINE = re.compile(
    r"^\s*([A-Za-z_$][A-Za-z0-9_$]*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;"
    r"\s*(?://(.*))?$"
)
SIZE_ATTRIBUTE = re.compile(r"(?:^|\s)size:(0x[0-9A-Fa-f]+|[0-9]+)(?:\s|$)")
NON_MATCHING_SUFFIX = ".NON_MATCHING"


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return the POSIX path relative to root, or absolute if outside root."""
    resolved = path.resolve()
    try:
        return resolved.relative_to(root.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


@dataclass(frozen=True)
class RecordedSymbol:
    name: str
    address: int
    size: int
    source: Path
    line_number: int


@dataclass(frozen=True)
class OversizedSymbol:
    recorded: RecordedSymbol
    linked_size: int


def load_recorded_symbols(path: Path) -> dict[str, RecordedSymbol]:
    """Load sized data symbols from one symbol metadata file."""
    symbols: dict[str, RecordedSymbol] = {}
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        match = SYMBOL_LINE.match(line)
        if match is None:
            continue

        attributes = match.group(3) or ""
        if "type:func" in attributes:
            continue
        size_match = SIZE_ATTRIBUTE.search(attributes)
        if size_match is None:
            continue

        name = match.group(1)
        symbols[name] = RecordedSymbol(
            name=name,
            address=int(match.group(2), 0),
            size=int(size_match.group(1), 0),
            source=path,
            line_number=line_number,
        )
    return symbols


def load_all_recorded_symbols(paths: Sequence[Path]) -> dict[str, RecordedSymbol]:
    """Merge symbol files; the first file listing a symbol wins."""
    symbols: dict[str, RecordedSymbol] = {}
    for path in paths:
        for name, symbol in load_recorded_symbols(path).items():
            symbols.setdefault(name, symbol)
    return symbols


def parse_readelf_symbols(
    output: str, recorded: dict[str, RecordedSymbol]
) -> dict[str, int]:
    """Return the largest linked OBJECT size for each recorded symbol."""
    linked_sizes: dict[str, int] = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 8 or fields[3] != "OBJECT" or fields[6] == "UND":
            continue

        name = fields[7]
        if name.endswith(NON_MATCHING_SUFFIX):
            name = name[: -len(NON_MATCHING_SUFFIX)]
        symbol = recorded.get(name)
        if symbol is None:
            continue

        try:
            address = int(fields[1], 16)
            size = int(fields[2], 0)
        except ValueError:
            continue
        if address != symbol.address:
            continue
        linked_sizes[name] = max(linked_sizes.get(name, 0), size)
    return linked_sizes


def find_oversized_symbols(
    symbol_addrs: Sequence[Path], readelf_output: str
) -> list[OversizedSymbol]:
    """Compare compiler-emitted object sizes with recorded symbol sizes."""
    recorded = load_all_recorded_symbols(symbol_addrs)
    linked_sizes = parse_readelf_symbols(readelf_output, recorded)
    return [
        OversizedSymbol(symbol, linked_sizes[symbol.name])
        for symbol in recorded.values()
        if linked_sizes.get(symbol.name, 0) > symbol.size
    ]


def read_elf_symbols(elf: Path, readelf: str) -> str:
    """Read the complete ELF symbol table using readelf."""
    try:
        result = subprocess.run(
            [readelf, "-Ws", str(elf)],
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as error:
        raise RuntimeError(f"readelf executable not found: {readelf}") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.strip() or str(error)
        raise RuntimeError(f"could not read {elf}: {detail}") from error
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--symbol-addrs",
        type=Path,
        action="append",
        default=None,
        help=(
            "symbol metadata to check; repeatable "
            "(default: symbol_addrs.txt and ghidra_symbols.txt at the repo root)"
        ),
    )
    parser.add_argument(
        "--elf",
        type=Path,
        default=DEFAULT_ELF,
        help="linked ELF containing compiler-emitted sizes (default: %(default)s)",
    )
    parser.add_argument(
        "--readelf",
        default="mips-linux-gnu-readelf",
        help="readelf executable (default: %(default)s)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="include recorded and linked sizes after each symbol name",
    )
    args = parser.parse_args()

    symbol_paths = args.symbol_addrs
    if symbol_paths is None:
        symbol_paths = [ROOT / name for name in DEFAULT_SYMBOL_FILES]
    for path in symbol_paths:
        if not path.is_file():
            parser.error(f"symbol metadata not found: {path}")
    if not args.elf.is_file():
        parser.error(f"linked ELF not found: {args.elf}; build the project first")

    try:
        readelf_output = read_elf_symbols(args.elf, args.readelf)
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    for mismatch in find_oversized_symbols(symbol_paths, readelf_output):
        recorded = mismatch.recorded
        if args.verbose:
            print(
                f"{recorded.name}: "
                f"symbol_addrs={recorded.size:#x} "
                f"linked={mismatch.linked_size:#x} "
                f"at={repo_rel(recorded.source)}:{recorded.line_number}"
            )
        else:
            print(recorded.name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
