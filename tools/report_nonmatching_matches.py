#!/usr/bin/env python3
"""Report logged match progress from nonmatchings/ workspaces.

Scans nonmatchings/*/match_log.txt and tools/scratches.json and reports:

- functions with a certified 100% match that are not yet integrated into src/
- decomp.me scratches with a 100% score that are not yet integrated into src/
- partially matched functions, ordered by best logged match percentage
- decomp.me scratch scores, when available, as supplemental context

A function is integrated once it has a C definition under src/. Log entries for
functions that are neither integrated nor still unmatched assembly are ignored.
Extra checkouts or worktrees can be scanned with --scan-root; each one is judged
against the primary checkout's src/ as well as its own.
"""

from __future__ import annotations

import argparse
import json
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
class MatchResult:
    function: str
    percent: float
    attempt: Path
    workspace: Path


@dataclass(frozen=True)
class ScratchResult:
    function: str
    percent: float
    score: int | float
    max_score: int | float | None
    slug: str
    match_override: bool


@dataclass(frozen=True)
class ReportRow:
    local: MatchResult
    scratch: ScratchResult | None = None


@dataclass(frozen=True)
class RootNames:
    """Integrated and still-unmatched function names for one checkout."""

    integrated: frozenset[str]
    unmatched: frozenset[str]
    aliases: dict[str, str]


def unique_resolved_paths(paths: Iterable[Path]) -> list[Path]:
    roots: list[Path] = []
    seen: set[Path] = set()

    for path in paths:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        roots.append(resolved)

    return roots


def load_root_names(root: Path, primary_aliases: dict[str, str]) -> RootNames:
    aliases = {**function_name_aliases(root), **primary_aliases}
    return RootNames(
        integrated=frozenset(canonicalize_function_names(integrated_function_names(root), aliases)),
        unmatched=frozenset(canonicalize_function_names(unmatched_function_names(root), aliases)),
        aliases=aliases,
    )


def parse_match_log(log_path: Path, repo_root: Path, aliases: dict[str, str]) -> Iterable[MatchResult]:
    workspace = log_path.parent
    function = canonical_function_name(base_workspace_name(workspace), aliases)
    nonmatchings_dir = (repo_root / "nonmatchings").resolve()

    try:
        lines = log_path.read_text().splitlines()
    except OSError as exc:
        print(f"warning: could not read {log_path}: {exc}", file=sys.stderr)
        return

    for line in lines:
        entry = parse_match_log_line(line)
        if entry is None:
            continue

        attempt = certified_attempt_path(entry, workspace)
        if attempt is None:
            continue

        # Ignore stale log entries and accidental paths outside nonmatchings.
        if not attempt.is_file() or nonmatchings_dir not in attempt.parents:
            continue

        yield MatchResult(
            function=function,
            percent=entry.percent,
            attempt=attempt,
            workspace=workspace,
        )


def scratch_match_percent(scratch: dict) -> float | None:
    score = scratch.get("score")
    max_score = scratch.get("max_score")

    if not isinstance(score, (int, float)):
        return None

    if score == 0:
        return FULL_MATCH_PERCENT

    if not isinstance(max_score, (int, float)) or max_score <= 0:
        return None

    return max(0.0, (max_score - score) / max_score * 100.0)


def parse_scratch_results(
    scratches_path: Path,
    integrated_functions: set[str],
    aliases: dict[str, str],
) -> Iterable[ScratchResult]:
    if not scratches_path.is_file():
        return

    try:
        scratches = json.loads(scratches_path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"warning: could not read {scratches_path}: {exc}", file=sys.stderr)
        return

    if not isinstance(scratches, list):
        print(f"warning: expected {scratches_path} to contain a JSON array", file=sys.stderr)
        return

    for scratch in scratches:
        if not isinstance(scratch, dict):
            continue

        scratch_name = scratch.get("name")
        if not isinstance(scratch_name, str) or not scratch_name:
            continue

        function = canonical_function_name(scratch_name, aliases)
        if function in integrated_functions:
            continue

        percent = scratch_match_percent(scratch)
        if percent is None:
            continue

        slug = scratch.get("slug") or scratch.get("id")
        if not slug:
            continue

        max_score = scratch.get("max_score")
        yield ScratchResult(
            function=function,
            percent=percent,
            score=scratch["score"],
            max_score=max_score if isinstance(max_score, (int, float)) else None,
            slug=str(slug),
            match_override=bool(scratch.get("match_override")),
        )


