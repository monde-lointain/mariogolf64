"""Writes the input script for a Stroke round that gives up each hole.

Sessions 12 to 16 come from here, not from a recording: each picks a
character and a course, takes a tee shot on each of the 18 holes, gives up
the hole from the pause menu, and goes back to the main menu. They start
from the same save as sessions 01 to 10, with CRC-32 1F93B89D.

The times are game frames, and fit that save: the menus' cursors start
where it left them. The unlock code on the title screen opens every course
and character but the last, Mario's Star, whose points it doesn't grant.
A session that needs more turns on the game's debug mode, with its own
code, and sets game flags in its flag panel: one opens all six courses,
and another rolls the staff after the round.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from typing import NamedTuple

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
CODE_SPACING = 6
# The code that turns on debug mode, entered with Z, L and R held; its
# letters are B and the C buttons
DEBUG_CODE = "BBuududdudllrlrrlrBBlr"
DEBUG_HELD = "Z L R"
DEBUG_BUTTONS = {
    "B": "B",
    "u": "CUP",
    "d": "CDOWN",
    "l": "CLEFT",
    "r": "CRIGHT",
}
# Without debug mode, the main menu takes A on One Player at this frame
MENU_FRAME = 330
# Frames from the title's START to the main menu's taking input
MENU_OPENING = 34
# In debug mode, holding L and pressing START opens and closes the flag
# panel, a grid of FLAG_COLUMNS flags to a row with the first one chosen
FLAG_COLUMNS = 16
FLAG_ROWS = 8
FLAG_SPACING = 8
# The flags that open all six courses, and that roll the staff after the
# round, as include/debug/game_flags.h numbers them
FLAG_OPEN_SIX_COURSES = 0x5C
FLAG_SHOW_STAFF_ROLL = 0x65
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
# Frames from the last scorecard to the staff roll's taking A, which it
# does from its frame 4300 on, about 4550 frames after the scorecard; and
# from there through the boot logos into the attract hole
STAFF_ROLL_END = 5000
AFTER_STAFF_ROLL = 300


class Session(NamedTuple):
    """A generated session, and the presses that choose its round."""

    number: int
    slug: str
    course: str
    character: str
    # RIGHT presses on the course menu, and on Character Select
    course_steps: int
    character_steps: int
    # Game flags set in debug mode's flag panel, in order
    flags: tuple[int, ...] = ()


SESSIONS = (
    Session(12, "koopa-park", "Koopa Park", "Plum", 1, 0),
    Session(13, "shy-guy-desert", "Shy Guy Desert", "Bowser", 2, 6),
    Session(14, "yoshis-island", "Yoshi's Island", "Luigi", 3, 2),
    Session(15, "boo-valley", "Boo Valley", "Harry", 4, 4),
    Session(
        16,
        "marios-star",
        "Mario's Star",
        "Peach",
        5,
        1,
        (FLAG_OPEN_SIX_COURSES, FLAG_SHOW_STAFF_ROLL),
    ),
)


class Script:
    """Gathers presses as an input script's lines, in frame order."""

    def __init__(self) -> None:
        """Start the script with nothing held."""
        self.lines: list[tuple[int, str]] = [(0, "")]

    def hold(self, frame: int, buttons: str) -> None:
        """Hold the buttons, and only them, from the frame on."""
        if frame <= self.lines[-1][0]:
            message = f"a change at frame {frame} is out of order"
            raise ValueError(message)
        self.lines.append((frame, buttons))

    def press(
        self, frame: int, button: str, frames: int = PRESS_FRAMES, held: str = ""
    ) -> None:
        """Press the button for the given frames, over what stays held."""
        self.hold(frame, f"{held} {button}".strip())
        self.hold(frame + frames, held)

    def text(self) -> str:
        """Return the lines, one change of the buttons on each."""
        return "".join(f"{frame} {held}".rstrip() + "\n" for frame, held in self.lines)


def enter_code(script: Script, frame: int, letters: str, held: str = "") -> int:
    """Enter a code on the title screen, and return the frame after it."""
    buttons = DEBUG_BUTTONS if held else UNLOCK_BUTTONS
    for letter in letters:
        script.press(frame, buttons[letter], held=held)
        frame += CODE_SPACING
    return frame


