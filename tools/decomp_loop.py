#!/usr/bin/env python3
"""Per-function compile + diff loop for Mario Golf 64.

Mirrors decomp.me's KMC GCC compile + asm-differ flow exactly:
  - Single-step `gcc -c -G0 -mgp32 -mfp32 ...` compile of nonmatchings/<func>/base.c
  - objdump the candidate (current.o) and the canonical reference (build/asm/<seg>.o)
  - Drive asm-differ as a Python library (mirrors decomp.me's
    backend/coreapp/diff_wrapper.py)

Stdout: one JSON object (the agent parses this).
Stderr: human-readable progress messages.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
ASMDIFF_DIR = SCRIPT_DIR / "asm-differ"
ASM_DIR = ROOT_DIR / "asm"
BUILD_ASM_DIR = ROOT_DIR / "build" / "asm"
NONMATCHINGS_DIR = ROOT_DIR / "nonmatchings"
SYMBOL_FILES = [ROOT_DIR / "symbol_addrs.txt", ROOT_DIR / "ghidra_symbols.txt"]

OBJDUMP = os.environ.get("OBJDUMP", "mips-linux-gnu-objdump")
PLACEHOLDER_RE = re.compile(r"^func_[0-9A-Fa-f]{8}$")
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)\s*$")
SYM_LINE_RE = re.compile(r"^\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)\s*;")


def emit(payload: dict) -> None:
    sys.stdout.write(json.dumps(payload) + "\n")
    sys.stdout.flush()


def log(msg: str) -> None:
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


def fail(error: str, code: int = 1, extra: dict | None = None) -> None:
    payload = {"compile_ok": False, "error": error}
    if extra:
        payload.update(extra)
    emit(payload)
    sys.exit(code)


def asmdiff_available() -> None:
    """Verify the asm-differ submodule is initialized."""
    if not (ASMDIFF_DIR / "diff.py").exists():
        fail(
            f"asm-differ not initialized at {ASMDIFF_DIR}. "
            "Run `git submodule update --init tools/asm-differ`."
        )


def resolve_placeholder(name: str) -> str:
    """Return the func_XXXXXXXX placeholder for `name`.

    Accepts either a placeholder already, or a curated name that appears
    in symbol_addrs.txt / ghidra_symbols.txt. Does NOT call Ghidra MCP;
    that's the slash command's job upstream.
    """
    if PLACEHOLDER_RE.match(name):
        return name
    for f in SYMBOL_FILES:
        if not f.exists():
            continue
        for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            m = SYM_LINE_RE.match(stripped)
            if m and m.group(1) == name:
                vram = int(m.group(2), 16)
                return f"func_{vram:08X}"
    fail(
        f"could not resolve '{name}' to a func_XXXXXXXX placeholder; not "
        f"present in {[str(f) for f in SYMBOL_FILES]} and not in placeholder form. "
        "Run /decomp from a slash command (which queries Ghidra MCP) or pass the placeholder directly."
    )
    raise SystemExit(1)  # unreachable; for type checker


def find_segment(placeholder: str) -> str:
    """Return the segment stem (e.g. '1050') whose asm file declares <placeholder>."""
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


def ensure_reference_object(seg_stem: str) -> Path:
    """Build build/asm/<seg>.o if missing or stale. Returns its path."""
    target = BUILD_ASM_DIR / f"{seg_stem}.o"
    src = ASM_DIR / f"{seg_stem}.s"
    if not src.exists():
        fail(f"asm source file missing: {src}")
    needs_build = (not target.exists()) or (target.stat().st_mtime < src.stat().st_mtime)
    if needs_build:
        log(f"[bootstrap] building {target.relative_to(ROOT_DIR)} ...")
        proc = subprocess.run(
            ["make", str(target.relative_to(ROOT_DIR))],
            cwd=ROOT_DIR,
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0 or not target.exists():
            fail(
                f"`make {target.relative_to(ROOT_DIR)}` failed (rc={proc.returncode})",
                extra={"stderr": proc.stderr[-2000:]},
            )
    return target


def compile_candidate(placeholder: str) -> tuple[bool, str, Path]:
    """Run `make nonmatching-func FUNC=<placeholder>`. Returns (ok, log_text, current_o)."""
    current_o = NONMATCHINGS_DIR / placeholder / "current.o"
    proc = subprocess.run(
        ["make", "nonmatching-func", f"FUNC={placeholder}"],
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
    )
    ok = (proc.returncode == 0) and current_o.exists()
    combined_log = (proc.stdout or "") + (proc.stderr or "")
    return ok, combined_log, current_o


def objdump_function(obj_path: Path, placeholder: str) -> str:
    """objdump one function from `obj_path` (raw text)."""
    cmd = [
        OBJDUMP,
        "-drz",
        "-m",
        "mips:4300",
        "--no-show-raw-insn",
        "--disassemble-zeroes",
        "--reloc",
        f"--disassemble={placeholder}",
        str(obj_path),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        fail(f"objdump failed on {obj_path}: {proc.stderr.strip()[:500]}")
    return proc.stdout


def run_asm_differ(base_dump: str, my_dump: str) -> dict:
    """Drive asm-differ's library API. Returns the raw PythonFormatter dict."""
    sys.path.insert(0, str(ASMDIFF_DIR))
    import diff as asm_differ  # type: ignore[import-not-found]

    arch = asm_differ.get_arch("mips")
    config = asm_differ.Config(
        arch=arch,
        # Build/objdump options (mirroring decomp.me's diff_wrapper.create_config)
        diff_obj=True,
        file="",
        make=False,
        source_old_binutils=True,
        diff_section=".text",
        inlines=False,
        max_function_size_lines=25000,
        max_function_size_bytes=25000 * 4,
        # Display options
        formatter=asm_differ.PythonFormatter(arch_str=arch.name),
        diff_mode=asm_differ.DiffMode.NORMAL,
        base_shift=0,
        skip_lines=0,
        compress=None,
        show_branches=True,
        show_line_numbers=False,
        show_source=False,
        stop_at_ret=False,
        ignore_large_imms=False,
        ignore_addr_diffs=True,
        algorithm="levenshtein",
        reg_categories={},
        show_rodata_refs=True,
        diff_function_symbols=False,
    )
    base_processed = asm_differ.preprocess_objdump_out(None, b"", base_dump, config)
    my_processed = asm_differ.preprocess_objdump_out(None, b"", my_dump, config)
    base_lines = asm_differ.process(base_processed, config)
    my_lines = asm_differ.process(my_processed, config)
    diff_output = asm_differ.do_diff(base_lines, my_lines, config)
    table = asm_differ.align_diffs(diff_output, diff_output, config)
    return config.formatter.raw(table)


