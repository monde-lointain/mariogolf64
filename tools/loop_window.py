#!/usr/bin/env python3
"""Print gcc 2.7.2's loop-invariant hoisting window for each loop in a function.

Usage: venv/bin/python3 tools/loop_window.py <src.c> <func>

Why this exists (S301). `func_800947A8` reached an exact 618/618 instruction count with six of its
seven global registers matching the ROM, and the whole residual came from one hoisting decision:
which loop-invariant constant takes the last callee-saved slot. That decision is arithmetic, not a
coin, and `loop.c` prints every input to it under `-dL` -- but reading a 400k dump by hand is how a
day gets spent. This is `tools/allocno_report.py` for the hoisting question rather than the
colouring one.

`move_movables` (loop.c:1631) moves an invariant when

    already_moved[regno] || (threshold * savings * lifetime) >= insn_count

`threshold` starts at `(loop_has_call ? 1 : 2) * (1 + n_non_fixed_regs)` (loop.c:532) and drops by 3
after each move (loop.c:1719, loop.c:1904). `savings` is a copy of `n_times_set` (loop.c:597, 793),
so a constant materialised once scores 1, and a constant fed straight into a `bne` has lifetime 1
because RTL expansion emits the `(set r C)` immediately before the branch. For those life-1
invariants the whole rule collapses to a count:

    moves = floor((threshold_0 - insn_count) / 3) + 1

Verified on both loops of `func_800947A8`: 115 real insns -> 3 moves, 117 -> 2 moves, matching the
dump exactly. So when the ROM hoists a different constant than your build does, compare the ORDER of
the movable list, not the constants themselves: the window is fixed by the loop's size, and only the
first N life-1 movables in scan order get in.

The movable list below is in `loop.c` scan order (the trailing low-numbered entry is the scan's
wrap-around, not a sort artifact), with each constant decoded from the post-pass RTL so a DL command
word is readable as a word rather than a signed decimal.
"""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from decomp_common import ROOT_DIR, reexec_into_venv  # noqa: E402

reexec_into_venv(__file__)

# The game -O2 profile, mirroring mk/src.mk plus mk/main.mk's F3DEX2 define.
CFLAGS = [
    "-S", "-G", "0", "-mips3", "-mgp32", "-mfp32", "-mno-abicalls", "-O2", "-dL",
    "-I", "include", "-I", "include/libultra", "-I", "include/libultra/internal",
    "-I", "include/libkmc", "-I", "include/libnusys", "-I", "include/libmus",
    "-I", "include/libnualstl", "-I", "include/libnaudio",
    "-DINCLUDE_ASM_USE_MACRO_INC", "-D_LANGUAGE_C", "-D_FINALROM", "-DF3DEX_GBI_2",
]

LOOP_RE = re.compile(r"^Loop from (\d+) to (\d+): (\d+) real insns\.")
MOVABLE_RE = re.compile(
    r"^Insn (\d+): regno (\d+) \(life (\d+)\), (?:move-insn )?savings (\d+)\s*(.*)$"
)
SETCONST_RE = re.compile(r"\(insn \d+ \d+ \d+ \(set \(reg[^\)]*:\w+ (\d+)\)\s*$")

# threshold = (loop_has_call ? 1 : 2) * (1 + n_non_fixed_regs); n_non_fixed_regs is 60 for this
# target, which the S301 dumps confirm on two loops. A loop containing a call halves it.
THRESHOLD_NOCALL = 122
THRESHOLD_CALL = 61


def const_map(dump: str) -> dict[int, int]:
    """regno -> integer constant, read from the post-pass RTL."""
    out: dict[int, int] = {}
    lines = dump.splitlines()
    for i, line in enumerate(lines):
        m = SETCONST_RE.match(line.strip())
        if not m or i + 1 >= len(lines):
            continue
        c = re.match(r"\(const_int (-?\d+)\)", lines[i + 1].strip())
        if c:
            out[int(m.group(1))] = int(c.group(1))
    return out


def fmt_const(value: int | None) -> str:
    if value is None:
        return ""
    return f"0x{value & 0xFFFFFFFF:08X} ({value})"


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    src, func = sys.argv[1], sys.argv[2]

    with tempfile.TemporaryDirectory() as tmp:
        copy = Path(tmp) / "lw.c"
        copy.write_text(Path(src).read_text())
        cmd = ["tools/cc/gcc", *CFLAGS, "-o", str(Path(tmp) / "lw.s"), str(copy)]
        proc = subprocess.run(
            cmd, cwd=ROOT_DIR, env={"COMPILER_PATH": "tools/cc", "PATH": "/usr/bin:/bin"},
            capture_output=True, text=True,
        )
        if proc.returncode != 0:
            print(proc.stderr[-2000:], file=sys.stderr)
            return 1
        # gcc writes <basename>.loop next to the CWD, not next to the output.
        dump_path = ROOT_DIR / "lw.c.loop"
        dump = dump_path.read_text()
        dump_path.unlink()

    marker = f";; Function {func}\n"
    if marker not in dump:
        print(f"loop_window: no ;; Function {func} in the -dL dump", file=sys.stderr)
        return 1
    body = dump.split(marker, 1)[1]
    body = re.split(r"^;; Function ", body, maxsplit=1, flags=re.M)[0]

    consts = const_map(body)

    loop = None
    for line in body.splitlines():
        m = LOOP_RE.match(line)
        if m:
            insn_count = int(m.group(3))
            window = max(0, (THRESHOLD_NOCALL - insn_count) // 3 + 1)
            loop = insn_count
            print(f"\nLoop {m.group(1)}..{m.group(2)}: {insn_count} real insns")
            print(f"  life-1 window (no call in loop): {window} moves"
                  f"   [thresholds {', '.join(str(THRESHOLD_NOCALL - 3 * k) for k in range(window + 1))}]")
            print(f"  life-1 window (call in loop):    "
                  f"{max(0, (THRESHOLD_CALL - insn_count) // 3 + 1)} moves")
            print(f"  {'insn':>6} {'regno':>6} {'life':>5} {'save':>5}  {'verdict':<16} const")
            continue
        m = MOVABLE_RE.match(line)
        if m and loop is not None:
            insn, regno, life, savings, tail = m.groups()
            verdict = "not desirable" if "not desirable" in tail else tail.strip() or "moved"
            print(f"  {insn:>6} {regno:>6} {life:>5} {savings:>5}  {verdict:<16} "
                  f"{fmt_const(consts.get(int(regno)))}")
    if loop is None:
        print("loop_window: no loops in this function")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
