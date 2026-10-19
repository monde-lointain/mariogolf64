#!/usr/bin/env python3
"""Report Mario Golf 64 decompilation progress from the last build.

Reads the linker map named in decomp.yaml and every object file it links, and
measures, overall and per category (game code, libraries) or segment:

  code bytes    matched .text bytes / all .text bytes
  functions     matched functions / all functions
  data bytes    matched .data, .rodata and .bss bytes / all of them
  data symbols  matched data objects / all data objects

What counts as matched depends on the object's source:

  src/**/*.c    everything except symbols marked `<name>.NON_MATCHING`
                (INCLUDE_ASM functions and the rodata included with them)
  src/**/*.s    everything: original hand-written assembly
  asm/**/*.s    nothing: splat data not yet moved into C

Binary blobs (build/assets/, `.incbin` sources such as the RSP microcode, and the
ROM header) are neither code nor data to decompile: they are reported but not counted.

The figures describe the last build, so run ./tools/build-and-verify.sh first.

Examples (from the repo root):
  python3 tools/progress.py
  python3 tools/progress.py --segments
  python3 tools/progress.py --unmatched
  python3 tools/progress.py -o report.json    # objdiff report for decomp.dev
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import re
import struct
import sys
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path, PurePosixPath
from typing import Iterable, Optional, Sequence

import yaml

ROOT = Path(__file__).resolve().parent.parent

MARKER = '.NON_MATCHING'
HEADER_OBJECT = PurePosixPath('asm/header.o')
LIBRARY_DIRS = frozenset({'libultra', 'libnusys', 'libnaudio', 'libmus', 'libkmc', 'mgu'})
CATEGORIES = (('game', 'Game'), ('libs', 'Libraries'))
REPORT_VERSION = 2  # objdiff report.proto version read by decomp.dev

# Object kinds, by source.
C, HASM, ASM, ASSET = 'c', 'hasm', 'asm', 'asset'

# ELF constants.
SHT_SYMTAB = 2
SHF_EXECINSTR = 0x4
STT_NOTYPE, STT_OBJECT, STT_FUNC = 0, 1, 2
STB_GLOBAL = 1


# --- object files --------------------------------------------------------------

@dataclass(frozen=True)
class ElfSymbol:
    name: str
    value: int
    size: int
    kind: int  # STT_*
    bind: int  # STB_*
    shndx: int


@dataclass(frozen=True)
class ElfObject:
    sections: dict[str, tuple[int, int]]  # name -> (index, sh_flags)
    symbols: list[ElfSymbol]


def read_elf(path: Path) -> ElfObject:
    """Section names and the symbol table of a 32-bit ELF relocatable object."""
    data = path.read_bytes()
    if data[:4] != b'\x7fELF' or data[4] != 1:
        raise ValueError(f'{path}: not a 32-bit ELF file')
    order = '>' if data[5] == 2 else '<'
    (shoff,) = struct.unpack_from(order + 'I', data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from(order + 'HHH', data, 0x2E)
    # sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, ...
    headers = [struct.unpack_from(order + '10I', data, shoff + i * shentsize) for i in range(shnum)]

    def string(table: int, offset: int) -> str:
        start = headers[table][4] + offset
        return data[start:data.index(b'\0', start)].decode()

    sections = {string(shstrndx, h[0]): (i, h[2]) for i, h in enumerate(headers)}
    symbols = []
    for h in headers:
        if h[1] != SHT_SYMTAB:
            continue
        for offset in range(h[4], h[4] + h[5], 16):
            name, value, size, info, _other, shndx = struct.unpack_from(order + 'IIIBBH', data, offset)
            symbols.append(ElfSymbol(string(h[6], name), value, size, info & 0xF, info >> 4, shndx))
    return ElfObject(sections, symbols)


# --- linker map ----------------------------------------------------------------

@dataclass(frozen=True)
class MapSection:
    name: str
    vram: int
    size: int


@dataclass
class MapObject:
    path: str     # as linked, e.g. build/src/main/audio_mgr.o
    segment: str  # output section without the leading '.' and a '_bss' suffix
    sections: list[MapSection] = field(default_factory=list)


_OUTPUT = re.compile(r'^(\.\S+)(?:\s|$)')
_INPUT = re.compile(r'^ (\.\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S+\.o)$')
_INPUT_NAME = re.compile(r'^ (\.\S+)$')
_INPUT_REST = re.compile(r'^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S+\.o)$')


def parse_map(text: str) -> list[MapObject]:
    """Objects in link order with the input sections the linker placed in ROM segments."""
    lines = text.splitlines()
    try:
        start = lines.index('Linker script and memory map') + 1
    except ValueError:
        raise ValueError('not a GNU ld map file: no "Linker script and memory map"') from None
    objects: dict[str, MapObject] = {}
    segment: Optional[str] = None
    wrapped: Optional[str] = None  # input section whose address and size are on the next line

    def add(section: str, vram: str, size: str, path: str) -> None:
        if segment is None:
            raise ValueError(f'map: input section {section} of {path} outside an output section')
        obj = objects.setdefault(path, MapObject(path, segment))
        obj.sections.append(MapSection(section, int(vram, 16), int(size, 16)))

    for line in lines[start:]:
        if line.startswith('/DISCARD/'):
            break
        if wrapped is not None:
            name, wrapped = wrapped, None
            m = _INPUT_REST.match(line)
            if m:
                add(name, *m.groups())
                continue
        if m := _OUTPUT.match(line):
            segment = m.group(1)[1:].removesuffix('_bss')
        elif m := _INPUT.match(line):
            add(*m.groups())
        elif m := _INPUT_NAME.match(line):
            wrapped = m.group(1)
    return list(objects.values())


def object_kind(root: Path, path: str) -> str:
    """C, HASM, ASM or ASSET for a linked object, from its source file."""
    rel = PurePosixPath(path).relative_to('build')
    top = rel.parts[0]
    if top == 'assets':
        return ASSET
    if top == 'src':
        if (root / rel.with_suffix('.c')).is_file():
            return C
        if (root / rel.with_suffix('.s')).is_file():
            return HASM
    if top == 'asm':
        if rel == HEADER_OBJECT:
            return ASSET
        source = root / rel.with_suffix('.s')
        if source.is_file():
            return ASSET if b'.incbin' in source.read_bytes() else ASM
    raise ValueError(f'{path}: no source file under src/ or asm/')


# --- measuring -----------------------------------------------------------------

@dataclass
class Measures:
    code_total: int = 0
    code_matched: int = 0
    functions_total: int = 0
    functions_matched: int = 0
    data_total: int = 0
    data_matched: int = 0
    symbols_total: int = 0
    symbols_matched: int = 0
    units_total: int = 0
    units_complete: int = 0
    code_complete: int = 0  # code bytes of fully matched units
    data_complete: int = 0  # data bytes of fully matched units

    def __iadd__(self, other: Measures) -> Measures:
        for f in dataclasses.fields(self):
            setattr(self, f.name, getattr(self, f.name) + getattr(other, f.name))
        return self


@dataclass(frozen=True)
class Item:
    """A function or data object: one address, however many aliases name it."""
    name: str
    offset: int  # within its section
    size: int
    matched: bool


@dataclass(frozen=True)
class SectionResult:
    name: str
    vram: int
    size: int
    matched: int
    code: bool
    items: tuple[Item, ...]


@dataclass
class Unit:
    name: str      # source path without extension, e.g. src/main/audio_mgr
    source: str    # e.g. src/main/audio_mgr.c
    segment: str
    category: str  # 'game' or 'libs'
    sections: list[SectionResult]
    measures: Measures


def _counts(sym: ElfSymbol, kind: str, code: bool) -> bool:
    """Whether a symbol names a function (code) or a data object (data)."""
    if sym.name.endswith(MARKER):
        return False
    if not code:
        return sym.kind == STT_OBJECT
    if sym.kind == STT_FUNC:
        return True
    # KMC's assembler types labels in hand-written assembly as OBJECT or leaves them
    # untyped; a global label there is an entry point.
    return kind != C and sym.bind == STB_GLOBAL and sym.kind in (STT_NOTYPE, STT_OBJECT)


def measure_section(elf: ElfObject, kind: str, section: MapSection) -> SectionResult:
    """Functions or data objects of one linked input section and its matched bytes."""
    if section.name not in elf.sections:
        raise ValueError(f'section {section.name} is in the map but not in the object')
    index, flags = elf.sections[section.name]
    code = bool(flags & SHF_EXECINSTR)
    markers = {s.name[:-len(MARKER)] for s in elf.symbols if s.name.endswith(MARKER)}
    by_offset: dict[int, list[ElfSymbol]] = {}
    for sym in elf.symbols:
        if sym.shndx == index and _counts(sym, kind, code):
            by_offset.setdefault(sym.value, []).append(sym)
    offsets = sorted(by_offset)
    items = []
    for i, offset in enumerate(offsets):
        aliases = by_offset[offset]
        end = offsets[i + 1] if i + 1 < len(offsets) else section.size
        # An unsized label (no .size directive) runs to the next item.
        size = max(s.size for s in aliases) or end - offset
        if kind == C:
            matched = not any(s.name in markers for s in aliases)
        else:
            matched = kind == HASM
        name = next((s.name for s in aliases if s.bind == STB_GLOBAL), aliases[0].name)
        items.append(Item(name, offset, size, matched))
    if kind == C:
        unmatched = sum(item.size for item in items if not item.matched)
        matched_bytes = max(section.size - unmatched, 0)
    else:
        matched_bytes = section.size if kind == HASM else 0
    return SectionResult(section.name, section.vram, section.size, matched_bytes, code, tuple(items))


def measure_unit(obj: MapObject, kind: str, elf: ElfObject) -> Unit:
    rel = PurePosixPath(obj.path).relative_to('build')
    source = rel.with_suffix('.c' if kind == C else '.s')
    category = 'libs' if rel.parts[0] == 'src' and rel.parts[1] in LIBRARY_DIRS else 'game'
    sections = [measure_section(elf, kind, s) for s in obj.sections]
    m = Measures(units_total=1)
    for s in sections:
        functions_or_symbols = len(s.items)
        matched_items = sum(item.matched for item in s.items)
        if s.code:
            m.code_total += s.size
            m.code_matched += s.matched
            m.functions_total += functions_or_symbols
            m.functions_matched += matched_items
        else:
            m.data_total += s.size
            m.data_matched += s.matched
            m.symbols_total += functions_or_symbols
            m.symbols_matched += matched_items
    if m.code_matched == m.code_total and m.data_matched == m.data_total:
        m.units_complete = 1
        m.code_complete = m.code_total
        m.data_complete = m.data_total
    return Unit(str(rel.with_suffix('')), str(source), obj.segment, category, sections, m)


@dataclass
class Progress:
    units: list[Unit]
    segments: list[str]  # in link order
    asset_objects: int
    asset_bytes: int

    def total(self, units: Optional[Iterable[Unit]] = None) -> Measures:
        m = Measures()
        for unit in self.units if units is None else units:
            m += unit.measures
        return m

    def by_category(self, category: str) -> Measures:
        return self.total(u for u in self.units if u.category == category)

    def by_segment(self, segment: str) -> Measures:
        return self.total(u for u in self.units if u.segment == segment)


def measure(root: Path, map_path: Path) -> Progress:
    objects = parse_map(map_path.read_text())
    units, segments = [], []
    asset_objects = asset_bytes = 0
    for obj in objects:
        kind = object_kind(root, obj.path)
        if kind == ASSET:
            asset_objects += 1
            asset_bytes += sum(s.size for s in obj.sections)
            continue
        if obj.segment not in segments:
            segments.append(obj.segment)
        units.append(measure_unit(obj, kind, read_elf(root / obj.path)))
    return Progress(units, segments, asset_objects, asset_bytes)


# --- output --------------------------------------------------------------------

def percent(part: int, total: int) -> float:
    """objdiff's convention: an empty total is 100% done."""
    return part / total * 100 if total else 100.0


