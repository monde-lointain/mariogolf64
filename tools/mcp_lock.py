#!/usr/bin/env python3
"""Cross-invocation mutex for the Ghidra MCP slot.

Why this exists instead of `flock`: `/decomp` and `/decomp-lead` run as a
series of separate Bash tool calls. `exec 9>lockfile; flock -n 9` releases
the lock the instant the bash subshell exits — i.e. after the first call.
A PID-based lockfile doesn't help either; each call has its own PID.

This script implements a directory-mutex (`mkdir` is atomic) with both a TTL
and a PID-liveness probe (`os.kill(pid, 0)`). Consecutive Bash calls within
one slash-command run all see the lock as held; a crashed previous run is
reclaimed immediately if its PID is gone, or after the TTL otherwise. The
PID probe is best-effort — unknown PIDs are treated as alive (conservative
refuse beats wrongful reclaim).

Usage:
  tools/mcp_lock.py acquire --command decomp --identifier func_800B3E40
  tools/mcp_lock.py release
  tools/mcp_lock.py status

Exit codes:
  0 - operation succeeded
  1 - acquire: another holder is alive and within TTL; refuse
  2 - generic error (bad args, IO failure)

acquire writes nonmatchings/.mcp.lock/holder.json with metadata; release
removes the lock directory. TTL defaults to 30 minutes; tune with
--ttl-minutes.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent

# --- venv re-exec ----------------------------------------------------------
_VENV_PY = ROOT_DIR / "venv" / "bin" / "python3"
if _VENV_PY.exists() and Path(sys.executable).resolve() != _VENV_PY.resolve():
    os.execv(str(_VENV_PY), [str(_VENV_PY), __file__, *sys.argv[1:]])
# ---------------------------------------------------------------------------

import argparse
import datetime as dt
import errno
import json
import shutil

_XDG_CACHE = Path(os.environ.get("XDG_CACHE_HOME", "")).expanduser() \
    if os.environ.get("XDG_CACHE_HOME") else Path.home() / ".cache"
LOCK_DIR = _XDG_CACHE / "mariogolf64" / "mcp.lock"
HOLDER_FILE = LOCK_DIR / "holder.json"
DEFAULT_TTL_MINUTES = 30


def now_iso() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")


def read_holder() -> dict | None:
    try:
        return json.loads(HOLDER_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def is_stale(holder: dict, ttl_minutes: int) -> tuple[bool, str]:
    started = holder.get("started")
    try:
        when = dt.datetime.fromisoformat(started)
    except (TypeError, ValueError):
        return True, "unparseable timestamp"
    age = dt.datetime.now(dt.timezone.utc) - when
    if age > dt.timedelta(minutes=ttl_minutes):
        return True, f"older than {ttl_minutes}min ({age})"
    return False, ""


def is_pid_alive(pid: object) -> tuple[bool, str]:
    """Best-effort liveness check. Returns (alive, note). Unknown PIDs are
    treated as alive — conservative refuse beats wrongful reclaim."""
    if not isinstance(pid, int) or pid <= 0:
        return True, "no usable pid in holder.json"
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False, f"pid {pid} not running"
    except PermissionError:
        return True, f"pid {pid} alive (different uid)"
    except OSError as err:
        return True, f"pid {pid} probe error: {err}"
    return True, f"pid {pid} alive"


def _wipe_lock_path() -> None:
    """Remove whatever sits at LOCK_DIR — file (legacy) or dir."""
    if LOCK_DIR.is_dir() and not LOCK_DIR.is_symlink():
        shutil.rmtree(LOCK_DIR, ignore_errors=True)
    else:
        try:
            LOCK_DIR.unlink()
        except FileNotFoundError:
            pass


def acquire(args: argparse.Namespace) -> int:
    LOCK_DIR.parent.mkdir(parents=True, exist_ok=True)
    try:
        LOCK_DIR.mkdir()
    except FileExistsError:
        existing = read_holder() if LOCK_DIR.is_dir() else None
        if existing is None:
            # Lock present but no parseable holder.json — abandoned. Reclaim.
            _wipe_lock_path()
        else:
            stale, reason = is_stale(existing, args.ttl_minutes)
            alive, alive_note = is_pid_alive(existing.get("pid"))
            if not stale and alive:
                sys.stderr.write(
                    f"mcp_lock: held by {existing.get('command','?')}"
                    f" for {existing.get('identifier','?')} since {existing.get('started','?')}\n"
                )
                sys.stderr.write("mcp_lock: refuse acquire — release the current holder first\n")
                return 1
            if not alive:
                sys.stderr.write(f"mcp_lock: reclaiming dead-PID lock ({alive_note})\n")
            else:
                sys.stderr.write(f"mcp_lock: reclaiming stale lock ({reason})\n")
            _wipe_lock_path()
        try:
            LOCK_DIR.mkdir()
        except FileExistsError:
            sys.stderr.write("mcp_lock: lock reappeared during reclaim — race or permissions issue\n")
            return 1

    holder = {
        "command": args.command,
        "identifier": args.identifier,
        "started": now_iso(),
        "pid": os.getpid(),
    }
    HOLDER_FILE.write_text(json.dumps(holder, indent=2), encoding="utf-8")
    sys.stdout.write(f"acquired by {args.command} ({args.identifier}) at {holder['started']}\n")
    return 0


def release(args: argparse.Namespace) -> int:
    if not LOCK_DIR.exists():
        sys.stdout.write("mcp_lock: no lock held\n")
        return 0
    if args.identifier:
        existing = read_holder() or {}
        if existing.get("identifier") and existing["identifier"] != args.identifier:
            sys.stderr.write(
                f"mcp_lock: refusing release — held by {existing.get('identifier')}, "
                f"requested {args.identifier}\n"
            )
            return 1
    _wipe_lock_path()
    sys.stdout.write("released\n")
    return 0


def status(args: argparse.Namespace) -> int:
    if not LOCK_DIR.exists():
        sys.stdout.write(json.dumps({"held": False}) + "\n")
        return 0
    holder = read_holder() or {}
    stale, reason = is_stale(holder, args.ttl_minutes) if holder else (True, "no holder.json")
    out = {"held": True, "stale": stale, "stale_reason": reason, **holder}
    sys.stdout.write(json.dumps(out, indent=2) + "\n")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="op", required=True)

    p_acquire = sub.add_parser("acquire")
    p_acquire.add_argument("--command", required=True, help="decomp or decomp-lead")
    p_acquire.add_argument("--identifier", default="-", help="func name or scope")
    p_acquire.add_argument("--ttl-minutes", type=int, default=DEFAULT_TTL_MINUTES)

    p_release = sub.add_parser("release")
    p_release.add_argument("--identifier", default="", help="(optional) refuse release if mismatch")

    p_status = sub.add_parser("status")
    p_status.add_argument("--ttl-minutes", type=int, default=DEFAULT_TTL_MINUTES)

    args = parser.parse_args()
    if args.op == "acquire":
        return acquire(args)
    if args.op == "release":
        return release(args)
    if args.op == "status":
        return status(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())
