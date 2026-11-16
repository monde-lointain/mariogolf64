"""Writes a Shift-JIS copy of a UTF-8 C source, as the N64 build compiles.

The game's strings are Shift-JIS, but its sources are UTF-8. The copy
starts with a line marker naming the source, so that the compiler's
messages point at it rather than at the copy.

Usage: encode_shift_jis.py SOURCE OUTPUT
"""

import pathlib
import sys


def encode_source(source: pathlib.Path, output: pathlib.Path) -> None:
    """Writes the Shift-JIS copy of source to output."""
    text = source.read_text(encoding="utf-8")
    marker = '# 1 "{}"\n'.format(source.as_posix())
    encoded = marker.encode("ascii") + text.encode("shift_jis")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Rewriting an unchanged copy would only rebuild its object
    if output.exists() and output.read_bytes() == encoded:
        return
    output.write_bytes(encoded)


def main(arguments: list[str]) -> int:
    if len(arguments) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        encode_source(pathlib.Path(arguments[0]), pathlib.Path(arguments[1]))
    except (OSError, UnicodeError) as error:
        print("encode_shift_jis.py: {}: {}".format(arguments[0], error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