def _cell(matched: int, total: int) -> str:
    if not total:
        return '-'
    return f'{matched:,} / {total:,}  {percent(matched, total):6.2f}%'


def render_table(rows: Sequence[tuple[str, Measures]]) -> str:
    header = ('', 'Code bytes', 'Functions', 'Data bytes', 'Data symbols')
    body = [
        (label, _cell(m.code_matched, m.code_total), _cell(m.functions_matched, m.functions_total),
         _cell(m.data_matched, m.data_total), _cell(m.symbols_matched, m.symbols_total))
        for label, m in rows
    ]
    widths = [max(len(row[i]) for row in (header, *body)) for i in range(len(header))]
    lines = []
    for row in (header, *body):
        cells = [row[0].ljust(widths[0])] + [cell.rjust(width) for cell, width in zip(row[1:], widths[1:])]
        lines.append('   '.join(cells).rstrip())
    return '\n'.join(lines)


def render_summary(progress: Progress, map_path: Path, root: Path, segments: bool) -> str:
    built = datetime.fromtimestamp(map_path.stat().st_mtime).strftime('%Y-%m-%d %H:%M')
    try:
        shown = map_path.relative_to(root)
    except ValueError:
        shown = map_path
    rows: list[tuple[str, Measures]] = []
    if segments:
        rows += [(segment, progress.by_segment(segment)) for segment in progress.segments]
    rows += [(name, progress.by_category(ident)) for ident, name in CATEGORIES]
    rows.append(('Total', progress.total()))
    return (
        f'Decompilation progress of {shown} (built {built})\n\n'
        f'{render_table(rows)}\n\n'
        f'Not counted: {progress.asset_objects} binary objects, {progress.asset_bytes:,} bytes '
        f'(build/assets/, .incbin sources, ROM header).\n'
    )


