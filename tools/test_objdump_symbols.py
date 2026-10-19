#!/usr/bin/env python3
"""Tests for relocation rendering in tools/decomp-env-template/objdump.py.

An auto-named symbol (D_<VRAM> or func_<VRAM>) encodes its address, and a named global resolves
through symbol_addrs.txt. Two relocations that resolve to the same address produce the same
linked immediate, so they must render identically; the name alone is naming noise. Different
addresses must still render differently.
"""

import sys
import unittest
from pathlib import Path

TEMPLATE = Path(__file__).resolve().parent / 'decomp-env-template'
sys.path.insert(0, str(TEMPLATE))

from objdump import process_mips_reloc  # noqa: E402


def render(reloc: str, symbol: str, imm: str) -> str:
    return process_mips_reloc(f'{reloc} {symbol}', symbol, imm)


class AutoNamedRelocTests(unittest.TestCase):
    def test_lo16_same_address_different_name_renders_equal(self):
        # D_800B786C and D_800B7868+4 are the same byte: 0x800B786C.
        self.assertEqual(
            render('R_MIPS_LO16', 'D_800B786C', '0'),
            render('R_MIPS_LO16', 'D_800B7868', '4'),
        )

    def test_hi16_same_address_different_name_renders_equal(self):
        self.assertEqual(
            render('R_MIPS_HI16', 'D_800B786C', '0'),
            render('R_MIPS_HI16', 'D_800B7868', '4'),
        )

    def test_lo16_different_address_renders_differently(self):
        self.assertNotEqual(
            render('R_MIPS_LO16', 'D_800B786C', '0'),
            render('R_MIPS_LO16', 'D_800B7870', '0'),
        )

    def test_hi16_carry_is_respected(self):
        # Linker %hi carries when bit 15 of the low half is set: 0x800B8000 -> 0x800C, 0x800B0000 -> 0x800B.
        self.assertNotEqual(
            render('R_MIPS_HI16', 'D_800B8000', '0'),
            render('R_MIPS_HI16', 'D_800B0000', '0'),
        )


class OverlayAutoNamedRelocTests(unittest.TestCase):
    def test_overlay_lo16_same_address_different_name_renders_equal(self):
        # Splat names overlay data D_ovl<N>_<VRAM>; D_ovl0_80227C90+4 is 0x80227C94.
        self.assertEqual(
            render('R_MIPS_LO16', 'D_ovl0_80227C94', '0'),
            render('R_MIPS_LO16', 'D_ovl0_80227C90', '4'),
        )

    def test_overlay_hi16_same_address_different_name_renders_equal(self):
        self.assertEqual(
            render('R_MIPS_HI16', 'D_ovl0_80227C94', '0'),
            render('R_MIPS_HI16', 'D_ovl0_80227C90', '4'),
        )

    def test_overlay_func_lo16_different_address_renders_differently(self):
        self.assertNotEqual(
            render('R_MIPS_LO16', 'func_ovl0_8020D784', '0'),
            render('R_MIPS_LO16', 'func_ovl0_8020D788', '0'),
        )


class NamedGlobalRelocTests(unittest.TestCase):
    def test_named_global_same_address_as_auto_name_renders_equal(self):
        # g_terrain_tile_cache+0x98E and D_80132D4A+0x944 are both 0x8013368E.
        self.assertEqual(
            render('R_MIPS_LO16', 'g_terrain_tile_cache', '2446'),
            render('R_MIPS_LO16', 'D_80132D4A', '2372'),
        )

    def test_named_global_hi16_same_address_as_auto_name_renders_equal(self):
        self.assertEqual(
            render('R_MIPS_HI16', 'g_terrain_tile_cache', '2446'),
            render('R_MIPS_HI16', 'D_80132D4A', '2372'),
        )

    def test_named_global_different_address_renders_differently(self):
        self.assertNotEqual(
            render('R_MIPS_LO16', 'g_terrain_tile_cache', '2446'),
            render('R_MIPS_LO16', 'D_80132D4A', '2376'),
        )


if __name__ == '__main__':
    unittest.main()
