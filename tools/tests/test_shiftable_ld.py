"""Test shiftable_ld.py's layout of the ROM, which custom files may resize.

A custom character's model files may take any even size, and every file
after them moves. The game's DMA and code expect each segment after main to
start on a 16-byte boundary, as in the original, so the layout aligns each.
"""

import re
import unittest

import shiftable_ld

HEAP_START = 0x80200000


def _segment(name: str, vram: int | None = None) -> str:
    """Return a splat block placing segment NAME, at VRAM if it has an address."""
    section = f"0x{vram:08X}" if vram is not None else ""
    return (
        f"    {name}_ROM_START = __romPos;\n"
        f"    .{name} {section} : AT({name}_ROM_START) SUBALIGN(16)\n"
        "    {\n"
        f"        build/{name}.o(.data);\n"
        f"        {name}_VRAM_END = .;\n"
        "    }\n"
        f"    __romPos += SIZEOF(.{name});"
    )


def _main_segment() -> str:
    """Return main's block, with the code and bss ends the layout looks for."""
    return (
        "    main_ROM_START = __romPos;\n"
        "    .main 0x80000400 : AT(main_ROM_START)\n"
        "    {\n"
        "        build/main.o(.text);\n"
        f"{shiftable_ld.MAIN_TEXT_END}"
        "        build/main.o(.bss);\n"
        f"{shiftable_ld.MAIN_BSS_END}"
        "        main_VRAM_END = .;\n"
        "    }\n"
        "    __romPos += SIZEOF(.main);"
    )


def _script() -> str:
    """Return a small splat script: a header, main, an overlay, the date, assets."""
    blocks = [
        _segment("header", 0xA4000000),
        _main_segment(),
        _segment("overlay", 0x80100000),
        _segment("build_date", 0x80180000),
        _segment("charlie"),
        _segment("charlie_reaction"),
    ]
    return "SECTIONS\n{\n" + "\n\n".join(blocks) + "\n}\n"


def _starts(script: str) -> list[tuple[str, bool]]:
    """Return each segment by its order in ROM, and whether it's aligned first."""
    alignment = rf"(__romPos = ALIGN\(__romPos, {shiftable_ld.ALIGNMENT}\);\n)?"
    aligned = re.compile(alignment + r"\s*(\w+)_ROM_START = __romPos;")
    return [(match[2], bool(match[1])) for match in aligned.finditer(script)]


class AlignmentTest(unittest.TestCase):
    """Check that every segment after main starts on the original's alignment."""

    def test_segments_after_main_are_aligned(self) -> None:
        """Check that each segment after main is aligned, and none before."""
        laid_out = shiftable_ld.lay_out(_script(), HEAP_START)
        self.assertEqual(
            _starts(laid_out),
            [
                ("header", False),
                ("main", False),
                ("overlay", True),
                ("build_date", True),
                ("charlie", True),
                ("charlie_reaction", True),
            ],
        )

    def test_alignment_comes_once(self) -> None:
        """Check that a segment's start is aligned once, before its symbol."""
        block = shiftable_ld.align_rom_start(_segment("charlie"))
        self.assertTrue(
            block.startswith("    __romPos = ALIGN(__romPos, 16);\n    charlie_ROM")
        )
        self.assertEqual(block.count("ALIGN(__romPos"), 1)


if __name__ == "__main__":
    _ = unittest.main()
