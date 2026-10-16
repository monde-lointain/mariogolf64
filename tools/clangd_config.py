#!/usr/bin/env python3
"""Generate an editor-only compilation database from the ROM compiler profiles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import rom_build

ROOT = Path(__file__).resolve().parent.parent


def compile_command(source: Path) -> list[str]:
    # Clang parses the N64 ABI; it never replaces KMC for object generation.
    translations = {"-mips3": "-march=mips3", "-mgp32": "-mabi=32"}
    flags = [translations.get(flag, flag) for flag in rom_build.profile_flags(source)]
    return [
        "clang",
        "--target=mips-linux-gnu",
        "-std=gnu89",
        "-nostdlibinc",
        "-ferror-limit=0",
        "-xc",
        *flags,
        "-c",
        str(source),
    ]


def generate() -> Path:
    # Include editable source TUs, not generated work/src copies or isolated WIP.
    entries = [
        {
            "directory": str(ROOT),
            "file": str(source),
            "arguments": compile_command(source),
        }
        for source in sorted((ROOT / "src").rglob("*.c"))
    ]
    output = ROOT / "compile_commands.json"
    output.write_text(json.dumps(entries, indent=2) + "\n")
    return output


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    print(generate())
