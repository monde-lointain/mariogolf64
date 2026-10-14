#!/usr/bin/env python3
"""Bound a serial experiment and its descendants; keep artifacts on expiry."""

from __future__ import annotations

import argparse
import contextlib
import math
import os
import signal
import subprocess
import sys
import time
from collections.abc import Mapping, Sequence

DEADLINE_ENV = "MG64_DEADLINE_UNIX"


def execution_seconds(
    seconds: float,
    reserve: float = 0,
    environment: Mapping[str, str] | None = None,
    now: float | None = None,
) -> float:
    """Intersect an operation limit with an inherited absolute deadline."""
    if not math.isfinite(seconds) or seconds <= 0:
        raise ValueError("seconds must be finite and positive")
    if not math.isfinite(reserve) or reserve < 0:
        raise ValueError("reserve must be finite and nonnegative")
    environment = os.environ if environment is None else environment
    deadline = environment.get(DEADLINE_ENV)
    if deadline is not None:
        end = float(deadline)
        if not math.isfinite(end):
            raise ValueError(f"{DEADLINE_ENV} must be a finite Unix timestamp")
        seconds = min(seconds, end - (time.time() if now is None else now) - reserve)
    if seconds <= 0:
        raise TimeoutError("deadline expired or inspection reserve exhausted")
    return seconds


def _owned_groups(groups: set[int], pids: set[int]) -> tuple[set[int], set[int]]:
    """Include descendant sessions as well as members of our isolated group."""
    listing = subprocess.run(
        ["ps", "-eo", "pid=,ppid=,pgid="],
        capture_output=True,
        text=True,
        check=True,
        timeout=1.0,
    )
    rows = [tuple(map(int, line.split())) for line in listing.stdout.splitlines()]
    owned = pids | {pid for pid, _, group in rows if group in groups}
    while True:
        children = {pid for pid, parent, _ in rows if parent in owned}
        expanded = owned | children
        if expanded == owned:
            break
        owned = expanded
    return groups | {group for pid, _, group in rows if pid in owned}, owned


def _stop_group(process: subprocess.Popen) -> None:
    """Let nested guards release their own groups before bounded hard escalation."""
    groups, pids = _owned_groups({process.pid}, {process.pid})
    for group in groups:
        with contextlib.suppress(ProcessLookupError):
            os.killpg(group, signal.SIGTERM)
    end = time.monotonic() + 1.0
    while time.monotonic() < end:
        process.poll()
        alive = False
        for group in groups:
            try:
                os.killpg(group, 0)
                alive = True
            except ProcessLookupError:
                continue
        if not alive:
            break
        time.sleep(0.01)
    # A nested guard has its own session. Retaining every observed descendant
    # group prevents hard escalation from killing the guard but missing its child.
    groups, _ = _owned_groups(groups, pids)
    for group in groups:
        with contextlib.suppress(ProcessLookupError):
            os.killpg(group, signal.SIGKILL)
    try:
        process.communicate(timeout=1.0)
    except subprocess.TimeoutExpired as error:
        # An escaped process can hold inherited pipes. Never let final reaping
        # wait forever; a cleanup failure must not be reported as command success.
        for stream in (process.stdout, process.stderr):
            if stream is not None:
                stream.close()
        process.wait(timeout=1.0)
        raise RuntimeError(
            "process-group cleanup did not release output pipes"
        ) from error


def run_bounded(
    command: Sequence[str],
    seconds: float,
    *,
    reserve: float = 0,
    cwd: str | os.PathLike | None = None,
    env: Mapping[str, str] | None = None,
    capture_output: bool = False,
) -> subprocess.CompletedProcess:
    """Own a process group so expiry cannot leave compiler/permuter children running."""
    environment = dict(os.environ if env is None else env)
    duration = execution_seconds(seconds, reserve, environment)
    # Preserve a campaign's inspection deadline. Otherwise expose this operation's
    # own deadline to the child, so nested bounded commands cannot extend it.
    environment.setdefault(DEADLINE_ENV, str(time.time() + duration))
    previous_handler = signal.getsignal(signal.SIGTERM)

    def terminate(signum, frame):
        raise InterruptedError("bounded operation terminated")

    signal.signal(signal.SIGTERM, terminate)
    process = None
    try:
        process = subprocess.Popen(
            list(command),
            cwd=cwd,
            env=environment,
            start_new_session=True,
            stdout=subprocess.PIPE if capture_output else None,
            stderr=subprocess.PIPE if capture_output else None,
            text=True,
        )
        stdout, stderr = process.communicate(timeout=duration)
    finally:
        try:
            if process is not None:
                # A second SIGTERM must not interrupt cascading nested cleanup.
                signal.signal(signal.SIGTERM, signal.SIG_IGN)
                _stop_group(process)
        finally:
            signal.signal(signal.SIGTERM, previous_handler)
    return subprocess.CompletedProcess(
        list(command), process.returncode, stdout, stderr
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, required=True)
    parser.add_argument("--reserve", type=float, default=0)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("supply a command after --")
    try:
        result = run_bounded(command, args.seconds, reserve=args.reserve)
    except (TimeoutError, subprocess.TimeoutExpired) as error:
        print(f"EXPIRED: {error}; artifacts retained, no bank verdict", file=sys.stderr)
        return 124
    except (ValueError, OSError, RuntimeError) as error:
        parser.error(str(error))
    return result.returncode if result.returncode >= 0 else 128 - result.returncode


if __name__ == "__main__":
    sys.exit(main())
