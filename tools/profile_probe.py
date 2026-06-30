#!/usr/bin/env python3
"""profile_probe.py - pin the COMPILE FLAGS of a SHA-missing build (S149).

When a built `.o` SHA-misses and the C logic looks right, the cause is often the
compile PROFILE (opt level, -g, -fdelayed-branch, frame pointer), not the C. This
assembles the target subseg's splat asm into a relocatable object and KMC-compiles
the candidate `src/<seg>.c` at each of a set of flag combos, then diffs the
normalized `objdump -dr` (instruction stream + reloc-symbol lines) per function. It
pins the matching flags in seconds, no full make. See
`docs/hazards.md#-o0-bootsdk-glue-file-profile-a-per-file-opt-level-exception`.

Pipeline gotchas baked in (so they don't have to be re-learned):
  * target asm -> MODERN GAS (mips-linux-gnu-as -I include). NOT `cpp -P | as`
    (cpp silently empties .text and the pipe "succeeds"); NOT KMC `as` (rejects
    `.set gp=64`).
  * candidate -> KMC `gcc -S` then KMC `as` (the mk/src.mk pipeline).
  * both disassembled with mips-linux-gnu-objdump; unresolved relocs (zeroed
    fields) then compare directly.
  * normalizer uses [[:space:]] not `\\s` (POSIX awk / re has no `\\s` shorthand
    in the awk sense; here we use Python re but keep the lesson explicit).

Usage:
  venv/bin/python3 tools/profile_probe.py --seg libnusys/nuboot
  venv/bin/python3 tools/profile_probe.py --seg main/main --flags "-O2,-O1,-O0,-g"
"""
from __future__ import annotations

import argparse
import difflib
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KMC_PREFIX = ROOT / "tools" / "cc"
KMC_GCC = KMC_PREFIX / "gcc"
KMC_AS = KMC_PREFIX / "as"
GAS = "mips-linux-gnu-as"
OBJDUMP = "mips-linux-gnu-objdump"

# Base compile flags (mirror Makefile CFLAGS, sans the -O level which the probe varies).
BASE_FLAGS = (
    "-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls "
    "-I include -I include/libultra -I include/libultra/internal -I include/libkmc "
    "-I include/libnusys -I include/libmus -I include/libnualstl -I include/libnaudio "
    "-DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM -DNONMATCHING"
).split()

# GAS recipe for the splat asm (mk asm rule), and the synthetic header that the
# per-function nonmatchings .s rely on the INCLUDE_ASM wrapper to provide.
GAS_FLAGS = ["-march=vr4300", "-32", "-I", "include", "--no-pad-sections"]
ASM_HEADER = '.include "macro.inc"\n.set noat\n.set noreorder\n.set gp=64\n.section .text, "ax"\n'

