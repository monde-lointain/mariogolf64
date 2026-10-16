#!/usr/bin/env python3
"""Run a fresh, serial omp review session for every src/main/*.c file."""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROMPT = """Read each of the guidelines from Chapters 7, 10-16, 31 and 32 in @/mnt/data12/library/notes/code-complete/, then walk over each function in @src/main/{filename}. Apply the guidelines in the chapters to each function in the file so that they are written in high-level, idiomatic C. Ensure that all identifiers (variables, routines, arguments etc) are given high quality names, variables are declared close to where they are used and given as limited a scope as possible and that the appropriate kind of loop structure is used for each kind of loop. Avoid unnatural control flows like goto or unnecessary pointer arithmetic. For naming, you're allowed to overwrite existing identifiers if a more fitting name is found from the current context. You'll likely need to modify the symbol_addrs.txt with the new symbols and re-split before compiling. A walkthrough of Mario Golf is available here for you to refer to for terminology: @~/Downloads/mario-golf-walkthrough.txt. Do not name symbols based on what is in any strings existent in the code. For any symbols overlapping in memory with a larger data structure or array of data structures, consider whether to use the original symbol name or the appropriate accessors into the data structure. Commit your work according to the guidelines in @~/MEGA/prompts/claude/CONTRIBUTING.md."""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--omp", default="omp", help="omp executable (default: PATH)")
    parser.add_argument("--model", help="omp model selector (default: omp settings)")
    parser.add_argument(
        "--session-dir",
        type=Path,
        default=ROOT / "scratchpad" / "omp-main-review",
        help="session parent directory (default: scratchpad/omp-main-review)",
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="print commands without starting sessions"
    )
    args = parser.parse_args()
    sources = sorted(path for path in (ROOT / "src" / "main").glob("*.c") if path.is_file())
    if not sources:
        parser.error(f"no .c files found in {ROOT / 'src' / 'main'}")
    session_root = args.session_dir.expanduser().resolve()
    print(f"Reviewing {len(sources)} files serially from {ROOT}", flush=True)
    for index, source in enumerate(sources, 1):
        command = [
            args.omp,
            "--print",
            "--session-dir",
            str(session_root / source.stem),
        ]
        if args.model:
            command += ["--model", args.model]
        command += ["--", PROMPT.format(filename=source.name)]
        print(f"[{index}/{len(sources)}] {source.relative_to(ROOT)}", flush=True)
        if args.dry_run:
            print(shlex.join(command), flush=True)
            continue
        # Explicit --session-dir bypasses omp's configured auto-resume behavior.
        # Never overlap sessions: source edits, builds and Ghidra are shared.
        if not source.is_file():
            print(f"Source disappeared before dispatch: {source}", file=sys.stderr)
            return 1
        try:
            result = subprocess.run(command, cwd=ROOT, stdin=subprocess.DEVNULL)
        except OSError as error:
            print(f"Cannot launch omp for {source.name}: {error}", file=sys.stderr)
            return 1
        if result.returncode:
            print(
                f"omp failed for {source.name} (exit {result.returncode}); stopping. "
                f"Session directory: {session_root / source.stem}",
                file=sys.stderr,
            )
            return result.returncode if result.returncode > 0 else 128 - result.returncode
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
