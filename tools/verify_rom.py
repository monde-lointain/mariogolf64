#!/usr/bin/env python3
"""The only ROM match oracle: successful matching build, then authoritative SHA-1."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

from rom_build import ROOT, build_dir


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--extract", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=build_dir())
    args = parser.parse_args()
    directory = (ROOT / args.build_dir).resolve()
    environment = {**os.environ, "MG64_BUILD_DIR": str(directory)}
    try:
        if not (directory / "CMakeCache.txt").is_file():
            subprocess.run(["cmake", "-S", str(ROOT), "-B", str(directory), "-G", "Ninja"], env=environment, check=True)
        config = json.loads((directory / "build-tools.json").read_text())
        if config["nonmatching"]:
            print("REFUSED: nonmatching builds are not eligible for the ROM gate", file=sys.stderr)
            return 1
        if args.extract:
            (directory / "extract.stamp").unlink(missing_ok=True)
            subprocess.run(["cmake", "--build", str(directory), "--target", "extract"], env=environment, check=True)
        subprocess.run(["cmake", "--build", str(directory), "--target", "rom"], env=environment, check=True)
    except (subprocess.CalledProcessError, OSError, ValueError) as error:
        print(f"BUILD FAILED — SHA-1 NOT trusted (stale-ROM guard): {error}", file=sys.stderr)
        return 1
    expected = re.search(r"^sha1:\s*(\w+)", (ROOT / "mariogolf64.yaml").read_text(), re.MULTILINE).group(1)
    path = directory / "mariogolf64.z64"
    try:
        with path.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha1").hexdigest()
    except OSError as error:
        print(f"ROM missing after build: {error}", file=sys.stderr)
        return 1
    if actual != expected:
        print(f"ROM SHA-1 MISMATCH: got {actual}, expected {expected}", file=sys.stderr)
        return 1
    print(f"CMake build OK + ROM SHA-1 MATCH ({actual})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
