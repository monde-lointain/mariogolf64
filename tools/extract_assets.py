#!/usr/bin/env python3
"""Cut the files that the build needs from the original ROM.

    extract_assets.py

Each line of assets.txt gives a file's start and end in the ROM and its
path; '#' starts a comment. The files go under assets/, which is never
committed. The ROM must be the original, whose SHA-1 mariogolf64.sha1 gives.
"""

import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASEROM = ROOT / "baserom.us.z64"
ASSET_LIST = ROOT / "assets.txt"
CHECKSUM = ROOT / "mariogolf64.sha1"
# A line's start, end and path
FIELD_COUNT = 3


def read_list(text: str) -> list[tuple[int, int, str]]:
    """Return the start, end and path of each file that TEXT lists."""
    files: list[tuple[int, int, str]] = []
    for number, line in enumerate(text.splitlines(), start=1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        if len(fields) != FIELD_COUNT:
            msg = f"{ASSET_LIST.name}:{number}: expected START END PATH"
            raise ValueError(msg)
        start, end, path = fields
        files.append((int(start, 0), int(end, 0), path))
    return files


def main() -> int:
    """Write every file that assets.txt lists; return the exit status."""
    if not BASEROM.exists():
        print(
            (
                f"Copy your Mario Golf (North America) ROM to {BASEROM.name} in "
                f"{ROOT}, as a big-endian .z64 file."
            ),
            file=sys.stderr,
        )
        return 1
    rom = BASEROM.read_bytes()
    expected = CHECKSUM.read_text().split()[0]
    if hashlib.sha1(rom).hexdigest() != expected:  # noqa: S324 - names the ROM, secures nothing
        print(
            (
                f"{BASEROM.name} isn't the original Mario Golf (North America) "
                f"ROM: its SHA-1 should be {expected}. A .n64 or .v64 file must "
                "be converted to big-endian .z64 first."
            ),
            file=sys.stderr,
        )
        return 1
    for start, end, path in read_list(ASSET_LIST.read_text()):
        out = ROOT / path
        out.parent.mkdir(parents=True, exist_ok=True)
        _ = out.write_bytes(rom[start:end])
    return 0


if __name__ == "__main__":
    sys.exit(main())
