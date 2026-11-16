"""Writes the copy of a game source that the PC build compiles.

The game's strings are Shift-JIS, as the N64 build compiles them, but its
sources are UTF-8, so the copy is Shift-JIS. Its nested functions, which not
every compiler has, are moved to file scope first (see nested_functions.py),
while the text is still UTF-8, as a Shift-JIS character's second byte can
read as C's, such as a backslash. The copy
starts with a line marker naming the source, so that the compiler's messages
point at it rather than at the copy.

Usage: encode_shift_jis.py SOURCE OUTPUT
"""

import pathlib
import sys

from nested_functions import NestedFunctionError, hoist_nested_functions


def encode_source(source: pathlib.Path, output: pathlib.Path) -> None:
    """Writes the compiled copy of source to output."""
    text = hoist_nested_functions(source.read_text(encoding="utf-8"), source.as_posix())
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
    except (OSError, UnicodeError, NestedFunctionError) as error:
        print("encode_shift_jis.py: {}: {}".format(arguments[0], error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
