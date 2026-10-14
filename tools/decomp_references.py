#!/usr/bin/env python3
"""Find advisory C references by ROM assembly shape, including explicit worktrees."""

from __future__ import annotations

import argparse
import hashlib
import re
from collections import Counter
from dataclasses import dataclass, replace
from difflib import SequenceMatcher
from pathlib import Path

import decomp_common as dc
import yaml
from cpreprocess import _strip_comments
from pick_target_index import _iter_upstream_functions
from rom_build import build_dir

INSTRUCTION = re.compile(
    r"/\*\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{8})\s*\*/\s+(\S+)\s*(.*)"
)
STUB = re.compile(r'INCLUDE_ASM\s*\(\s*"[^"]*"\s*,\s*(\w+)\s*\)')
REGISTER = re.compile(r"\$(f\w+|[A-Za-z0-9]+)")
SYMBOL = re.compile(r"%(?:hi|lo)\([^)]*\)|\.L\w+|\bD_(?:ovl\d+_)?[0-9A-Fa-f]+\b")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def rom_identity(root: Path) -> str:
    text = (root / "mariogolf64.yaml").read_text()
    match = re.search(r"^sha1:\s*([0-9A-Fa-f]{40})\s*$", text, re.MULTILINE)
    if match is None:
        raise ValueError(f"missing ROM identity in {root}/mariogolf64.yaml")
    return match[1].lower()


@dataclass(frozen=True)
class Function:
    name: str
    segment: str
    rom: int
    vram: int
    words: tuple[str, ...]
    shape: tuple[str, ...]
    calls: frozenset[str]
    assembly: Path
    line: int
    bank: str = ""

    def identity(self, rom_sha1: str) -> str:
        # VRAM and names are not globally unique across overlay banks or renames.
        return f"{rom_sha1}:{self.bank or self.segment.split('/')[0]}:{self.rom:X}"

    def provenance(self, root: Path, rom_sha1: str) -> dict:
        return {
            "identity": self.identity(rom_sha1),
            "func": self.name,
            "segment": self.segment,
            "bank": self.bank,
            "rom": f"0x{self.rom:X}",
            "vram": f"0x{self.vram:08X}",
            "instructions": len(self.words),
            "assembly": str(self.assembly.relative_to(root)),
            "assembly_line": self.line,
            "target_sha256": digest(bytes.fromhex("".join(self.words))),
        }


def parse_assembly(path: Path, asm_root: Path) -> list[Function]:
    """Read original words separately from deliberately lossy similarity features."""
    result = []
    name = None
    start = None
    words, shape, calls = [], [], set()
    line_number = 0

    def finish():
        if name is not None and start is not None:
            result.append(
                Function(
                    name,
                    path.relative_to(asm_root).with_suffix("").as_posix(),
                    start[0],
                    start[1],
                    tuple(words),
                    tuple(shape),
                    frozenset(calls),
                    path,
                    line_number,
                )
            )

    for number, line in enumerate(path.read_text(errors="replace").splitlines(), 1):
        label = dc.GLABEL_RE.match(line)
        if label:
            finish()
            name, start, line_number = label[1], None, number
            words, shape, calls = [], [], set()
            continue
        if re.match(r"^\s*endlabel\b", line):
            finish()
            name, start = None, None
            continue
        instruction = INSTRUCTION.search(line)
        if name is None or instruction is None:
            continue
        if start is None:
            start = (int(instruction[1], 16), int(instruction[2], 16))
        words.append(instruction[3])
        opcode, operands = instruction[4], instruction[5].split("/*", 1)[0].strip()
        if opcode == "jal":
            calls.add(operands)
            operands = "call"
        operands = REGISTER.sub(
            lambda m: "freg" if m[1].startswith("f") else "reg", operands
        )
        operands = SYMBOL.sub("symbol", operands)
        shape.append(opcode + " " + re.sub(r"\s+", "", operands))
    finish()
    return result


def function_index(root: Path, binary_dir: Path | None = None) -> list[Function]:
    directory = (binary_dir or root / "build/us") / "work/asm"
    if not directory.is_dir():
        raise FileNotFoundError(f"extract reference assembly first: {directory}")
    functions = []
    segments = yaml.safe_load((root / "mariogolf64.yaml").read_text())["segments"]
    banks = []
    for index, segment in enumerate(segments[:-1]):
        if isinstance(segment, dict) and segment["type"] == "code":
            following = segments[index + 1]
            end = following["start"] if isinstance(following, dict) else following[0]
            banks.append((segment["name"], segment["start"], end))
    for path in sorted(directory.rglob("*.s")):
        if path.relative_to(directory).parts[0] in ("data", "matching", "nonmatchings"):
            continue
        for function in parse_assembly(path, directory):
            owners = [name for name, start, end in banks if start <= function.rom < end]
            if len(owners) != 1:
                raise ValueError(
                    f"{function.name}: ROM offset has {len(owners)} bank owners"
                )
            functions.append(replace(function, bank=owners[0]))
    return functions


