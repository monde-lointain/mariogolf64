#!/usr/bin/env python3
"""Decode an asset from the archive in the base ROM, as ReadEncodedAsset does.

    decode_asset.py ASSET_ID [[FORMAT] OUT.png]

Without OUT.png, prints the decoded size and the image header that most
image assets start with, and the format its first halfword gives. With
OUT.png, writes the image there, taking its size from that header. FORMAT
(ci4, ci8, i4, i8, ia4, ia8, ia16 or rgba16) defaults to the header's;
the header does not tell intensity from intensity-alpha, so i4 and i8
stand for both. CI images keep their palette after the header. The
images are for reading the code, not for committing.
"""

import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
BASEROM = ROOT / "baserom.us.z64"
ARCHIVE_START = 0xE473F0  # g_AssetArchiveRomStart
ARCHIVE_ENTRY_SIZE = 8
ENCODING_BYTE_RUNS = 1
ENCODING_HEADER_SIZE = 4
METHOD_SHIFT = 24
DECODED_SIZE_MASK = 0xFFFFFF
# The longest run that a count byte gives, either way. The original's
# encoder went no further, though a copy could take 128.
RUN_LENGTH_MAX = 127
IMAGE_HEADER_SIZE = 8
IMAGE_SIZE_OFFSET = 2
OUTPUT_ARGUMENT_COUNT = 3
FORMAT_ARGUMENT_COUNT = 4
# ImageHeader.format's codes
HEADER_FORMATS = {0: "i4", 2: "ci4", 4: "i8", 6: "ci8", 8: "rgba16"}

# Texel widths, in bits
NIBBLE_BITS = 4
BYTE_BITS = 8
HALFWORD_BITS = 16
NIBBLE_MASK = 0xF
BYTE_MASK = 0xFF
CI4_COLOR_COUNT = 16
CI8_COLOR_COUNT = 256
PALETTE_ENTRY_SIZE = 2

# An RGBA16 texel is 5 bits for each color, then 1 bit of alpha
RGBA16_RED_SHIFT = 11
RGBA16_GREEN_SHIFT = 6
RGBA16_BLUE_SHIFT = 1
RGBA16_CHANNEL_MASK = 0x1F
RGBA16_CHANNEL_SHIFT = 3
RGBA16_LOW_BITS_SHIFT = 2
OPAQUE = 255
TRANSPARENT = 0
# Scales a 3-bit IA4 intensity, about 255 / 7
IA4_INTENSITY_SCALE = 36
# Scales a 4-bit value to 8 bits, 255 / 15
NIBBLE_SCALE = 17

Pixel = tuple[int, int, int, int]
TRANSPARENT_BLACK: Pixel = (0, 0, 0, TRANSPARENT)


def read_asset(rom: bytes, asset_id: int) -> bytes:
    """Return the encoded bytes of an asset."""
    entry = ARCHIVE_START + asset_id * ARCHIVE_ENTRY_SIZE
    size, offset = struct.unpack_from(">iI", rom, entry)
    start = ARCHIVE_START + offset
    return rom[start : start + size]


def decode_runs(data: bytes, unit: int) -> bytes:
    """Decode runs of signed counts: positive repeats a unit, negative copies units."""
    out = bytearray()
    position = 0
    while True:
        if position >= len(data):
            message = "the runs end without a zero count"
            raise struct.error(message)
        count = int.from_bytes(data[position : position + 1], "big", signed=True)
        position += 1
        if count == 0:
            return bytes(out)
        if count > 0:
            out += data[position : position + unit] * count
            position += unit
        else:
            out += data[position : position + unit * -count]
            position += unit * -count


def encode_runs(data: bytes, unit: int) -> bytes:
    """Return DATA as decode_runs reads it: repeated units as runs, the rest copied."""
    units = [data[i : i + unit] for i in range(0, len(data), unit)]
    out = bytearray()
    copied: list[bytes] = []

    def copy_out() -> None:
        while copied:
            chunk = copied[:RUN_LENGTH_MAX]
            del copied[:RUN_LENGTH_MAX]
            out.extend((-len(chunk)).to_bytes(1, "big", signed=True))
            out.extend(b"".join(chunk))

    position = 0
    while position < len(units):
        end = position
        while (
            end < len(units)
            and units[end] == units[position]
            and end - position < RUN_LENGTH_MAX
        ):
            end += 1
        if end - position > 1:
            copy_out()
            out.append(end - position)
            out += units[position]
            position = end
        else:
            copied.append(units[position])
            position += 1
    copy_out()
    out.append(0)
    return bytes(out)


def encode_asset(data: bytes) -> bytes:
    """Return DATA as an asset that ReadEncodedAsset decodes, in byte runs."""
    word = ENCODING_BYTE_RUNS << METHOD_SHIFT | len(data)
    return word.to_bytes(ENCODING_HEADER_SIZE, "big") + encode_runs(data, 1)


def decode_asset(rom: bytes, asset_id: int) -> bytes:
    """Return an asset decoded, as ReadEncodedAsset leaves it."""
    return decode_encoded(read_asset(rom, asset_id))


def decode_encoded(data: bytes) -> bytes:
    """Return an encoded asset's bytes decoded."""
    word = int.from_bytes(data[:ENCODING_HEADER_SIZE], "big")
    method = word >> METHOD_SHIFT
    size = word & DECODED_SIZE_MASK
    unit = 1 if method == ENCODING_BYTE_RUNS else 2
    return decode_runs(data[ENCODING_HEADER_SIZE:], unit)[:size]


