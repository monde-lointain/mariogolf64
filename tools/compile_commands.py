#!/usr/bin/env python3
"""Writes a compile_commands.json that lets clang tools read the sources.

Usage: compile_commands.py OUTPUT FLAG... -- SOURCE...

The flags are the build's include paths and defines. clang stands in
for KMC GCC 2.7.2, so it parses the sources as GNU C89 for a MIPS
target and keeps quiet about the warnings GCC 2.7 never gave.
"""
import json
import os
import sys

CLANG_FLAGS = [
    "--target=mips-unknown-linux-gnu",
    "-std=gnu89",
    "-nostdinc",
    "-funsigned-char",
    "-w",
]


def main():
    output = sys.argv[1]
    separator = sys.argv.index("--")
    flags = sys.argv[2:separator]
    sources = sys.argv[separator + 1 :]
    directory = os.getcwd()
    commands = [
        {
            "directory": directory,
            "file": source,
            "arguments": ["clang"] + CLANG_FLAGS + flags + ["-c", source],
        }
        for source in sources
    ]
    with open(output, "w") as file:
        json.dump(commands, file, indent=2)


if __name__ == "__main__":
    main()
