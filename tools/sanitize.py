#!/usr/bin/env python3
"""Run the sanitized build and report the checks of its code that failed.

    sanitize.py [--frames N] [--harness PATH] [--build DIR]

make SANITIZE=1 builds build/sanitize/mariogolf64.z64, whose code checks
itself, with GCC's undefined behavior sanitizer, for what C leaves
undefined and the layout or the compiler then decides: an index past an
array, which reads whatever the layout puts after it, a shift by the width
or more, a division by zero, and a float out of the range of the integer
it becomes, whose result the compiler and the VR4300 may each give
differently. A failed check calls a handler that notes it and returns
(see src/debug/sanitizer.c), so the game runs on as an unchecked build
would.

The build runs in ares's n64-harness, without buttons, as equivalence.py
runs it, for N frames of the game's logic, and then the handlers' record
is read. Each place whose check failed is reported once, by source line,
routine and segment, with what failed, the values of its first failure,
how many times it failed, and the retrace of the first. GCC's description
of each check is kept in the ELF only, at an address of no memory (see
tools/shiftable_ld.py). The link map tells which segment's object each
description comes from, which tells apart the overlays that share
addresses.

It fails if a check failed, and so finds what a modder's change brings in
as well as what the original does.
"""

import argparse
import contextlib
import os
import re
import signal
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import cast, final

SUMMARY = "Run the sanitized build and report the checks of its code that failed."
ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BUILD = ROOT / "build" / "sanitize"
DEFAULT_HARNESS = os.environ.get("N64_HARNESS", "n64-harness")
DEFAULT_FRAMES = 3000
ELF_NAME = "mariogolf64.elf"
ROM_NAME = "mariogolf64.z64"
MAP_NAME = "mariogolf64.map"
OBJDUMP = "mips-linux-gnu-objdump"

# The video frames that each run command covers before the frame of logic
# is read again, and how many runs may pass without the game moving on
RUN_RETRACES = 120
STALLED_RUN_MAX = 10

DESCRIPTORS_SECTION = ".sanitizer_descriptors"
FRAME_COUNT = "g_FrameCount"
REPORTS = "g_SanitizerReports"
REPORT_COUNT = "g_SanitizerReportCount"
UNRECORDED_COUNT = "g_SanitizerUnrecordedCount"
# SanitizerReport in include/debug/sanitizer.h, as big-endian words
REPORT_FORMAT = ">7I"
REPORT_SIZE = struct.calcsize(REPORT_FORMAT)
BYTE_ORDER = "big"
WORD_FORMAT = ">I"
FLOAT_FORMAT = ">f"
WORD_SIZE = 4
HALF_SIZE = 2
# A jal and its delay slot, which the return address follows
CALL_SIZE = 8
SIGN_BIT = 0x8000_0000
WORD_RANGE = 0x1_0000_0000

# A SourceLocation, which starts most descriptions: the file's name, its
# line and its column
LOCATION_SIZE = 12
# A TypeDescriptor: its kind, what it says of the type, and its name
TYPE_NAME_OFFSET = 4
INTEGER_KIND = 0
FLOAT_KIND = 1
SIGNED_INTEGER = 1

# SanitizerCheck in include/debug/sanitizer.h, UB_ and so on
CHECK_OUT_OF_BOUNDS = 0
CHECK_SHIFT = 1
CHECK_DIVISION = 2
CHECK_FLOAT_CAST = 3
CHECK_VARIABLE_LENGTH = 4

# objdump -h: index, name, size, VMA, LMA, file offset
OBJDUMP_SECTION = re.compile(
    r"""
    ^\s*\d+\s+(?P<name>\S+)\s+(?P<size>[0-9a-f]{8})\s+(?P<address>[0-9a-f]{8})
    \s+[0-9a-f]{8}\s+(?P<offset>[0-9a-f]{8})
    """,
    re.MULTILINE | re.VERBOSE,
)
# objdump -t, for functions: address, section, size and name
OBJDUMP_FUNCTION = re.compile(
    r"""
    ^(?P<address>[0-9a-f]{8})\ \S+\s+F\ (?P<section>\S+)\s+(?P<size>[0-9a-f]{8})
    \ (?P<name>.+)$
    """,
    re.MULTILINE | re.VERBOSE,
)
# objdump -t, for any symbol: its address and name
OBJDUMP_ANY_SYMBOL = re.compile(
    r"^(?P<address>[0-9a-f]{8}) .*\s(?P<name>\S+)$", re.MULTILINE
)
# The map's output sections start in its first column
MAP_OUTPUT_SECTION = re.compile(r"^(?P<name>\.\S+)")
# An input section's place in the map: its address, size and object
MAP_INPUT = re.compile(
    r"^\s+(?:\S+\s+)?0x(?P<address>[0-9a-f]+)\s+0x(?P<size>[0-9a-f]+) (?P<file>\S+\.o)$"
)
BSS_SUFFIX = "_bss"