def is_better_match(candidate: MatchResult, current: MatchResult | None) -> bool:
    if current is None:
        return True
    if candidate.percent != current.percent:
        return candidate.percent > current.percent
    return candidate.attempt < current.attempt


def is_better_scratch(candidate: ScratchResult, current: ScratchResult | None) -> bool:
    if current is None:
        return True
    if candidate.percent != current.percent:
        return candidate.percent > current.percent
    return candidate.slug < current.slug


def best_local_results_by_function(
    repo_root: Path,
    scan_roots: list[Path],
    names_by_root: dict[Path, RootNames],
) -> dict[str, MatchResult]:
    primary = names_by_root[repo_root]
    best_by_function: dict[str, MatchResult] = {}

    for scan_root in scan_roots:
        source = names_by_root[scan_root]
        for log_path in sorted((scan_root / "nonmatchings").glob("*/match_log.txt")):
            function = canonical_function_name(base_workspace_name(log_path.parent), source.aliases)
            if function not in primary.unmatched and function not in source.unmatched:
                continue  # Stale: no longer unmatched assembly in either checkout.
            if function in primary.integrated or function in source.integrated:
                continue  # Already defined in src/.

            for result in parse_match_log(log_path, scan_root, source.aliases):
                best = best_by_function.get(result.function)
                if is_better_match(result, best):
                    best_by_function[result.function] = result

    return best_by_function


def best_scratch_results_by_function(
    scratches_paths: Iterable[Path],
    integrated_functions: set[str],
    aliases: dict[str, str],
) -> dict[str, ScratchResult]:
    best_by_function: dict[str, ScratchResult] = {}
    for scratches_path in scratches_paths:
        for result in parse_scratch_results(scratches_path, integrated_functions, aliases):
            best = best_by_function.get(result.function)
            if is_better_scratch(result, best):
                best_by_function[result.function] = result

    return best_by_function


def full_unintegrated_scratch_results(
    repo_root: Path,
    scratches_paths: Iterable[Path],
) -> list[ScratchResult]:
    aliases = function_name_aliases(repo_root)
    integrated = canonicalize_function_names(integrated_function_names(repo_root), aliases)
    scratch_results = best_scratch_results_by_function(scratches_paths, integrated, aliases)
    return sorted(
        (result for result in scratch_results.values() if result.percent >= FULL_MATCH_PERCENT),
        key=lambda result: result.function,
    )


def report_rows_by_function(
    repo_root: Path,
    scan_roots: Iterable[Path],
    scratches_paths: Iterable[Path],
) -> dict[str, ReportRow]:
    repo_root = repo_root.resolve()
    scan_roots = unique_resolved_paths(scan_roots)
    primary_aliases = function_name_aliases(repo_root)
    names_by_root = {
        root: load_root_names(root, primary_aliases)
        for root in unique_resolved_paths([repo_root, *scan_roots])
    }
    local_results = best_local_results_by_function(repo_root, scan_roots, names_by_root)
    primary = names_by_root[repo_root]
    scratch_results = best_scratch_results_by_function(
        scratches_paths,
        set(primary.integrated),
        primary_aliases,
    )

    return {
        function: ReportRow(local=local, scratch=scratch_results.get(function))
        for function, local in local_results.items()
    }


def format_scratch(scratch: ScratchResult | None) -> str:
    if scratch is None:
        return ""

    score_text = f"{scratch.score}/{scratch.max_score}" if scratch.max_score is not None else str(scratch.score)
    override = " override" if scratch.match_override else ""
    return f"decomp.me {scratch.percent:.3f}% ({score_text}{override}) {scratch.slug}"


def print_rows(title: str, rows: list[ReportRow], repo_root: Path) -> None:
    print(title)
    if not rows:
        print("  none")
        return

    print(f"{'Function':<24} {'Local':>9}  Attempt")
    print("-" * 48)
    for row in rows:
        print(f"{row.local.function:<24} {row.local.percent:8.3f}%  {row.local.attempt.name}")
        print(f"{'':<24} {'':>9}   local: {format_path(row.local.workspace, repo_root)}")
        scratch_text = format_scratch(row.scratch)
        if scratch_text:
            print(f"{'':<24} {'':>9}   {scratch_text}")


