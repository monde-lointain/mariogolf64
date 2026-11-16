#!/usr/bin/env python3
"""Split the asset archive into its assets, or pack them into it again.

    asset_archive.py split ARCHIVE DIRECTORY
    asset_archive.py pack DIRECTORY ARCHIVE [REPLACEMENTS...]

The archive begins with a table of each asset's size and its offset from
the archive's start, eight bytes an asset, which ReadAssetFileBounds
indexes by asset ID. The assets follow in the order of their IDs, each
padded with zeros to a multiple of eight bytes.

split writes each asset to DIRECTORY as its ID in three hexadecimal
digits, such as 688.bin for ASSET_PLACES. pack reads them back in that
order, so an asset can change size, but a new one must take the next ID.
An asset in one of the directories REPLACEMENTS, such as a custom
character's face sprites, takes the place of the one of the same name. Where
several have it, the first's is taken, as a mod's file is taken before those
of the folders it builds on.
"""

import struct
import sys
from collections.abc import Sequence
from pathlib import Path
from typing import cast

TABLE_ENTRY = struct.Struct(">iI")  # An asset's size, then its offset
ASSET_ALIGNMENT = 8
ARCHIVE_ALIGNMENT = 16
ASSET_NAME_DIGITS = 3
ASSET_SUFFIX = ".bin"
# The original's writer filled a buffer of this many bytes and wrote it out
# each time it was full. It padded a file to ARCHIVE_ALIGNMENT without
# clearing the buffer, so the padding repeats the bytes this far before it.
WRITE_BUFFER_SIZE = 0x2000

USAGE_EXIT = 2
ARGUMENT_COUNT = 4


def padding_to(size: int, alignment: int) -> int:
    """Return how many bytes bring SIZE up to a multiple of ALIGNMENT."""
    return -size % alignment


def pad_as_written(data: bytes) -> bytes:
    """Return DATA padded to ARCHIVE_ALIGNMENT as the original's writer did."""
    padded = bytearray(data)
    for _ in range(padding_to(len(data), ARCHIVE_ALIGNMENT)):
        stale = len(padded) - WRITE_BUFFER_SIZE
        padded.append(padded[stale] if stale >= 0 else 0)
    return bytes(padded)


def read_entry(archive: bytes, asset_id: int) -> tuple[int, int]:
    """Return the size and offset that ARCHIVE's table gives an asset."""
    entry = TABLE_ENTRY.unpack_from(archive, asset_id * TABLE_ENTRY.size)
    return cast("tuple[int, int]", entry)


def split(archive: bytes, directory: Path) -> None:
    """Write each asset of ARCHIVE to its own file in DIRECTORY."""
    # The first asset follows the table, so its offset is the table's size
    _, table_size = read_entry(archive, 0)
    directory.mkdir(parents=True, exist_ok=True)
    for asset_id in range(table_size // TABLE_ENTRY.size):
        size, offset = read_entry(archive, asset_id)
        name = f"{asset_id:0{ASSET_NAME_DIGITS}X}{ASSET_SUFFIX}"
        _ = (directory / name).write_bytes(archive[offset : offset + size])


def read_assets(directory: Path, replacements: Sequence[Path] = ()) -> list[bytes]:
    """Return the assets in DIRECTORY in the order of their IDs, or REPLACEMENTS'.

    An asset is taken from the first of REPLACEMENTS that has it.
    """
    paths = sorted(
        directory.glob(f"*{ASSET_SUFFIX}"), key=lambda path: int(path.stem, 16)
    )
    if not paths:
        sys.exit(f"expected the assets in {directory}; run make extract")
    for asset_id, path in enumerate(paths):
        if int(path.stem, 16) != asset_id:
            sys.exit(
                f"expected asset {asset_id:X} in {directory}, but found {path.name}"
            )
    for folder in reversed(replacements):
        for path in folder.glob(f"*{ASSET_SUFFIX}"):
            if not (directory / path.name).is_file():
                sys.exit(f"{path} replaces no asset of {directory}")
        paths = [
            folder / path.name if (folder / path.name).is_file() else path
            for path in paths
        ]
    return [path.read_bytes() for path in paths]


def pack(assets: list[bytes]) -> bytes:
    """Return the archive that holds ASSETS."""
    table = bytearray()
    body = bytearray()
    table_size = len(assets) * TABLE_ENTRY.size
    for asset in assets:
        table += TABLE_ENTRY.pack(len(asset), table_size + len(body))
        body += asset
        body += bytes(padding_to(len(asset), ASSET_ALIGNMENT))

    return pad_as_written(bytes(table + body))


def main() -> None:
    """Split or pack the archive that the command line names."""
    count = len(sys.argv)
    command = sys.argv[1] if count > 1 else ""
    usable = (command == "split" and count == ARGUMENT_COUNT) or (
        command == "pack" and count >= ARGUMENT_COUNT
    )
    if not usable:
        print(__doc__, file=sys.stderr)
        sys.exit(USAGE_EXIT)
    if sys.argv[1] == "split":
        split(Path(sys.argv[2]).read_bytes(), Path(sys.argv[3]))
    else:
        out = Path(sys.argv[3])
        out.parent.mkdir(parents=True, exist_ok=True)
        replacements = [Path(folder) for folder in sys.argv[ARGUMENT_COUNT:]]
        _ = out.write_bytes(pack(read_assets(Path(sys.argv[2]), replacements)))


if __name__ == "__main__":
    main()
