#!/usr/bin/env python3
"""Find the CPU stores that race the RCP's reads of a queued frame.

    find_races.py BUILD_DIR [--inputs FILE] [--frames N] [--delay US]
                  [--harness PATH]

Runs BUILD_DIR's mariogolf64.z64 in ares's n64-harness in Homebrew Mode,
in which ares reports each read by the RSP or the RDP of bytes that the CPU
stored after the display list that reads them was queued. Such a read sees
the old bytes or the new ones as the RCP keeps pace with the CPU or falls
behind it, so a frame may differ from build to build or from run to run.
The tool marks each list queued where nuGfxTaskStart has written back the
data cache for it, and holds each graphics task back --delay microseconds
before it runs, as an RDP that falls behind would; without a delay, ares
runs each task before the CPU starts on the next frame, and few stores can
race. It prints how many reads raced in which frames of logic, and the
routines whose stores they read. ares reports each racing read only the
first time it sees it, for each place read and store's PC, and stops
after 256 reports, so each routine is given with how many reports name it.

An inputs file holds lines of "FRAME BUTTON VALUE", where BUTTON names a
button of the first controller as ares does, such as A, Start or C-Up, and
VALUE presses it (1) or releases it (0) as the game reaches FRAME; text
from "#" is a comment. nusys's controller thread reads the controller on
retraces, so the tool also writes the buttons where that thread leaves
them, for the next frame to read whatever the speed. Without --inputs, the
game runs unattended.

The harness is built from the project's fork of ares, on its
mariogolf-harness branch; --harness or the N64_HARNESS environment variable
gives its path.
"""

import argparse
import collections
import contextlib
import os
import re
import signal
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import cast, final

from binutils import binutil

SUMMARY = "Find the CPU stores that race the RCP's reads of a queued frame."

ELF_NAME = "mariogolf64.elf"
ROM_NAME = "mariogolf64.z64"
DEFAULT_HARNESS = os.environ.get("N64_HARNESS", "n64-harness")
DEFAULT_FRAMES = 1000
# Long enough for the RDP to fall most of a frame behind at 30 frames a
# second, which shows most races without slowing the game too much
DEFAULT_DELAY_US = 20_000
# Retraces within which the game must queue a frame; one that takes longer
# has hung
FRAME_TIMEOUT = 600
# The register that holds a routine's first argument: nuGfxTaskStart's
# display list
FIRST_ARGUMENT_REGISTER = 4
# The size of a jump and its delay slot, after which a call returns
CALL_SIZE = 8
WORD_SIZE = 4
# The bits that hold an address in the harness's 64-bit registers and PCs
ADDRESS_MASK = 0xFFFF_FFFF

# An objdump -t line for a function: address, size and name
FUNCTION_LINE = re.compile(
    r"^(?P<address>[0-9a-f]{8}) [lg ] +F \S+\t(?P<size>[0-9a-f]{8}) (?P<name>\S+)$",
    re.MULTILINE,
)
# An objdump -t line for any symbol: its address and name
SYMBOL_LINE = re.compile(
    r"^(?P<address>[0-9a-f]{8}) .*\t[0-9a-f]{8} (?P<name>\S+)$", re.MULTILINE
)
# An objdump -d line that calls osWritebackDCacheAll, by its address
WRITEBACK_CALL = re.compile(
    r"^\s*(?P<address>[0-9a-f]+):.*\sjal\s.*<osWritebackDCacheAll>$", re.MULTILINE
)
# The bytes of nuGfxTaskStart in which to look for that call
WRITEBACK_SEARCH_SIZE = 0x200
# ares's report of a racing read, by the PC of the store that it read
RACED_STORE = re.compile(r"Stored at CPU PC: 0x(?P<pc>[0-9a-f]+)")

# The fields of a line of an inputs file
INPUT_FIELD_COUNT = 3
# The bits of the buttons in an OSContPad's button field, by ares's names
BUTTON_BITS = {
    "A": 0x8000,
    "B": 0x4000,
    "Z": 0x2000,
    "Start": 0x1000,
    "Up": 0x0800,
    "Down": 0x0400,
    "Left": 0x0200,
    "Right": 0x0100,
    "L": 0x0020,
    "R": 0x0010,
    "C-Up": 0x0008,
    "C-Down": 0x0004,
    "C-Left": 0x0002,
    "C-Right": 0x0001,
}
BUTTON_FIELD_SIZE = 2


class HarnessError(Exception):
    """The harness replied with an error, or exited."""


@dataclass(frozen=True)
class Input:
    """A button of the first controller, pressed or released at a frame."""

    frame: int
    button: str
    is_pressed: bool


