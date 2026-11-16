"""Test layout.py's reading of the layout and the linker scripts it writes.

The scripts that it writes from the real layout link the original ROM, and
the shiftable builds' ROMs, byte for byte, which the build checks. These
tests check the rules that hold for any layout, such as one that a mod
changes: that the overlays' regions follow each other, that every segment
after main starts on a 16-byte boundary, and that the errors name the line.
"""

import re
import unittest

import layout

HEAP_START = 0x80200000

SMALL_LAYOUT = """\
file assets/header.bin
segment main 0x80000400
    src/entry.s
    src/main.c
    src/data_only.c data
    pad 0x10 text
    src/pins.c bss matching
segment ovl_a 0x80100000
    src/a.c
segment ovl_b 0x80100000
    src/b.c
    src/b.c common
segment ovl_c 0x80140000
    src/c.c
segment editor 0x80400000
    src/editor.c
file assets/text/glyphs.bin
file assets/character_models/charlie.bin
"""


def _small() -> layout.Layout:
    return layout.parse(SMALL_LAYOUT)


def _shiftable() -> str:
    return layout.shiftable_script(_small(), "build/x", HEAP_START)


def _rom_order(script: str) -> list[tuple[str, bool]]:
    """Return each segment by its order in ROM, and whether it's aligned first."""
    alignment = rf"(__romPos = ALIGN\(__romPos, {layout.ALIGNMENT}\);\n)?"
    aligned = re.compile(alignment + r"\s*(\w+)_ROM_START = __romPos;")
    return [(match[2], bool(match[1])) for match in aligned.finditer(script)]


def _segment_block(script: str, name: str) -> str:
    """Return the statements that place segment NAME in SCRIPT."""
    start = script.index(f"    {name}_ROM_START = __romPos;")
    return script[start : script.index(f"    {name}_ROM_END = __romPos;", start)]


class ParseTest(unittest.TestCase):
    """Check the reading of a layout."""

    def test_reads_segments_and_files(self) -> None:
        """Check that each line becomes its entry, in order."""
        small = _small()
        self.assertEqual(
            [item.name for item in small],
            [
                "header",
                "main",
                "ovl_a",
                "ovl_b",
                "ovl_c",
                "editor",
                "text_glyphs",
                "character_models_charlie",
            ],
        )
        main = small[1]
        assert isinstance(main, layout.Segment)  # noqa: S101 - narrows the type
        self.assertEqual(main.address, 0x80000400)
        self.assertEqual(
            main.entries,
            (
                layout.Placement("src/entry.s", layout.PLAIN_KINDS),
                layout.Placement("src/main.c", layout.PLAIN_KINDS),
                layout.Placement("src/data_only.c", frozenset({"data"})),
                layout.Pad(0x10, "text"),
                layout.Placement("src/pins.c", frozenset({"bss"}), matching_only=True),
            ),
        )

    def test_ignores_comments_and_blank_lines(self) -> None:
        """Check that '#' starts a comment, on a line of its own or after one."""
        text = "# The ROM\n\nsegment main 0x80000400  # code\n    src/main.c # all\n"
        self.assertEqual(
            layout.parse(text),
            [
                layout.Segment(
                    "main",
                    0x80000400,
                    (layout.Placement("src/main.c", layout.PLAIN_KINDS),),
                )
            ],
        )

    def test_errors_name_the_line(self) -> None:
        """Check that each mistake is reported with its line and what was expected."""
        cases = {
            "src/main.c\n": "layout:1: 'src/main.c' comes before any segment",
            "segment main\n": "layout:1: expected segment NAME ADDRESS",
            "segment main 0x80000400\n    src/main.c code\n": (
                "layout:2: 'code' isn't a kind of section"
            ),
            "segment main 0x8000040G\n": "layout:1: '0x8000040G' isn't a number",
            "segment main 0x80000400\n    pad 0x10\n": (
                "layout:2: expected pad SIZE KIND"
            ),
            "file\n": "layout:1: expected file PATH",
        }
        for text, message in cases.items():
            with (
                self.subTest(text=text),
                self.assertRaises(layout.LayoutError) as caught,
            ):
                _ = layout.parse(text)
            self.assertIn(message, str(caught.exception))


class ObjectsTest(unittest.TestCase):
    """Check the lists of objects that the build makes."""

    def test_lists_each_object_once(self) -> None:
        """Check that an object listed twice, and the files, are listed once."""
        every, matching = layout.objects(_small())
        self.assertEqual(
            every,
            [
                "assets/header.bin",
                "src/entry.s",
                "src/main.c",
                "src/data_only.c",
                "src/pins.c",
                "src/a.c",
                "src/b.c",
                "src/c.c",
                "src/editor.c",
                "assets/text/glyphs.bin",
                "assets/character_models/charlie.bin",
            ],
        )
        self.assertEqual(matching, ["src/pins.c"])

    def test_segment_objects_follow_the_first_placement(self) -> None:
        """Check that a segment's objects come in the order the layout places them."""
        text = (
            "segment s 0x80000400\n"
            "    rodata.c rodata\n"
            "    data.c data\n"
            "    code.c\n"
            "    pins.c bss matching\n"
        )
        segment = layout.parse(text)[0]
        assert isinstance(segment, layout.Segment)  # noqa: S101 - narrows the type
        self.assertEqual(
            layout.segment_objects(segment), ["code.c", "data.c", "rodata.c"]
        )


