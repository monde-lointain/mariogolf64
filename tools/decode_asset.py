#!/usr/bin/env python3
"""Decode an asset from the archive in the base ROM, as ReadEncodedAsset does.

    decode_asset.py ASSET_ID [FORMAT OUT.png]

Without FORMAT, prints the decoded size and the image header that most
image assets start with. With FORMAT (ci4, ci8, i4, i8, ia4, ia8, ia16 or
rgba16), writes the image to OUT.png, taking its size from that header.
CI images keep their palette after the header. The images are for
reading the code, not for committing.
"""

import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASEROM = os.path.join(ROOT, "baserom.us.z64")
ARCHIVE_START = 0xE473F0  # g_AssetArchiveRomStart
ENCODING_BYTE_RUNS = 1
IMAGE_HEADER_SIZE = 8


def read_asset(rom, asset_id):
    size, offset = struct.unpack_from(">iI", rom, ARCHIVE_START + asset_id * 8)
    start = ARCHIVE_START + offset
    return rom[start:start + size]


def decode_runs(data, unit):
    """Decodes runs of signed counts: positive repeats a unit, negative copies units."""
    out = bytearray()
    position = 0
    while True:
        count = struct.unpack_from(">b", data, position)[0]
        position += 1
        if count == 0:
            return bytes(out)
        if count > 0:
            out += data[position:position + unit] * count
            position += unit
        else:
            out += data[position:position + unit * -count]
            position += unit * -count


def decode_asset(rom, asset_id):
    data = read_asset(rom, asset_id)
    word = struct.unpack_from(">I", data)[0]
    method, size = word >> 24, word & 0xFFFFFF
    unit = 1 if method == ENCODING_BYTE_RUNS else 2
    return decode_runs(data[4:], unit)[:size]


def rgba16(value):
    def channel(bits):
        return bits << 3 | bits >> 2
    return (channel(value >> 11 & 31), channel(value >> 6 & 31), channel(value >> 1 & 31),
            255 if value & 1 else 0)


def texel_values(texels, bits, count):
    if bits == 4:
        return [texels[i // 2] >> (4 if i % 2 == 0 else 0) & 15 for i in range(count)]
    if bits == 8:
        return list(texels[:count])
    return list(struct.unpack_from(f">{count}H", texels))


def to_image(decoded, image_format):
    width, height = struct.unpack_from(">HH", decoded, 2)
    count = width * height
    body = decoded[IMAGE_HEADER_SIZE:]
    if image_format.startswith("ci"):
        colors = 16 if image_format == "ci4" else 256
        palette = [rgba16(v) for v in struct.unpack_from(f">{colors}H", body)]
        values = texel_values(body[colors * 2:], int(image_format[2:]), count)
        pixels = [palette[v] for v in values]
    elif image_format == "rgba16":
        pixels = [rgba16(v) for v in texel_values(body, 16, count)]
    elif image_format.startswith("ia"):
        bits = int(image_format[2:])
        values = texel_values(body, bits, count)
        if bits == 4:
            pixels = [((v >> 1) * 36, (v >> 1) * 36, (v >> 1) * 36, 255 * (v & 1)) for v in values]
        elif bits == 8:
            pixels = [((v >> 4) * 17,) * 3 + ((v & 15) * 17,) for v in values]
        else:
            pixels = [(v >> 8,) * 3 + (v & 255,) for v in values]
    else:
        bits = int(image_format[1:])
        scale = 17 if bits == 4 else 1
        pixels = [(v * scale,) * 3 + (255,) for v in texel_values(body, bits, count)]
    image = Image.new("RGBA", (width, height))
    image.putdata(pixels)
    return image


def main():
    asset_id = int(sys.argv[1], 0)
    with open(BASEROM, "rb") as rom_file:
        rom = rom_file.read()
    decoded = decode_asset(rom, asset_id)
    if len(sys.argv) < 4:
        header = struct.unpack_from(">4H", decoded) if len(decoded) >= IMAGE_HEADER_SIZE else ()
        print(f"asset {asset_id:#x}: {len(decoded):#x} bytes, header {[hex(v) for v in header]}")
        return
    to_image(decoded, sys.argv[2]).save(sys.argv[3])


if __name__ == "__main__":
    main()
