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
import itertools
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
    # 2D form: *((i * a) + ((j * b) + &X))
    code = re.sub(r"\*\(\(([^()]*? \* \d+)\) \+ \(\(([^()]*? \* \d+)\) \+ &(\w+)\)\)",
                  lambda m: "*" + byte_ptr(m.group(3), f"({m.group(2)}) + ({m.group(1)})"), code)
    code = re.sub(r"\*\(&(\w+) \+ \(([^()]*? \* \d+)\)\)", lambda m: "*" + byte_ptr(m.group(1), m.group(2)), code)
    code = re.sub(r"\*\(\(([^()]*? \* \d+)\) \+ &(\w+)\)", lambda m: "*" + byte_ptr(m.group(2), m.group(1)), code)
    code = re.sub(r"\(&(\w+) \+ \(([^()]*? \* \d+)\)\)", lambda m: byte_ptr(m.group(1), m.group(2)), code)
    code = re.sub(r"\(([^()]*? \* \d+)\) \+ &(\w+)", lambda m: byte_ptr(m.group(2), m.group(1)), code)
    return code


def fix_signed_div(code):
    """m2c expands signed division by 2^n as `v = (s32) X >> n; if (X < 0)
    { v = (s32) (X + (2^n - 1)) >> n; }`. Turn it back into `v = X / 2^n;`."""
    pat = re.compile(
        r"(?P<ind>[ \t]*)(?P<v>\w+) = \(s32\) (?P<x>[\w.>\-\[\]]+) >> (?P<n>0x[0-9A-F]+|\d+);\n"
        r"(?P=ind)if \((?P=x) < 0\) \{\n"
        r"[ \t]*(?P=v) = \(s32\) \((?P=x) \+ (?P<m>0x[0-9A-F]+|\d+)\) >> (?P=n);\n"
        r"(?P=ind)\}\n")
    def rep(m):
        n = int(m.group("n"), 0)
        if int(m.group("m"), 0) != (1 << n) - 1:
            return m.group(0)
        return f"{m.group('ind')}{m.group('v')} = {m.group('x')} / 0x{1 << n:X};\n"
    return pat.sub(rep, code)


SIZES = {"s8": 1, "u8": 1, "s16": 2, "u16": 2, "s32": 4, "u32": 4, "f32": 4, "f64": 8, "s64": 8, "u64": 8}


def fix_pointer_stride(code):
    """m2c steps a typed pointer by a byte count (`p += 4` on s32 *); C scales
    that by the element size, so make it one element."""
    for t, name in re.findall(r"^\s+(\w+) \*(\w+);$", code, re.M):
        size = SIZES.get(t)
        if size and size > 1:
            code = re.sub(rf"\b{name} \+= {size};", f"{name} += 1;", code)
    return code


def fix_read_args_first(code, func):
    """Hoist a single read of `*argN` into a local initialised at the top of
    the function, so the load comes first, as in the original."""
    m = re.search(rf"^\w[^\n]*\b{func}\(([^)]*)\) \{{\n", code, re.M)
    if not m:
        return code
    params = [p.strip() for p in m.group(1).split(",")]
    for p in params:
        pm = re.match(r"(?:const )?(\w+) \*(arg\d+)$", p)
        if not pm:
            continue
        t, arg = pm.groups()
        body = code[m.end():]
        reads = re.findall(rf"(?<![\w)])\*{arg}\b(?!\s*=[^=])", body)
        if len(reads) != 1:
            continue
        body = re.sub(rf"(?<![\w)])\*{arg}\b(?!\s*=[^=])", f"{arg}_val", body, count=1)
        code = code[:m.end()] + f"    {t} {arg}_val = *{arg};\n" + body
        m = re.search(rf"^\w[^\n]*\b{func}\(([^)]*)\) \{{\n", code, re.M)
    return code


def stack_layout(ins):
    """(frame size, lowest saved-register slot) from the asm prologue."""
    frame, saved = None, None
    for op in ins[:12]:
        m = re.match(r"addiu\s+\$sp, \$sp, -(0x[0-9A-F]+|\d+)$", op)
        if m:
            frame = int(m.group(1), 0)
        m = re.match(r"s[wd]c?1?\s+\$(ra|s\d|fp|fs\d|fv\d), (0x[0-9A-F]+|\d+)\(\$sp\)$", op)
        if m:
            off = int(m.group(2), 0)
            saved = off if saved is None else min(saved, off)
    return frame, saved


def fix_stack_buffer(code, ins):
    """m2c declares a stack buffer whose address is passed to a callee as a
    single `M2C_UNK spXX;`. Size it as an s32 array reaching up to the first
    saved register."""
    frame, saved = stack_layout(ins)
    if not saved:
        return code
    def rep(m):
        off = int(m.group(2), 16)
        size = saved - off
        if size <= 4 or size % 4:
            return m.group(0)
        return f"{m.group(1)}s32 sp{m.group(2)}[{size // 4}];"
    new = re.sub(r"^([ \t]+)M2C_UNK sp([0-9A-F]+);", rep, code, flags=re.M)
    if new != code:
        for name in re.findall(r"s32 (sp[0-9A-F]+)\[", new):
            new = new.replace(f"&{name}", name)
    return new


def frame_size(obj, func):
    out = run(["mips-linux-gnu-objdump", "-d", f"--disassemble={func}", obj]).stdout
    m = re.search(r"addiu\s+sp,sp,-(\d+)", out)
    return int(m.group(1)) if m else 0


def pad_frame(code, func, nbytes):
    """Insert an unused local of nbytes at the top of func's body."""
    return re.sub(rf"^(\w[^\n]*\b{func}\([^)]*\) \{{\n)",
                  lambda m: m.group(1) + f"    u8 pad_unused[0x{nbytes:X}]; /* unused; sizes the stack frame */\n",
                  code, count=1, flags=re.M)


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

            # Known m2c gaps, each a transform; try every applicable
            # combination, fewest transforms first.
            transforms = [
                ("s32 return", lambda c: re.sub(rf"^void {func}\(", f"s32 {func}(", c, flags=re.M)),
                # `&X + (i * k)` means a byte offset, but C scales it by sizeof(X)
                ("byte offsets", fix_table_arith),
                ("signed div", fix_signed_div),
                ("stack buffer", lambda c: fix_stack_buffer(c, ins)),
                ("pointer stride", fix_pointer_stride),
                ("read args first", lambda c: fix_read_args_first(c, func)),
            ]
            applicable = [(n, fn) for n, fn in transforms if fn(draft) != draft]
            attempts = []
            for k in range(len(applicable) + 1):
                for combo in itertools.combinations(applicable, k):
                    code = draft
                    for _, fn in combo:
                        code = fn(code)
                    attempts.append((", ".join(["m2c"] + [n for n, _ in combo]), code))
            target_frame, _ = stack_layout(ins)

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
                # Frame smaller than the original's: retry once with an unused
                # local making up the difference.
                got = frame_size(tobj2, func)
                if (target_frame and got and got < target_frame and "frame pad" not in label
                        and len(attempts) < 200):
                    attempts.append((label + ", frame pad", pad_frame(code, func, target_frame - got)))
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