def print_scratch_rows(title: str, rows: list[ScratchResult]) -> None:
    print(title)
    if not rows:
        print("  none")
        return

    print(f"{'Function':<32} {'Scratch'}")
    print("-" * 64)
    for scratch in rows:
        print(f"{scratch.function:<32} {format_scratch(scratch)}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Report full and partial matches from nonmatchings/**/match_log.txt, "
            "supplemented with decomp.me scratches from tools/scratches.json. Functions already "
            "defined in src/ are excluded, as are functions that are no longer unmatched assembly."
        )
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=get_project_root(),
        help="Primary repository root used for integration checks and relative paths. Defaults to this checkout.",
    )
    parser.add_argument(
        "--scan-root",
        type=Path,
        action="append",
        default=[],
        help=(
            "Additional repository or worktree root to scan for nonmatchings/*/match_log.txt. "
            "Can be repeated. The primary --repo-root is always scanned."
        ),
    )
    parser.add_argument(
        "--partial-only",
        action="store_true",
        help="Only print partially matched functions.",
    )
    parser.add_argument(
        "--full-only",
        action="store_true",
        help="Only print 100%% matched functions not yet integrated into src/.",
    )
    parser.add_argument(
        "--scratches",
        type=Path,
        default=None,
        help=(
            "Path to decomp.me scratch JSON. Defaults to tools/scratches.json under each scanned root "
            "if it exists."
        ),
    )
    parser.add_argument(
        "--no-scratches",
        action="store_true",
        help="Only use local nonmatchings/**/match_log.txt entries.",
    )
    parser.add_argument(
        "--min-percent",
        type=float,
        default=0.0,
        help="Only report local match_log entries at or above this percentage.",
    )
    args = parser.parse_args()

    if args.partial_only and args.full_only:
        print("error: --partial-only and --full-only cannot be used together", file=sys.stderr)
        return 2
    if args.no_scratches and args.scratches is not None:
        print("error: --no-scratches and --scratches cannot be used together", file=sys.stderr)
        return 2

    repo_root = args.repo_root.resolve()
    explicit_roots = set(unique_resolved_paths(args.scan_root))
    scan_roots: list[Path] = []
    for root in unique_resolved_paths([repo_root, *args.scan_root]):
        if (root / "nonmatchings").is_dir():
            scan_roots.append(root)
        elif root in explicit_roots:
            print(f"warning: skipping {root}; {root / 'nonmatchings'} does not exist", file=sys.stderr)

    if not scan_roots:
        print("error: no scan roots with a nonmatchings directory", file=sys.stderr)
        return 2

    scratches_paths: list[Path] = []
    if not args.no_scratches:
        if args.scratches is not None:
            scratches_paths = [args.scratches.resolve()]
        else:
            scratches_paths = [
                root / "tools" / "scratches.json"
                for root in scan_roots
                if (root / "tools" / "scratches.json").is_file()
            ]

    results = [
        row
        for row in report_rows_by_function(repo_root, scan_roots, scratches_paths).values()
        if row.local.percent >= args.min_percent
    ]
    full_matches = sorted(
        (row for row in results if row.local.percent >= FULL_MATCH_PERCENT),
        key=lambda row: row.local.function,
    )
    partial_matches = sorted(
        (row for row in results if row.local.percent < FULL_MATCH_PERCENT),
        key=lambda row: (-row.local.percent, row.local.function),
    )
    full_scratch_matches = full_unintegrated_scratch_results(repo_root, scratches_paths)

    if not args.partial_only:
        print_rows("100% matched in match_log but not yet integrated into src/", full_matches, repo_root)
        if scratches_paths:
            print()
            print_scratch_rows(
                "100% matched on decomp.me but not yet integrated into src/",
                full_scratch_matches,
            )
    if not args.partial_only and not args.full_only:
        print()
    if not args.full_only:
        print_rows("Partially matched functions", partial_matches, repo_root)

    if not args.partial_only and not args.full_only:
        print(
            f"\nTotals: full={len(full_matches)} "
            f"decomp.me_full={len(full_scratch_matches)} partial={len(partial_matches)}"
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
