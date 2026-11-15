#!/usr/bin/env python3
"""Compress and decompress the game's LZ files, and the patches made of them.

    lz_codec.py decode IN OUT
    lz_codec.py encode IN OUT
    lz_codec.py patch BASE IN OUT

decode and encode convert between an LZ file, such as a character model,
and its decompressed bytes. patch decodes a patch file, such as a
character's palette, onto BASE, a decompressed file.

The format works in big-endian 16-bit words, as DecodeLzChunk in
src/course/hole_loader.c reads them. A file starts with two header words,
which the game skips; they hold the decompressed size. Then come groups: a
control word, whose bits from the top say whether each of the next 16 items
is a literal word (0) or a reference word (1). A reference copies a run of
words from earlier in the output: its top 11 bits are the distance back and
its low 5 bits the run's length less 2. A reference of 0 ends the file.

A patch file is coded the same way, but the words it decodes to are commands
that DecodePatchedLzChunk applies to a base file already in the buffer. A
command's high byte is its kind and its low byte its count less 1: 0x80
replaces the next count words with the literal words that follow it, and any
other high byte is a signed offset, from which it copies each word in turn.
An offset of 0 copies each word onto itself, which keeps it.
"""

import struct
import sys
from pathlib import Path

WORD_SIZE = 2
HEADER_SIZE = 4
ITEMS_PER_CONTROL_WORD = 16
CONTROL_TOP_BIT = 0x8000
DISTANCE_SHIFT = 5
LENGTH_MASK = 0x1F
LENGTH_MIN = 2
LENGTH_MAX = LENGTH_MASK + LENGTH_MIN
# The 11 bits above the length, and DecodePatchedLzChunk's window of the
# words it has decoded, LZ_WINDOW_MASK + 1 of them
DISTANCE_MAX = 0x7FF
END_OF_STREAM = 0
# How many earlier positions with the same next two words the encoder tries,
# newest first. More finds longer runs, slowly; the models compress within a
# percent of the original's size at this depth.
MATCH_CANDIDATES_MAX = 48

PATCH_KIND_SHIFT = 8
PATCH_COUNT_MASK = 0xFF
PATCH_COUNT_MAX = PATCH_COUNT_MASK + 1
PATCH_KIND_LITERALS = 0x80
PATCH_KEEP = 0x00
PATCH_HISTORY_SIZE = 0x100
BYTE_SIGN_BIT = 0x80
BYTE_RANGE = 0x100


class LzError(ValueError):
    """The data isn't a well-formed LZ file or patch."""


def to_words(data: bytes) -> list[int]:
    """Read DATA as big-endian u16 words; an odd last byte is dropped."""
    count = len(data) // WORD_SIZE
    return list(struct.unpack(f">{count}H", data[: count * WORD_SIZE]))


def from_words(words: list[int]) -> bytes:
    """Write WORDS as big-endian u16s."""
    return struct.pack(f">{len(words)}H", *words)


def _read_items(data: bytes) -> list[int | tuple[int, int]]:
    """Read the items of an LZ stream: literal words and (distance, length) runs."""
    words = to_words(data)
    position = HEADER_SIZE // WORD_SIZE
    items: list[int | tuple[int, int]] = []
    try:
        while True:
            control = words[position]
            position += 1
            for bit in range(ITEMS_PER_CONTROL_WORD):
                if not control & (CONTROL_TOP_BIT >> bit):
                    items.append(words[position])
                    position += 1
                    continue
                reference = words[position]
                position += 1
                if reference == END_OF_STREAM:
                    return items
                items.append(
                    (
                        reference >> DISTANCE_SHIFT,
                        (reference & LENGTH_MASK) + LENGTH_MIN,
                    )
                )
    except IndexError:
        msg = "the LZ data ends before its end marker"
        raise LzError(msg) from None


def decode(data: bytes) -> bytes:
    """Decompress an LZ file."""
    output: list[int] = []
    for item in _read_items(data):
        if isinstance(item, int):
            output.append(item)
            continue
        distance, length = item
        if distance > len(output):
            msg = f"a reference reaches {distance} words back, before the start"
            raise LzError(msg)
        # A run may overlap the words it writes, so it's copied a word at a time
        for _ in range(length):
            output.append(output[-distance])
    return from_words(output)


def _decode_commands(data: bytes) -> list[int]:
    """Decompress a patch file into its command words."""
    return to_words(decode(data))


def apply_patch(base: bytes, patch: bytes) -> bytes:
    """Apply a patch file to BASE, a decompressed file, as the game does."""
    output = to_words(base)
    commands = _decode_commands(patch)
    history = [0] * PATCH_HISTORY_SIZE
    history_index = 0
    position = 0
    index = 0
    try:
        while index < len(commands):
            command = commands[index]
            index += 1
            count = (command & PATCH_COUNT_MASK) + 1
            kind = command >> PATCH_KIND_SHIFT
            for _ in range(count):
                if kind == PATCH_KIND_LITERALS:
                    word = commands[index]
                    index += 1
                elif kind & BYTE_SIGN_BIT:
                    # A negative offset reaches back into the words overwritten
                    offset = kind - BYTE_RANGE
                    word = history[(history_index + offset) % PATCH_HISTORY_SIZE]
                else:
                    word = output[position + kind]
                history[history_index] = output[position]
                history_index = (history_index + 1) % PATCH_HISTORY_SIZE
                output[position] = word
                position += 1
    except IndexError:
        msg = "the patch runs past the end of its base or its commands"
        raise LzError(msg) from None
    return from_words(output)