class Arguments(argparse.Namespace):
    """The command line's settings."""

    frames: int = DEFAULT_FRAMES
    harness: str = DEFAULT_HARNESS
    build: Path = DEFAULT_BUILD


class HarnessError(Exception):
    """The harness failed a command or stopped."""


@final
@dataclass(frozen=True)
class Section:
    """An output section of the ELF: where it is in memory and in the file."""

    name: str
    address: int
    size: int
    offset: int

    def holds(self, address: int) -> bool:
        """Whether ADDRESS is in the section."""
        return self.address <= address < self.address + self.size


@final
@dataclass(frozen=True)
class Function:
    """A routine, by the section it is in."""

    name: str
    section: str
    address: int
    size: int


@final
@dataclass(frozen=True)
class Placement:
    """Where the link put an object's input section."""

    output: str
    address: int
    size: int
    file: str


@final
@dataclass(frozen=True)
class Report:
    """A place whose check failed, as the handlers noted it."""

    return_address: int
    descriptor: int
    check: int
    failure_count: int
    first_retrace: int
    operand: int
    other_operand: int


@final
class Image:
    """The sanitized build's ELF, its routines, and where the link put things."""

    def __init__(self, directory: Path) -> None:
        """Read the ELF's sections and routines, and the link map's placements."""
        self.elf = directory / ELF_NAME
        self.contents = self.elf.read_bytes()
        headers = run_tool(OBJDUMP, "-h", str(self.elf))
        self.sections = {
            match.group("name"): Section(
                match.group("name"),
                int(match.group("address"), 16),
                int(match.group("size"), 16),
                int(match.group("offset"), 16),
            )
            for match in OBJDUMP_SECTION.finditer(headers)
        }
        table = run_tool(OBJDUMP, "-t", str(self.elf))
        self.functions = [
            Function(
                match.group("name"),
                match.group("section"),
                int(match.group("address"), 16),
                int(match.group("size"), 16),
            )
            for match in OBJDUMP_FUNCTION.finditer(table)
        ]
        self.addresses = {
            match.group("name"): int(match.group("address"), 16)
            for match in OBJDUMP_ANY_SYMBOL.finditer(table)
        }
        self.placements = read_placements(directory / MAP_NAME)

    def read(self, section: str, address: int, size: int) -> bytes:
        """Read SIZE bytes at ADDRESS of the output section SECTION."""
        found = self.sections[section]
        if not (found.holds(address) and found.holds(address + size - 1)):
            message = f"0x{address:08X} is not in {section}"
            raise KeyError(message)
        start = found.offset + address - found.address
        return self.contents[start : start + size]

    def word(self, section: str, address: int) -> int:
        """Read a big-endian word at ADDRESS of SECTION."""
        return int.from_bytes(self.read(section, address, WORD_SIZE), BYTE_ORDER)

    def string(self, section: str, address: int) -> str:
        """Read the C string at ADDRESS of SECTION."""
        found = self.sections[section]
        start = found.offset + address - found.address
        end = self.contents.index(b"\0", start)
        return self.contents[start:end].decode("latin-1")

    def segment_of_descriptor(self, descriptor: int) -> str:
        """Find the output section of code that the object of DESCRIPTOR went to."""
        files = [
            placement.file
            for placement in self.placements
            if placement.output == DESCRIPTORS_SECTION
            and placement.address <= descriptor < placement.address + placement.size
        ]
        if len(files) != 1:
            message = f"no one object has the description at 0x{descriptor:08X}"
            raise KeyError(message)
        outputs = {
            placement.output
            for placement in self.placements
            if placement.file == files[0]
            and placement.output != DESCRIPTORS_SECTION
            and not placement.output.endswith(BSS_SUFFIX)
        }
        if len(outputs) != 1:
            message = f"{files[0]} is in {len(outputs)} segments"
            raise KeyError(message)
        return outputs.pop()

    def routine_at(self, section: str, address: int) -> str:
        """Name the routine of SECTION that ADDRESS is in."""
        for function in self.functions:
            if function.section == section and (
                function.address <= address < function.address + function.size
            ):
                return function.name
        return f"0x{address:08X}"


