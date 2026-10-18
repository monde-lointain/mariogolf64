#!/usr/bin/env python3
"""Print known global data symbols that have inconsistent C types."""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parent.parent
SYMBOL_FILES = ("symbol_addrs.txt", "ghidra_symbols.txt")
VENDORED_SRC_DIRS = ("libkmc", "libmus", "libnaudio", "libnusys", "libultra")
PROJECT_EXCEPTIONS = (Path("include/common.h"),)
SYMBOL_LINE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
STORAGE_CLASS = re.compile(r"\b(?:extern|static|register|auto)\b")
ARRAY_BOUND = re.compile(r"\[[^\]]*\]")


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return the root-relative POSIX path, or the absolute one outside root."""
    resolved = path.resolve()
    try:
        return resolved.relative_to(root.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def is_vendored(path: Path, root: Path = ROOT) -> bool:
    """Return True for vendored code, which project scans exclude."""
    rel = path.relative_to(root)
    if rel in PROJECT_EXCEPTIONS:
        return False
    if rel.parts[0] == "include":
        return True
    return (
        rel.parts[0] == "src"
        and len(rel.parts) > 2
        and rel.parts[1] in VENDORED_SRC_DIRS
    )


def project_files(suffix: str, root: Path = ROOT) -> list[Path]:
    """Return sorted project-owned files with a suffix under src/ and include/."""
    files = []
    for base in (root / "src", root / "include"):
        for path in base.rglob(f"*{suffix}"):
            if path.is_file() and path.name != "ctx.c" and not is_vendored(path, root):
                files.append(path)
    return sorted(files)


def data_symbols(path: Path) -> set[str]:
    """Read non-function symbol names from a symbol-address file."""
    symbols = set()
    for line in path.read_text().splitlines():
        match = SYMBOL_LINE.match(line)
        if match is not None and "type:func" not in line:
            symbols.add(match.group(1))
    return symbols


def known_data_symbols(root: Path) -> set[str]:
    """Union the data symbols from every root symbol file that exists."""
    symbols: set[str] = set()
    for name in SYMBOL_FILES:
        path = root / name
        if not path.is_file():
            print(f"note: {repo_rel(path, root)} not found; skipping", file=sys.stderr)
            continue
        symbols |= data_symbols(path)
    return symbols


def mask_non_code(text: str) -> str:
    """Blank comments, literals, and directives while preserving newlines."""
    result = list(text)
    index = 0
    state = "code"
    line_start = True

    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""

        if state == "code":
            if line_start and char in " \t":
                index += 1
                continue
            if line_start and char == "#":
                state = "directive"
            elif char == "/" and next_char == "/":
                result[index] = result[index + 1] = " "
                index += 2
                state = "line_comment"
                continue
            elif char == "/" and next_char == "*":
                result[index] = result[index + 1] = " "
                index += 2
                state = "block_comment"
                continue
            elif char in "\"'":
                result[index] = " "
                index += 1
                state = "string" if char == '"' else "character"
                continue

        if state in {"line_comment", "directive"}:
            if char == "\n":
                state = "code"
                line_start = True
                index += 1
                continue
            result[index] = " "
        elif state == "block_comment":
            if char == "*" and next_char == "/":
                result[index] = result[index + 1] = " "
                index += 2
                state = "code"
                continue
            if char != "\n":
                result[index] = " "
        elif state in {"string", "character"}:
            if char == "\\" and next_char:
                result[index] = " "
                if next_char != "\n":
                    result[index + 1] = " "
                index += 2
                continue
            terminator = '"' if state == "string" else "'"
            if char == terminator:
                result[index] = " "
                state = "code"
            elif char != "\n":
                result[index] = " "

        if char == "\n":
            line_start = True
        elif char not in " \t":
            line_start = False
        index += 1

    return "".join(result)


def global_statements(text: str) -> Iterable[str]:
    """Yield semicolon-terminated statements outside function bodies."""
    statement: list[str] = []
    brace_depth = 0
    paren_depth = 0
    function_depth: int | None = None

    for char in mask_non_code(text):
        if function_depth is not None:
            if char == "{":
                brace_depth += 1
            elif char == "}":
                brace_depth -= 1
                if brace_depth < function_depth:
                    function_depth = None
                    statement.clear()
            continue

        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "{" and brace_depth == 0:
            prefix = "".join(statement)
            is_type_body = re.search(r"\b(?:struct|union|enum)\b", prefix)
            if not is_type_body and "=" not in prefix and paren_depth == 0:
                brace_depth = 1
                function_depth = 1
                statement.clear()
                continue
            brace_depth += 1
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth = max(0, brace_depth - 1)

        statement.append(char)
        if char == ";" and brace_depth == 0 and paren_depth == 0:
            yield "".join(statement)
            statement.clear()


def before_initializer(statement: str) -> str:
    """Remove a top-level initializer and the terminating semicolon."""
    paren_depth = bracket_depth = brace_depth = 0
    for index, char in enumerate(statement):
        if char == "(":
            paren_depth += 1
        elif char == ")":
            paren_depth = max(0, paren_depth - 1)
        elif char == "[":
            bracket_depth += 1
        elif char == "]":
            bracket_depth = max(0, bracket_depth - 1)
        elif char == "{":
            brace_depth += 1
        elif char == "}":
            brace_depth = max(0, brace_depth - 1)
        elif char == "=" and not (paren_depth or bracket_depth or brace_depth):
            return statement[:index]
    return statement.rstrip().removesuffix(";")


def canonical_type(declaration: str, symbol: str) -> str | None:
    """Return a whitespace-independent type spelling for one declarator."""
    matches = list(re.finditer(rf"\b{re.escape(symbol)}\b", declaration))
    if len(matches) != 1:
        return None

    match = matches[0]
    prefix = declaration[: match.start()]
    suffix = declaration[match.end() :]
    if suffix.lstrip().startswith("(") and not re.search(r"\(\s*\*\s*$", prefix):
        return None
    if re.search(r"\btypedef\b", prefix):
        return None

    prefix = STORAGE_CLASS.sub(" ", prefix)
    declarator = prefix + "@GLOBAL@" + suffix
    declarator = ARRAY_BOUND.sub("[]", declarator)
    declarator = re.sub(r"\s+", "", declarator)
    return declarator.replace("@GLOBAL@", "")


def declarations_in_file(path: Path, symbols: set[str]) -> dict[str, set[str]]:
    """Collect observed canonical types of known symbols in one file."""
    declarations: dict[str, set[str]] = defaultdict(set)
    for statement in global_statements(path.read_text(errors="replace")):
        declaration = before_initializer(statement)
        for symbol in set(IDENTIFIER.findall(declaration)) & symbols:
            symbol_type = canonical_type(declaration, symbol)
            if symbol_type is not None:
                declarations[symbol].add(symbol_type)
    return declarations


def inconsistent_symbols(root: Path) -> list[str]:
    """Return known data symbols with more than one observed C type."""
    symbols = known_data_symbols(root)
    observed: dict[str, set[str]] = defaultdict(set)
    paths = [*project_files(".c", root), *project_files(".h", root)]
    for path in paths:
        for symbol, types in declarations_in_file(path, symbols).items():
            observed[symbol].update(types)
    return sorted(symbol for symbol, types in observed.items() if len(types) > 1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=ROOT,
        help="repository root (defaults to the parent of tools/)",
    )
    args = parser.parse_args()
    for symbol in inconsistent_symbols(args.root.resolve()):
        print(symbol)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