def read_inputs(path: Path) -> list[Input]:
    """Read an inputs file, in the order of the frames that set them."""
    inputs: list[Input] = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        if (
            len(fields) != INPUT_FIELD_COUNT
            or not fields[0].isdigit()
            or fields[1] not in BUTTON_BITS
            or fields[2] not in {"0", "1"}
        ):
            message = f"{path}:{number}: expected FRAME BUTTON 0|1"
            raise ValueError(message)
        frame, button, value = fields
        inputs.append(Input(int(frame), button, value == "1"))
    return sorted(inputs, key=lambda pressed: pressed.frame)


def run_tool(*command: str) -> str:
    """Run a program and return its output."""
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


@final
class Program:
    """The symbols of a build's ELF, and the routines that hold its PCs."""

    def __init__(self, elf: Path) -> None:
        """Read ELF's symbols."""
        table = run_tool(binutil("objdump"), "-t", str(elf))
        self.addresses: dict[str, int] = {}
        for match in SYMBOL_LINE.finditer(table):
            _ = self.addresses.setdefault(
                match.group("name"), int(match.group("address"), 16)
            )
        self.functions = [
            (
                int(match.group("address"), 16),
                int(match.group("size"), 16),
                match.group("name"),
            )
            for match in FUNCTION_LINE.finditer(table)
        ]
        self.queued_address = self.after_writeback(elf)

    def after_writeback(self, elf: Path) -> int:
        """Find where nuGfxTaskStart returns from writing back the data cache."""
        start = self.addresses["nuGfxTaskStart"]
        listing = run_tool(
            binutil("objdump"),
            "-d",
            f"--start-address={start:#x}",
            f"--stop-address={start + WRITEBACK_SEARCH_SIZE:#x}",
            str(elf),
        )
        match = WRITEBACK_CALL.search(listing)
        if match is None:
            message = f"{elf}: nuGfxTaskStart calls no osWritebackDCacheAll"
            raise ValueError(message)
        return int(match.group("address"), 16) + CALL_SIZE

    def routines_at(self, pc: int) -> str:
        """Name the routines that hold PC, of which overlays may have more."""
        names = sorted(
            name for start, size, name in self.functions if start <= pc < start + size
        )
        return " or ".join(names) or "no routine"


@final
class Harness:
    """A running n64-harness in Homebrew Mode, which reports races."""

    def __init__(self, harness: str, rom: Path, log: Path) -> None:
        """Start the harness on ROM, with its reports going to LOG."""
        with log.open("w") as reports:
            self.process: subprocess.Popen[str] = subprocess.Popen(
                [harness, "--homebrew", str(rom)],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=reports,
                text=True,
                start_new_session=True,
            )

    def command(self, text: str) -> str:
        """Send a command and return its reply."""
        stdin, stdout = self.process.stdin, self.process.stdout
        if stdin is None or stdout is None:
            raise HarnessError(text)
        _ = stdin.write(text + "\n")
        stdin.flush()
        line = cast("str", stdout.readline()).strip()
        if not line or line.startswith("error"):
            raise HarnessError(line or "the harness exited")
        return line

    def expect_ok(self, text: str) -> None:
        """Send a command that replies ok."""
        reply = self.command(text)
        if reply != "ok":
            message = f"{text}: {reply}"
            raise HarnessError(message)

    def read_word(self, address: int) -> int:
        """Read a word of memory."""
        return int(self.command(f"read {address:x} {WORD_SIZE:x}"), 16)

    def run_to_break(self) -> int:
        """Run to the next breakpoint and return its address."""
        reply = self.command(f"run {FRAME_TIMEOUT}").split()
        if reply[0] != "halt":
            raise HarnessError(" ".join(reply))
        return int(reply[1], 16) & ADDRESS_MASK

    def close(self) -> None:
        """Stop the harness and anything it started."""
        with contextlib.suppress(ProcessLookupError):
            os.killpg(self.process.pid, signal.SIGKILL)
        _ = self.process.wait()


@final
class Controller:
    """The first controller's buttons, which the game reads each frame."""

    def __init__(self, harness: Harness, buttons_address: int) -> None:
        """Hold no buttons; nusys keeps the last read at BUTTONS_ADDRESS."""
        self.harness = harness
        self.buttons_address = buttons_address
        self.held = 0

    def set(self, inputs: list[Input]) -> None:
        """Press or release INPUTS, for the next frame to read."""
        for pressed in inputs:
            self.harness.expect_ok(
                f"input 1 {pressed.button} {int(pressed.is_pressed)}"
            )
            if pressed.is_pressed:
                self.held |= BUTTON_BITS[pressed.button]
            else:
                self.held &= ~BUTTON_BITS[pressed.button]
        held = self.held.to_bytes(BUTTON_FIELD_SIZE, "big").hex()
        self.harness.expect_ok(f"write {self.buttons_address:x} {held}")


