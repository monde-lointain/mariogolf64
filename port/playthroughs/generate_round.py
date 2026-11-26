"""Writes the input script for a Stroke round that gives up each hole.

Sessions 12 to 15 come from here, not from a recording: each picks a
character and a course, takes a tee shot on each of the 18 holes, gives up
the hole from the pause menu, and goes back to the main menu. They start
from the same save as sessions 01 to 10, with CRC-32 1F93B89D.

The times are game frames, and fit that save: the menus' cursors start
where it left them. The unlock code on the title screen opens every course
and character, so that the script needs no other progress.
"""

from __future__ import annotations

import argparse
import pathlib
import sys

# Frames that a press is held for, unless it says otherwise
PRESS_FRAMES = 3
# The title screen's code that unlocks everything, as stick and C buttons
UNLOCK_CODE = "DDLLLRRRddlllrrr"
UNLOCK_BUTTONS = {
    "D": "DOWN",
    "L": "LEFT",
    "R": "RIGHT",
    "d": "CDOWN",
    "l": "CLEFT",
    "r": "CRIGHT",
}
UNLOCK_START = 180
UNLOCK_SPACING = 6
# RIGHT presses on the mode menu from One Player to Stroke
MODE_STEPS = 4
MENU_SPACING = 40
CHARACTER_SPACING = 10
# Frames from the tee shot's last press to the pause, which lets the ball
# land and the next shot's camera settle
SHOT_SETTLE = 800
# Frames from the scorecard's dismissal to the next hole's tee
NEXT_TEE = 600
# The pause menu's Give Up entry, below Course View
GIVE_UP_STEPS = 4
ROUND_HOLES = 18

# The sessions: number, file name, course, character, RIGHT presses on the
# course menu, and RIGHT presses on Character Select
SESSIONS = (
    (12, "koopa-park", "Koopa Park", "Plum", 1, 0),
    (13, "shy-guy-desert", "Shy Guy Desert", "Bowser", 2, 6),
    (14, "yoshis-island", "Yoshi's Island", "Luigi", 3, 2),
    (15, "boo-valley", "Boo Valley", "Harry", 4, 4),
)


class Script:
    """Gathers presses as an input script's lines, in frame order."""

    def __init__(self) -> None:
        """Start the script with nothing held."""
        self.lines: list[tuple[int, str]] = [(0, "")]

    def press(self, frame: int, button: str, frames: int = PRESS_FRAMES) -> None:
        """Hold the button from the frame for the given frames."""
        if frame <= self.lines[-1][0]:
            message = f"a press at frame {frame} is out of order"
            raise ValueError(message)
        self.lines.append((frame, button))
        self.lines.append((frame + frames, ""))

    def text(self) -> str:
        """Return the lines, one change of the buttons on each."""
        return "".join(f"{frame} {held}".rstrip() + "\n" for frame, held in self.lines)


def enter_menus(script: Script, course_steps: int, character_steps: int) -> int:
    """Get from the opening to the first tee, and return the tee's frame."""
    script.press(164, "A", 5)
    for index, letter in enumerate(UNLOCK_CODE):
        script.press(UNLOCK_START + (index * UNLOCK_SPACING), UNLOCK_BUTTONS[letter])
    script.press(296, "START", 4)
    script.press(330, "A", 4)
    for index in range(character_steps):
        script.press(380 + (index * CHARACTER_SPACING), "RIGHT")
    shift = character_steps * CHARACTER_SPACING
    script.press(380 + shift, "A", 4)
    for index in range(MODE_STEPS):
        script.press(500 + shift + (index * MENU_SPACING), "RIGHT")
    script.press(660 + shift, "A", 4)
    for index in range(course_steps):
        script.press(880 + shift + (index * MENU_SPACING), "RIGHT")
    course_shift = shift + ((course_steps - 1) * MENU_SPACING)
    script.press(940 + course_shift, "A", 4)
    return 1600 + course_shift


def give_up_hole(script: Script, tee: int) -> int:
    """Shoot from the tee, give up the hole, and return when the card goes."""
    script.press(tee, "A")
    script.press(tee + 30, "A")
    script.press(tee + 59, "A")
    pause = tee + 59 + SHOT_SETTLE
    script.press(pause, "START", 4)
    for index in range(GIVE_UP_STEPS):
        script.press(pause + 40 + (index * 15), "DOWN")
    script.press(pause + 110, "A", 4)
    # LEFT and A answer the question that follows, giving up the hole
    script.press(pause + 150, "LEFT")
    script.press(pause + 160, "A", 4)
    script.press(pause + 400, "A", 4)
    return pause + 400


def write_round(course_steps: int, character_steps: int, holes: int) -> tuple[str, int]:
    """Return a round's script without its comments, and its last frame."""
    script = Script()
    tee = enter_menus(script, course_steps, character_steps)
    card = tee
    for _ in range(holes):
        card = give_up_hole(script, tee)
        tee = card + NEXT_TEE
    # The results tally the round's points, then wait for another A
    script.press(card + 138, "A", 4)
    script.press(card + 238, "A", 4)
    return script.text(), card + 550


def write_session(number: int) -> tuple[str, str]:
    """Return a session's file name and its whole script."""
    for session in SESSIONS:
        if session[0] == number:
            break
    else:
        message = f"no session {number}"
        raise ValueError(message)
    _, slug, course, character, course_steps, character_steps = session
    body, end = write_round(course_steps, character_steps, ROUND_HOLES)
    text = (
        f"# Generated from a template: Stroke at {course} with {character},"
        " each hole a tee shot and Give Up\n"
        "# Starts from the save with CRC-32 1F93B89D\n"
        f"{body}# Ends at frame {end}\n"
    )
    return f"{number:02d}-{slug}.txt", text


def main() -> int:
    """Write the sessions' scripts, or with --check compare them."""
    parser = argparse.ArgumentParser(description="Writes the rounds' scripts.")
    _ = parser.add_argument("--check", action="store_true", help="compare, don't write")
    _ = parser.add_argument("sessions", nargs="*", type=int, help="all by default")
    arguments = parser.parse_args()
    is_checking = bool(arguments.check)  # pyright: ignore[reportAny]
    wanted: list[int] = arguments.sessions  # pyright: ignore[reportAny]
    folder = pathlib.Path(__file__).resolve().parent
    numbers = wanted or [session[0] for session in SESSIONS]
    differing = 0
    for number in numbers:
        name, text = write_session(number)
        path = folder / name
        if not is_checking:
            _ = path.write_text(text, encoding="ascii", newline="\n")
        elif not path.exists() or path.read_text(encoding="ascii") != text:
            print(f"{name} differs from what the template writes", file=sys.stderr)
            differing += 1
    return 1 if differing else 0


if __name__ == "__main__":
    sys.exit(main())
