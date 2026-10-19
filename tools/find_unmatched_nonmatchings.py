#!/usr/bin/env python3
"""
List nonmatchings/ workspaces that do not have a certified full local match.

Scans nonmatchings/*/match_log.txt, takes the best certified C attempt per
workspace, and reports workspaces whose best attempt is below 100% (or that
have no certified attempt). A workspace is skipped once its function is defined
in src/, and also when the function no longer exists as unmatched assembly.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import get_project_root  # noqa: E402
from nonmatching_match_log import (  # noqa: E402
    FULL_MATCH_PERCENT,
    base_workspace_name,
    canonical_function_name,
    canonicalize_function_names,
    certified_attempt_path,
    format_path,
    function_name_aliases,
    integrated_function_names,
    parse_match_log_line,
    unmatched_function_names,
)


@dataclass(frozen=True)
class Attempt:
    path: Path
    percent: float


@dataclass(frozen=True)
class WorkspaceStatus:
    workspace: Path
    best_attempt: Attempt | None
    logged_attempt_count: int
    c_file_count: int


def parse_match_log(log_path: Path, repo_root: Path) -> list[Attempt]:
    attempts: list[Attempt] = []
    workspace = log_path.parent
    nonmatchings_dir = (repo_root / "nonmatchings").resolve()

    try:
        lines = log_path.read_text().splitlines()
    except OSError:
        return attempts

    for line in lines:
        entry = parse_match_log_line(line)
        if entry is None:
            continue

        attempt_path = certified_attempt_path(entry, workspace)
        if attempt_path is None:
            continue

        # Ignore stale or accidental paths outside nonmatchings.
        if not attempt_path.exists() or not attempt_path.is_file():
            continue
        if nonmatchings_dir not in attempt_path.parents:
            continue

        attempts.append(Attempt(attempt_path, entry.percent))

    return attempts


def best_attempt(attempts: Iterable[Attempt]) -> Attempt | None:
    best: Attempt | None = None
    for attempt in attempts:
        if best is None or attempt.percent > best.percent:
            best = attempt
    return best


def workspace_statuses(repo_root: Path) -> list[WorkspaceStatus]:
    nonmatchings_dir = repo_root / "nonmatchings"
    aliases = function_name_aliases(repo_root)
    integrated = canonicalize_function_names(integrated_function_names(repo_root), aliases)
    unmatched = canonicalize_function_names(unmatched_function_names(repo_root), aliases)
    statuses: list[WorkspaceStatus] = []

    for workspace in sorted(path for path in nonmatchings_dir.iterdir() if path.is_dir()):
        function = canonical_function_name(base_workspace_name(workspace), aliases)
        if function in integrated or function not in unmatched:
            continue

        c_files = sorted(workspace.glob("*.c"))
        log_path = workspace / "match_log.txt"
        attempts = parse_match_log(log_path, repo_root) if log_path.exists() else []
        best = best_attempt(attempts)

        if best is None or best.percent < FULL_MATCH_PERCENT:
            statuses.append(
                WorkspaceStatus(
                    workspace=workspace,
                    best_attempt=best,
                    logged_attempt_count=len(attempts),
                    c_file_count=len(c_files),
                )
            )

    statuses.sort(
        key=lambda status: (
            -(status.best_attempt.percent if status.best_attempt is not None else -1.0),
            str(status.workspace),
        )
    )
    return statuses


def print_statuses(statuses: list[WorkspaceStatus], repo_root: Path) -> None:
    print(f"{'Workspace':<56} {'Best':>8}  {'Best file':<16} {'Logged':>6} {'C files':>7}")
    print("-" * 100)

    for status in statuses:
        if status.best_attempt is None:
            best_percent = "none"
            best_file = ""
        else:
            best_percent = f"{status.best_attempt.percent:7.3f}%"
            best_file = format_path(status.best_attempt.path, status.workspace)

        print(
            f"{format_path(status.workspace, repo_root):<56} "
            f"{best_percent:>8}  "
            f"{best_file:<16} "
            f"{status.logged_attempt_count:>6} "
            f"{status.c_file_count:>7}"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Find nonmatchings/ workspaces whose best certified logged C attempt is not 100%."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=get_project_root(),
        help="Repository root to scan. Defaults to the parent of this script's tools directory.",
    )
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    nonmatchings_dir = repo_root / "nonmatchings"
    if not nonmatchings_dir.is_dir():
        print(f"error: {nonmatchings_dir} does not exist", file=sys.stderr)
        return 2

    statuses = workspace_statuses(repo_root)
    if not statuses:
        print("All unintegrated nonmatchings workspaces are fully matched.")
        return 0

    print_statuses(statuses, repo_root)
    print(f"\nTotal not fully matched: {len(statuses)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