def run_tool(*command: str) -> str:
    """Run a program and return its output."""
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


def read_placements(map_path: Path) -> list[Placement]:
    """Read where the link map puts each object's input sections."""
    placements: list[Placement] = []
    output = ""
    for line in map_path.read_text(encoding="utf-8").splitlines():
        section = MAP_OUTPUT_SECTION.match(line)
        if section:
            output = section.group("name")
            continue
        found = MAP_INPUT.match(line)
        if found and output:
            placements.append(
                Placement(
                    output,
                    int(found.group("address"), 16),
                    int(found.group("size"), 16),
                    found.group("file"),
                )
            )
    return placements


@final
class Harness:
    """A running n64-harness, without video or breakpoints."""

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
        _ = self.command("video off")

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
        """Read SIZE bytes of memory."""
        return bytes.fromhex(self.command(f"read {address:x} {size:x}"))

    def read_word(self, address: int) -> int:
        """Read a word of memory."""
        return int.from_bytes(self.read(address, WORD_SIZE), BYTE_ORDER)

    def close(self) -> None:
        """Stop the harness and anything it started."""
        with contextlib.suppress(ProcessLookupError):
            os.killpg(self.process.pid, signal.SIGKILL)
        _ = self.process.wait()


@final
@dataclass(frozen=True)
class Run:
    """How far the game ran, and whether it stopped before it got there."""

    frame_count: int
    has_stopped: bool


def run_game(harness: Harness, frame_count: int, addresses: dict[str, int]) -> Run:
    """Run the game until FRAME_COUNT frames of logic pass, or it stops."""
    frame = 0
    stalled_runs = 0
    while frame < frame_count:
        reply = harness.command(f"run {RUN_RETRACES}")
        if not reply.startswith("timeout"):
            raise HarnessError(reply)
        last = frame
        frame = harness.read_word(addresses[FRAME_COUNT])
        stalled_runs = stalled_runs + 1 if frame == last else 0
        if stalled_runs > STALLED_RUN_MAX:
            return Run(frame, has_stopped=True)
    return Run(frame, has_stopped=False)


def read_reports(
    harness: Harness, addresses: dict[str, int]
) -> tuple[list[Report], int]:
    """Read the handlers' record: the reports and the failures past its end."""
    count = harness.read_word(addresses[REPORT_COUNT])
    unrecorded = harness.read_word(addresses[UNRECORDED_COUNT])
    record = harness.read(addresses[REPORTS], count * REPORT_SIZE) if count else b""
    reports = [
        Report(*cast("tuple[int, ...]", fields))
        for fields in struct.iter_unpack(REPORT_FORMAT, record)
    ]
    return reports, unrecorded


@final
@dataclass(frozen=True)
class TypeDescription:
    """A type as GCC describes it to the handlers."""

    kind: int
    info: int
    name: str

    def value(self, handle: int) -> str:
        """Write the value that HANDLE passes for the type."""
        if self.kind == INTEGER_KIND and 1 << (self.info >> 1) <= WORD_SIZE * 8:
            is_signed = self.info & SIGNED_INTEGER and handle & SIGN_BIT
            return str(handle - WORD_RANGE if is_signed else handle)
        if self.kind == FLOAT_KIND and self.info == WORD_SIZE * 8:
            return repr(
                struct.unpack(FLOAT_FORMAT, struct.pack(WORD_FORMAT, handle))[0]
            )
        return f"(the value at 0x{handle:08X})"


