#!/usr/bin/env python3
"""Draft-and-check pipeline for one flipped segment.

For each INCLUDE_ASM function in src/<seg>.c up to --max instructions (skipping
GCC nested functions), this:
  1. asks m2c for a C draft, with the segment's existing C as type context;
  2. compiles src/<seg>.c with only that function swapped for the draft
     (KMC GCC + KMC as, the same pipeline as the ROM build);
  3. compares the function's instructions with the original, built from its
     INCLUDE_ASM the same way (words carrying relocations are ignored, since
     the linker fills them; a full `make` must still confirm any match);
  4. if it doesn't match and the draft returns void, retries with an s32
     return type (the empty-delay-slot pattern).
Drafts and a report go to permuter/pipeline/<seg>/. src/ is never left modified.

usage: tools/decomp-pipeline.py <seg> [--max N] [--only func ...]
Run from the repo root, on the segment's decompile/<seg> branch.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.getcwd()
CC = os.path.join(ROOT, "tools/cc")
M2C = [os.path.join(ROOT, "venv/bin/python"), os.path.join(ROOT, "tools/m2c/m2c.py")]
CFLAGS = (
    "-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 "
    "-I include -I include/libultra -I include/libultra/internal -I include/libkmc "
    "-I include/libnusys -I include/libmus -I include/libnaudio "
    "-DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM"
).split()
INS_RE = re.compile(r"^\s+/\* [0-9A-F]+ [0-9A-F]+ [0-9A-F]{8} \*/\s+(.*)$")
ENV = dict(os.environ, COMPILER_PATH=CC)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, env=ENV, **kw)


def asm_instructions(seg, func):
    with open(f"asm/nonmatchings/{seg}/{func}.s") as f:
        return [m.group(1) for line in f if (m := INS_RE.match(line))]


def is_nested(ins):
    """A GCC nested function stores $v0 (the static chain) before writing it."""
    for op in ins:
        parts = op.split(None, 1)
        if len(parts) < 2:
            continue
        mn, args = parts
        if mn == "sw" and args.startswith("$v0,") and "($sp)" in args:
            return True
        if args.split(",")[0].strip() == "$v0" and not mn.startswith(("s", "b", "j")):
            return False
    return False


def compile_c(path, obj):
    s = obj + ".s"
    r = run([f"{CC}/gcc", "-S", *CFLAGS, "-o", s, path])
    if r.returncode:
        return r.stderr.strip()
    r = run([f"{CC}/as", "-EB", "-mips2", "-G", "0", "-I", "include", "-o", obj, s])
    os.remove(s)
    return r.stderr.strip() if r.returncode else None


def func_words(obj, func):
    """Instruction words of func; words with a relocation become None."""
    out = run(["mips-linux-gnu-objdump", "-dr", f"--disassemble={func}", obj]).stdout
    words = []
    for line in out.splitlines():
        if "R_MIPS_" in line:
            if words:
                words[-1] = None
        elif re.match(r"^\s+[0-9a-f]+:\s+[0-9a-f]{8}\b", line):
            words.append(line.split()[1])
    while words and words[-1] == "00000000":  # alignment padding
        words.pop()
    return words


def fix_table_arith(code):
    """Rewrite m2c's `&X + (i * k)` / `(i * k) + &X` (meant as byte offsets)
    into explicit byte arithmetic that keeps X's element type."""
    def byte_ptr(x, expr):
        return f"((__typeof__({x}) *)((u8 *)&{x} + ({expr})))"
    code = re.sub(r"\*\(&(\w+) \+ \(([^()]*? \* \d+)\)\)", lambda m: "*" + byte_ptr(m.group(1), m.group(2)), code)
    code = re.sub(r"\*\(\(([^()]*? \* \d+)\) \+ &(\w+)\)", lambda m: "*" + byte_ptr(m.group(2), m.group(1)), code)
    code = re.sub(r"\(&(\w+) \+ \(([^()]*? \* \d+)\)\)", lambda m: byte_ptr(m.group(1), m.group(2)), code)
    code = re.sub(r"\(([^()]*? \* \d+)\) \+ &(\w+)", lambda m: byte_ptr(m.group(2), m.group(1)), code)
    return code


