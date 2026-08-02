#!/usr/bin/env python3
"""Print KMC gcc 2.7.2's own allocno priority table for one function.

Register-permutation residuals (`docs/hazards.md#multi-register-allocno-permutation`) are decided by
arithmetic, not by source spelling: `global.c allocno_compare` orders allocnos by
`floor_log2(n_refs)*n_refs / live_length * 10000 * size`, ties break on allocno number (declaration
order), and `find_reg` then hands out the lowest free hard reg in that order. Guessing at source
permutations to move that order is what makes the class read terminal; reading the numbers turns it
into a calculation (S290 cracked a five-register permutation off one table after ~15 blind
permutations failed).

This compiles the file the way the real build does -- the recipe comes from `make -n`, so per-file
overrides such as `-ffast-math` are picked up -- adds `-dg -dl`, and joins the two dumps:

  .lreg  "Register N used R times across L insns"   -> REG_N_REFS and REG_LIVE_LENGTH
  .greg  ";; N regs to allocate: <allocno order>"   -> allocno_compare's sorted order
  .greg  "Register dispositions: N in H ..."        -> the hard reg each pseudo got

Output is one row per allocno in allocation order, with the computed priority and the assigned
register, followed by the local (block-scoped) quantities that took a hard reg. Compare two builds'
tables, or read one against the target's registers, to name which ref count or live length has to
move. Reminder for a call-free function: `local-alloc`'s `qty_compare_1` scales every term by
`qty_n_calls_crossed`, so all local quantities tie there and their order is birth order -- a local's
scope, not its priority, is the knob (Axis 8).

Usage:
  venv/bin/python3 tools/allocno_report.py src/main/func_8006A2C0.c [func_8006C484]
  venv/bin/python3 tools/allocno_report.py nonmatchings/func_8005DFE8/base.c [func_8005DFE8]

The second form reads a crack slice's own seed: any directory holding a `compile.sh` (a permuter
import or a decomp_loop scratch dir) works, so the table can be read without first inlining the body
into `src/` (S292).

Dumps are written in a scratch copy, never next to the source. Nothing is rebuilt: the real object is
left alone, so this is safe to run mid-iteration.
"""

import argparse
import math
import re
from collections import Counter
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from decomp_common import PROJECT_ROOT

ROOT = Path(PROJECT_ROOT)

# MIPS hard-reg numbers as gcc prints them in "Register dispositions".
REG_NAMES = {0: "zero", 1: "at", 2: "v0", 3: "v1", 4: "a0", 5: "a1", 6: "a2", 7: "a3"}
REG_NAMES.update({8 + i: f"t{i}" for i in range(8)})
REG_NAMES.update({16 + i: f"s{i}" for i in range(8)})
REG_NAMES.update({24: "t8", 25: "t9", 28: "gp", 29: "sp", 30: "fp", 31: "ra"})
REG_NAMES.update({32 + i: f"f{i}" for i in range(32)})
REG_NAMES.update({64: "hi", 65: "lo", 66: "fcc"})


LOCAL_ROWS = 16  # longest-lived locals shown; the rest are single-insn scratch


def reg_name(num):
    return REG_NAMES.get(num, f"r{num}")


def compile_command_from_sh(sh, src):
    """The gcc half of a permuter `compile.sh` (`gcc -S ... -o /dev/stdout "$INPUT" | as ...`)."""
    for line in sh.read_text().splitlines():
        if "tools/cc/gcc" in line and " -S " in line:
            gcc = line.split("|", 1)[0].strip()
            return gcc.replace('"$INPUT"', str(src)).replace("$INPUT", str(src))
    return None


