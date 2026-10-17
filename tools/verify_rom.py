#!/usr/bin/env python3
"""The only ROM match oracle: successful matching build, then authoritative SHA-1.

Build output (configure, splat extraction, ninja) goes to `<build-dir>/verify-rom.log`; the
terminal gets one verdict line. A failed build prints the failing ninja step and its error lines;
a hash mismatch also names the first differing ROM offset against the retail image, which tells a
layout shift (an early offset in unrelated code) from a local code difference.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

from rom_build import ROOT, build_dir

# Lines that carry the failure in a captured build log. Splat's always-present
# "Unable to determine a segment" warning is not a failure and is excluded.
FAILURE_LINE = re.compile(
    r"FAILED:|undefined reference|error:|Error:|Fatal error|\.c:\d+:|multiple definition|"
    r"Traceback|subcommand failed",
)
BENIGN_LINE = re.compile(r"Unable to determine a segment")
FAILURE_EXCERPT = 40


def config_value(text: str, key: str) -> str:
    match = re.search(rf"^\s*{key}:\s*(\S+)", text, re.MULTILINE)
    if match is None:
        raise SystemExit(f"mariogolf64.yaml has no `{key}:` entry")
    return match.group(1)


def run_logged(command: list[str], environment: dict, log, verbose: bool) -> None:
    log.write(f"$ {' '.join(command)}\n")
    log.flush()
    if verbose:
        subprocess.run(command, env=environment, check=True)
        return
    subprocess.run(command, env=environment, check=True, stdout=log, stderr=subprocess.STDOUT)


def failure_excerpt(log_path: Path) -> list[str]:
    lines = log_path.read_text(errors="replace").splitlines()
    hits = [line for line in lines if FAILURE_LINE.search(line) and not BENIGN_LINE.search(line)]
    return (hits or lines)[-FAILURE_EXCERPT:]


def first_difference(built: Path, retail: Path) -> str:
    try:
        left, right = built.read_bytes(), retail.read_bytes()
    except OSError as error:
        return f"retail image unavailable for comparison ({error})"
    for offset, (a, b) in enumerate(zip(left, right, strict=False)):
        if a != b:
            return f"first differing ROM offset 0x{offset:X} (sizes 0x{len(left):X} vs 0x{len(right):X})"
    return f"common prefix identical; sizes differ 0x{len(left):X} vs 0x{len(right):X}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--extract", action="store_true")
    parser.add_argument("--build-dir", type=Path, default=build_dir())
    parser.add_argument("--verbose", action="store_true", help="stream build output instead of logging it")
    args = parser.parse_args()
    directory = (ROOT / args.build_dir).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    environment = {**os.environ, "MG64_BUILD_DIR": str(directory)}
    log_path = directory / "verify-rom.log"
    config_text = (ROOT / "mariogolf64.yaml").read_text()
    with log_path.open("w") as log:
        try:
            if not (directory / "CMakeCache.txt").is_file():
                run_logged(["cmake", "-S", str(ROOT), "-B", str(directory), "-G", "Ninja"], environment, log, args.verbose)
            config = json.loads((directory / "build-tools.json").read_text())
            if config["nonmatching"]:
                print("REFUSED: nonmatching builds are not eligible for the ROM gate", file=sys.stderr)
                return 1
            if args.extract:
                (directory / "extract.stamp").unlink(missing_ok=True)
                run_logged(["cmake", "--build", str(directory), "--target", "extract"], environment, log, args.verbose)
            run_logged(["cmake", "--build", str(directory), "--target", "rom"], environment, log, args.verbose)
        except (subprocess.CalledProcessError, OSError, ValueError) as error:
            log.flush()
            print(f"BUILD FAILED — SHA-1 NOT trusted (stale-ROM guard): {error}", file=sys.stderr)
            if not args.verbose:
                for line in failure_excerpt(log_path):
                    print(f"  {line}", file=sys.stderr)
                print(f"full log: {log_path}", file=sys.stderr)
            return 1
    expected = config_value(config_text, "sha1")
    path = directory / "mariogolf64.z64"
    try:
        with path.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha1").hexdigest()
    except OSError as error:
        print(f"ROM missing after build: {error}", file=sys.stderr)
        return 1
    if actual != expected:
        retail = ROOT / config_value(config_text, "target_path")
        print(f"ROM SHA-1 MISMATCH: got {actual}, expected {expected}", file=sys.stderr)
        print(f"  {first_difference(path, retail)}; full log: {log_path}", file=sys.stderr)
        return 1
    print(f"CMake build OK + ROM SHA-1 MATCH ({actual})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
