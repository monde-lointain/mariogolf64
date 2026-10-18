#!/usr/bin/env python3
"""List project-owned C files and the same-named project headers they include."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT_EXCEPTIONS = (Path("include/common.h"),)
VENDORED_ROOTS = (
    Path("include"),
    Path("src/libkmc"),
    Path("src/libmus"),
    Path("src/libnaudio"),
    Path("src/libnusys"),
    Path("src/libultra"),
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+\.h)"', re.MULTILINE)


def _absolute(path: Path, root: Path) -> Path:
    return path if path.is_absolute() else root / path


def repo_rel(path: Path, root: Path = ROOT) -> str:
    """Return the repo-relative POSIX path, or the absolute one if outside root."""
    absolute = _absolute(path, root)
    try:
        return absolute.relative_to(root).as_posix()
    except ValueError:
        return absolute.as_posix()


def is_vendored(path: Path, root: Path = ROOT) -> bool:
    """Return True for third-party code under include/ or the vendored src/ libraries."""
    try:
        rel = _absolute(path, root).relative_to(root)
    except ValueError:
        return False
    if rel in PROJECT_EXCEPTIONS:
        return False
    return any(rel == base or base in rel.parents for base in VENDORED_ROOTS)


def project_files(suffix: str, root: Path = ROOT) -> list[Path]:
    """Return sorted project-owned files with the suffix under src/ and include/."""
    files = [
        path
        for base in (root / "src", root / "include")
        if base.is_dir()
        for path in base.rglob(f"*{suffix}")
        if path.is_file()
        and path.name != "ctx.c"
        and not is_vendored(path, root)
    ]
    return sorted(files, key=str)


def resolve_include(
    source_path: Path, include_path: Path, root: Path = ROOT
) -> Path | None:
    """Resolve a quoted include: the including file's directory first, then include/."""
    source_dir = _absolute(source_path, root).parent
    for base in (source_dir, root / "include"):
        candidate = base / include_path
        if candidate.is_file():
            return candidate.resolve()
    return None


def find_pairs() -> list[str]:
    """Return "src/x.c src/x.h" strings for each C file and its same-stem project header."""
    owned_headers = {path.resolve() for path in project_files(".h")}
    pairs: list[str] = []

    for source_path in project_files(".c"):
        source = source_path.read_text(encoding="utf-8", errors="replace")
        matching_headers: set[str] = set()
        for include_name in INCLUDE_RE.findall(source):
            include_path = Path(include_name)
            if include_path.stem != source_path.stem:
                continue
            header_path = resolve_include(source_path, include_path)
            if header_path is not None and header_path in owned_headers:
                matching_headers.add(repo_rel(header_path))

        source_rel = repo_rel(source_path)
        pairs.extend(
            f"{source_rel} {header_rel}" for header_rel in sorted(matching_headers)
        )

    return pairs


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List source/header pairs suitable for task-runner."
    )
    parser.add_argument(
        "--json", action="store_true", help="emit a JSON array for task-runner"
    )
    args = parser.parse_args()

    pairs = find_pairs()
    if args.json:
        print(json.dumps(pairs))
    else:
        for pair in pairs:
            print(pair)


if __name__ == "__main__":
    main()
