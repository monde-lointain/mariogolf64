#!/usr/bin/env python3
"""Shared helpers for nonmatchings/ workspaces and match_log.txt entries.

Parses match_log.txt lines written by decomp-env-template/build.sh. A 100% entry
is only trusted when build.sh archived the exact source under .matches/ and that
archive still hashes to the sha256 recorded in the entry.

Also maps workspace and assembly names to functions, and decides which functions
are integrated (defined in src/) or still unmatched (present as assembly). Used by
find_unmatched_nonmatchings.py and report_nonmatching_matches.py.
"""

from __future__ import annotations

import hashlib
import re
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import scan_src_definitions, unmatched_asm_files  # noqa: E402

FULL_MATCH_PERCENT = 100.0
MATCH_LINE_RE = re.compile(
    r"(?P<filename>[A-Za-z0-9_./-]+\.c)\s+"
    r"(?P<percent>\d+(?:\.\d+)?)\s*%"
    r"(?P<metadata>.*)$"
)
SHA256_RE = re.compile(r"(?:^|\s)sha256=(?P<sha256>[0-9a-f]{64})(?:\s|$)")
ASM_LABEL_LINE_RE = re.compile(r"^\s*(?:glabel|dlabel)\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\b")
ASM_ADDRESS_RE = re.compile(r"/\*\s+[0-9A-Fa-f]+\s+(?P<addr>[0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s+\*/")
FUNC_ADDR_NAME_RE = re.compile(r"^func_(?P<addr>[0-9A-Fa-f]{8})$")
SYMBOL_ADDR_RE = re.compile(
    r"^\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*0x(?P<addr>[0-9A-Fa-f]{8})\s*;(?P<comment>.*)$"
)


@dataclass(frozen=True)
class MatchLogEntry:
    filename: str
    percent: float
    exact: bool
    sha256: str | None


def parse_match_log_line(line: str) -> MatchLogEntry | None:
    match = MATCH_LINE_RE.search(line)
    if match is None:
        return None

    try:
        percent = float(match.group("percent"))
    except ValueError:
        return None

    metadata = match.group("metadata").split()
    sha_match = SHA256_RE.search(match.group("metadata"))
    return MatchLogEntry(
        filename=match.group("filename"),
        percent=percent,
        exact="exact" in metadata,
        sha256=sha_match.group("sha256") if sha_match is not None else None,
    )


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def certified_attempt_path(entry: MatchLogEntry, workspace: Path) -> Path | None:
    """Resolve an entry, rejecting uncertified or modified full matches."""
    attempt = (workspace / entry.filename).resolve()

    if entry.percent < FULL_MATCH_PERCENT:
        return attempt

    if not entry.exact or entry.sha256 is None:
        return None

    match_dir = (workspace / ".matches").resolve()
    if match_dir not in attempt.parents or not attempt.is_file():
        return None
    if not attempt.name.endswith(f"-{entry.sha256}.c"):
        return None

    try:
        if file_sha256(attempt) != entry.sha256:
            return None
    except OSError:
        return None
    return attempt


def format_path(path: Path, repo_root: Path) -> str:
    try:
        return str(path.relative_to(repo_root))
    except ValueError:
        return str(path)


def base_workspace_name(workspace: Path) -> str:
    return workspace.name.split("-", 1)[0]


def function_name_aliases(repo_root: Path) -> dict[str, str]:
    """Map func_<ADDR> placeholder names to their real names from symbol_addrs.txt."""
    aliases: dict[str, str] = {}
    symbol_addrs_path = repo_root / "symbol_addrs.txt"
    if not symbol_addrs_path.is_file():
        return aliases

    try:
        lines = symbol_addrs_path.read_text(errors="ignore").splitlines()
    except OSError as exc:
        print(f"warning: could not read {symbol_addrs_path}: {exc}", file=sys.stderr)
        return aliases

    for line in lines:
        match = SYMBOL_ADDR_RE.match(line)
        if match is None or "type:func" not in match.group("comment"):
            continue

        name = match.group("name")
        alias = f"func_{match.group('addr').upper()}"
        if name != alias:
            aliases[alias] = name

    return aliases


def canonical_function_name(function: str, aliases: dict[str, str]) -> str:
    address_match = FUNC_ADDR_NAME_RE.match(function)
    if address_match is None:
        return function

    return aliases.get(f"func_{address_match.group('addr').upper()}", function)


def canonicalize_function_names(names: set[str], aliases: dict[str, str]) -> set[str]:
    return names | {canonical_function_name(name, aliases) for name in names}


def asm_function_names(path: Path) -> set[str]:
    """Function labels in an assembly file, plus func_<ADDR> for each label's first instruction."""
    names = {path.stem}
    try:
        lines = path.read_text(errors="ignore").splitlines()
    except OSError:
        return names

    awaiting_label_address = False
    for line in lines:
        label = ASM_LABEL_LINE_RE.match(line)
        if label is not None:
            names.add(label.group("name"))
            awaiting_label_address = True
            continue

        if not awaiting_label_address:
            continue

        address = ASM_ADDRESS_RE.search(line)
        if address is None:
            continue

        names.add(f"func_{address.group('addr').upper()}")
        awaiting_label_address = False

    return names


def integrated_function_names(repo_root: Path) -> set[str]:
    """Functions with a C definition under src/, i.e. already integrated."""
    return set(scan_src_definitions(repo_root))


def unmatched_function_names(repo_root: Path) -> set[str]:
    """Functions still present as unmatched assembly (asm/nonmatchings stubs)."""
    names: set[str] = set()
    for path in unmatched_asm_files(repo_root):
        names.update(asm_function_names(path))
    return names
