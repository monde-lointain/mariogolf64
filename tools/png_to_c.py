#!/usr/bin/env python3
"""Turn a texture PNG into the texels of a C array initializer.

    png_to_c.py IN.FORMAT.png OUT.inc.c

FORMAT is i8 or rgba16, as extract_textures.py names the files. The
output lists the texels, eight to a line, for a C file to include
between the braces of a u8 or u16 array.
"""

import sys
from pathlib import Path

from PIL import Image

TEXELS_PER_LINE = 8

# Bits that RGBA16 keeps of each 8-bit color channel
COLOR_SHIFT = 3
RED_SHIFT = 11
GREEN_SHIFT = 6
BLUE_SHIFT = 1
ALPHA_HALF = 0x80


def encode_rgba16(image: Image.Image) -> list[str]:
    """Pack each 8-bit RGBA texel into the 5-5-5-1 bits of RGBA16."""
    data = image.convert("RGBA").tobytes()
    texels: list[str] = []
    for i in range(0, len(data), 4):
        red, green, blue, alpha = data[i : i + 4]
        texel = (
            (red >> COLOR_SHIFT) << RED_SHIFT
            | (green >> COLOR_SHIFT) << GREEN_SHIFT
            | (blue >> COLOR_SHIFT) << BLUE_SHIFT
            | (alpha >= ALPHA_HALF)
        )
        texels.append(f"0x{texel:04X}")
    return texels


def encode_i8(image: Image.Image) -> list[str]:
    """List each texel's intensity byte."""
    return [f"0x{texel:02X}" for texel in image.convert("L").tobytes()]


ENCODERS = {"i8": encode_i8, "rgba16": encode_rgba16}


def main() -> None:
    """Write the initializer for the PNG named on the command line."""
    source, out = Path(sys.argv[1]), Path(sys.argv[2])
    texture_format = source.suffixes[-2].lstrip(".")
    texels = ENCODERS[texture_format](Image.open(source))
    lines = [
        ", ".join(texels[i : i + TEXELS_PER_LINE]) + ","
        for i in range(0, len(texels), TEXELS_PER_LINE)
    ]
    out.parent.mkdir(parents=True, exist_ok=True)
    _ = out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
