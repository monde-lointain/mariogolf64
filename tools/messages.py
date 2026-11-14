#!/usr/bin/env python3
"""Turn a message file into text, or text into a message file.

    messages.py decode MESSAGES TEXT
    messages.py encode TEXT MESSAGES

The message file begins with its size and its message count, a halfword
each, then gives each message's first code and its length in codes, and
then the codes, a halfword each, which text.c's enum MessageCode names.

TEXT holds a message a line, the first line being message 0. A capital,
digit or mark of ASCII stands for its own code and a small letter for
the code 0x80 above its own, where the fonts keep the small letters.
Braces hold the name of a control code, such as {br} for a line break,
or any other code as a number, such as {0xDE}.

The staff roll's text is a file of the same form, whose messages are the
lines that the roll shows. It draws no control code, so a line with one
and nothing else is an empty line.
"""

import struct
import sys
from pathlib import Path
from typing import cast

from asset_archive import pad_as_written

HEADER = struct.Struct(">HH")  # The file's size, then its message count
ENTRY = struct.Struct(">HH")  # A message's first code, then its length
CODE_SIZE = 2

# The codes that are their characters' own, and the small letters' offset
FIRST_PLAIN_CODE = 0x20
LAST_PLAIN_CODE = 0x5F
SMALL_LETTER_OFFSET = 0x80

# enum MessageCode's control codes, by the names that the text gives them
CONTROL_NAMES = {
    0x8000: "A",
    0x8001: "B",
    0x8006: "Z",
    0x8007: "C-up",
    0x8008: "C-down",
    0x8009: "C-left",
    0x800A: "C-right",
    0x800C: "Start",
    0x800D: "L",
    0x800E: "R",
    0x800F: "name",
    0x8014: "value0",
    0x8015: "value1",
    0x8016: "value2",
    0x8017: "value3",
    0x8018: "score",
    0x8019: "br",
    0x8022: "color0",
    0x8023: "color1",
    0x8024: "color2",
    0x8025: "color3",
    0x8026: "slot-color",
}
CONTROL_CODES = {name: code for code, name in CONTROL_NAMES.items()}

USAGE_EXIT = 2
ARGUMENT_COUNT = 4


def write_code(code: int) -> str:
    """Return the text that stands for a code."""
    if FIRST_PLAIN_CODE <= code <= LAST_PLAIN_CODE:
        return chr(code)
    if ord("a") <= code - SMALL_LETTER_OFFSET <= ord("z"):
        return chr(code - SMALL_LETTER_OFFSET)
    if code in CONTROL_NAMES:
        return f"{{{CONTROL_NAMES[code]}}}"
    return f"{{0x{code:X}}}"


def decode(data: bytes) -> str:
    """Return the text of a message file's messages, a message a line."""
    _, count = cast("tuple[int, int]", HEADER.unpack_from(data))
    codes_start = HEADER.size + count * ENTRY.size
    lines: list[str] = []
    for index in range(count):
        offset = HEADER.size + index * ENTRY.size
        start, length = cast("tuple[int, int]", ENTRY.unpack_from(data, offset))
        first = codes_start + start * CODE_SIZE
        codes = cast("tuple[int, ...]", struct.unpack_from(f">{length}H", data, first))
        lines.append("".join(write_code(code) for code in codes))
    return "\n".join(lines) + "\n"


def read_codes(line: str) -> list[int]:
    """Return the codes that a line of text stands for."""
    codes: list[int] = []
    position = 0
    while position < len(line):
        character = line[position]
        position += 1
        if character == "{":
            end = line.find("}", position)
            if end < 0:
                sys.exit(f"expected a closing brace in: {line}")
            name = line[position:end]
            position = end + 1
            codes.append(CONTROL_CODES[name] if name in CONTROL_CODES else int(name, 0))
        elif "a" <= character <= "z":
            codes.append(ord(character) + SMALL_LETTER_OFFSET)
        elif FIRST_PLAIN_CODE <= ord(character) <= LAST_PLAIN_CODE:
            codes.append(ord(character))
        else:
            sys.exit(f"expected {character!r} in braces, as its code, in: {line}")
    return codes


def encode(text: str) -> bytes:
    """Return the message file that holds the messages of TEXT."""
    messages = [read_codes(line) for line in text.splitlines()]
    entries = bytearray()
    codes = bytearray()
    for message in messages:
        entries += ENTRY.pack(len(codes) // CODE_SIZE, len(message))
        codes += struct.pack(f">{len(message)}H", *message)
    size = HEADER.size + len(entries) + len(codes)
    return pad_as_written(HEADER.pack(size, len(messages)) + entries + codes)


def main() -> None:
    """Decode or encode the message file that the command line names."""
    if len(sys.argv) != ARGUMENT_COUNT or sys.argv[1] not in {"decode", "encode"}:
        print(__doc__, file=sys.stderr)
        sys.exit(USAGE_EXIT)
    source = Path(sys.argv[2])
    out = Path(sys.argv[3])
    out.parent.mkdir(parents=True, exist_ok=True)
    if sys.argv[1] == "decode":
        _ = out.write_text(decode(source.read_bytes()), encoding="utf-8")
    else:
        _ = out.write_bytes(encode(source.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()
