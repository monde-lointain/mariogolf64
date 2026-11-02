#!/usr/bin/env python3
"""Measure the commits before the progress history began, from their sources.

    progress_seed.py OUT CONFIG FIRST_BUILD OBJECT ...

The progress target measures the build it runs on, so the history it keeps
starts at its first run, FIRST_BUILD, a commit of history.json. This measures
each commit before that one from its sources alone, without building it,
and writes the points to OUT as progress_map.py's history.json has them,
dated in UTC. The Makefile passes OUT, tools/progress_seed.json, to
progress_map.py with the history, which keeps a build's own point over the
seed's.

Each OBJECT is a unit of the current matching build, as for
progress_report.py, and CONFIG is the current splat configuration, which
gives the ROM offset of each unit's code. Together they give every
routine's offset and size. A commit matched a routine where its splat
configuration put the routine's offset in a C or handwritten assembly
subsegment, and the C source didn't include the routine's asm with
INCLUDE_ASM. Its asm subsegments and INCLUDE_ASM'd routines didn't match. So
every commit is measured against the current build's total code, unlike a
build of that commit, which leaves out its asm subsegments.

As the commits have no drafts measured, the points have no fuzzy match or
registers only. A commit that matched no more than the one before it adds
no point.
"""

import bisect
import json
import os
import re
import subprocess
import sys
from collections.abc import Iterator
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import IO, cast

import yaml

from fncompare import STT_FUNC, read_object
from progress_report import CODE_SECTION, MARKER_SUFFIX, PERCENT, STB_GLOBAL

CONFIG_NAME = "mariogolf64.yaml"
SOURCE_DIR = Path("src")
SYMBOL_ADDRS = "symbol_addrs.txt"
# The subsegments whose routines match: those built from C, less the
# routines they INCLUDE_ASM, and the handwritten assembly
C_TYPE = "c"
MATCHED_TYPES = (C_TYPE, "hasm")
INCLUDE_ASM = re.compile(r'\bINCLUDE_ASM\(\s*"[^"]*"\s*,\s*(\w+)\s*\)')
# A C file another includes, as libmus's player.c does its commands
INCLUDE_C = re.compile(r'^[ \t]*#[ \t]*include[ \t]+"([^"]+\.c)"', re.MULTILINE)
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
# A symbol_addrs.txt entry: NAME = 0xADDRESS;
SYMBOL_ADDR = re.compile(r"^[ \t]*(\w+)[ \t]*=[ \t]*0x([0-9A-Fa-f]+)\s*;", re.MULTILINE)
# splat's name for a routine it hasn't been given one
DEFAULT_NAME = re.compile(r"^func_([0-9A-Fa-f]{8})")
PERCENT_DIGITS = 4
USAGE_EXIT = 2


@dataclass(frozen=True)
class Routine:
    """A routine of the current build: where its code starts in the ROM."""

    name: str
    rom: int
    size: int


@dataclass(frozen=True)
class Subsegment:
    """A subsegment of code or data in a splat configuration."""

    kind: str
    name: str
    start: int
    end: int
    # The ROM offset and address of the segment it is part of
    segment_start: int
    segment_vram: int

    def get_rom(self, vram: int) -> int:
        """Return the ROM offset of VRAM in this subsegment's segment."""
        return self.segment_start + vram - self.segment_vram

    def holds(self, rom: int) -> bool:
        """Return whether the subsegment holds ROM offset ROM."""
        return self.start <= rom < self.end


