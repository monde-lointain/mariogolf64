#!/usr/bin/env python3
"""Run a test ROM's unit tests on ares and print their report.

    run_target_tests.py ELF ROM [--frames N] [--harness PATH]

The test ROM (see tests/target/) boots into the tests rather than the
game, runs every test group with Unity, keeps the report in memory and
calls TargetTestsFinished. This runs ROM in ares's n64-harness until that
call, then prints the report and passes if no test failed. ELF gives the
addresses of the routine and of the report. A ROM that hasn't finished
after N video frames fails, with the report so far, which names the test
that was running. The harness is built from the project's fork of ares, on
its mariogolf-harness branch; --harness or the N64_HARNESS environment
variable gives its path.
"""

import argparse
import contextlib
import os
import re
import signal
import subprocess
import sys
from pathlib import Path
from typing import cast, final

SUMMARY = "Run a test ROM's unit tests on ares and print their report."
DEFAULT_HARNESS = os.environ.get("N64_HARNESS", "n64-harness")
# About ten seconds of the console's time. The tests take about one.
DEFAULT_FRAMES = 600
# An nm -S line: address, size, type, name. Only the report has a size that
# this reads, and only symbols with sizes match.
NM_SIZED_SYMBOL = re.compile(r"^([0-9a-f]+) ([0-9a-f]+) \S (\S+)$", re.MULTILINE)
NM_SYMBOL = re.compile(r"^([0-9a-f]+) \S (\S+)$", re.MULTILINE)
WORD_SIZE = 4


class HarnessError(Exception):
    """The harness replied with an error or exited."""


@final
class Harness:
    """A running n64-harness."""

    def __init__(self, program: str, rom: Path) -> None:
        """Start the harness on ROM."""
        self.process: subprocess.Popen[str] = subprocess.Popen(
            [program, str(rom)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
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

    def read(self, address: int, size: int) -> bytes:
        """Read SIZE bytes of memory from ADDRESS."""
        if not size:
            return b""
        return bytes.fromhex(self.command(f"read {address:x} {size:x}"))

    def read_word(self, address: int) -> int:
        """Read a word of memory."""
        return int.from_bytes(self.read(address, WORD_SIZE), "big")

    def close(self) -> None:
        """Stop the harness and anything it started."""
        with contextlib.suppress(ProcessLookupError):
            os.killpg(self.process.pid, signal.SIGKILL)
        _ = self.process.wait()


@final
class Symbols:
    """The test ROM's symbols that the driver reads."""

    def __init__(self, elf: Path) -> None:
        """Read ELF's symbols."""
        listing = subprocess.run(
            ["mips-linux-gnu-nm", "-S", str(elf)],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        self.addresses: dict[str, int] = {
            match.group(2): int(match.group(1), 16)
            for match in NM_SYMBOL.finditer(listing)
        }
        self.sizes: dict[str, int] = {}
        for match in NM_SIZED_SYMBOL.finditer(listing):
            self.addresses[match.group(3)] = int(match.group(1), 16)
            self.sizes[match.group(3)] = int(match.group(2), 16)


def read_report(harness: Harness, symbols: Symbols) -> tuple[str, bool]:
    """Read the report, and whether it all fit in its buffer."""
    length = harness.read_word(symbols.addresses["g_TargetTestReportLength"])
    room = symbols.sizes["g_TargetTestReport"]
    text = harness.read(symbols.addresses["g_TargetTestReport"], min(length, room))
    return text.decode("ascii", errors="replace"), length <= room


def run_tests(harness: Harness, symbols: Symbols, frames: int) -> bool:
    """Run the tests, print their report and return whether they passed."""
    finished = symbols.addresses["TargetTestsFinished"]
    _ = harness.command(f"break {finished:x}")
    reply = harness.command(f"run {frames}")
    report, fits = read_report(harness, symbols)
    print(report, end="" if report.endswith("\n") else "\n")
    if not fits:
        print("The report was cut short where its buffer filled")
    # A halt's reply names the PC and the frames that the run took
    if reply.split()[:2] != ["halt", f"{finished:08x}"]:
        print(f"The tests didn't finish within {frames} frames ({reply})")
        return False
    failures = harness.read_word(symbols.addresses["g_TargetTestFailures"])
    return failures == 0


class Arguments(argparse.Namespace):
    """The command's arguments."""

    elf: Path = Path()
    rom: Path = Path()
    frames: int = DEFAULT_FRAMES
    harness: str = DEFAULT_HARNESS


def main() -> int:
    """Parse the arguments and run the tests."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("elf", type=Path, help="the test ROM's ELF")
    _ = parser.add_argument("rom", type=Path, help="the test ROM")
    _ = parser.add_argument(
        "--frames", type=int, default=DEFAULT_FRAMES, help="video frames to allow"
    )
    _ = parser.add_argument(
        "--harness", default=DEFAULT_HARNESS, help="the n64-harness program"
    )
    arguments = parser.parse_args(namespace=Arguments())
    symbols = Symbols(arguments.elf)
    harness = Harness(arguments.harness, arguments.rom)
    try:
        passed = run_tests(harness, symbols, arguments.frames)
    except HarnessError as error:
        print(f"{arguments.rom}: {error}", file=sys.stderr)
        return 1
    finally:
        harness.close()
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