def render_unmatched(progress: Progress) -> str:
    rows = sorted(
        ((item.size, item.name, unit.source)
         for unit in progress.units for section in unit.sections if section.code
         for item in section.items if not item.matched),
        key=lambda row: (-row[0], row[1]),
    )
    if not rows:
        return 'All functions are matched.\n'
    lines = [f'Unmatched functions, largest first ({len(rows)}):']
    lines += [f'  0x{size:05X}  {name}  ({source})' for size, name, source in rows]
    return '\n'.join(lines) + '\n'


def measures_json(m: Measures) -> dict[str, object]:
    """objdiff Measures in proto3 JSON form: 64-bit integers as strings."""
    return {
        'fuzzy_match_percent': percent(m.code_matched, m.code_total),
        'total_code': str(m.code_total),
        'matched_code': str(m.code_matched),
        'matched_code_percent': percent(m.code_matched, m.code_total),
        'total_data': str(m.data_total),
        'matched_data': str(m.data_matched),
        'matched_data_percent': percent(m.data_matched, m.data_total),
        'total_functions': m.functions_total,
        'matched_functions': m.functions_matched,
        'matched_functions_percent': percent(m.functions_matched, m.functions_total),
        'complete_code': str(m.code_complete),
        'complete_code_percent': percent(m.code_complete, m.code_total),
        'complete_data': str(m.data_complete),
        'complete_data_percent': percent(m.data_complete, m.data_total),
        'total_units': m.units_total,
        'complete_units': m.units_complete,
    }