class Repository:
    """The files of the repository's commits, read through one git process."""

    def __init__(self) -> None:
        """Start the process that reads the files."""
        process = subprocess.Popen(
            ["git", "cat-file", "--batch"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )
        if process.stdin is None or process.stdout is None:
            sys.exit("git cat-file gave no pipes")
        self.requests: IO[bytes] = process.stdin
        self.replies: IO[bytes] = process.stdout
        self.texts: dict[str, str] = {}

    def read_tree(self, commit: str, paths: list[str]) -> dict[str, str]:
        """Return {path: blob} of COMMIT's files under PATHS."""
        listing = subprocess.run(
            ["git", "ls-tree", "-r", commit, "--", *paths],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        tree: dict[str, str] = {}
        for line in listing.splitlines():
            info, path = line.split("\t", 1)
            tree[path] = info.split()[2]
        return tree

    def read_text(self, blob: str) -> str:
        """Return the contents of BLOB."""
        if blob not in self.texts:
            _ = self.requests.write(blob.encode() + b"\n")
            self.requests.flush()
            size = int(self.replies.readline().split()[2])
            text = self.replies.read(size + 1)[:size]
            self.texts[blob] = text.decode(errors="replace")
        return self.texts[blob]


def read_routines(obj_paths: list[Path], config: list[Subsegment]) -> list[Routine]:
    """Return the routines of the objects at OBJ_PATHS, which CONFIG places."""
    starts = {
        subsegment.name: subsegment.start
        for subsegment in config
        if subsegment.kind in MATCHED_TYPES
    }
    routines: list[Routine] = []
    for obj_path in obj_paths:
        source = obj_path.relative_to(obj_path.parts[0]).with_suffix("")
        name = Path(source).with_suffix("").as_posix()
        name = (
            name.removeprefix(f"{SOURCE_DIR}/")
            if name.startswith(f"{SOURCE_DIR}/")
            else f"../{name}"
        )
        obj = read_object(obj_path)
        symbols = [
            symbol
            for symbol in obj.symbols
            if symbol.section is not None
            and obj.section_names[symbol.section] == CODE_SECTION
            and (symbol.type == STT_FUNC or symbol.bind == STB_GLOBAL)
            and symbol.size
            and not symbol.name.endswith(MARKER_SUFFIX)
        ]
        # A unit of bss only, such as errno, has no code subsegment
        if symbols and name not in starts:
            sys.exit(f"the configuration has no subsegment {name} for {obj_path}")
        routines.extend(
            Routine(symbol.name, starts[name] + symbol.value, symbol.size)
            for symbol in symbols
        )
    return routines


def read_entries(value: object) -> Iterator[dict[str, object]]:
    """Yield the entries of a list of splat segments or subsegments."""
    if not isinstance(value, list):
        return
    for entry in cast("list[object]", value):
        if isinstance(entry, dict):
            yield cast("dict[str, object]", entry)
        elif isinstance(entry, list):
            fields = cast("list[object]", entry)
            yield dict(zip(("start", "type", "name"), fields, strict=False))


def read_config(text: str) -> list[Subsegment]:
    """Return the subsegments of the code segments of splat configuration TEXT."""
    config = cast("object", yaml.load(text, Loader=yaml.CSafeLoader))
    if not isinstance(config, dict):
        return []
    segments = list(read_entries(cast("dict[str, object]", config).get("segments")))
    starts = [segment.get("start") for segment in segments]
    subsegments: list[Subsegment] = []
    for index, segment in enumerate(segments):
        segment_start, vram = segment.get("start"), segment.get("vram")
        if not isinstance(segment_start, int) or not isinstance(vram, int):
            continue
        segment_end = next(
            (start for start in starts[index + 1 :] if isinstance(start, int)),
            segment_start,
        )
        entries = [
            entry
            for entry in read_entries(segment.get("subsegments"))
            if isinstance(entry.get("start"), int)
        ]
        for entry, end in zip(
            entries,
            [cast("int", entry["start"]) for entry in entries[1:]] + [segment_end],
            strict=True,
        ):
            start = cast("int", entry["start"])
            subsegments.append(
                Subsegment(
                    kind=str(entry.get("type")),
                    name=str(entry.get("name", f"{start:X}")),
                    start=start,
                    end=end,
                    segment_start=segment_start,
                    segment_vram=vram,
                )
            )
    return subsegments


def read_symbol_addrs(text: str) -> dict[str, list[int]]:
    """Return {name: [address, ...]} of symbol_addrs.txt TEXT."""
    addresses: dict[str, list[int]] = {}
    for name, address in cast("list[tuple[str, str]]", SYMBOL_ADDR.findall(text)):
        addresses.setdefault(name, []).append(int(address, 16))
    return addresses


@dataclass(frozen=True)
class Source:
    """What a C source includes: the routines it includes as asm, and C files."""

    included_asm: list[str]
    included_c: list[str]


def read_source(text: str) -> Source:
    """Return what C source TEXT includes."""
    text = COMMENT.sub("", text)
    return Source(INCLUDE_ASM.findall(text), INCLUDE_C.findall(text))


@dataclass(frozen=True)
class Placement:
    """Where a commit's splat configuration placed the current build's routines."""

    # Each C subsegment, its source and the routines it holds
    c_subsegments: list[tuple[Subsegment, Path, list[Routine]]]
    # The routines of the subsegments that match, less what they INCLUDE_ASM
    matched: list[Routine]


def place_routines(config: list[Subsegment], routines: list[Routine]) -> Placement:
    """Return where CONFIG places ROUTINES, which are in ROM order."""
    roms = [routine.rom for routine in routines]

    def get_held(subsegment: Subsegment) -> list[Routine]:
        start = bisect.bisect_left(roms, subsegment.start)
        return routines[start : bisect.bisect_left(roms, subsegment.end, start)]

    return Placement(
        c_subsegments=[
            (
                subsegment,
                Path(os.path.normpath(SOURCE_DIR / f"{subsegment.name}.c")),
                get_held(subsegment),
            )
            for subsegment in config
            if subsegment.kind == C_TYPE
        ],
        matched=sorted(
            {
                routine
                for subsegment in config
                if subsegment.kind in MATCHED_TYPES
                for routine in get_held(subsegment)
            },
            key=lambda routine: routine.rom,
        ),
    )


def find_routine(
    name: str,
    subsegment: Subsegment,
    held: list[Routine],
    addresses: dict[str, list[int]],
) -> Routine | None:
    """Return the routine NAME of SUBSEGMENT's HELD, placed by a commit's ADDRESSES."""
    vrams = list(addresses.get(name, []))
    default = DEFAULT_NAME.match(name)
    if default:
        vrams.append(int(default[1], 16))
    roms = {subsegment.get_rom(vram) for vram in vrams}
    return next((routine for routine in held if routine.rom in roms), None) or next(
        (routine for routine in held if routine.name == name), None
    )


class Measurer:
    """Measures commits against the current build's routines.

    The commits share most of their files, so each file is read once.
    """

    def __init__(self, repo: Repository, routines: list[Routine]) -> None:
        """Measure the commits of REPO against ROUTINES."""
        self.repo: Repository = repo
        self.routines: list[Routine] = routines
        self.placements: dict[str, Placement] = {}
        self.addresses: dict[str, dict[str, list[int]]] = {}
        self.sources: dict[str, Source] = {}

    def read_included_asm(self, tree: dict[str, str], path: Path) -> list[str]:
        """Return the routines the C source at PATH in TREE includes as asm."""
        blob = tree.get(path.as_posix())
        if blob is None:
            return []
        if blob not in self.sources:
            self.sources[blob] = read_source(self.repo.read_text(blob))
        source = self.sources[blob]
        names = list(source.included_asm)
        for included in source.included_c:
            names += self.read_included_asm(tree, Path(path.parent, included))
        return names

    def measure(self, commit: str) -> tuple[list[Routine], list[str]] | None:
        """Return the routines COMMIT matched, and asm it includes but can't place.

        A commit before the configuration has nothing to measure: None.
        """
        tree = self.repo.read_tree(commit, [CONFIG_NAME, SYMBOL_ADDRS, "src", "lib"])
        config_blob = tree.get(CONFIG_NAME)
        if config_blob is None:
            return None
        if config_blob not in self.placements:
            config = read_config(self.repo.read_text(config_blob))
            self.placements[config_blob] = place_routines(config, self.routines)
        placement = self.placements[config_blob]
        addresses_blob = tree.get(SYMBOL_ADDRS, "")
        if addresses_blob not in self.addresses:
            text = self.repo.read_text(addresses_blob) if addresses_blob else ""
            self.addresses[addresses_blob] = read_symbol_addrs(text)
        addresses = self.addresses[addresses_blob]
        unmatched: set[Routine] = set()
        unplaced: list[str] = []
        for subsegment, source, held in placement.c_subsegments:
            for name in self.read_included_asm(tree, source):
                routine = find_routine(name, subsegment, held, addresses)
                if routine is None:
                    unplaced.append(name)
                else:
                    unmatched.add(routine)
        return [
            routine for routine in placement.matched if routine not in unmatched
        ], unplaced


def read_commits(first_build: str) -> list[tuple[str, str]]:
    """Return the commits before FIRST_BUILD and their dates, oldest first.

    Each date keeps its commit's time zone.
    """
    log = subprocess.run(
        ["git", "log", "--reverse", "--format=%H %cI", f"{first_build}^"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return [cast("tuple[str, str]", tuple(line.split())) for line in log.splitlines()]


def main() -> None:
    """Write the seed of the commits before the one named on the command line."""
    if len(sys.argv) < 5:  # noqa: PLR2004
        sys.exit(__doc__.split("\n\n")[1] if __doc__ else USAGE_EXIT)
    out_path, config_path, first_build, *objects = sys.argv[1:]
    routines = read_routines(
        list(map(Path, objects)), read_config(Path(config_path).read_text())
    )
    total_code = sum(routine.size for routine in routines)
    routines.sort(key=lambda routine: routine.rom)
    measurer = Measurer(Repository(), routines)
    points: list[dict[str, object]] = []
    unplaced: dict[str, str] = {}
    for commit, date in read_commits(first_build):
        measured = measurer.measure(commit)
        if measured is None:
            continue
        matched, names = measured
        unplaced.update(dict.fromkeys(names, commit))
        matched_code = sum(routine.size for routine in matched)
        if (
            points
            and points[-1]["matched_code"] == matched_code
            and points[-1]["matched_functions"] == len(matched)
        ):
            continue
        points.append(
            {
                "date": datetime.fromisoformat(date)
                .astimezone(timezone.utc)
                .isoformat(),
                "commit": commit,
                "total_code": total_code,
                "matched_code": matched_code,
                "matched_code_percent": round(
                    PERCENT * matched_code / total_code, PERCENT_DIGITS
                ),
                "total_functions": len(routines),
                "matched_functions": len(matched),
            }
        )
    for name, commit in sorted(unplaced.items()):
        print(
            f"progress_seed: no routine {name}, which {commit[:8]} includes as asm",
            file=sys.stderr,
        )
    _ = Path(out_path).write_text(json.dumps(points, indent=1) + "\n")
    print(
        f"progress_seed: {len(points)} points, {len(routines)} routines -> {out_path}"
    )


if __name__ == "__main__":
    main()
