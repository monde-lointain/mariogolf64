#!/usr/bin/env python3
"""Prepare native /decomp queues and run an opt-in serial, isolated m2c prepass."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

import decomp_common as dc
from decomp_budget import DEADLINE_ENV, run_bounded
from decomp_references import (
    c_sources,
    find_function,
    function_index,
    rank_references,
    reference_index,
    rom_identity,
)
from rom_build import build_dir


def shard_number(identity: str, total: int) -> int:
    """Schedule by ROM/bank/offset, never by mutable names or reference annotations."""
    return (
        int.from_bytes(hashlib.sha256(identity.encode()).digest()[:8], "big") % total
        + 1
    )


def parse_shard(value: str) -> tuple[int, int]:
    try:
        index, total = (int(part) for part in value.split("/"))
    except ValueError as error:
        raise argparse.ArgumentTypeError("shard must be I/N") from error
    if not 1 <= index <= total:
        raise argparse.ArgumentTypeError("shard requires 1 <= I <= N")
    return index, total


def command_json(
    command: list[str], seconds: float, environment: dict[str, str]
) -> dict | list:
    process = run_bounded(
        command, seconds, env=environment, cwd=dc.ROOT_DIR, capture_output=True
    )
    if process.returncode:
        raise RuntimeError(
            f"command exited {process.returncode}: {process.stderr[-4000:]}"
        )
    payload = json.loads(process.stdout)
    if isinstance(payload, dict) and process.stderr:
        payload["diagnostics"] = process.stderr[-4000:]
    return payload


def prepass(candidate: dict, seconds: float) -> dict:
    """Never overwrite a carry or integrate a normalized isolated score of zero."""
    name = candidate["func"]
    directory = dc.NONMATCHINGS_DIR / name
    if directory.exists():
        return {"status": "existing-candidate-preserved", "directory": str(directory)}
    if (
        candidate.get("nested")
        or candidate.get("intrinsic")
        or candidate.get("pool_blocked")
    ):
        return {"status": "coupled-or-intrinsic-skipped"}
    if candidate["segment"].split("/")[0] in {
        "libultra",
        "libkmc",
        "libnusys",
        "libnaudio",
        "libmus",
        "mgu",
    }:
        return {"status": "upstream-first-skipped"}
    if name in c_sources(dc.ROOT_DIR):
        return {"status": "already-c-skipped"}
    # The packet exposes a duration, not a stale deadline generated while queued.
    # Start the absolute deadline only when this candidate gets execution ownership.
    environment = dict(os.environ)
    deadline = time.time() + seconds
    if DEADLINE_ENV in environment:
        deadline = min(deadline, float(environment[DEADLINE_ENV]))
    environment[DEADLINE_ENV] = str(deadline)
    environment["MG64_TASK_SECONDS"] = str(seconds)
    seed_command = [sys.executable, str(dc.SCRIPT_DIR / "seed_c.py"), "--func", name]
    if candidate.get("file"):
        seed_command += ["--parent", candidate["file"]]
    try:
        seed = command_json(seed_command, seconds, environment)
        if not isinstance(seed, dict) or not seed.get("ok"):
            return {"status": "seed-failed", "seed": seed}
        if seed["body_source"] != "m2c":
            return {"status": "m2c-unavailable", "seed": seed}
        measurement = command_json(
            [sys.executable, str(dc.SCRIPT_DIR / "decomp_loop.py"), "--func", name],
            seconds,
            environment,
        )
        if not isinstance(measurement, dict):
            raise ValueError("decomp_loop must return a measurement object")
        if not measurement.get("compile_ok"):
            status = "compile-failed"
        elif measurement["score"] == 0:
            status = "provisional-zero"
        else:
            status = "residual"
        return {
            "status": status,
            "seed": seed,
            "measurement": measurement,
            "deadline_unix": deadline,
            "banked": False,
        }
    except (TimeoutError, subprocess.TimeoutExpired):
        return {
            "status": "expired",
            "banked": False,
            "artifacts_retained": str(directory),
        }
    except (OSError, ValueError, RuntimeError) as error:
        return {"status": "failed", "banked": False, "error": str(error)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--segment", default="main", help="YAML code bank for loose-stub selection"
    )
    selection.add_argument(
        "--func", action="append", help="explicit exact labels; retain risk tags"
    )
    parser.add_argument(
        "-n", type=int, default=12, help="final queue limit, after ranking/sharding"
    )
    parser.add_argument(
        "--pool-size", type=int, default=60, help="bounded ranker shortlist to inspect"
    )
    parser.add_argument("--by-similarity", action="store_true")
    parser.add_argument("--reference-root", type=Path, action="append", default=[])
    parser.add_argument("--shard", type=parse_shard, default=(1, 1))
    parser.add_argument(
        "--seconds", type=float, default=1800, help="per-candidate operation budget"
    )
    parser.add_argument(
        "--prepass",
        action="store_true",
        help="seed/measure serially; no source integration",
    )
    args = parser.parse_args()
    if args.n <= 0 or args.pool_size < args.n:
        parser.error("require 0 < -n <= --pool-size")
    from decomp_budget import execution_seconds

    try:
        execution_seconds(args.seconds)
        rom = rom_identity(dc.ROOT_DIR)
        functions = function_index(dc.ROOT_DIR, build_dir())
        if args.func:
            candidates = [{"fn": name} for name in dict.fromkeys(args.func)]
            # Keep explicit candidate ownership/types and coupling evidence.
            from decomp_loop import find_profile_source
            from pick_target_score import (
                carried_wall_names,
                intrinsic_stub_tell,
                jtbl_carve_tell,
                nested_tell,
                pool_blocked_tell,
            )

            for candidate in candidates:
                function = find_function(functions, candidate["fn"])
                parent = find_profile_source(function.name, function.segment)
                candidate.update(
                    {
                        "file": str(parent.relative_to(dc.ROOT_DIR))
                        if parent
                        else None,
                        "nested": nested_tell(function.name),
                        "intrinsic": intrinsic_stub_tell(function.name),
                        "carried": function.name in carried_wall_names(),
                        "pool_blocked": pool_blocked_tell(function.name),
                        "jtbl_carve": jtbl_carve_tell(
                            function.name, str(parent.relative_to(dc.ROOT_DIR))
                        )
                        if parent
                        else "",
                    }
                )
        else:
            process = subprocess.run(
                [
                    sys.executable,
                    str(dc.SCRIPT_DIR / "pick_target.py"),
                    "--loose-stubs",
                    args.segment,
                    "-n",
                    str(args.pool_size),
                    "--json",
                ],
                capture_output=True,
                text=True,
                cwd=dc.ROOT_DIR,
            )
            if process.returncode not in (0, 1):
                raise RuntimeError(process.stderr)
            candidates = json.loads(process.stdout)
        entries, coverage = reference_index([dc.ROOT_DIR, *args.reference_root], rom)
        queue = []
        shard, total = args.shard
        for candidate in candidates:
            function = find_function(functions, candidate["fn"])
            identity = function.identity(rom)
            if shard_number(identity, total) != shard:
                continue
            references = rank_references(function, rom, entries)
            queue.append(
                {
                    **candidate,
                    **function.provenance(dc.ROOT_DIR, rom),
                    "references": references,
                    "best_similarity": references[0]["similarity"]
                    if references
                    else None,
                    "budget_seconds": args.seconds,
                    "banked": False,
                }
            )
        if args.by_similarity:
            queue.sort(
                key=lambda row: (
                    row["best_similarity"] is None,
                    -(row["best_similarity"] or 0),
                    row["instructions"],
                    row["identity"],
                )
            )
        queue = queue[: args.n]
        if args.prepass:
            # One prepass process owns the shared build/candidate resource at a time.
            # This lock does not license overlap with manual builds or live Ghidra.
            import fcntl

            dc.NONMATCHINGS_DIR.mkdir(exist_ok=True)
            with (dc.NONMATCHINGS_DIR / ".prepass.lock").open("w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                for candidate in queue:
                    candidate["prepass"] = prepass(candidate, args.seconds)
        dc.emit(
            {
                "rom_sha1": rom,
                "queue": queue,
                "reference_coverage": coverage,
                "selection_scope": "explicit labels"
                if args.func
                else f"first {args.pool_size} eligible loose stubs",
                "shard": f"{shard}/{total}",
                "authority": "unbanked; parent must integrate and verify ROM",
                "execution": "native /decomp; reasoning may fan out, shared operations remain serial",
            }
        )
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
