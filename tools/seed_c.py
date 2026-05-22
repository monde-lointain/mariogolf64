#!/usr/bin/env python3
"""Generate `nonmatchings/<func>/base.c` for the /decomp loop.

Pipeline:
  1. (--parent) m2ctx.py <parent.c> -> ctx.c          (for m2c type context)
  2. extract_functions slice of <placeholder> from asm/<seg>.s -> target.s
  3. (best-effort) rodata slicing from asm/data/*.{rodata,data}.s -> target.rodata.s
  4. m2c --context ctx.c target.s [target.rodata.s] -> m2c.c
  5. Read nonmatchings/<func>/ghidra.c if the slash command wrote one
  6. Stitch base.c: <ultra64.h> + DLIST_HINT + externs + commented m2c + body

The Ghidra MCP decompile fetch is the slash command's responsibility, not
this script's. If `ghidra.c` is missing, base.c gets a TODO body that the
agent must replace.

Stdout: one JSON object describing what was produced.
Stderr: human-readable progress.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
ASM_DIR = ROOT_DIR / "asm"
ASM_DATA_DIR = ASM_DIR / "data"
NONMATCHINGS_DIR = ROOT_DIR / "nonmatchings"
M2CTX_PATH = SCRIPT_DIR / "m2ctx.py"
M2C_PATH = SCRIPT_DIR / "m2c" / "m2c.py"

PLACEHOLDER_RE = re.compile(r"^func_[0-9A-Fa-f]{8}$")
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)\s*$")
HI_LO_REF_RE = re.compile(r"%(?:hi|lo)\(([A-Za-z_][\w.]*)\)")
INCLUDE_ASM_RE = re.compile(r'INCLUDE_ASM\([^,]+,\s*([A-Za-z_][\w]*)\s*\)\s*;')
EXTERN_LINE_RE = re.compile(r"^\s*extern\s+[^;]+;\s*$")
DATA_LABEL_RE = re.compile(r"^\s*([A-Za-z_][\w.]*)\s*:\s*$")


def emit(payload: dict) -> None:
    sys.stdout.write(json.dumps(payload) + "\n")
    sys.stdout.flush()


def log(msg: str) -> None:
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


def fail(error: str) -> None:
    emit({"ok": False, "error": error})
    sys.exit(1)


def load_extract_functions():
    """Import tools/extract_functions.py as a module."""
    spec = importlib.util.spec_from_file_location(
        "extract_functions", SCRIPT_DIR / "extract_functions.py"
    )
    if spec is None or spec.loader is None:
        fail("could not load extract_functions.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]
    return module


def find_segment(placeholder: str) -> str:
    """Return the segment stem (e.g. '1050') containing <placeholder>."""
    for s_file in sorted(ASM_DIR.glob("*.s")):
        try:
            for line in s_file.read_text(encoding="utf-8", errors="replace").splitlines():
                m = GLABEL_RE.match(line)
                if m and m.group(1) == placeholder:
                    return s_file.stem
        except OSError:
            continue
    fail(f"no `glabel {placeholder}` found in any asm/*.s file")
    raise SystemExit(1)


def slice_target_asm(placeholder: str, seg_stem: str, out_dir: Path) -> Path:
    """Extract the formatted slice for <placeholder> into out_dir/target.s."""
    extract_mod = load_extract_functions()
    seg_path = ASM_DIR / f"{seg_stem}.s"
    if not seg_path.exists():
        fail(f"missing asm source: {seg_path}")
    content = seg_path.read_text(encoding="utf-8", errors="replace")
    for func_name, func_text in extract_mod.extract_functions(content):
        if func_name == placeholder:
            target_path = out_dir / "target.s"
            target_path.write_text(func_text + "\n", encoding="utf-8")
            return target_path
    fail(f"`{placeholder}` listed in {seg_path} but extract_functions did not return it")
    raise SystemExit(1)


def collect_hi_lo_labels(target_asm_path: Path) -> set[str]:
    """Return all labels referenced via %hi/%lo in the target asm."""
    text = target_asm_path.read_text(encoding="utf-8", errors="replace")
    return set(HI_LO_REF_RE.findall(text))


def slice_rodata(labels: set[str], out_path: Path) -> tuple[Path | None, list[str]]:
    """Extract each label's contents from asm/data/*.{rodata,data}.s into out_path.

    Returns (path_or_None, missing_labels).
    """
    if not labels:
        return None, []
    if not ASM_DATA_DIR.exists():
        return None, sorted(labels)

    found: dict[str, list[str]] = {}
    missing = set(labels)

    for data_file in sorted(ASM_DATA_DIR.glob("*.s")):
        try:
            lines = data_file.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        i = 0
        while i < len(lines):
            m = DATA_LABEL_RE.match(lines[i])
            if not m or m.group(1) not in missing:
                i += 1
                continue
            label = m.group(1)
            chunk: list[str] = [lines[i]]
            i += 1
            while i < len(lines):
                next_lbl = DATA_LABEL_RE.match(lines[i])
                if next_lbl is not None:
                    break
                if lines[i].lstrip().startswith(".section"):
                    break
                if lines[i].lstrip().startswith("glabel"):
                    break
                chunk.append(lines[i])
                i += 1
            found[label] = chunk
            missing.discard(label)

    if not found:
        return None, sorted(missing)

    out_lines = [".section .rodata", ""]
    for label in labels:
        if label in found:
            out_lines.extend(found[label])
            out_lines.append("")
    out_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
    return out_path, sorted(missing)


def run_m2ctx(parent: Path, out_path: Path) -> bool:
    """Generate ctx.c via m2ctx.py. Returns True on success."""
    try:
        proc = subprocess.run(
            [sys.executable, str(M2CTX_PATH), str(parent)],
            cwd=ROOT_DIR,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.TimeoutExpired:
        log("[seed] m2ctx.py timed out; continuing without --context")
        return False
    # m2ctx.py writes ctx.c at root_dir/ctx.c; relocate to out_path.
    produced = ROOT_DIR / "ctx.c"
    if proc.returncode != 0 or not produced.exists():
        log(f"[seed] m2ctx.py failed (rc={proc.returncode}); continuing without --context")
        log(f"[seed] stderr: {proc.stderr.strip()[:500]}")
        return False
    produced.replace(out_path)
    return True


def run_m2c(
    target_asm: Path, ctx: Path | None, rodata: Path | None, out_path: Path
) -> bool:
    """Run m2c. Returns True on success."""
    cmd = [sys.executable, str(M2C_PATH)]
    if ctx is not None and ctx.exists():
        cmd += ["--context", str(ctx)]
    cmd += [str(target_asm)]
    if rodata is not None:
        cmd += [str(rodata)]
    proc = subprocess.run(cmd, cwd=ROOT_DIR, capture_output=True, text=True)
    if proc.returncode != 0:
        log(f"[seed] m2c failed (rc={proc.returncode}); m2c.c will be a stub")
        log(f"[seed] stderr: {proc.stderr.strip()[:500]}")
        out_path.write_text(
            f"/* m2c failed; stderr:\n{proc.stderr.strip()[:2000]}\n*/\n",
            encoding="utf-8",
        )
        return False
    out_path.write_text(proc.stdout, encoding="utf-8")
    return True


def collect_parent_externs(parent: Path) -> tuple[list[str], list[str]]:
    """Return (extern_decls, include_asm_names) from the parent .c."""
    if parent is None or not parent.exists():
        return [], []
    externs: list[str] = []
    asm_names: list[str] = []
    for line in parent.read_text(encoding="utf-8", errors="replace").splitlines():
        if EXTERN_LINE_RE.match(line):
            externs.append(line.rstrip())
            continue
        m = INCLUDE_ASM_RE.search(line)
        if m:
            asm_names.append(m.group(1))
    return externs, asm_names


def stitch_base_c(
    out_dir: Path,
    placeholder: str,
    externs: list[str],
    sibling_asm_names: list[str],
    m2c_path: Path,
    ghidra_path: Path,
    missing_rodata: list[str],
) -> Path:
    """Assemble base.c per Section 4 step 6 of the plan."""
    lines: list[str] = []
    lines.append("// Generated by tools/seed_c.py for the /decomp loop.")
    lines.append("// Edit freely; this is a seed, not the final answer.")
    lines.append("")
    lines.append("#include <ultra64.h>")
    lines.append('// DLIST_HINT: if this function builds/walks Gfx*, add `#include <PR/gbi.h>`')
    lines.append('//             and run gfxdis on any static dlists in target.rodata.s.')
    lines.append("")

    if missing_rodata:
        lines.append("// SEED WARNING: rodata labels not found in asm/data/:")
        for lbl in missing_rodata[:10]:
            lines.append(f"//   - {lbl}")
        if len(missing_rodata) > 10:
            lines.append(f"//   ...and {len(missing_rodata) - 10} more")
        lines.append("")

    if externs or sibling_asm_names:
        lines.append("// === Externs from parent file ===")
        for e in externs:
            lines.append(e)
        for name in sibling_asm_names:
            if name == placeholder:
                continue
            lines.append(f"extern void {name}(void); // TODO: refine signature from MCP")
        lines.append("")

    if m2c_path.exists():
        lines.append("/* === m2c reference (do not compile) =========================== */")
        lines.append("#if 0")
        for ml in m2c_path.read_text(encoding="utf-8", errors="replace").splitlines():
            lines.append(ml)
        lines.append("#endif")
        lines.append("/* === end m2c reference ========================================= */")
        lines.append("")

    lines.append("// === Function body ============================================")
    if ghidra_path.exists():
        body = ghidra_path.read_text(encoding="utf-8", errors="replace").rstrip()
        lines.append(body)
    else:
        lines.append(f"// TODO: agent — paste Ghidra MCP `decompile_function` output for {placeholder}")
        lines.append(f"// here, then rename m2c synthetics per CLAUDE.md naming conventions.")
        lines.append(f"void {placeholder}(void) {{")
        lines.append(f'    /* unimplemented */')
        lines.append(f"}}")
    lines.append("")

    out_path = out_dir / "base.c"
    out_path.write_text("\n".join(lines), encoding="utf-8")
    return out_path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True, help="func_XXXXXXXX placeholder")
    parser.add_argument("--parent", help="Path to parent src/<seg>.c for m2ctx")
    parser.add_argument(
        "--no-rodata", action="store_true", help="Skip rodata slicing (debug)"
    )
    args = parser.parse_args()

    if not PLACEHOLDER_RE.match(args.func):
        fail(f"--func must match {PLACEHOLDER_RE.pattern}; got {args.func!r}. "
             "(seed_c.py expects the placeholder; slash command resolves curated names.)")
    placeholder = args.func

    out_dir = NONMATCHINGS_DIR / placeholder
    out_dir.mkdir(parents=True, exist_ok=True)
    log(f"[seed] working dir: {out_dir.relative_to(ROOT_DIR)}")

    seg_stem = find_segment(placeholder)
    log(f"[seed] segment: {seg_stem}")

    target_asm = slice_target_asm(placeholder, seg_stem, out_dir)
    log(f"[seed] target.s written")

    # Rodata slicing (best-effort)
    rodata_path: Path | None = None
    missing_rodata: list[str] = []
    if not args.no_rodata:
        labels = collect_hi_lo_labels(target_asm)
        log(f"[seed] {len(labels)} %hi/%lo labels referenced")
        if labels:
            rodata_path, missing_rodata = slice_rodata(labels, out_dir / "target.rodata.s")
            if rodata_path is not None:
                log(f"[seed] target.rodata.s written ({len(labels) - len(missing_rodata)} labels)")
            if missing_rodata:
                log(f"[seed] WARN: {len(missing_rodata)} labels not found in asm/data/")

    # m2ctx (best-effort)
    ctx_path = out_dir / "ctx.c"
    has_ctx = False
    if args.parent:
        parent_path = Path(args.parent)
        if parent_path.exists():
            has_ctx = run_m2ctx(parent_path, ctx_path)
        else:
            log(f"[seed] --parent {args.parent} not found; skipping m2ctx")

    # m2c
    m2c_path = out_dir / "m2c.c"
    run_m2c(target_asm, ctx_path if has_ctx else None, rodata_path, m2c_path)

    # Externs from parent
    externs: list[str] = []
    sibling_asm_names: list[str] = []
    if args.parent:
        parent_path = Path(args.parent)
        if parent_path.exists():
            externs, sibling_asm_names = collect_parent_externs(parent_path)
            log(f"[seed] parent externs: {len(externs)}  sibling INCLUDE_ASM: {len(sibling_asm_names)}")

    # Ghidra body (written by slash command, not this script)
    ghidra_path = out_dir / "ghidra.c"

    base_path = stitch_base_c(
        out_dir, placeholder, externs, sibling_asm_names, m2c_path, ghidra_path, missing_rodata,
    )
    log(f"[seed] {base_path.relative_to(ROOT_DIR)} written")

    emit({
        "ok": True,
        "placeholder": placeholder,
        "segment": seg_stem,
        "out_dir": str(out_dir.relative_to(ROOT_DIR)),
        "base_c": str(base_path.relative_to(ROOT_DIR)),
        "target_s": str(target_asm.relative_to(ROOT_DIR)),
        "target_rodata_s": str(rodata_path.relative_to(ROOT_DIR)) if rodata_path else None,
        "missing_rodata_labels": missing_rodata,
        "ctx_c": str(ctx_path.relative_to(ROOT_DIR)) if has_ctx else None,
        "m2c_c": str(m2c_path.relative_to(ROOT_DIR)),
        "ghidra_c_expected": str(ghidra_path.relative_to(ROOT_DIR)),
        "ghidra_c_present": ghidra_path.exists(),
        "extern_count": len(externs),
        "sibling_asm_count": len(sibling_asm_names),
    })


if __name__ == "__main__":
    main()
