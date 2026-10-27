#!/usr/bin/env python3
"""Write ctx.c, the context m2c reads the types and globals of a C file from.

    m2ctx.py C_FILE

The context is C_FILE preprocessed twice: once for its macro definitions and
once for its declarations. Macros the preprocessor defines on its own are
dropped.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP_FLAGS = [
    "-I.",
    "-Iinclude",
    "-Ilib/ultralib/include",
    "-Ilib/ultralib/include/PR",
    "-Ilib/libmus/include",
    "-Ilib/ultralib/src",
    "-Ilib/libmus/src",
    "-D_LANGUAGE_C",
    "-DF3DEX_GBI_2",
    "-D__USE_ISOC99",
    "-DNDEBUG",
    "-D_FINALROM",
    "-D_MIPS_SZLONG=32",
    "-D__attribute__(test...)=",
    "-D__asm__(test...)=",
    "-ffreestanding",
    "-DM2CTX",
    "-DCC_CHECK=1",
    "-DNON_MATCHING",
    "-D_OS_HOST_H_",
    "-std=gnu89",
    "-DVERSION_US=1",
    "-D__IS_KMC__=1",
]


class Arguments(argparse.Namespace):
    """The command line's arguments."""

    c_file: str = ""


def preprocess(arguments: list[str]) -> str:
    """Return the output of the host preprocessor run in the repo."""
    return subprocess.check_output(
        ["gcc", "-E", "-P", "-undef", *arguments], cwd=ROOT, encoding="utf-8"
    )


def import_c_file(c_path: str) -> str:
    """Return the context of the C file at C_PATH."""
    relative_path = Path(c_path).resolve().relative_to(ROOT)
    with tempfile.NamedTemporaryFile(suffix=".c") as empty_file:
        stock_macros = preprocess(["-dM", empty_file.name])

    macros_command = ["-dM", *CPP_FLAGS, str(relative_path)]
    try:
        context = preprocess(macros_command)
        context += preprocess([*CPP_FLAGS, str(relative_path)])
    except subprocess.CalledProcessError:
        command = " ".join(["gcc", "-E", "-P", "-undef", *macros_command])
        print(
            f"Failed to preprocess input file, when running command:\n{command}",
            file=sys.stderr,
        )
        sys.exit(1)

    if not context:
        print("Output is empty - aborting")
        sys.exit(1)

    for line in stock_macros.strip().splitlines():
        context = context.replace(line + "\n", "")
    return context


def main() -> None:
    """Write the context of the C file named on the command line."""
    parser = argparse.ArgumentParser(
        description="Create a context file which can be used for m2c"
    )
    _ = parser.add_argument("c_file", help="File from which to create context")
    arguments = parser.parse_args(namespace=Arguments())
    context = import_c_file(arguments.c_file)
    _ = (ROOT / "ctx.c").write_text(context, encoding="utf-8")


if __name__ == "__main__":
    main()
