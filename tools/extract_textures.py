#!/usr/bin/env python3
"""Extract the textures that C files hold in their data from the base ROM.

    extract_textures.py

Each line of textures.txt gives a texture's ROM offset, format, width,
height and PNG path; '#' starts a comment. The PNGs go under assets/,
and png_to_c.py turns them back into the C initializers that the files
include.
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
BASEROM = ROOT / "baserom.us.z64"
TEXTURE_LIST = ROOT / "textures.txt"

# Bits of each RGBA16 color channel, and of its alpha
COLOR_BITS = 5
COLOR_MASK = 0x1F
RED_SHIFT = 11
GREEN_SHIFT = 6
BLUE_SHIFT = 1
ALPHA_MASK = 1
BYTE_MAX = 0xFF

# IA4 packs two texels to a byte, each three bits of intensity and one of
# alpha; the intensity is spread over 0 to 252 in the PNG
NIBBLE_SHIFT = 4
NIBBLE_MASK = 0xF
IA4_INTENSITY_SCALE = 36
# I4 packs two texels to a byte, each four bits of intensity, spread over
# 0 to 255 in the PNG
I4_INTENSITY_SCALE = 17


def expand_color(value: int) -> int:
    """Widen a 5-bit channel to 8 bits so that png_to_c.py gets it back."""
    return (value << (8 - COLOR_BITS)) | (value >> (2 * COLOR_BITS - 8))


def decode_rgba16(data: bytes, width: int, height: int) -> Image.Image:
    """Read RGBA16 texels into an 8-bit RGBA image."""
    texels = bytearray()
    for i in range(0, 2 * width * height, 2):
        texel = int.from_bytes(data[i : i + 2], "big")
        texels += bytes(
            (
                expand_color((texel >> RED_SHIFT) & COLOR_MASK),
                expand_color((texel >> GREEN_SHIFT) & COLOR_MASK),
                expand_color((texel >> BLUE_SHIFT) & COLOR_MASK),
                BYTE_MAX if texel & ALPHA_MASK else 0,
            )
        )
    return Image.frombytes("RGBA", (width, height), bytes(texels))


def decode_i8(data: bytes, width: int, height: int) -> Image.Image:
    """Read I8 texels into a grayscale image."""
    return Image.frombytes("L", (width, height), data[: width * height])


def decode_i4(data: bytes, width: int, height: int) -> Image.Image:
    """Read I4 texels into a grayscale image."""
    pixels = bytearray()
    for byte in data[: width * height // 2]:
        for texel in (byte >> NIBBLE_SHIFT, byte & NIBBLE_MASK):
            pixels.append(texel * I4_INTENSITY_SCALE)
    return Image.frombytes("L", (width, height), bytes(pixels))


def decode_ia4(data: bytes, width: int, height: int) -> Image.Image:
    """Read IA4 texels into a grayscale image with alpha."""
    pixels = bytearray()
    for byte in data[: width * height // 2]:
        for texel in (byte >> NIBBLE_SHIFT, byte & NIBBLE_MASK):
            pixels.append((texel >> 1) * IA4_INTENSITY_SCALE)
            pixels.append(BYTE_MAX if texel & ALPHA_MASK else 0)
    return Image.frombytes("LA", (width, height), bytes(pixels))


DECODERS = {
    "i4": decode_i4,
    "i8": decode_i8,
    "ia4": decode_ia4,
    "rgba16": decode_rgba16,
}


def main() -> None:
    """Write every texture that textures.txt lists."""
    rom = BASEROM.read_bytes()
    for line in TEXTURE_LIST.read_text().splitlines():
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        offset, texture_format, width, height, path = fields
        start = int(offset, 0)
        image = DECODERS[texture_format](rom[start:], int(width), int(height))
        out = ROOT / path
        out.parent.mkdir(parents=True, exist_ok=True)
        image.save(out)


if __name__ == "__main__":
    main()