def _find_longest_run(
    words: list[int], position: int, candidates: list[int]
) -> tuple[int, int]:
    """Find the longest run at POSITION that starts at one of CANDIDATES.

    Returns (distance, length), the length 0 when none is long enough.
    """
    best_distance = 0
    best_length = 0
    limit = min(LENGTH_MAX, len(words) - position)
    for start in reversed(candidates[-MATCH_CANDIDATES_MAX:]):
        distance = position - start
        if distance > DISTANCE_MAX:
            break
        length = 0
        while length < limit and words[start + length] == words[position + length]:
            length += 1
        if length > best_length:
            best_distance, best_length = distance, length
            if length == limit:
                break
    if best_length < LENGTH_MIN:
        return 0, 0
    return best_distance, best_length


def _choose_items(words: list[int]) -> list[int | tuple[int, int]]:
    """Choose the literals and runs that encode WORDS.

    Greedy with one step of lookahead: a run is put off by a word when the
    next word starts a longer one. Positions are indexed by the pair of words
    starting at them, so that only runs of at least two are tried.
    """
    starts: dict[tuple[int, int], list[int]] = {}

    def index_position(at: int) -> None:
        if at + 1 < len(words):
            starts.setdefault((words[at], words[at + 1]), []).append(at)

    def run_at(at: int) -> tuple[int, int]:
        if at + 1 >= len(words):
            return 0, 0
        return _find_longest_run(words, at, starts.get((words[at], words[at + 1]), []))

    items: list[int | tuple[int, int]] = []
    position = 0
    while position < len(words):
        distance, length = run_at(position)
        if length:
            index_position(position)
            next_length = run_at(position + 1)[1]
            if next_length > length:
                items.append(words[position])
                position += 1
                continue
            items.append((distance, length))
            for covered in range(position + 1, position + length):
                index_position(covered)
            position += length
            continue
        items.append(words[position])
        index_position(position)
        position += 1
    return items


def _write_items(items: list[int | tuple[int, int]], size: int) -> bytes:
    """Write ITEMS as LZ groups, after a header of SIZE and before the end marker."""
    output: list[int] = [size >> 16, size & 0xFFFF]
    items = [*items, (0, 0)]
    for group_start in range(0, len(items), ITEMS_PER_CONTROL_WORD):
        group = items[group_start : group_start + ITEMS_PER_CONTROL_WORD]
        control = 0
        group_words: list[int] = []
        for bit, item in enumerate(group):
            if isinstance(item, int):
                group_words.append(item)
                continue
            control |= CONTROL_TOP_BIT >> bit
            distance, length = item
            if length == 0:
                group_words.append(END_OF_STREAM)
            else:
                group_words.append((distance << DISTANCE_SHIFT) | (length - LENGTH_MIN))
        output.append(control)
        output.extend(group_words)
    return from_words(output)


def encode(data: bytes) -> bytes:
    """Compress DATA, whose size must be even, into an LZ file."""
    if len(data) % WORD_SIZE:
        msg = f"LZ data is made of 16-bit words, but its size, {len(data)}, is odd"
        raise LzError(msg)
    return _write_items(_choose_items(to_words(data)), len(data))


def _patch_commands(base: list[int], target: list[int]) -> list[int]:
    """List the commands that turn BASE into TARGET, which is no longer.

    Unchanged words are kept with offset 0 and changed ones written as
    literals, PATCH_COUNT_MAX at most to a command. Words past the last change
    need no command, as the patch ends there.
    """
    commands: list[int] = []
    position = 0
    last_change = max(
        (i for i, word in enumerate(target) if word != base[i]), default=-1
    )
    while position <= last_change:
        is_changed = target[position] != base[position]
        run_end = position
        while (
            run_end <= last_change
            and run_end - position < PATCH_COUNT_MAX
            and (target[run_end] != base[run_end]) == is_changed
        ):
            run_end += 1
        count = run_end - position
        kind = PATCH_KIND_LITERALS if is_changed else PATCH_KEEP
        commands.append((kind << PATCH_KIND_SHIFT) | (count - 1))
        if is_changed:
            commands.extend(target[position:run_end])
        position = run_end
    return commands


def encode_patch(base: bytes, target: bytes) -> bytes:
    """Compress the patch that turns BASE into TARGET, both decompressed.

    The game patches the base in place, so TARGET must be the same size.
    """
    if len(base) != len(target):
        msg = (
            f"a patch's target must be its base's size, {len(base)} bytes, "
            f"not {len(target)}"
        )
        raise LzError(msg)
    commands = _patch_commands(to_words(base), to_words(target))
    return _write_items(_choose_items(commands), len(commands) * WORD_SIZE)


def main() -> None:
    """Run the command that the arguments name."""
    arguments = sys.argv[1:]
    if len(arguments) == 3 and arguments[0] in {"decode", "encode"}:  # noqa: PLR2004 - a command, 2 files
        command, source, destination = arguments
        convert = decode if command == "decode" else encode
        _ = Path(destination).write_bytes(convert(Path(source).read_bytes()))
    elif len(arguments) == 4 and arguments[0] == "patch":  # noqa: PLR2004 - a command, 3 files
        _, base, patch, destination = arguments
        patched = apply_patch(Path(base).read_bytes(), Path(patch).read_bytes())
        _ = Path(destination).write_bytes(patched)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
