#!/usr/bin/env python3
"""Check that each static buffer the RCP or a DMA uses asks for 8 bytes.

    check_alignment.py BUILD_DIR [--include DIR]...

The RDP's image addresses, the RSP's and the DMA engines' DRAM addresses
ignore their low three bits, and TextureAddress clears them, so a buffer
that one of them reads or writes must start on an 8-byte boundary. The
original's linker placed each one so, mostly by chance. Another layout,
which a change to the code makes, keeps only the alignment that a buffer
asks for: with RCP_ALIGNED (include/system/compiler.h), or through its
type, as ImageHeader and gbi.h's types do. A buffer that doesn't ask can
land 4 bytes off and be drawn or read from the wrong place.

clang-query finds, in each file that BUILD_DIR's compile_commands.json
lists, every statically allocated array or structure whose address, or
the address of a part of it, reaches the RCP or a DMA: the buffer argument
of the routines that read the ROM or SRAM by DMA (TRANSFER_ROUTINES), or
an argument of the macros that hand an address to the RCP
(RCP_ADDRESS_MACROS). Addresses held in pointers aren't followed; the
heap's blocks are 8-byte aligned.

The alignment that each buffer asks for is read from BUILD_DIR's objects,
which -fdata-sections gives a section for each object, aligned as the
compiler was asked: the modern build's.

clang can't read GCC's nested routines, so it skips their bodies. Within
those, each argument of a routine or macro of the lists above is searched
for the names of the file's objects larger than a pointer instead, which
must ask for 8 bytes too.

Each buffer found that asks for less is printed, and the exit status is 1
if there were any.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import cast, final

from binutils import binutil

SUMMARY = "Check that each static buffer the RCP or a DMA uses asks for 8 bytes."

OBJDUMP = binutil("objdump")
DEFAULT_CLANG_QUERY = "clang-query-23"
COMPILE_COMMANDS = "compile_commands.json"
OBJECT_SUFFIX = ".o"
RCP_ALIGNMENT = 8
POINTER_SIZE = 4

# The routines that read or write a buffer by DMA, through their parameter
# of type void *
TRANSFER_ROUTINES = (
    "ReadEncodedAsset",
    "ReadAssetFile",
    "nuPiReadRom",
    "nuPiReadWriteSram",
    "osVirtualToPhysical",
    "osPiStartDma",
    "osEPiStartDma",
)
# The macros that hand an address to the RCP, directly or through the
# macros built on them, such as gDPLoadTextureBlock
RCP_ADDRESS_MACROS = (
    "TextureAddress",
    "RenderTargetAddress",
    "gDPSetTextureImage",
    "gsDPSetTextureImage",
    "gDPSetColorImage",
    "gsDPSetColorImage",
    "gDPSetDepthImage",
    "gsDPSetDepthImage",
    "gSPSegment",
    "gsSPSegment",
    "gSPVertex",
    "gsSPVertex",
    "gSPMatrix",
    "gsSPMatrix",
    "gSPDisplayList",
    "gsSPDisplayList",
    "gSPBranchList",
    "gsSPBranchList",
)
# The macros of gbi.h that load a texture or palette, which expand to
# gDPSetTextureImage; nested routines are searched for these by name too
LOAD_MACROS = r"gs?DP(?:Load(?:Texture|TLUT|MultiBlock|MultiTile|Block|Tile)\w*)"

BINDING = "object"


def node(name: str, *arguments: str) -> str:
    """Return the matcher NAME applied to ARGUMENTS."""
    return f"{name}({', '.join(arguments)})"


def quoted(text: str) -> str:
    """Return TEXT as a matcher's string argument."""
    return f'"{text}"'


def ignoring(matcher: str) -> str:
    """Return a matcher of MATCHER under any parentheses and implicit casts."""
    return node("ignoringParenImpCasts", matcher)


def member_of(matcher: str) -> str:
    """Return a matcher of a member (with .) of what MATCHER matches."""
    return node("memberExpr", node("hasObjectExpression", ignoring(matcher)))


def element_of(matcher: str) -> str:
    """Return a matcher of an element of the array that MATCHER matches."""
    return node("arraySubscriptExpr", node("hasBase", ignoring(matcher)))


def cast_kind(kind: str) -> str:
    """Return a matcher of an implicit cast of KIND."""
    return node("implicitCastExpr", node("hasCastKind", quoted(kind)))


def type_is(matcher: str) -> str:
    """Return a matcher of an expression or declaration of type MATCHER."""
    return node("hasType", node("hasCanonicalType", matcher))