def set_flags(script: Script, frame: int, flags: tuple[int, ...]) -> int:
    """Set flags in the flag panel, and return the frame that it closes on."""
    script.hold(frame, "L")
    script.hold(frame + 4, "L START")
    script.hold(frame + 8, "")
    frame += 30
    row = 0
    column = 0
    for flag in flags:
        if flag >= FLAG_ROWS * FLAG_COLUMNS:
            message = f"flag {flag:#x} is below the panel's first page"
            raise ValueError(message)
        moves = (
            ("DOWN", (flag // FLAG_COLUMNS) - row),
            ("UP", row - (flag // FLAG_COLUMNS)),
            ("RIGHT", (flag % FLAG_COLUMNS) - column),
            ("LEFT", column - (flag % FLAG_COLUMNS)),
        )
        for button, count in moves:
            for _ in range(count):
                script.press(frame, button)
                frame += FLAG_SPACING
        script.press(frame, "A")
        frame += FLAG_SPACING
        row, column = divmod(flag, FLAG_COLUMNS)
    frame += 20
    script.hold(frame, "L")
    script.hold(frame + 4, "L START")
    script.hold(frame + 8, "")
    return frame + 8


def enter_title(script: Script, flags: tuple[int, ...]) -> int:
    """Get from the opening to the main menu, and return its first frame."""
    script.press(164, "A", 5)
    if not flags:
        frame = enter_code(script, UNLOCK_START, UNLOCK_CODE)
        script.press(frame + 20, "START", 4)
        return frame + 20 + MENU_OPENING
    script.hold(UNLOCK_START - 4, DEBUG_HELD)
    frame = enter_code(script, UNLOCK_START + 2, DEBUG_CODE, DEBUG_HELD)
    script.hold(frame, "")
    frame = enter_code(script, frame + 12, UNLOCK_CODE)
    script.press(frame + 24, "START", 4)
    return set_flags(script, frame + 120, flags) + MENU_OPENING


def enter_menus(script: Script, session: Session) -> int:
    """Get from the opening to the first tee, and return the tee's frame."""
    shift = enter_title(script, session.flags) - MENU_FRAME
    script.press(330 + shift, "A", 4)
    for index in range(session.character_steps):
        script.press(380 + shift + (index * CHARACTER_SPACING), "RIGHT")
    shift += session.character_steps * CHARACTER_SPACING
    script.press(380 + shift, "A", 4)
    for index in range(MODE_STEPS):
        script.press(500 + shift + (index * MENU_SPACING), "RIGHT")
    script.press(660 + shift, "A", 4)
    for index in range(session.course_steps):
        script.press(880 + shift + (index * MENU_SPACING), "RIGHT")
    shift += (session.course_steps - 1) * MENU_SPACING
    script.press(940 + shift, "A", 4)
    return 1600 + shift


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


def write_round(session: Session, holes: int) -> tuple[str, int]:
    """Return a round's script without its comments, and its last frame."""
    script = Script()
    tee = enter_menus(script, session)
    card = tee
    for _ in range(holes):
        card = give_up_hole(script, tee)
        tee = card + NEXT_TEE
    # The results tally the round's points, then wait for another A
    script.press(card + 138, "A", 4)
    script.press(card + 238, "A", 4)
    if FLAG_SHOW_STAFF_ROLL not in session.flags:
        return script.text(), card + 550
    # The staff roll takes A once it has run its course, and goes back to
    # the boot logos and the attract hole
    script.press(card + STAFF_ROLL_END, "A", 4)
    return script.text(), card + STAFF_ROLL_END + AFTER_STAFF_ROLL


def write_session(number: int) -> tuple[str, str]:
    """Return a session's file name and its whole script."""
    for session in SESSIONS:
        if session.number == number:
            break
    else:
        message = f"no session {number}"
        raise ValueError(message)
    body, end = write_round(session, ROUND_HOLES)
    extra = ", in debug mode" if session.flags else ""
    text = (
        f"# Generated from a template: Stroke at {session.course} with"
        f" {session.character}{extra}, each hole a tee shot and Give Up\n"
        "# Starts from the save with CRC-32 1F93B89D\n"
        f"{body}# Ends at frame {end}\n"
    )
    return f"{number:02d}-{session.slug}.txt", text


def main() -> int:
    """Write the sessions' scripts, or with --check compare them."""
    parser = argparse.ArgumentParser(description="Writes the rounds' scripts.")
    _ = parser.add_argument("--check", action="store_true", help="compare, don't write")
    _ = parser.add_argument("sessions", nargs="*", type=int, help="all by default")
    arguments = parser.parse_args()
    is_checking = bool(arguments.check)  # pyright: ignore[reportAny]
    wanted: list[int] = arguments.sessions  # pyright: ignore[reportAny]
    folder = pathlib.Path(__file__).resolve().parent
    numbers = wanted or [session.number for session in SESSIONS]
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
