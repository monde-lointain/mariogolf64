#!/usr/bin/env python3
"""List project files that use union-typed fields in structs."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PATHS = (ROOT / "src", ROOT / "include")
VENDOR_DIRS = (
    Path("src/libkmc"),
    Path("src/libmus"),
    Path("src/libnaudio"),
    Path("src/libnusys"),
    Path("src/libultra"),
)
PROJECT_EXCEPTIONS = (Path("include/common.h"),)
TOKEN_RE = re.compile(r"[A-Za-z_]\w*|[{};]")


def strip_comments_and_literals(source: str) -> str:
    """Blank comments and literals while preserving newlines and offsets."""
    result = list(source)
    i = 0
    state = "code"

    while i < len(source):
        char = source[i]
        following = source[i + 1] if i + 1 < len(source) else ""

        if state == "code":
            if char == "/" and following == "/":
                result[i] = result[i + 1] = " "
                i += 2
                state = "line_comment"
                continue
            if char == "/" and following == "*":
                result[i] = result[i + 1] = " "
                i += 2
                state = "block_comment"
                continue
            if char == '"':
                result[i] = " "
                state = "string"
            elif char == "'":
                result[i] = " "
                state = "character"
        elif state == "line_comment":
            if char == "\n":
                state = "code"
            else:
                result[i] = " "
        elif state == "block_comment":
            if char == "*" and following == "/":
                result[i] = result[i + 1] = " "
                i += 2
                state = "code"
                continue
            if char != "\n":
                result[i] = " "
        elif state in ("string", "character"):
            delimiter = '"' if state == "string" else "'"
            if char == "\\" and following:
                result[i] = " "
                if following != "\n":
                    result[i + 1] = " "
                i += 2
                continue
            if char == delimiter:
                result[i] = " "
                state = "code"
            elif char != "\n":
                result[i] = " "

        i += 1

    return "".join(result)


def find_struct_union_lines(source: str) -> list[int]:
    """Return lines containing inline or typedef'd union fields in structs."""
    cleaned = strip_comments_and_literals(source)
    tokens = list(TOKEN_RE.finditer(cleaned))
    aggregate_braces: dict[int, str] = {}
    aggregate_keywords: dict[int, int] = {}

    for index, token in enumerate(tokens):
        if token.group() not in ("struct", "union"):
            continue

        next_index = index + 1
        if next_index < len(tokens) and tokens[next_index].group() not in ("{", ";"):
            next_index += 1  # Skip an optional aggregate tag.
        if next_index < len(tokens) and tokens[next_index].group() == "{":
            aggregate_braces[next_index] = token.group()
            aggregate_keywords[next_index] = index

    opening_braces: list[int] = []
    closing_braces: dict[int, int] = {}
    for index, token in enumerate(tokens):
        if token.group() == "{":
            opening_braces.append(index)
        elif token.group() == "}" and opening_braces:
            closing_braces[opening_braces.pop()] = index

    union_typedefs: set[str] = set()
    for opening_index, aggregate in aggregate_braces.items():
        keyword_index = aggregate_keywords[opening_index]
        if (
            aggregate == "union"
            and keyword_index > 0
            and tokens[keyword_index - 1].group() == "typedef"
            and opening_index in closing_braces
        ):
            alias_index = closing_braces[opening_index] + 1
            if (
                alias_index < len(tokens)
                and tokens[alias_index].group() not in ("{", "}", ";")
            ):
                union_typedefs.add(tokens[alias_index].group())

    brace_stack: list[str | None] = []
    lines: list[int] = []
    for index, token in enumerate(tokens):
        value = token.group()
        if value == "{":
            aggregate = aggregate_braces.get(index)
            brace_stack.append(aggregate)
        elif value == "}" and brace_stack:
            brace_stack.pop()
        elif "struct" in brace_stack and (
            value == "union" or value in union_typedefs
        ):
            lines.append(cleaned.count("\n", 0, token.start()) + 1)

    return lines


def _root_relative(path: Path, root: Path = ROOT) -> Path | None:
    """Return the resolved path relative to root, or None when outside root."""
    try:
        return (root / path).resolve().relative_to(root.resolve())
    except ValueError:
        return None


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return a POSIX path relative to root, or an absolute one if outside root."""
    relative = _root_relative(path, root)
    if relative is None:
        return (root / path).resolve().as_posix()
    return relative.as_posix()


def is_vendored(path: Path, root: Path = ROOT) -> bool:
    """Return True for vendored code, which is skipped unless --include-vendor."""
    relative = _root_relative(path, root)
    if relative is None or relative in PROJECT_EXCEPTIONS:
        return False
    if relative.parts and relative.parts[0] == "include":
        return True
    return any(
        relative == vendor or vendor in relative.parents for vendor in VENDOR_DIRS
    )


def is_excluded(path: Path, include_vendor: bool) -> bool:
    """Return True for ctx.c always, and for vendored files unless include_vendor."""
    if path.name == "ctx.c":
        return True
    return not include_vendor and is_vendored(path)


def source_files(paths: list[Path], include_vendor: bool) -> list[Path]:
    files: set[Path] = set()
    for path in paths:
        if path.is_file():
            candidates = [path]
        elif path.is_dir():
            candidates = path.rglob("*")
        else:
            raise FileNotFoundError(path)

        for candidate in candidates:
            if (
                candidate.is_file()
                and candidate.suffix in (".c", ".h")
                and not is_excluded(candidate, include_vendor)
            ):
                files.add(candidate)
    return sorted(files, key=str)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="List C files containing inline or typedef'd union fields in structs."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        type=Path,
        help="files or directories to scan (default: src and include)",
    )
    parser.add_argument(
        "--include-vendor",
        action="store_true",
        help="include vendored code (include/ except common.h, and src/lib*)",
    )
    parser.add_argument(
        "--json", action="store_true", help="emit a JSON array for task-runner"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="show union counts and line numbers"
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    paths = [path.resolve() for path in args.paths] or list(DEFAULT_PATHS)

    try:
        files = source_files(paths, args.include_vendor)
    except FileNotFoundError as error:
        print(f"error: path does not exist: {error}", file=sys.stderr)
        return 2

    matches: list[tuple[Path, list[int]]] = []
    for path in files:
        lines = find_struct_union_lines(path.read_text(encoding="utf-8", errors="replace"))
        if lines:
            matches.append((path, lines))

    if args.json:
        print(json.dumps([repo_rel(path) for path, _ in matches]))
    elif args.verbose:
        for path, lines in matches:
            locations = ", ".join(str(line) for line in lines)
            print(f"{repo_rel(path)}: {len(lines)} (lines {locations})")
    else:
        for path, _ in matches:
            print(repo_rel(path))

    return 0


if __name__ == "__main__":
    sys.exit(main())