def build_query() -> list[str]:
    """Return clang-query's commands that find each buffer as BINDING."""
    aggregate = node("anyOf", type_is("arrayType()"), type_is("recordType()"))
    variable = node("varDecl", "hasGlobalStorage()", aggregate)
    reference = f"{node('declRefExpr', node('to', variable))}.bind({quoted(BINDING)})"
    # The object, a member or element of it, or an element's member and so on
    place = node(
        "expr",
        node(
            "anyOf",
            reference,
            member_of(reference),
            element_of(reference),
            member_of(element_of(reference)),
            element_of(member_of(reference)),
        ),
    )
    taken = node(
        "unaryOperator",
        node("hasOperatorName", quoted("&")),
        node("hasUnaryOperand", node("ignoringParens", place)),
    )
    # An array used for its address, not one indexed for an element's value
    decayed = node(
        "implicitCastExpr",
        node("hasCastKind", quoted("CK_ArrayToPointerDecay")),
        node("hasSourceExpression", node("ignoringParens", place)),
        node("unless", node("hasParent", "arraySubscriptExpr()")),
    )
    # An address that is loaded from, as an array of pointers' is to read
    # one, doesn't reach the routine or macro itself
    loaded = node("hasAncestor", cast_kind("CK_LValueToRValue"))
    address = node("expr", node("anyOf", taken, decayed), node("unless", loaded))
    # hasArgument and its kind see past implicit casts, so an array named as
    # an argument shows as itself rather than as its decay
    argument = node(
        "expr",
        node(
            "anyOf",
            address,
            node("hasDescendant", address),
            node("expr", place, type_is("arrayType()")),
        ),
    )
    routines = node("hasAnyName", *map(quoted, TRANSFER_ROUTINES))
    void_pointer = node("pointerType", node("pointee", "voidType()"))
    transfer = node(
        "callExpr",
        node("callee", node("functionDecl", routines)),
        node(
            "forEachArgumentWithParam",
            argument,
            node("parmVarDecl", node("hasType", void_pointer)),
        ),
    )
    expanded = (
        node("isExpandedFromMacro", quoted(name)) for name in RCP_ADDRESS_MACROS
    )
    drawn = node("expr", address, node("anyOf", *expanded))
    return [
        "set output diag",
        "enable output print",
        "set bind-root false",
        f"match {node('expr', node('anyOf', transfer, drawn))}",
    ]


# clang-query's output: a file it reads, a match's place and the name
# bound, and clang's errors
PROCESSING = re.compile(r"^\[\d+/\d+\] Processing file ", re.MULTILINE)
BOUND = re.compile(
    rf"""
    ^(?P<place>\S+:\d+:\d+):\ note:\ "{BINDING}"\ binds\ here$
    (?:\n.*)*?
    \nBinding\ for\ "{BINDING}":\n(?P<name>\w+)$
    """,
    re.MULTILINE | re.VERBOSE,
)
ERROR = re.compile(
    r"^(?P<file>[^\s:]+):(?P<line>\d+):\d+: (?:fatal )?error: (?P<message>.+)$",
    re.MULTILINE,
)
# What clang says of GCC's nested routines, which it can't read
NESTED_ROUTINE_ERRORS = (
    "function definition is not allowed here",
    "illegal storage class on function",
)
NESTED_ROUTINE_START = NESTED_ROUTINE_ERRORS[0]

# objdump -h's lines: an object's file, and a section with its size and
# alignment. -fdata-sections names an object's section .KIND.NAME, and a
# routine's static .KIND.NAME.NUMBER.
OBJECT_FILE = re.compile(r"^(?P<file>\S+):\s+file format", re.MULTILINE)
DATA_SECTION = re.compile(
    r"""
    ^\s*\d+\s+
    \.(?:bss|data|rodata|sbss|sdata)\.(?P<name>[A-Za-z_]\w*?)(?:\.\d+)?\s+
    (?P<size>[0-9a-f]+)(?:\s+[0-9a-f]+){3}\s+
    2\*\*(?P<power>\d+)$
    """,
    re.MULTILINE | re.VERBOSE,
)
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")
NESTED_USE = re.compile(
    r"\b(?:{})\s*\(".format(
        "|".join([*TRANSFER_ROUTINES, *RCP_ADDRESS_MACROS, LOAD_MACROS])
    )
)


@final
class DataObject:
    """A statically allocated object's size and the alignment it asks for."""

    def __init__(self, size: int, alignment: int) -> None:
        """Hold SIZE and ALIGNMENT, in bytes."""
        self.size: int = size
        self.alignment: int = alignment