def compare(a, b):
    if len(a) != len(b):
        return f"length {len(b)} vs {len(a)}"
    diffs = sum(1 for x, y in zip(a, b) if x is not None and y is not None and x != y)
    return "MATCH" if diffs == 0 else f"{diffs} words differ"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seg")
    ap.add_argument("--max", type=int, default=60)
    ap.add_argument("--only", nargs="*")
    a = ap.parse_args()
    seg = a.seg
    src_path = f"src/{seg}.c"
    src = open(src_path).read()
    outdir = os.path.join("permuter/pipeline", seg)
    os.makedirs(outdir, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="pipeline-")

    ctx = os.path.join(tmp, "ctx.c")
    r = run([f"{CC}/gcc", "-E", "-P", *CFLAGS, "-DM2CTX", src_path])
    if r.returncode:
        sys.exit("context preprocessing failed:\n" + r.stderr)
    open(ctx, "w").write(r.stdout)

    funcs = re.findall(rf'INCLUDE_ASM\("asm/nonmatchings/{seg}", (\w+)\);', src)
    results = []
    try:
        for func in funcs:
            if a.only and func not in a.only:
                continue
            ins = asm_instructions(seg, func)
            if len(ins) > a.max:
                continue
            if is_nested(ins):
                results.append((func, len(ins), "skipped: nested function", ""))
                continue

            # Target: the original, built from its INCLUDE_ASM.
            stub = os.path.join(tmp, "stub.c")
            open(stub, "w").write(f'#include "common.h"\nINCLUDE_ASM("asm/nonmatchings/{seg}", {func});\n')
            tobj = os.path.join(tmp, "target.o")
            err = compile_c(stub, tobj)
            if err:
                results.append((func, len(ins), "target build failed", err[:200]))
                continue
            target = func_words(tobj, func)

            # m2c draft.
            r = run([*M2C, "-t", "mips-gcc-c", "--context", ctx, "--valid-syntax",
                     f"asm/nonmatchings/{seg}/{func}.s"])
            if r.returncode or not r.stdout.strip():
                results.append((func, len(ins), "m2c failed", r.stderr.strip()[:200]))
                continue
            draft = r.stdout
            open(os.path.join(outdir, f"{func}.c"), "w").write(draft)

            attempts = [("m2c", draft)]
            if re.search(rf"^void {func}\(", draft, re.M):
                attempts.append(("m2c, s32 return", re.sub(rf"^void {func}\(", f"s32 {func}(", draft, flags=re.M)))
            # m2c writes table indexing as `&X + (i * k)` meaning a byte offset,
            # but C scales that by sizeof(X). Retry with explicit byte offsets.
            for label, code in list(attempts):
                fixed = fix_table_arith(code)
                if fixed != code:
                    attempts.append((label + ", byte offsets", fixed))

            verdict, detail, last_err = "", "", ""
            # A hand-written prototype for func may clash with the draft's real
            # signature (m2c's comes from the asm). Only if the compiler says so,
            # retry with the draft's signature as the prototype instead.
            sig = re.search(rf"^([^\n;{{}}]*\b{func}\([^)]*\))\s*\{{", draft, re.M)
            for label, code in list(attempts):
                if sig:
                    attempts.append((label + ", own prototype", code))
            base_src = src
            for label, code in attempts:
                if label.endswith(", own prototype"):
                    if "conflicting types" not in last_err:
                        continue
                    proto = re.search(rf"^([^\n;{{}}]*\b{func}\([^)]*\))\s*\{{", code, re.M).group(1) + ";\n"
                    base_src = re.sub(rf"^[^\n;{{}}]*\b{func}\([^;{{}}]*\);\n", proto, src, count=1, flags=re.M)
                trial_src = base_src.replace(
                    f'INCLUDE_ASM("asm/nonmatchings/{seg}", {func});', code)
                # After common.h: the macros header uses s32 etc.
                trial_src = trial_src.replace(
                    '#include "common.h"\n',
                    '#include "common.h"\n#include "../tools/m2c/m2c_macros.h"\n', 1)
                trial = os.path.join(ROOT, "src", f".pipeline_{seg}.c")
                open(trial, "w").write(trial_src)
                try:
                    tobj2 = os.path.join(tmp, "trial.o")
                    err = compile_c(trial, tobj2)
                finally:
                    os.remove(trial)
                if err:
                    last_err = err
                    verdict, detail = f"{label}: compile error", err.splitlines()[0][:160]
                    continue
                verdict = f"{label}: " + compare(target, func_words(tobj2, func))
                if verdict.endswith("MATCH"):
                    if label != "m2c":
                        open(os.path.join(outdir, f"{func}.c"), "w").write(code)
                    break
            uses_macros = "M2C_" in draft
            results.append((func, len(ins), verdict, ("uses M2C macros; " if uses_macros else "") + detail))
    finally:
        for f in os.listdir(tmp):
            os.remove(os.path.join(tmp, f))
        os.rmdir(tmp)
        assert open(src_path).read() == src, "src was modified!"

    report = [f"{f:16s} {n:4d}  {v}  {d}".rstrip() for f, n, v, d in results]
    open(os.path.join(outdir, "report.txt"), "w").write("\n".join(report) + "\n")
    print("\n".join(report))
    matches = sum(1 for r in results if r[2].endswith("MATCH"))
    print(f"\n{matches} of {len(results)} functions matched; drafts in {outdir}/")


if __name__ == "__main__":
    main()
