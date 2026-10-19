#!/usr/bin/env python3
"""Tests for progress.py: map parsing, object classification and what counts as matched."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import progress as pg  # noqa: E402
from progress import ElfObject, ElfSymbol, MapObject, MapSection  # noqa: E402

TEXT, DATA, RODATA = 1, 2, 3
SECTIONS = {'.text': (TEXT, 0x6), '.data': (DATA, 0x3), '.rodata': (RODATA, 0x2)}

MAP = """\
Discarded input sections

 .reginfo       0x00000000       0x18 build/src/main/gone.o

Linker script and memory map

.main           0x80026000      0x120 load address 0x00001000
 build/src/main/a.o(.text)
 .text          0x80026000      0x100 build/src/main/a.o
                0x80026000                func_80026000
 .rodata.longname
                0x80026100       0x20 build/src/main/a.o

.main_bss       0x80026120       0x40
 .bss           0x80026120       0x40 build/asm/data/1000.bss.o

.asset_directory
                0x80100000     0x1000 load address 0x00002000
 .data          0x80100000     0x1000 build/assets/asset_directory.o

/DISCARD/
 .text          0x00000000       0x10 build/src/main/late.o
"""


def sym(name, value, size=0, kind=pg.STT_FUNC, bind=pg.STB_GLOBAL, shndx=TEXT):
    return ElfSymbol(name, value, size, kind, bind, shndx)


def marker(name, value, size, shndx=TEXT):
    return sym(name + pg.MARKER, value, size, kind=pg.STT_OBJECT, shndx=shndx)


def unit(kind, symbols, sections, path='build/src/main/a.o'):
    obj = MapObject(path, 'main', [MapSection(name, 0x80026000, size) for name, size in sections])
    return pg.measure_unit(obj, kind, ElfObject(SECTIONS, symbols))


class ParseMapTest(unittest.TestCase):
    def test_reads_linked_sections_with_wrapped_lines(self):
        objects = {o.path: o for o in pg.parse_map(MAP)}

        self.assertEqual(list(objects), ['build/src/main/a.o', 'build/asm/data/1000.bss.o',
                                         'build/assets/asset_directory.o'])
        self.assertEqual(objects['build/src/main/a.o'].sections, [
            MapSection('.text', 0x80026000, 0x100),
            MapSection('.rodata.longname', 0x80026100, 0x20),
        ])
        self.assertEqual(objects['build/assets/asset_directory.o'].segment, 'asset_directory')

    def test_bss_output_section_belongs_to_its_segment(self):
        objects = {o.path: o for o in pg.parse_map(MAP)}
        self.assertEqual(objects['build/asm/data/1000.bss.o'].segment, 'main')


class ObjectKindTest(unittest.TestCase):
    def test_kind_follows_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            files = {
                'src/main/a.c': '',
                'src/libultra/os/getcount.s': 'LEAF(osGetCount)\n',
                'asm/data/1000.data.s': 'dlabel D_80001000\n',
                'asm/data/rsp/rspboot.s': '.incbin "assets/rsp/rspboot.bin"\n',
                'asm/header.s': '.word 0x80371240\n',
            }
            for rel, text in files.items():
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                (root / rel).write_text(text)

            self.assertEqual(pg.object_kind(root, 'build/src/main/a.o'), pg.C)
            self.assertEqual(pg.object_kind(root, 'build/src/libultra/os/getcount.o'), pg.HASM)
            self.assertEqual(pg.object_kind(root, 'build/asm/data/1000.data.o'), pg.ASM)
            self.assertEqual(pg.object_kind(root, 'build/asm/data/rsp/rspboot.o'), pg.ASSET)
            self.assertEqual(pg.object_kind(root, 'build/asm/header.o'), pg.ASSET)
            self.assertEqual(pg.object_kind(root, 'build/assets/ipl3.o'), pg.ASSET)
            with self.assertRaises(ValueError):
                pg.object_kind(root, 'build/src/main/missing.o')


class MeasureTest(unittest.TestCase):
    def test_c_object_counts_include_asm_functions_and_their_rodata_as_unmatched(self):
        u = unit(pg.C, [
            sym('gcc2_compiled.', 0, kind=pg.STT_OBJECT, bind=0),
            sym('done', 0x0, 0x40),
            marker('todo', 0x40, 0xC0), sym('todo', 0x40, 0xC0),
            sym('.L80026060', 0x60, kind=pg.STT_NOTYPE),  # jump-table label inside todo
            marker('D_str', 0x0, 0x18, shndx=RODATA),
            sym('D_str', 0x0, 0x18, kind=pg.STT_OBJECT, shndx=RODATA),
            sym('D_table', 0x18, 0x8, kind=pg.STT_OBJECT, shndx=RODATA),
        ], [('.text', 0x100), ('.rodata', 0x20)])
        m = u.measures

        self.assertEqual((m.code_matched, m.code_total), (0x40, 0x100))
        self.assertEqual((m.functions_matched, m.functions_total), (1, 2))
        self.assertEqual((m.data_matched, m.data_total), (0x8, 0x20))
        self.assertEqual((m.symbols_matched, m.symbols_total), (1, 2))
        self.assertEqual(m.units_complete, 0)

    def test_aliases_count_once_and_an_unsized_symbol_runs_to_the_next(self):
        u = unit(pg.C, [
            marker('todo', 0x0, 0x1), sym('todo', 0x0), sym('todo_alias', 0x0),
            sym('done', 0x30, 0x10),
        ], [('.text', 0x40)])

        self.assertEqual([(i.name, i.size, i.matched) for i in u.sections[0].items],
                         [('todo', 0x30, False), ('done', 0x10, True)])
        self.assertEqual(u.measures.code_matched, 0x10)

    def test_hand_written_asm_is_matched_and_counts_global_labels(self):
        u = unit(pg.HASM, [
            marker('_start', 0x0, 0x3C), sym('_start', 0x0, 0x3C),
            sym('osGetCount', 0x40, kind=pg.STT_OBJECT),
            sym('loop', 0x48, kind=pg.STT_OBJECT, bind=0),
            sym('D_80001000', 0x0, kind=pg.STT_OBJECT, bind=0, shndx=DATA),
        ], [('.text', 0x50), ('.data', 0x10)], path='build/src/entry/entry.o')
        m = u.measures

        self.assertEqual((m.code_matched, m.functions_matched, m.functions_total), (0x50, 2, 2))
        self.assertEqual((m.data_matched, m.symbols_matched, m.symbols_total), (0x10, 1, 1))
        self.assertEqual((m.units_complete, m.code_complete, m.data_complete), (1, 0x50, 0x10))

    def test_splat_data_is_unmatched(self):
        u = unit(pg.ASM, [
            marker('D_80001000', 0x0, 0x1, shndx=DATA),
            sym('D_80001000', 0x0, 0x8, kind=pg.STT_OBJECT, shndx=DATA),
            sym('D_80001008', 0x8, kind=pg.STT_OBJECT, shndx=DATA),
        ], [('.data', 0x10)], path='build/asm/data/1000.data.o')
        m = u.measures

        self.assertEqual((m.data_matched, m.data_total, m.symbols_matched, m.symbols_total),
                         (0, 0x10, 0, 2))
        self.assertEqual(u.category, 'game')

    def test_library_sources_form_the_libraries_category(self):
        u = unit(pg.C, [sym('osCreateViManager', 0x0, 0x10)], [('.text', 0x10)],
                 path='build/src/libultra/io/vimgr.o')
        self.assertEqual((u.category, u.name, u.source),
                         ('libs', 'src/libultra/io/vimgr', 'src/libultra/io/vimgr.c'))


if __name__ == '__main__':
    unittest.main()
