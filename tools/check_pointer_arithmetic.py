#!/usr/bin/env python3
"""
Detect pointer arithmetic on casted pointers in project C code.

Flagged patterns:
    (u8*)ptr + offset
    (SomeStruct*)ptr + index
    *(s32*)((char*)ptr + 0x10)

These should be replaced with proper struct field access; see
docs/project/code-quality.md, "Typed access instead of manual offsets".

Each file is preprocessed with the include paths and defines its Makefile
profile uses (`make print-cflags`), so the parse matches the build.
Only project-owned files are checked unless --include-vendored is given.
Check-only: nothing is modified.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from datetime import datetime
from pathlib import Path

from pycparser import c_ast, parse_file
from pycparser.c_parser import ParseError

from find_header_hygiene_candidates import ROOT, is_project_owned, repo_rel
from m2ctx import profile_flags


# pycparser cannot parse GNU extensions; these defines neutralise them for parsing only.
PYCPARSER_COMPAT_ARGS = [
    "-D__attribute__(x)=",
    "-D__extension__=",
    "-D__asm__(x)=",
    "-D__restrict=",
    "-D__volatile__=volatile",
    "-D__inline=",
    "-D__const=const",
]


class PointerArithmeticDetector(c_ast.NodeVisitor):
    """Collect `(type*)expr +/- offset` BinaryOps, attributed to the file they come from."""

    def __init__(self):
        self.violations = []

    def visit_BinaryOp(self, node):
        if node.op in ['+', '-'] and isinstance(node.left, c_ast.Cast):
            if self._is_pointer_cast(node.left):
                self.violations.append({
                    'file': node.coord.file if node.coord else None,
                    'line': node.coord.line if node.coord else 0,
                    'column': node.coord.column if node.coord else 0,
                    'type': self._get_cast_type(node.left),
                })
        self.generic_visit(node)

    def _is_pointer_cast(self, cast_node):
        """Check if a Cast node casts to a pointer type."""
        to_type = cast_node.to_type
        while isinstance(to_type, c_ast.Typename):
            to_type = to_type.type
        return isinstance(to_type, c_ast.PtrDecl)

    def _get_cast_type(self, cast_node):
        """Get a string representation of the cast type."""
        to_type = cast_node.to_type
        while isinstance(to_type, c_ast.Typename):
            to_type = to_type.type
        if isinstance(to_type, c_ast.PtrDecl):
            return self._typename_to_string(to_type)
        return "unknown type"

    def _typename_to_string(self, type_node):
        """Convert a type node to a string like 'u8*' or 'struct Foo*'."""
        type_names = self._get_type_names(type_node)
        if self._has_pointer(type_node):
            return ' '.join(type_names) + '*'
        return ' '.join(type_names)

    def _get_type_names(self, node):
        """Extract type names from a type node."""
        if isinstance(node, c_ast.IdentifierType):
            return node.names
        if isinstance(node, c_ast.TypeDecl):
            return self._get_type_names(node.type)
        if isinstance(node, c_ast.PtrDecl):
            return self._get_type_names(node.type)
        if isinstance(node, c_ast.Struct):
            return [f"struct {node.name}"] if node.name else ["struct"]
        return ["unknown"]

    def _has_pointer(self, node):
        """Check if a type node contains a pointer."""
        if isinstance(node, c_ast.PtrDecl):
            return True
        if isinstance(node, c_ast.TypeDecl):
            return self._has_pointer(node.type)
        return False


def cpp_args_for(source: Path) -> list[str]:
    """Preprocessor arguments matching the file's Makefile profile, with include dirs made absolute."""
    args = ["-E", *PYCPARSER_COMPAT_ARGS]
    flags = profile_flags(source.relative_to(ROOT).as_posix())
    index = 0
    while index < len(flags):
        flag = flags[index]
        if flag == "-I":
            args.append(f"-I{ROOT / flags[index + 1]}")
            index += 2
            continue
        if flag.startswith("-I"):
            args.append(f"-I{ROOT / flag[2:]}")
        else:
            args.append(flag)
        index += 1
    return args


def parse_source(source: Path) -> c_ast.FileAST:
    """Preprocess and parse one C file. Raises pycparser/cpp errors on failure."""
    return parse_file(str(source), use_cpp=True, cpp_path="gcc", cpp_args=cpp_args_for(source))


def check_file(source: Path, verbose: bool = False) -> list[dict] | None:
    """Return the file's violations, or None if it could not be parsed."""
    if verbose:
        print(f"Checking {repo_rel(source, ROOT)}...", file=sys.stderr)
    try:
        ast = parse_source(source)
    except (ParseError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"Warning: Could not parse {repo_rel(source, ROOT)}: {error}", file=sys.stderr)
        return None
    detector = PointerArithmeticDetector()
    detector.visit(ast)
    return detector.violations