def read_type(image: Image, address: int) -> TypeDescription:
    """Read the description of a type at ADDRESS."""
    header = image.read(DESCRIPTORS_SECTION, address, TYPE_NAME_OFFSET)
    kind = int.from_bytes(header[:HALF_SIZE], BYTE_ORDER)
    info = int.from_bytes(header[HALF_SIZE:], BYTE_ORDER)
    name = image.string(DESCRIPTORS_SECTION, address + TYPE_NAME_OFFSET)
    return TypeDescription(kind, info, name)


def describe_failure(report: Report, types: list[TypeDescription]) -> str:
    """Write what failed the check, as UBSan's own runtime words it."""
    first, second = report.operand, report.other_operand
    if report.check == CHECK_OUT_OF_BOUNDS:
        array, index = types
        return f"index {index.value(first)} out of bounds for type {array.name}"
    if report.check == CHECK_SHIFT:
        shifted, amount = types
        return (
            f"shift of {shifted.value(first)} by {amount.value(second)} places"
            f" for type {shifted.name}"
        )
    if report.check == CHECK_DIVISION:
        (operand,) = types
        return f"division of {operand.value(first)} by {operand.value(second)}"
    if report.check == CHECK_FLOAT_CAST:
        source, target = types
        return f"{source.value(first)} is outside the range of type {target.name}"
    if report.check == CHECK_VARIABLE_LENGTH:
        (bound,) = types
        return f"variable length array bound evaluates to {bound.value(first)}"
    return f"check {report.check}"


# How many types each check's description names after its location
TYPE_COUNTS = {
    CHECK_OUT_OF_BOUNDS: 2,
    CHECK_SHIFT: 2,
    CHECK_DIVISION: 1,
    CHECK_FLOAT_CAST: 2,
    CHECK_VARIABLE_LENGTH: 1,
}


def describe(image: Image, report: Report) -> str:
    """Write a report as a compiler's diagnostic: where, what, and how often."""
    segment = image.segment_of_descriptor(report.descriptor)
    first_word = image.word(DESCRIPTORS_SECTION, report.descriptor)
    # A float cast's description has no location where GCC knew none, so
    # that it starts with a type's, which is among the descriptions
    has_location = not image.sections[DESCRIPTORS_SECTION].holds(first_word)
    location = "(no location)"
    types_start = report.descriptor
    if has_location:
        line = image.word(DESCRIPTORS_SECTION, report.descriptor + WORD_SIZE)
        column = image.word(DESCRIPTORS_SECTION, report.descriptor + 2 * WORD_SIZE)
        location = f"{image.string(segment, first_word)}:{line}:{column}"
        types_start += LOCATION_SIZE
    types = [
        read_type(
            image, image.word(DESCRIPTORS_SECTION, types_start + index * WORD_SIZE)
        )
        for index in range(TYPE_COUNTS.get(report.check, 0))
    ]
    routine = image.routine_at(segment, report.return_address - CALL_SIZE)
    times = "once" if report.failure_count == 1 else f"{report.failure_count} times"
    return (
        f"{location}: {describe_failure(report, types)}\n"
        f"    in {routine} ({segment.lstrip('.')}), {times},"
        f" first at retrace {report.first_retrace}"
    )


def parse_arguments() -> Arguments:
    """Read the command line."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("--frames", type=int, help="frames of logic to run")
    _ = parser.add_argument("--harness", help="the path of n64-harness")
    _ = parser.add_argument(
        "--build", type=Path, help="the sanitized build's directory"
    )
    return parser.parse_args(namespace=Arguments())


def main() -> int:
    """Run the sanitized build and print its reports; return the exit status."""
    arguments = parse_arguments()
    image = Image(arguments.build)
    harness = Harness(arguments.harness, arguments.build / ROM_NAME)
    try:
        run = run_game(harness, arguments.frames, image.addresses)
        reports, unrecorded = read_reports(harness, image.addresses)
    finally:
        harness.close()
    for report in reports:
        print(describe(image, report))
    if unrecorded:
        print(f"{unrecorded} failures at places past the record's end")
    if run.has_stopped:
        print(f"the game stopped at frame {run.frame_count}")
    places = "place" if len(reports) == 1 else "places"
    print(f"{len(reports)} {places} failed their checks in {run.frame_count} frames")
    return 1 if reports or unrecorded or run.has_stopped else 0


if __name__ == "__main__":
    sys.exit(main())