def score_diff(raw: dict, max_mismatches: int = 5) -> dict:
    """Distill the asm-differ raw dict into a structured score + top mismatches."""
    rows = raw.get("rows", [])
    total = len(rows)
    match_count = 0
    mismatches: list[dict] = []
    for idx, row in enumerate(rows):
        # asm-differ's PythonFormatter row schema:
        #   row = {"key": ..., "base": {"text": [...], "kind": "..."}, "current": {...}, ...}
        # 'kind' values include 'DiffMatch', 'DiffDelete', 'DiffInsert', 'DiffMove', etc.
        base = row.get("base") or {}
        current = row.get("current") or {}
        base_kind = base.get("key") or base.get("kind") or ""
        cur_kind = current.get("key") or current.get("kind") or ""
        if base_kind == "" and cur_kind == "":
            continue
        is_match = (base_kind == cur_kind) and (base.get("text", []) == current.get("text", []))
        if is_match:
            match_count += 1
        elif len(mismatches) < max_mismatches:
            mismatches.append({
                "row_idx": idx,
                "base_kind": base_kind,
                "current_kind": cur_kind,
                "base_text": "".join(seg.get("text", "") for seg in base.get("text", []) if isinstance(seg, dict)) or str(base.get("text", "")),
                "current_text": "".join(seg.get("text", "") for seg in current.get("text", []) if isinstance(seg, dict)) or str(current.get("text", "")),
            })
    percent = (match_count / total) if total else 0.0
    score = 0 if (total > 0 and match_count == total) else (total - match_count)
    return {
        "score": score,
        "percent": percent,
        "total_rows": total,
        "match_count": match_count,
        "top_mismatches": mismatches,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True, help="func_XXXXXXXX or curated name")
    parser.add_argument(
        "--score-only",
        action="store_true",
        help="Skip mismatch detail; just report score/percent.",
    )
    args = parser.parse_args()

    asmdiff_available()

    placeholder = resolve_placeholder(args.func)
    seg_stem = find_segment(placeholder)
    log(f"[resolve] {args.func} -> {placeholder} (segment {seg_stem})")

    reference_o = ensure_reference_object(seg_stem)
    log(f"[reference] {reference_o.relative_to(ROOT_DIR)}")

    ok, compile_log, current_o = compile_candidate(placeholder)
    if not ok:
        emit({
            "compile_ok": False,
            "placeholder": placeholder,
            "segment": seg_stem,
            "compile_log": compile_log[-4000:],
        })
        sys.exit(0)
    log(f"[compile] {current_o.relative_to(ROOT_DIR)}")

    base_dump = objdump_function(reference_o, placeholder)
    my_dump = objdump_function(current_o, placeholder)
    log("[objdump] both objects disassembled")

    raw = run_asm_differ(base_dump, my_dump)
    scored = score_diff(raw, max_mismatches=0 if args.score_only else 5)
    log(f"[diff] score={scored['score']} percent={scored['percent']:.4f}")

    emit({
        "compile_ok": True,
        "placeholder": placeholder,
        "segment": seg_stem,
        "reference_path": str(reference_o.relative_to(ROOT_DIR)),
        "current_path": str(current_o.relative_to(ROOT_DIR)),
        **scored,
    })


if __name__ == "__main__":
    main()