def compile_command(src):
    """The build's own gcc line for this source, from `make -n`.

    Falls back to the seed's own `compile.sh` when the source is not a build source. A crack
    slice iterates on `nonmatchings/<fn>/base.c`, which `make -n` has no recipe for, so before
    S292 the table could only be read by first inlining the body into `src/` -- the one thing the
    isolated loop exists to avoid. The permuter/decomp_loop seed dir already carries the exact
    compile line, so use it.
    """
    obj = Path("build") / src.with_suffix(".o")
    out = subprocess.run(
        ["make", "-B", "-n", str(obj)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    for line in out.stdout.splitlines():
        if "tools/cc/gcc" in line and " -S " in line:
            return line.strip()
    sh = (ROOT / src).parent / "compile.sh"
    if sh.is_file():
        cmd = compile_command_from_sh(sh, src)
        if cmd:
            return cmd
    sys.exit(
        f"no gcc recipe for {obj} in `make -n` output, and no usable {sh} "
        "(is the path a build source or a seed dir?)"
    )


def rewrite_for_dumps(cmd, src, workdir):
    """Absolutise the recipe's relative -I/compiler paths so it can run in a scratch dir."""
    parts = cmd.split()
    out = []
    i = 0
    while i < len(parts):
        tok = parts[i]
        if tok == "-I":
            out += ["-I", str(ROOT / parts[i + 1])]
            i += 2
            continue
        if tok == "-o":
            out += ["-o", str(workdir / "dump.s")]
            i += 2
            continue
        if tok.startswith("COMPILER_PATH="):
            pass  # passed through env instead, since this runs without a shell
        elif tok.endswith("tools/cc/gcc"):
            out.append(str(ROOT / tok))
        elif tok == str(src):
            out.append(str(workdir / src.name))
        else:
            out.append(tok)
        i += 1
    return out + ["-dg", "-dl"]


def function_section(text, func):
    """The `;; Function <name>` block for func, or the whole text when it has no headers."""
    blocks = text.split(";; Function ")
    if len(blocks) == 1:
        return text
    for block in blocks[1:]:
        if block.split("\n", 1)[0].strip() == func:
            return block
    sys.exit(f"function {func} not found in the dump (names present: see the .greg headers)")


def parse_dumps(greg, lreg, func):
    sec, lsec = function_section(greg, func), function_section(lreg, func)

    # "N regs to allocate: 111 (2) 126 (2) 78 ..." -- a trailing "(k)" is the allocno's size in
    # words (a DImode pseudo is 2), which allocno_compare divides by, so keep it.
    order = []
    match = re.search(r"regs to allocate: (.+)", sec)
    if match:
        for pseudo, size in re.findall(r"(\d+)(?:\s+\((\d+)\))?", match.group(1)):
            order.append((int(pseudo), int(size) if size else 1))

    info = {}
    for m in re.finditer(r"Register (\d+) used (\d+) times across (\d+) insns", lsec):
        info[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))

    disp = {}
    if "Register dispositions:" in sec:
        tail = sec.split("Register dispositions:", 1)[1]
        for pseudo, hard in re.findall(r"(\d+) in (\d+)", tail):
            disp[int(pseudo)] = int(hard)
    return order, info, disp


def priority(refs, length, size=1):
    """global.c allocno_compare: floor_log2(refs)*refs / live_length * 10000 * size.

    Note the size factor MULTIPLIES: a DImode (`s64`) pseudo has size 2 and so outranks an otherwise
    identical word-sized one. Ties fall through to the allocno number, i.e. declaration order.
    """
    if not refs or not length:
        return 0
    return int(math.floor(math.log2(refs)) * refs / length * 10000 * size)


def report(order, info, disp, show_locals):
    print(f"{'allocno':>8} {'refs':>5} {'len':>5} {'size':>4} {'priority':>9}  reg")
    for pseudo, size in order:
        refs, length = info.get(pseudo, (0, 0))
        got = disp.get(pseudo)
        print(
            f"{pseudo:>8} {refs:>5} {length:>5} {size:>4} {priority(refs, length, size):>9}  "
            f"{reg_name(got) if got is not None else '-'}"
        )
    if not order:
        print("  (no global allocnos: every pseudo was handled by local-alloc)")
    else:
        computed = [priority(*info.get(p, (0, 0)), size) for p, size in order]
        if computed != sorted(computed, reverse=True):
            print(
                "  WARNING: rows are not in descending priority -- the dump was mis-parsed, or an\n"
                "  allocno merges several pseudos so its refs/live_length are sums, not this row."
            )

    if not show_locals:
        return
    globals_ = {p for p, _ in order}
    locals_ = sorted(
        (p for p in disp if p not in globals_),
        key=lambda p: -info.get(p, (0, 0))[1],
    )
    if not locals_:
        return
    shown, cap = locals_[:LOCAL_ROWS], len(locals_)
    # A quantity sharing both refs and live length with another has no priority
    # gap left: local-alloc falls through to birth order, so no weight edit can
    # move it and the lever is emission order. Mark those rows.
    tied = {
        key
        for key, count in Counter(info.get(p, (0, 0)) for p in locals_).items()
        if count > 1
    }
    print("\nlocal quantities by live length (local-alloc; ties on birth order when call-free)")
    print("  TIE = equal refs AND len, so birth order decides: the lever is emission order, not weight")
    print(f"{'pseudo':>8} {'refs':>5} {'len':>5}  reg   flag")
    for pseudo in shown:
        refs, length = info.get(pseudo, (0, 0))
        flag = "TIE" if (refs, length) in tied else ""
        print(f"{pseudo:>8} {refs:>5} {length:>5}  {reg_name(disp[pseudo]):<4}  {flag}".rstrip())
    if cap > LOCAL_ROWS:
        print(f"  ({cap - LOCAL_ROWS} shorter-lived quantities not shown)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("source", help="build source, e.g. src/main/func_8006A2C0.c")
    ap.add_argument("function", nargs="?", help="function name (default: the file's first)")
    ap.add_argument(
        "--no-locals",
        action="store_true",
        help="global allocnos only (skip the local-alloc quantities)",
    )
    args = ap.parse_args()

    src = Path(args.source)
    if not (ROOT / src).is_file():
        sys.exit(f"no such source: {src}")

    cmd = compile_command(src)
    with tempfile.TemporaryDirectory(prefix="allocno-") as tmp:
        workdir = Path(tmp)
        shutil.copy(ROOT / src, workdir / src.name)
        run = subprocess.run(
            rewrite_for_dumps(cmd, src, workdir),
            cwd=workdir,
            capture_output=True,
            text=True,
            shell=False,
            env={"PATH": "/usr/bin:/bin", "COMPILER_PATH": str(ROOT / "tools/cc")},
        )
        greg_path = workdir / f"{src.name}.greg"
        lreg_path = workdir / f"{src.name}.lreg"
        if not greg_path.is_file() or not lreg_path.is_file():
            sys.exit(f"gcc produced no dumps (exit {run.returncode}):\n{run.stderr.strip()}")
        greg, lreg = greg_path.read_text(), lreg_path.read_text()

    func = args.function
    if not func:
        headers = re.findall(r";; Function (\S+)", greg)
        if not headers:
            sys.exit("no `;; Function` header in the .greg dump")
        func = headers[0]
        print(f"# function: {func}")

    order, info, disp = parse_dumps(greg, lreg, func)
    report(order, info, disp, not args.no_locals)


if __name__ == "__main__":
    main()
