#!/usr/bin/env python3
"""Turn a texture PNG into the texels of a C array initializer.

    png_to_c.py IN.FORMAT.png OUT.inc.c

FORMAT is i4, i8, ia4 or rgba16, as extract_textures.py names the files.
The output lists the texels, eight array elements to a line, for a C
file to include between the braces of a u8 or u16 array; i4 packs
two texels into each u8, and ia4 four into each u16.
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

# IA4 keeps three bits of intensity, spread over 0 to 252 in the PNG, and
# one of alpha, four texels to a u16
IA4_INTENSITY_SCALE = 36
IA4_BITS = 4
IA4_TEXELS_PER_WORD = 4

# I4 keeps four bits of intensity, spread over 0 to 255 in the PNG, two
# texels to a byte
I4_INTENSITY_SCALE = 17
I4_BITS = 4


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


def encode_i4(image: Image.Image) -> list[str]:
    """Pack each texel's intensity into I4, two to a byte."""
    texels = [value // I4_INTENSITY_SCALE for value in image.convert("L").tobytes()]
    return [
        f"0x{texels[i] << I4_BITS | texels[i + 1]:02X}"
        for i in range(0, len(texels), 2)
    ]


def encode_ia4(image: Image.Image) -> list[str]:
    """Pack each texel's intensity and alpha into IA4, four to a u16."""
    data = image.convert("LA").tobytes()
    texels = [
        (data[i] // IA4_INTENSITY_SCALE) << 1 | (data[i + 1] >= ALPHA_HALF)
        for i in range(0, len(data), 2)
    ]
    words: list[str] = []
    for i in range(0, len(texels), IA4_TEXELS_PER_WORD):
        word = 0
        for texel in texels[i : i + IA4_TEXELS_PER_WORD]:
            word = word << IA4_BITS | texel
        words.append(f"0x{word:04X}")
    return words


ENCODERS = {
    "i4": encode_i4,
    "i8": encode_i8,
    "ia4": encode_ia4,
    "rgba16": encode_rgba16,
}


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