def report_json(progress: Progress) -> dict[str, object]:
    """An objdiff progress report (report.proto, version 2), the format decomp.dev reads."""
    units = []
    for unit in progress.units:
        functions = [
            {
                'name': item.name,
                'size': str(item.size),
                'fuzzy_match_percent': 100.0 if item.matched else 0.0,
                'address': str(item.offset),
                'metadata': {'virtual_address': str(section.vram + item.offset)},
            }
            for section in unit.sections if section.code for item in section.items
        ]
        units.append({
            'name': unit.name,
            'measures': measures_json(unit.measures),
            'sections': [
                {
                    'name': section.name,
                    'size': str(section.size),
                    'fuzzy_match_percent': percent(section.matched, section.size),
                    'metadata': {'virtual_address': str(section.vram)},
                }
                for section in unit.sections
            ],
            'functions': functions,
            'metadata': {
                'complete': bool(unit.measures.units_complete),
                'module_name': unit.segment,
                'module_id': progress.segments.index(unit.segment),
                'source_path': unit.source,
                'progress_categories': [unit.category],
            },
        })
    return {
        'measures': measures_json(progress.total()),
        'units': units,
        'version': REPORT_VERSION,
        'categories': [
            {'id': ident, 'name': name, 'measures': measures_json(progress.by_category(ident))}
            for ident, name in CATEGORIES
        ],
    }


def default_map_path(root: Path) -> Path:
    """The map of the first version in decomp.yaml."""
    config = yaml.safe_load((root / 'decomp.yaml').read_text())
    return root / config['versions'][0]['paths']['map']


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description='Report MG64 decompilation progress (code and data, by bytes and by symbols) '
                    'from the last build.')
    parser.add_argument('--map', type=Path, help='linker map to measure (default: from decomp.yaml)')
    parser.add_argument('--segments', action='store_true', help='add one row per ROM segment')
    parser.add_argument('-u', '--unmatched', action='store_true',
                        help='list unmatched functions, largest first')
    parser.add_argument('-o', '--report', type=Path, metavar='PATH',
                        help='write an objdiff progress report (report.json) for decomp.dev')
    args = parser.parse_args(argv)

    map_path = args.map or default_map_path(ROOT)
    if not map_path.is_file():
        print(f'error: {map_path} not found; run ./tools/build-and-verify.sh first', file=sys.stderr)
        return 1
    progress = measure(ROOT, map_path)

    print(render_summary(progress, map_path, ROOT, args.segments), end='')
    if args.unmatched:
        print()
        print(render_unmatched(progress), end='')
    if args.report:
        args.report.write_text(json.dumps(report_json(progress), indent=2) + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
