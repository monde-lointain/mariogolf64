"""Name the MIPS binutils programs that read the build's objects and ELF files.

The Makefile exports CROSS, the prefix of the binutils that it found, such
as mips-linux-gnu- or mips64-elf-, so the tools that it runs use the same
ones. A tool run by hand without it uses mips-linux-gnu-, Debian's and
Ubuntu's prefix.
"""

import os

DEFAULT_PREFIX = "mips-linux-gnu-"


def binutil(name: str) -> str:
    """Return the command that runs a MIPS binutils program, such as "objdump"."""
    return os.environ.get("CROSS", DEFAULT_PREFIX) + name
