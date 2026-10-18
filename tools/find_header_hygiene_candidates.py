#!/usr/bin/env python3
"""Find project headers/source pairs that violate header ownership rules."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
OWNED_TOP_DIRS = ("src", "include")
VENDORED_SRC_DIRS = ("libkmc", "libmus", "libnaudio", "libnusys", "libultra")
PROJECT_EXCEPTIONS = (Path("include/common.h"),)
GENERATED_CONTEXT_NAME = "ctx.c"
NO_OWNING_HEADER = "NO_OWNING_HEADER"
# GNU keywords that EXTERN_FUNCTION_RE can capture as if they were function names.
NON_FUNCTION_NAMES = frozenset(
    {"__asm__", "__asm", "asm", "__attribute__", "__attribute"}
)

FORWARD_TYPE_RE = re.compile(
    r"\btypedef\s+(struct|union)\s+([A-Za-z_]\w*)\s+\2\s*;"
)
EXTERN_FUNCTION_RE = re.compile(
    r"^[ \t]*extern[ \t]+[^;={}]*?\b([A-Za-z_]\w*)[ \t\r\n]*"
    r"\([^;{}]*\)[ \t\r\n]*;",
    re.MULTILINE,
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+\.h)"', re.MULTILINE)


def _root_relative(path: Path, root: Path = ROOT) -> Path | None:
    """Return path relative to root, or None if it lies outside root."""
    absolute = path if path.is_absolute() else root / path
    try:
        return absolute.relative_to(root)
    except ValueError:
        return None


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return the relative POSIX path, or the absolute POSIX path outside root."""
    rel = _root_relative(path, root)
    return (rel if rel is not None else path).as_posix()


def is_vendored(path: Path, root: Path = ROOT) -> bool:
    """True for third-party code: all of include/ and the vendored src/lib* trees."""
    rel = _root_relative(path, root)
    if rel is None or not rel.parts or rel in PROJECT_EXCEPTIONS:
        return False
    if rel.parts[0] == "include":
        return True
    return (
        rel.parts[0] == "src"
        and len(rel.parts) > 1
        and rel.parts[1] in VENDORED_SRC_DIRS
    )


def is_project_owned(path: Path, root: Path = ROOT) -> bool:
    """True for files under src/ or include/ that are neither vendored nor generated."""
    rel = _root_relative(path, root)
    if rel is None or not rel.parts or rel.name == GENERATED_CONTEXT_NAME:
        return False
    return rel.parts[0] in OWNED_TOP_DIRS and not is_vendored(path, root)


def project_files(suffix: str, root: Path = ROOT) -> list[Path]:
    """Sorted project-owned files with the given suffix under src/ and include/."""
    files = [
        path
        for top in OWNED_TOP_DIRS
        if (root / top).is_dir()
        for path in (root / top).rglob(f"*{suffix}")
        if path.is_file() and is_project_owned(path, root)
    ]
    return sorted(files, key=str)


def resolve_include(
    source_path: Path, include_path: str, root: Path = ROOT
) -> Path | None:
    """Resolve a quoted include: the including file's directory first, then include/."""
    for base in (source_path.parent, root / "include"):
        candidate = base / include_path
        if candidate.is_file():
            return candidate.resolve()
    return None


def has_complete_definition(text: str, kind: str, name: str) -> bool:
    definition = re.compile(
        rf"\b(?:typedef\s+)?{kind}\s+{re.escape(name)}\s*\{{"
    )
    return definition.search(text) is not None


def extern_function_names(text: str) -> list[str]:
    """Names of extern function-style declarations, excluding GNU keywords."""
    return [
        name
        for name in EXTERN_FUNCTION_RE.findall(text)
        if name not in NON_FUNCTION_NAMES
    ]


def matching_headers(source_path: Path, source: str) -> list[Path]:
    headers: set[Path] = set()
    likely_header = source_path.with_suffix(".h")
    if likely_header.is_file() and is_project_owned(likely_header):
        headers.add(likely_header)
    for include_path in INCLUDE_RE.findall(source):
        if Path(include_path).stem != source_path.stem:
            continue
        header_path = resolve_include(source_path, include_path)
        if header_path is not None and is_project_owned(header_path):
            headers.add(header_path)
    return sorted(headers, key=str)


def find_candidates(stable: bool = False) -> list:
    issues: dict[tuple[str, str], set[str]] = defaultdict(set)

    for header_path in project_files(".h"):
        text = header_path.read_text(encoding="utf-8", errors="replace")
        header = repo_rel(header_path)
        for kind, name in FORWARD_TYPE_RE.findall(text):
            if not has_complete_definition(text, kind, name):
                issues[(header, "")].add(f"INCOMPLETE_TYPE:{name}")

        for name in extern_function_names(text):
            issues[(header, "")].add(f"EXTERN_FUNCTION_IN_HEADER:{name}")

    for source_path in project_files(".c"):
        text = source_path.read_text(encoding="utf-8", errors="replace")
        function_names = extern_function_names(text)
        if not function_names:
            continue

        source = repo_rel(source_path)
        problems = {f"SOURCE_EXTERN_FUNCTION:{name}" for name in function_names}
        headers = matching_headers(source_path, text)
        if headers:
            for header_path in headers:
                issues[(repo_rel(header_path), source)].update(problems)
        else:
            issues[(NO_OWNING_HEADER, source)].update(problems)

    # Merge header-only findings into a source/header candidate when possible.
    merged: dict[tuple[str, str], set[str]] = defaultdict(set)
    for (header, source), problem_set in issues.items():
        if source or header == NO_OWNING_HEADER:
            merged[(header, source)].update(problem_set)
            continue

        likely_source = (ROOT / header).with_suffix(".c")
        sibling = (
            repo_rel(likely_source)
            if likely_source.is_file() and is_project_owned(likely_source)
            else ""
        )
        merged[(header, sibling)].update(problem_set)

    candidates = []
    for (header, source), problem_set in sorted(merged.items()):
        if stable:
            # task-runner keys candidates by the whole JSON value, so identity
            # stays path-based: fixing one of several problems cannot make a
            # still-problematic candidate look done.
            candidates.append([header, source])
            continue
        paths = " ".join(path for path in (header, source) if path)
        candidates.append(f"{paths} [{' '.join(sorted(problem_set))}]")
    return candidates


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List header-hygiene candidates for task-runner"
    )
    parser.add_argument(
        "--json", action="store_true", help="emit a JSON array for task-runner"
    )
    args = parser.parse_args()

    if args.json:
        print(json.dumps(find_candidates(stable=True)))
    else:
        print("\n".join(find_candidates()))


if __name__ == "__main__":
    main()