@final
class Objects:
    """The data objects that the build's object files define."""

    def __init__(self, object_files: list[Path]) -> None:
        """Read the sections of OBJECT_FILES."""
        self.by_file: dict[str, dict[str, DataObject]] = {}
        self.by_name: dict[str, list[DataObject]] = {}
        text = subprocess.run(
            [OBJDUMP, "-h", *map(str, object_files)],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        starts = list(OBJECT_FILE.finditer(text))
        for index, start in enumerate(starts):
            end = starts[index + 1].start() if index + 1 < len(starts) else len(text)
            sections = self.by_file.setdefault(start.group("file"), {})
            for match in DATA_SECTION.finditer(text, start.end(), end):
                found = DataObject(
                    int(match.group("size"), 16), 1 << int(match.group("power"))
                )
                sections[match.group("name")] = found
                self.by_name.setdefault(match.group("name"), []).append(found)

    def find(self, name: str, object_file: str) -> DataObject | None:
        """Return NAME's object, OBJECT_FILE's own if it defines one."""
        own = self.by_file.get(object_file, {}).get(name)
        if own:
            return own
        found = self.by_name.get(name, [])
        return min(found, key=lambda each: each.alignment) if found else None


@final
class Arguments(argparse.Namespace):
    """The command line's arguments."""

    build_dir: str = ""
    include: list[str] | None = None
    clang_query: str = DEFAULT_CLANG_QUERY


def run_query(arguments: Arguments, sources: list[str]) -> str:
    """Return clang-query's output for SOURCES."""
    command = [arguments.clang_query, "-p", arguments.build_dir]
    command += [f"--extra-arg=-I{directory}" for directory in arguments.include or []]
    for line in build_query():
        command += ["-c", line]
    result = subprocess.run(
        [*command, *sources], capture_output=True, text=True, check=False
    )
    return result.stdout + result.stderr


def object_file_of(build_dir: str, source: str) -> str:
    """Return the object file that BUILD_DIR's build compiles SOURCE to."""
    return str(Path(build_dir) / f"{source}{OBJECT_SUFFIX}")


def unaligned(found: DataObject | None, name: str, place: str) -> list[str]:
    """Return the mistake, if any, of the buffer NAME at PLACE."""
    if found is None:
        return [f"{place}: finds no section for {name}, so can't check it"]
    if found.alignment >= RCP_ALIGNMENT:
        return []
    asked = f"{found.alignment}-byte alignment"
    return [f"{place}: {name} reaches the RCP or a DMA but asks for {asked}"]


def bound_buffers(output: str, build_dir: str, objects: Objects) -> list[str]:
    """Return the mistakes among the buffers that clang-query's OUTPUT binds.

    A static object is used only in its own file, so each buffer is looked
    for first in the object compiled from the file where it is used.
    """
    mistakes: list[str] = []
    for place, name in dict.fromkeys(
        (match.group("place"), match.group("name")) for match in BOUND.finditer(output)
    ):
        source = place.split(":", 1)[0]
        found = objects.find(name, object_file_of(build_dir, source))
        mistakes += unaligned(found, name, place)
    return mistakes


def balanced(text: str, start: int, opening: str, closing: str) -> str:
    """Return TEXT from START to the OPENING character's matching CLOSING."""
    depth = 0
    for index in range(text.index(opening, start), len(text)):
        depth += {opening: 1, closing: -1}.get(text[index], 0)
        if depth == 0:
            return text[start : index + 1]
    return text[start:]


def nested_buffers(
    source: str, line: int, build_dir: str, objects: Objects
) -> list[str]:
    """Return the mistakes in the nested routine at LINE of SOURCE."""
    text = Path(source).read_text()
    start = sum(len(each) + 1 for each in text.split("\n")[: line - 1])
    body = balanced(text, start, "{", "}")
    own = objects.by_file.get(object_file_of(build_dir, source), {})
    mistakes: list[str] = []
    for use in NESTED_USE.finditer(body):
        arguments = balanced(body, use.start(), "(", ")")
        place = f"{source}:{line + body.count(chr(10), 0, use.start())}"
        for name in dict.fromkeys(
            match.group(0) for match in IDENTIFIER.finditer(arguments)
        ):
            found = own.get(name)
            if found and found.size > POINTER_SIZE:
                mistakes += unaligned(found, name, f"{place} (a nested routine)")
    return mistakes


def parse_errors(output: str, build_dir: str, objects: Objects) -> list[str]:
    """Return the mistakes that clang's errors in OUTPUT hide or are."""
    mistakes: list[str] = []
    for match in ERROR.finditer(output):
        message = match.group("message")
        if message == NESTED_ROUTINE_START:
            mistakes += nested_buffers(
                match.group("file"), int(match.group("line")), build_dir, objects
            )
        elif message not in NESTED_ROUTINE_ERRORS:
            mistakes.append(f"{match.group(0)}: clang can't read the file")
    return mistakes


def main() -> int:
    """Check the build that the command line names; return the exit status."""
    parser = argparse.ArgumentParser(description=SUMMARY)
    _ = parser.add_argument("build_dir", help="the modern build's directory")
    _ = parser.add_argument(
        "--include", action="append", help="another directory of headers"
    )
    _ = parser.add_argument("--clang-query", help="the clang-query program")
    arguments = parser.parse_args(namespace=Arguments())
    commands = cast(
        "list[dict[str, str]]",
        json.loads((Path(arguments.build_dir) / COMPILE_COMMANDS).read_text()),
    )
    sources = [command["file"] for command in commands]
    objects = Objects(sorted(Path(arguments.build_dir).rglob(f"*{OBJECT_SUFFIX}")))
    output = run_query(arguments, sources)
    if not PROCESSING.search(output):
        print(output, file=sys.stderr)
        return 1
    mistakes = bound_buffers(output, arguments.build_dir, objects) + parse_errors(
        output, arguments.build_dir, objects
    )
    for mistake in mistakes:
        print(mistake, file=sys.stderr)
    return 1 if mistakes else 0


if __name__ == "__main__":
    sys.exit(main())
