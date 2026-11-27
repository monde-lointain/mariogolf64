"""Test the playthrough template's parts, each on its own.

The template's whole scripts are checked by generate_round.py --check; these
tests say which part broke: the order of the presses, the codes on the title
screen, the flag panel's cursor, or the choice of session.
"""

from __future__ import annotations

import pathlib
import sys
import unittest

# The test runs as a script, beside the template, and never as a module
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import generate_round as gr  # pyright: ignore[reportImplicitRelativeImport]


def pressed(script: gr.Script) -> list[str]:
    """Return the buttons that each line of the script holds, without blanks."""
    return [held for _, held in script.lines if held]


class ScriptOrderTest(unittest.TestCase):
    """A script's changes must come in frame order."""

    def test_takes_a_change_one_frame_later(self) -> None:
        """Take a change on the frame after the last one."""
        script = gr.Script()
        script.hold(10, "A")
        script.hold(11, "")
        self.assertEqual(script.lines, [(0, ""), (10, "A"), (11, "")])

    def test_refuses_a_change_on_the_same_frame(self) -> None:
        """Refuse a second change on a frame, which the reader can't hold."""
        script = gr.Script()
        script.hold(10, "A")
        with self.assertRaises(ValueError):
            script.hold(10, "B")

    def test_refuses_a_press_that_overlaps_the_last(self) -> None:
        """Refuse a press that starts before the last one is let go."""
        script = gr.Script()
        script.press(10, "A")
        with self.assertRaises(ValueError):
            script.press(10 + gr.PRESS_FRAMES, "B")

    def test_keeps_held_buttons_through_a_press(self) -> None:
        """Hold the other buttons under a press, and after it."""
        script = gr.Script()
        script.press(10, "B", held="Z L R")
        self.assertEqual(script.lines[1:], [(10, "Z L R B"), (13, "Z L R")])


class TitleCodeTest(unittest.TestCase):
    """The title screen's codes, entered a letter at a time."""

    def test_enters_the_unlock_code_on_the_stick_and_c_buttons(self) -> None:
        """Press each of the unlock code's letters as its button, nothing held."""
        script = gr.Script()
        _ = gr.enter_code(script, 100, "DdlR")
        self.assertEqual(pressed(script), ["DOWN", "CDOWN", "CLEFT", "RIGHT"])

    def test_enters_the_debug_code_under_the_held_buttons(self) -> None:
        """Press the debug code's letters with Z, L and R held throughout."""
        script = gr.Script()
        _ = gr.enter_code(script, 100, "Bu", gr.DEBUG_HELD)
        self.assertEqual(pressed(script), ["Z L R B", "Z L R", "Z L R CUP", "Z L R"])

    def test_returns_the_frame_after_the_last_letter(self) -> None:
        """Space the letters CODE_SPACING apart, and return the next slot."""
        script = gr.Script()
        end = gr.enter_code(script, 100, "DDL")
        self.assertEqual(end, 100 + (3 * gr.CODE_SPACING))

    def test_reaches_the_menu_on_the_recorded_frame_without_flags(self) -> None:
        """Reach the main menu on MENU_FRAME, which sessions 12 to 15 are timed to."""
        self.assertEqual(gr.enter_title(gr.Script(), ()), gr.MENU_FRAME)


class FlagPanelTest(unittest.TestCase):
    """The flag panel: opened and closed with L and START, a grid of flags."""

    def test_opens_and_closes_the_panel_with_l_then_start(self) -> None:
        """Hold L, add START, and let go, before the flags and after them."""
        script = gr.Script()
        _ = gr.set_flags(script, 100, ())
        self.assertEqual(pressed(script), ["L", "L START", "L", "L START"])

    def test_moves_to_each_flag_from_the_last(self) -> None:
        """Set 0x5C and 0x65 as the panel showed: row 5 column 12, then 6 and 5."""
        script = gr.Script()
        _ = gr.set_flags(
            script, 100, (gr.FLAG_OPEN_SIX_COURSES, gr.FLAG_SHOW_STAFF_ROLL)
        )
        moves = pressed(script)[2:-2]
        expected = (
            ["DOWN"] * 5 + ["RIGHT"] * 12 + ["A"] + ["DOWN"] + ["LEFT"] * 7 + ["A"]
        )
        self.assertEqual(moves, expected)

    def test_moves_up_to_an_earlier_row(self) -> None:
        """Move up when a flag is in a row above the last one's."""
        script = gr.Script()
        _ = gr.set_flags(script, 100, (0x21, 0x01))
        moves = pressed(script)[2:-2]
        expected = ["DOWN"] * 2 + ["RIGHT"] + ["A"] + ["UP"] * 2 + ["A"]
        self.assertEqual(moves, expected)

    def test_takes_the_last_flag_on_the_first_page(self) -> None:
        """Reach the first page's last flag, 0x7F."""
        script = gr.Script()
        _ = gr.set_flags(script, 100, (0x7F,))
        self.assertEqual(pressed(script).count("A"), 1)

    def test_refuses_a_flag_past_the_first_page(self) -> None:
        """Refuse 0x80, which only paging down with R would reach."""
        with self.assertRaises(ValueError):
            _ = gr.set_flags(gr.Script(), 100, (0x80,))


class SessionTest(unittest.TestCase):
    """The choice of session, and the round that it ends with."""

    def test_names_the_file_by_number_and_slug(self) -> None:
        """Name a session's file by its two-digit number and slug."""
        name, _ = gr.write_session(16)
        self.assertEqual(name, "16-marios-star.txt")

    def test_refuses_a_session_not_in_the_table(self) -> None:
        """Refuse a number that no session has."""
        with self.assertRaises(ValueError):
            _ = gr.write_session(11)

    def test_ends_after_the_staff_roll_with_its_flag(self) -> None:
        """End a round later by the staff roll when the session sets its flag.

        The other session sets 0x56, which the cursor reaches in as many moves
        as 0x65, so that both rounds start on the same frame.
        """
        plain = gr.Session(99, "plain", "Course", "Mario", 1, 0, (0x56,))
        rolled = plain._replace(flags=(gr.FLAG_SHOW_STAFF_ROLL,))
        _, plain_end = gr.write_round(plain, 1)
        _, rolled_end = gr.write_round(rolled, 1)
        self.assertEqual(
            rolled_end - plain_end, gr.STAFF_ROLL_END + gr.AFTER_STAFF_ROLL - 550
        )


if __name__ == "__main__":
    _ = unittest.main()