def get_git_blame_timestamp(filename: str, line_number: int) -> datetime | None:
    """Get the author timestamp of a line via git blame (filename relative to ROOT)."""
    try:
        result = subprocess.run(
            ['git', 'blame', '-L', f'{line_number},{line_number}', '--porcelain', filename],
            capture_output=True,
            text=True,
            timeout=10,
            cwd=ROOT,
        )
        if result.returncode != 0:
            return None
        for line in result.stdout.split('\n'):
            if line.startswith('author-time '):
                return datetime.fromtimestamp(int(line.split(' ', 1)[1]))
    except (subprocess.TimeoutExpired, OSError, ValueError):
        pass
    return None


def format_violation(filename: str, violation: dict) -> str:
    """Format a violation as file:line:col:[date] message."""
    timestamp = violation.get('timestamp')
    timestamp_str = f" [{timestamp.strftime('%Y-%m-%d')}]" if timestamp else ""
    return (
        f"{filename}:{violation['line']}:{violation['column']}:{timestamp_str} "
        f"pointer arithmetic with cast: ({violation['type']})ptr + offset"
    )


def collect_sources(paths: list[Path], include_vendored: bool) -> list[Path]:
    """Sorted, de-duplicated .c files under paths; project-owned only unless include_vendored."""
    sources = set()
    for path in paths:
        candidates = [path] if path.is_file() else sorted(path.rglob("*.c"))
        for candidate in candidates:
            if candidate.suffix != ".c":
                continue
            resolved = candidate.resolve()
            if include_vendored or is_project_owned(resolved, ROOT):
                sources.add(resolved)
    return sorted(sources, key=str)


def check_files(sources: list[Path], include_vendored: bool, verbose: bool):
    """Return (violations keyed by file, unparsed sources). Duplicate sites from re-included headers are merged."""
    by_file: dict[str, list[dict]] = {}
    seen = set()
    unparsed = []
    for source in sources:
        violations = check_file(source, verbose)
        if violations is None:
            unparsed.append(source)
            continue
        for violation in violations:
            file = violation['file']
            if not file:
                continue
            file_path = Path(file).resolve()
            if not include_vendored and not is_project_owned(file_path, ROOT):
                continue
            key = (str(file_path), violation['line'], violation['column'])
            if key in seen:
                continue
            seen.add(key)
            by_file.setdefault(repo_rel(file_path, ROOT), []).append(violation)
    return by_file, unparsed


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Detect pointer arithmetic on casted pointers (check-only).",
        epilog="Exit status: 0 normally; 1 with --strict if violations were found or a file "
        "could not be parsed; 2 on a bad path.",
    )
    parser.add_argument("paths", nargs="*", type=Path, default=[Path("src")],
                        help="C files or directories to check (default: src)")
    parser.add_argument("--include-vendored", action="store_true",
                        help="also check vendored SDK sources (src/lib*, include/)")
    parser.add_argument("--strict", action="store_true",
                        help="exit 1 if violations were found or any file could not be parsed")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="print each file as it is checked")
    args = parser.parse_args(argv)

    paths = [path if path.is_absolute() else ROOT / path for path in args.paths]
    for path in paths:
        if not path.exists():
            print(f"error: path does not exist: {path}", file=sys.stderr)
            return 2
        if path.is_file() and (path.suffix != ".c" or not (args.include_vendored or is_project_owned(path.resolve(), ROOT))):
            print(f"error: not a project-owned C file: {path}", file=sys.stderr)
            return 2

    sources = collect_sources(paths, args.include_vendored)
    violations_by_file, unparsed = check_files(sources, args.include_vendored, args.verbose)

    if unparsed:
        print(f"{len(unparsed)} file(s) could not be parsed and were not checked.", file=sys.stderr)

    total = sum(len(v) for v in violations_by_file.values())
    header = "\n" + "=" * 70 + "\nPointer Arithmetic Check\n" + "=" * 70
    if total == 0:
        print(header)
        print("No violations found.")
    else:
        if args.verbose:
            print("Fetching git blame timestamps...", file=sys.stderr)
        entries = []
        for filename, violations in violations_by_file.items():
            for violation in violations:
                violation['timestamp'] = get_git_blame_timestamp(filename, violation['line'])
                entries.append((filename, violation))
        entries.sort(key=lambda e: (0, e[1]['timestamp'], e[0], e[1]['line'])
                     if e[1]['timestamp'] else (1, datetime.max, e[0], e[1]['line']))
        print(header)
        print(f"Found {total} violation(s) in {len(violations_by_file)} file(s):")
        print("(sorted by when introduced, oldest first)\n")
        for filename, violation in entries:
            print(format_violation(filename, violation))
        print()
        print("These patterns should be replaced with proper struct field access.")
        print('See docs/project/code-quality.md, "Typed access instead of manual offsets", for examples of correct struct usage.')

    if args.strict and (total or unparsed):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