def find_function(functions: list[Function], name: str) -> Function:
    matches = [function for function in functions if function.name == name]
    if len(matches) != 1:
        raise ValueError(
            f"{name}: expected one bank-qualified assembly label, found {len(matches)}"
        )
    return matches[0]


def c_sources(root: Path) -> dict[str, list[Path]]:
    """Conservative source discovery; a retained ASM stub excludes partial C bodies."""
    definitions: dict[str, list[Path]] = {}
    stubs = set()
    for source in sorted((root / "src").rglob("*.c")):
        text = source.read_text(errors="replace")
        stubs.update(STUB.findall(_strip_comments(text)))
        for name, _ in _iter_upstream_functions(text):
            definitions.setdefault(name, []).append(source)
    return {name: paths for name, paths in definitions.items() if name not in stubs}


def similarity(query: Function, reference: Function) -> float | None:
    if not query.shape or not reference.shape:
        return None
    sequence = SequenceMatcher(
        None, query.shape, reference.shape, autojunk=False
    ).ratio()
    left = Counter(item.split(" ", 1)[0] for item in query.shape)
    right = Counter(item.split(" ", 1)[0] for item in reference.shape)
    opcodes = sum((left & right).values()) / sum((left | right).values())
    if query.calls or reference.calls:
        callees = len(query.calls & reference.calls) / len(
            query.calls | reference.calls
        )
        return 0.65 * sequence + 0.2 * opcodes + 0.15 * callees
    return (0.65 * sequence + 0.2 * opcodes) / 0.85


def reference_index(
    roots: list[Path], expected_rom: str
) -> tuple[list[tuple], list[dict]]:
    entries, coverage = [], []
    for root in dict.fromkeys(path.resolve() for path in roots):
        try:
            actual = rom_identity(root)
            if actual != expected_rom:
                raise ValueError(f"different ROM identity: {actual}")
            functions = function_index(
                root, build_dir() if root == dc.ROOT_DIR else None
            )
            sources = c_sources(root)
            identities: dict[str, set[str]] = {}
            for function in functions:
                identities.setdefault(function.name, set()).add(
                    function.identity(actual)
                )
            ambiguous = {
                name for name, owners in identities.items() if len(owners) != 1
            }
            for function in functions:
                # Neither ambiguous C definitions nor ambiguous bank associations
                # can establish which implementation supplies a reference.
                paths = sources.get(function.name, [])
                if len(paths) == 1 and function.name not in ambiguous:
                    source = paths[0]
                    entries.append(
                        (root, function, source, digest(source.read_bytes()))
                    )
            coverage.append(
                {
                    "root": str(root),
                    "available": True,
                    "c_references": sum(1 for entry in entries if entry[0] == root),
                    "ambiguous_labels": sorted(ambiguous),
                }
            )
        except (OSError, ValueError) as error:
            coverage.append(
                {"root": str(root), "available": False, "reason": str(error)}
            )
    return entries, coverage


def rank_references(
    query: Function, rom_sha1: str, entries: list[tuple], limit: int = 3
) -> list[dict]:
    ranked = []
    for root, reference, source, source_hash in entries:
        if root == dc.ROOT_DIR and reference.identity(rom_sha1) == query.identity(
            rom_sha1
        ):
            continue
        score = similarity(query, reference)
        if score is None:
            continue
        ranked.append(
            {
                **reference.provenance(root, rom_sha1),
                "root": str(root),
                "source": str(source.relative_to(root)),
                "source_sha256": source_hash,
                "similarity": round(score, 6),
                "status": "source-present-unverified",
            }
        )
    ranked.sort(
        key=lambda row: (
            -row["similarity"],
            row["root"] != str(dc.ROOT_DIR),
            row["identity"],
            row["source"],
        )
    )
    result, seen = [], set()
    for row in ranked:
        if row["identity"] not in seen:
            result.append(row)
            seen.add(row["identity"])
        if len(result) == limit:
            break
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True)
    parser.add_argument("--reference-root", type=Path, action="append", default=[])
    parser.add_argument("-n", type=int, default=3)
    args = parser.parse_args()
    if args.n <= 0:
        parser.error("-n must be positive")
    try:
        rom = rom_identity(dc.ROOT_DIR)
        query = find_function(function_index(dc.ROOT_DIR, build_dir()), args.func)
        entries, coverage = reference_index([dc.ROOT_DIR, *args.reference_root], rom)
        references = rank_references(query, rom, entries, args.n)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    dc.emit(
        {
            "target": query.provenance(dc.ROOT_DIR, rom),
            "references": references,
            "best_similarity": references[0]["similarity"] if references else None,
            "coverage": coverage,
            "authority": "advisory; not a bank verdict",
        }
    )


if __name__ == "__main__":
    main()