@dataclass
class Races:
    """The frames in which reads raced, and the PCs of the stores they read."""

    reads_by_frame: dict[int, int]
    store_pcs: list[int]


def watch_frames(
    harness: Harness, program: Program, inputs: list[Input], frames: int
) -> dict[int, int]:
    """Run FRAMES frames of logic; count the racing reads of each."""
    task_start = program.addresses["nuGfxTaskStart"]
    frame_count = program.addresses["g_FrameCount"]
    controller = Controller(harness, program.addresses["nuContData"])
    harness.expect_ok(f"break {task_start:x}")
    harness.expect_ok(f"break {program.queued_address:x}")
    reads_by_frame: dict[int, int] = {}
    pending = collections.deque(inputs)
    frame, display_list, reads = 0, 0, 0
    while frame < frames:
        if harness.run_to_break() == program.queued_address:
            harness.expect_ok(f"queued {display_list:x}")
            continue
        argument = harness.command(f"reg {FIRST_ARGUMENT_REGISTER}")
        display_list = int(argument, 16) & ADDRESS_MASK
        frame = harness.read_word(frame_count)
        due: list[Input] = []
        while pending and pending[0].frame <= frame:
            due.append(pending.popleft())
        if due:
            controller.set(due)
        total = int(harness.command("races"))
        if total != reads:
            reads_by_frame[frame] = reads_by_frame.get(frame, 0) + total - reads
        reads = total
    return reads_by_frame


def find_races(arguments: "Arguments") -> tuple[Program, Races]:
    """Run the build and collect its races."""
    build = Path(arguments.build_dir)
    program = Program(build / ELF_NAME)
    inputs = read_inputs(Path(arguments.inputs)) if arguments.inputs else []
    with tempfile.TemporaryDirectory() as scratch:
        log = Path(scratch) / "reports.txt"
        harness = Harness(arguments.harness, build / ROM_NAME, log)
        try:
            harness.expect_ok(f"graphicsdelay {arguments.delay}")
            reads_by_frame = watch_frames(harness, program, inputs, arguments.frames)
        finally:
            harness.close()
        store_pcs = [
            int(match.group("pc"), 16) & ADDRESS_MASK
            for match in RACED_STORE.finditer(log.read_text())
        ]
    return program, Races(reads_by_frame, store_pcs)


def frame_spans(frames: list[int]) -> str:
    """Write sorted FRAMES as spans of consecutive frames."""
    spans: list[str] = []
    first = previous = frames[0]
    for frame in [*frames[1:], None]:
        if frame is not None and frame == previous + 1:
            previous = frame
            continue
        spans.append(str(first) if first == previous else f"{first}-{previous}")
        if frame is not None:
            first = previous = frame
    return ", ".join(spans)


def report(program: Program, races: Races) -> None:
    """Print the frames that raced and the routines whose stores raced."""
    total = sum(races.reads_by_frame.values())
    print(f"{total} racing reads in {len(races.reads_by_frame)} frames")
    if not races.reads_by_frame:
        return
    print(f"frames: {frame_spans(sorted(races.reads_by_frame))}")
    print("reports, by the routine and PC of the store read:")
    stores = collections.Counter(races.store_pcs)
    for pc, count in stores.most_common():
        print(f"{count:6} {program.routines_at(pc)} ({pc:08x})")


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    build_dir: str = ""
    inputs: str | None = None
    frames: int = DEFAULT_FRAMES
    delay: int = DEFAULT_DELAY_US
    harness: str = DEFAULT_HARNESS


def main() -> int:
    """Parse the arguments, find the races and report them."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("build_dir", help="the build, e.g. build/modern")
    _ = parser.add_argument("--inputs", help="a file of controller inputs")
    _ = parser.add_argument("--frames", type=int, help="frames of logic to run")
    _ = parser.add_argument(
        "--delay", type=int, help="microseconds to hold each graphics task back"
    )
    _ = parser.add_argument("--harness", help="the n64-harness program")
    arguments = parser.parse_args(namespace=Arguments())
    try:
        program, races = find_races(arguments)
    except (OSError, ValueError, HarnessError, subprocess.CalledProcessError) as error:
        print(f"find_races.py: {error}", file=sys.stderr)
        return 1
    report(program, races)
    return 0


if __name__ == "__main__":
    sys.exit(main())