def widen_channel(bits: int) -> int:
    """Return a 5-bit channel scaled to 8 bits."""
    return bits << RGBA16_CHANNEL_SHIFT | bits >> RGBA16_LOW_BITS_SHIFT


def convert_rgba16(value: int) -> Pixel:
    """Return the RGBA pixel of an RGBA16 texel."""
    return (
        widen_channel(value >> RGBA16_RED_SHIFT & RGBA16_CHANNEL_MASK),
        widen_channel(value >> RGBA16_GREEN_SHIFT & RGBA16_CHANNEL_MASK),
        widen_channel(value >> RGBA16_BLUE_SHIFT & RGBA16_CHANNEL_MASK),
        OPAQUE if value & 1 else TRANSPARENT,
    )


def read_texel_values(texels: bytes, bits: int, count: int) -> list[int]:
    """Return COUNT texels of BITS bits each, high nibble first."""
    if bits == NIBBLE_BITS:
        return [
            texels[i // 2] >> (NIBBLE_BITS if i % 2 == 0 else 0) & NIBBLE_MASK
            for i in range(count)
        ]
    if bits == BYTE_BITS:
        return list(texels[:count])
    if len(texels) < count * 2:
        message = f"{count} halfwords don't fit in {len(texels)} bytes"
        raise struct.error(message)
    return [int.from_bytes(texels[i * 2 : i * 2 + 2], "big") for i in range(count)]


def convert_ci(body: bytes, image_format: str, count: int) -> list[Pixel]:
    """Return the pixels of a CI image, whose palette comes first."""
    color_count = CI4_COLOR_COUNT if image_format == "ci4" else CI8_COLOR_COUNT
    palette = [
        convert_rgba16(value)
        for value in read_texel_values(body, HALFWORD_BITS, color_count)
    ]
    texels = body[color_count * PALETTE_ENTRY_SIZE :]
    values = read_texel_values(texels, int(image_format[2:]), count)
    return [palette[value] for value in values]


def convert_ia(body: bytes, bits: int, count: int) -> list[Pixel]:
    """Return the pixels of an IA image."""
    values = read_texel_values(body, bits, count)
    if bits == NIBBLE_BITS:
        pixels: list[Pixel] = []
        for value in values:
            intensity = (value >> 1) * IA4_INTENSITY_SCALE
            pixels.append((intensity, intensity, intensity, OPAQUE * (value & 1)))
        return pixels
    if bits == BYTE_BITS:
        pixels = []
        for value in values:
            intensity = (value >> NIBBLE_BITS) * NIBBLE_SCALE
            alpha = (value & NIBBLE_MASK) * NIBBLE_SCALE
            pixels.append((intensity, intensity, intensity, alpha))
        return pixels
    pixels = []
    for value in values:
        intensity = value >> BYTE_BITS
        pixels.append((intensity, intensity, intensity, value & BYTE_MASK))
    return pixels


def convert_image(decoded: bytes, image_format: str) -> Image.Image:
    """Return a decoded image asset as an RGBA image."""
    width, height = struct.unpack_from(">HH", decoded, IMAGE_SIZE_OFFSET)
    count = width * height
    body = decoded[IMAGE_HEADER_SIZE:]
    pixels: list[Pixel] = []
    if image_format.startswith("ci"):
        pixels = convert_ci(body, image_format, count)
    elif image_format == "rgba16":
        values = read_texel_values(body, HALFWORD_BITS, count)
        pixels = [convert_rgba16(value) for value in values]
    elif image_format.startswith("ia"):
        pixels = convert_ia(body, int(image_format[2:]), count)
    else:
        bits = int(image_format[1:])
        scale = NIBBLE_SCALE if bits == NIBBLE_BITS else 1
        for value in read_texel_values(body, bits, count):
            intensity = value * scale
            pixels.append((intensity, intensity, intensity, OPAQUE))
    # Short image data leaves the rest of the image transparent black
    rgba = bytes(channel for pixel in pixels for channel in pixel)
    rgba = rgba.ljust(count * len(TRANSPARENT_BLACK), b"\0")
    return Image.frombytes("RGBA", (width, height), rgba)


def main() -> None:
    """Print an asset's header, or write it as an image."""
    asset_id = int(sys.argv[1], 0)
    decoded = decode_asset(BASEROM.read_bytes(), asset_id)
    header = (
        [
            int.from_bytes(decoded[i : i + 2], "big")
            for i in range(0, IMAGE_HEADER_SIZE, 2)
        ]
        if len(decoded) >= IMAGE_HEADER_SIZE
        else []
    )
    header_format = HEADER_FORMATS.get(header[0]) if header else None
    if len(sys.argv) < OUTPUT_ARGUMENT_COUNT:
        shown_header = [hex(value) for value in header]
        print(
            f"asset {asset_id:#x}: {len(decoded):#x} bytes,",
            f"header {shown_header}, format {header_format}",
        )
        return
    if len(sys.argv) >= FORMAT_ARGUMENT_COUNT:
        image_format, output = sys.argv[2], sys.argv[3]
    elif header_format:
        image_format, output = header_format, sys.argv[2]
    else:
        sys.exit(f"asset {asset_id:#x} has no known format; give one")
    convert_image(decoded, image_format).save(output)


if __name__ == "__main__":
    main()
