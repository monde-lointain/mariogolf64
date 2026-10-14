#!/usr/bin/env python3
"""Retain isolated measurements against their exact source, target and tool profile."""

from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path

from rom_build import PROFILES, ROOT, build_dir, profile_flags, tool_path


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_hash(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def measurement_context(
    source: Path,
    source_bytes: bytes,
    profile: str,
    parent: Path | None,
    fastmath: bool,
    target_dump: str,
) -> dict:
    """Capture observed inputs; never imply an isolated score is linked-byte proof."""
    tools = {name: Path(tool_path(name)) for name in ("gcc", "kmc_as")}
    rom = re.search(
        r"^sha1:\s*(\w+)", (ROOT / "mariogolf64.yaml").read_text(), re.MULTILINE
    )
    if rom is None:
        raise ValueError("missing configured ROM SHA-1")
    return {
        "rom_sha1": rom[1],
        "candidate_sha256": sha256(source_bytes),
        "candidate_path": str(source.relative_to(ROOT)),
        "target_dump_sha256": sha256(target_dump.encode()),
        "profile": profile,
        "profile_source": str(parent.relative_to(ROOT)) if parent else None,
        "profile_source_sha256": file_hash(parent) if parent else None,
        "flags": profile_flags(
            parent or source, profile, nonmatching=True, fastmath=fastmath
        ),
        "profiles_sha256": file_hash(PROFILES),
        "toolchain": {
            name: {"path": str(path), "sha256": file_hash(path)}
            for name, path in tools.items()
        },
        "build_dir": str(build_dir()),
        "limitations": [
            "isolated normalized comparison, not ROM banking",
            "header/context inputs are not retained; regenerate before resuming",
        ],
    }


def retain_measurement(
    source: Path, source_bytes: bytes, context: dict, outcome: dict
) -> str:
    """Never associate a compile with a candidate edited during the measurement."""
    if source.read_bytes() != source_bytes:
        raise ValueError("candidate changed during measurement; score not attributable")
    # Outcome is part of identity: a header change can alter output while the
    # candidate and profile stay the same. Previous observations remain immutable.
    payload = {"authority": "unbanked", "context": context, "outcome": outcome}
    content = json.dumps(payload, sort_keys=True, indent=2) + "\n"
    identity = sha256(content.encode())
    directory = source.parent / "measurements"
    directory.mkdir(exist_ok=True)
    record = directory / f"{identity}.json"
    snapshot = directory / f"{identity}.c"
    # Content-addressed names avoid overwriting a previous source/score pair.
    for path, data in ((snapshot, source_bytes), (record, content.encode())):
        if path.exists():
            if path.read_bytes() != data:
                raise ValueError(f"measurement archive conflict: {path}")
        else:
            with path.open("xb") as stream:
                stream.write(data)
    return str(record.relative_to(ROOT))