class MatchingScriptTest(unittest.TestCase):
    """Check the script of the original's layout."""

    def test_places_each_kind_in_turn(self) -> None:
        """Check that main's sections go text, data, rodata and bss, with its pad."""
        script = layout.matching_script(_small(), "build")
        main = _segment_block(script, "main")
        placed = re.findall(r"^\s+(build/\S+?|\. \+= \S+?);$", main, re.MULTILINE)
        self.assertEqual(
            placed,
            [
                "build/src/entry.s.o(.text*)",
                "build/src/main.c.o(.text*)",
                ". += 0x10",
                "build/src/entry.s.o(.data*)",
                "build/src/main.c.o(.data*)",
                "build/src/data_only.c.o(.data*)",
                "build/src/entry.s.o(.rodata*)",
                "build/src/main.c.o(.rodata*)",
                "build/src/entry.s.o(.bss*)",
                "build/src/main.c.o(.bss*)",
                "build/src/pins.c.o(.bss*)",
            ],
        )

    def test_keeps_the_original_addresses(self) -> None:
        """Check that every overlay loads at the address that the layout gives it."""
        script = layout.matching_script(_small(), "build")
        self.assertIn(".ovl_b 0x80100000 :", script)
        self.assertIn(".ovl_c 0x80140000 :", script)
        self.assertNotIn("ALIGN(__romPos", _segment_block(script, "text_glyphs"))


class ShiftableScriptTest(unittest.TestCase):
    """Check the script of the layout that lets the code and data grow."""

    def test_segments_after_main_are_aligned(self) -> None:
        """Check that each segment after main starts aligned, and none before."""
        self.assertEqual(
            _rom_order(_shiftable()),
            [
                ("header", False),
                ("main", False),
                ("ovl_a", True),
                ("ovl_b", True),
                ("ovl_c", True),
                ("editor", True),
                ("text_glyphs", True),
                ("character_models_charlie", True),
            ],
        )

    def test_regions_follow_each_other(self) -> None:
        """Check that each region starts past the largest overlay of the one before."""
        self.assertIn(
            "overlay_region_1_VRAM = ALIGN(main_VRAM_END + 1, 16);", _shiftable()
        )
        self.assertIn(
            (
                "overlay_region_2_VRAM = "
                "ALIGN(MAX(ovl_a_VRAM_END, ovl_b_VRAM_END) + 1, 16);"
            ),
            _shiftable(),
        )
        self.assertIn(".ovl_c overlay_region_2_VRAM :", _shiftable())
        self.assertIn("ASSERT(overlay_regions_VRAM_END <= 0x80200000,", _shiftable())

    def test_other_segments_keep_their_addresses(self) -> None:
        """Check that a segment outside base RAM stays where the layout puts it."""
        self.assertIn(".editor 0x80400000 :", _shiftable())

    def test_leaves_out_the_matching_objects(self) -> None:
        """Check that an object only the original's layout links isn't placed."""
        self.assertNotIn("pins.c", _shiftable())

    def test_places_commons_with_bss(self) -> None:
        """Check that an object's commons follow its bss if its segment lists none."""
        main = _segment_block(_shiftable(), "main")
        self.assertIn(
            (
                "KEEP(build/x/src/main.c.o(.bss .bss[!.]*));\n"
                "        build/x/src/main.c.o(.bss.*);\n"
                "        KEEP(build/x/src/main.c.o(COMMON .scommon .scommon[!.]*));"
            ),
            main,
        )
        ovl_b = _segment_block(_shiftable(), "ovl_b")
        self.assertEqual(ovl_b.count("COMMON"), 1)

    def test_archives_go_in_main(self) -> None:
        """Check that the archives' routines and bss are placed in main only."""
        self.assertEqual(_shiftable().count(layout.ARCHIVE_SECTIONS), 1)
        self.assertIn(layout.ARCHIVE_SECTIONS, _segment_block(_shiftable(), "main"))
        self.assertIn(layout.ARCHIVE_BSS, _segment_block(_shiftable(), "main"))

    def test_segment_script_goes_before_the_end(self) -> None:
        """Check that an added segment comes before the discarded sections."""
        script = layout.shiftable_script(
            _small(), "build/x", HEAP_START, "    /* tests */\n"
        )
        self.assertLess(script.index("/* tests */"), script.index("/DISCARD/"))

    def test_linked_segments_name_their_object(self) -> None:
        """Check that an LTO link's object places all of its segment's objects."""
        script = layout.shiftable_script(
            _small(), "build/x", HEAP_START, linked={"main": "build/x/lto/main.o"}
        )
        main = _segment_block(script, "main")
        self.assertNotIn("src/main.c", main)
        self.assertEqual(main.count("KEEP(build/x/lto/main.o(.text .text[!.]*));"), 1)
        self.assertIn("build/x/src/a.c.o", script)


if __name__ == "__main__":
    _ = unittest.main()
