#!/usr/bin/env python3
"""Shared helpers for the Mario Golf 64 decomp tooling.

Single source of truth for the pieces that were copy-pasted across seed_c.py and
decomp_loop.py (plus the SDK-path config decomp_loop relies on): the venv
re-exec bootstrap, project path constants, the asm/symbol regexes, the
JSON/stderr emitters, and find_segment.

Import-time imports are stdlib-only so this module stays cheap to import and the
re-exec bootstrap can run before a caller pulls in its heavier dependencies.
"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

# --- project paths --------------------------------------------------------
# tools/decomp_common.py -> tools/ -> project root
SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
# Literal, symlink-stable project root (os.path, NOT pathlib .resolve()), as a str. The rankers
# (pick_target, decomp_asm) require this: .resolve() follows symlinks, which could shift the asm/
# lookup and change ranked rows. Distinct from ROOT_DIR above, which is internal to decomp_common
# (ASM_DIR/SYMBOL_FILES); shared by the os.path-based tools that key paths off the literal root.
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASM_DIR = ROOT_DIR / "asm"
ASM_DATA_DIR = ASM_DIR / "data"
NONMATCHINGS_DIR = ROOT_DIR / "nonmatchings"
BUILD_ASM_DIR = ROOT_DIR / "build" / "asm"
# The two disjoint symbol-name files (maintainer override + Ghidra-synced).
SYMBOL_FILES = [ROOT_DIR / "symbol_addrs.txt", ROOT_DIR / "ghidra_symbols.txt"]


# --- SDK source trees (env-overridable) -----------------------------------
def _sdk_path(env_var: str, default: str) -> Path:
    """An SDK source root: env override if set, else the home-relative default."""
    return Path(os.environ.get(env_var) or os.path.expanduser(default))


# libkmc upstream src — decomp_loop's compile-profile detection (gcc -O).
LIBKMC_SRC = _sdk_path("MG_LIBKMC_SRC", "~/development/repos/libkmc/src")
# libultra upstream src for decomp_loop's profile detection. Points to
# ultralib/src (Nintendo VERSION_J tree, the sole libultra source), mirroring the
# folder structure used in src/libultra/ (io/, os/, audio/, libc/). Set
# MG_LIBULTRA_SRC to override.
# Note: ultralib's gu/mgu helpers are C files, so profile detection for those
# subsystems may differ; pass `--profile libultra` to force the -O profile if needed.
LIBULTRA_SRC = _sdk_path("MG_LIBULTRA_SRC", "~/development/repos/ultralib/src")


# --- regexes --------------------------------------------------------------
PLACEHOLDER_RE = re.compile(r"^(?:func_[0-9A-Fa-f]{8}|[A-Za-z_][A-Za-z0-9_]*)$")
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)\s*$")
SYM_LINE_RE = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;")


# --- venv re-exec ---------------------------------------------------------
def reexec_into_venv(script: str) -> None:
    """Self-promote into ./venv if launched from another interpreter.

    Project policy: all Python tooling runs out of ./venv (which carries the
    asm-differ deps: watchdog, levenshtein, colorama). Call this at the very top
    of a script, before importing heavier deps, passing ``__file__``.
    """
    venv_py = ROOT_DIR / "venv" / "bin" / "python3"
    if venv_py.exists() and Path(sys.executable).resolve() != venv_py.resolve():
        os.execv(str(venv_py), [str(venv_py), script, *sys.argv[1:]])


# --- io helpers -----------------------------------------------------------
def safe_read_text(path, *, errors: str = "replace") -> str:
    """Read a text file as UTF-8 with a forgiving decode policy.

    One source of truth for the `read_text(encoding="utf-8", errors=...)` idiom
    that was copy-pasted across the tools. `errors` defaults to "replace"; pass
    "ignore" to match the few sites that drop undecodable bytes instead.
    """
    return Path(path).read_text(encoding="utf-8", errors=errors)


def capture_stderr(proc, limit: int = 500) -> str:
    """The first `limit` chars of a finished subprocess's stderr, stripped.

    Consolidates the head-truncated stderr idiom in the seed/loop error paths.
    (Sites that deliberately keep the LAST N chars, or the full stream, are
    left as-is - they are a different, intentional choice.)
    """
    return (proc.stderr or "").strip()[:limit]


# --- output helpers -------------------------------------------------------
def emit(payload: dict) -> None:
    """Write one JSON object to stdout (the agent parses this)."""
    sys.stdout.write(json.dumps(payload) + "\n")
    sys.stdout.flush()


def log(msg: str) -> None:
    """Write a human-readable progress line to stderr."""
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


# --- asm helpers ----------------------------------------------------------
def find_segment(placeholder: str, asm_dir: Path = ASM_DIR) -> str | None:
    """Return the segment stem (e.g. '1050') whose asm file declares
    `glabel <placeholder>`, or None if no asm/*.s file declares it."""
    for s_file in sorted(asm_dir.glob("*.s")):
        try:
            for line in s_file.read_text(
                encoding="utf-8", errors="replace"
            ).splitlines():
                m = GLABEL_RE.match(line)
                if m and m.group(1) == placeholder:
                    return s_file.stem
        except OSError:
            continue
    return None