DEFAULT_FLAG_SETS = [
    "-O2",
    "-O1",
    "-O0",
    "-O0 -fno-omit-frame-pointer",
    "-g",
    "-g -fdelayed-branch",
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def find_target_asm(seg: str) -> list[Path]:
    """Per-function splat .s files for the (flipped) subseg, in address order."""
    d = ROOT / "asm" / "nonmatchings" / seg
    if not d.is_dir():
        sys.exit(f"profile_probe: no asm/nonmatchings/{seg}/ (is the subseg flipped + extracted?)")
    return sorted(d.glob("*.s"))


def assemble_target(seg: str, workdir: Path) -> Path:
    """Wrap the per-fn .s with the macro header and assemble with modern GAS."""
    body = ASM_HEADER
    for s in find_target_asm(seg):
        body += "\n" + s.read_text()
    src = workdir / "target.s"
    src.write_text(body)
    obj = workdir / "target.o"
    r = run([GAS, *GAS_FLAGS, "-o", str(obj), str(src)], cwd=ROOT)
    if r.returncode != 0 or not obj.exists():
        sys.exit(f"profile_probe: target GAS failed:\n{r.stderr}")
    return obj


def compile_candidate(seg: str, flags: list[str], workdir: Path) -> Path | None:
    cfile = ROOT / "src" / f"{seg}.c"
    if not cfile.exists():
        sys.exit(f"profile_probe: no src/{seg}.c")
    asm = workdir / "cand.s"
    obj = workdir / "cand.o"
    env = dict(os.environ, COMPILER_PATH=str(KMC_PREFIX))
    r = run([str(KMC_GCC), "-S", *BASE_FLAGS, *flags, "-o", str(asm), str(cfile)], cwd=ROOT, env=env)
    if r.returncode != 0:
        print(f"  (gcc failed: {r.stderr.strip().splitlines()[-1:] }) ", file=sys.stderr)
        return None
    r = run([str(KMC_AS), "-EB", "-mips2", "-G", "0", "-I", "include", "-o", str(obj), str(asm)], cwd=ROOT, env=env)
    if r.returncode != 0 or not obj.exists():
        return None
    return obj


_INSN = re.compile(r"^\s+[0-9a-f]+:\s+[0-9a-f]+\s+(.*)$")
_HDR = re.compile(r"^[0-9a-f]+ <(.+)>:")
_REL = re.compile(r"R_MIPS\S*\s+\S+")


def normalize(obj: Path) -> dict[str, list[str]]:
    """objdump -dr -> {func: [normalized instr / reloc lines]}, per function."""
    r = run([OBJDUMP, "-dr", str(obj)])
    out: dict[str, list[str]] = {}
    cur = None
    for line in r.stdout.splitlines():
        h = _HDR.match(line)
        if h:
            name = h.group(1)
            # drop spim's .NON_MATCHING shadow symbols
            cur = None if name.endswith(".NON_MATCHING") else name
            if cur is not None:
                out.setdefault(cur, [])
            continue
        if cur is None:
            continue
        m = _INSN.match(line)
        if m:
            out[cur].append(re.sub(r"\s+", " ", m.group(1)).strip())
        else:
            rel = _REL.search(line)
            if rel:
                out[cur].append(rel.group(0))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Pin compile flags of a SHA-missing build.")
    ap.add_argument("--seg", required=True, help="subseg path stem, e.g. libnusys/nuboot")
    ap.add_argument("--flags", default=None, help="comma-separated flag sets (default: a standard ladder)")
    args = ap.parse_args()

    flag_sets = [f.strip() for f in args.flags.split(",")] if args.flags else DEFAULT_FLAG_SETS

    with tempfile.TemporaryDirectory(prefix="profprobe_") as td:
        workdir = Path(td)
        tgt = normalize(assemble_target(args.seg, workdir))
        funcs = list(tgt.keys())
        if not funcs:
            sys.exit("profile_probe: target object has no functions")
        print(f"target funcs: {', '.join(funcs)}\n")
        rows = []
        for fs in flag_sets:
            obj = compile_candidate(args.seg, fs.split(), workdir)
            if obj is None:
                rows.append((fs, None))
                continue
            cand = normalize(obj)
            per = {}
            for fn in funcs:
                a, b = tgt.get(fn, []), cand.get(fn, [])
                # real edit distance (difflib opcodes), so a single insertion does
                # not cascade-inflate every following line into a "diff".
                sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
                diff = sum(max(i2 - i1, j2 - j1) for op, i1, i2, j1, j2 in sm.get_opcodes() if op != "equal")
                per[fn] = diff
            rows.append((fs, per))
        total = lambda per: sum(per.values()) if per else 10**9
        for fs, per in sorted(rows, key=lambda r: total(r[1])):
            if per is None:
                print(f"[{fs:32}] compile FAILED")
            else:
                detail = "  ".join(f"{fn}={d}" for fn, d in per.items())
                tag = " <-- MATCH" if total(per) == 0 else ""
                print(f"[{fs:32}] total={total(per):<4} {detail}{tag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
