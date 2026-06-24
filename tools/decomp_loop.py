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

import os
import sys
from pathlib import Path

import decomp_common as dc

dc.reexec_into_venv(__file__)

import argparse
import re
import subprocess

# Shared constants/helpers live in decomp_common (single source of truth);
# re-bound here so the rest of this module reads unchanged.
SCRIPT_DIR = dc.SCRIPT_DIR
ROOT_DIR = dc.ROOT_DIR
ASM_DIR = dc.ASM_DIR
BUILD_ASM_DIR = dc.BUILD_ASM_DIR
NONMATCHINGS_DIR = dc.NONMATCHINGS_DIR
SYMBOL_FILES = dc.SYMBOL_FILES
PLACEHOLDER_RE = dc.PLACEHOLDER_RE
SYM_LINE_RE = dc.SYM_LINE_RE
emit = dc.emit
log = dc.log
safe_read_text = dc.safe_read_text
capture_stderr = dc.capture_stderr

ASMDIFF_DIR = SCRIPT_DIR / "asm-differ"
OBJDUMP = os.environ.get("OBJDUMP", "mips-linux-gnu-objdump")


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
    """Return the label name to drive the loop against.

    Accepts either a func_XXXXXXXX placeholder, or a curated name that
    appears in symbol_addrs.txt / ghidra_symbols.txt. Splat uses the
    curated name verbatim in asm output (`glabel rand`, not
    `glabel func_800B3220`), so for curated inputs we return the name
    unchanged after confirming it is known. Does NOT call Ghidra MCP;
    that's the slash command's job upstream.
    """
    if PLACEHOLDER_RE.match(name):
        return name
    for f in SYMBOL_FILES:
        if not f.exists():
            continue
        for line in safe_read_text(f).splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            m = SYM_LINE_RE.match(stripped)
            if m and m.group(1) == name:
                return name
    fail(
        f"could not resolve '{name}' to a known function; not present in "
        f"{[str(f) for f in SYMBOL_FILES]} and not in placeholder form. "
        "Run the execution loop (which queries Ghidra MCP) or pass the placeholder directly."
    )
    raise SystemExit(1)  # unreachable; for type checker


def ensure_reference_object(seg_stem: str) -> Path:
    """Build build/asm/<seg>.o if missing or stale. Returns its path."""
    target = BUILD_ASM_DIR / f"{seg_stem}.o"
    src = ASM_DIR / f"{seg_stem}.s"
    if not src.exists():
        fail(f"asm source file missing: {src}")
    needs_build = (not target.exists()) or (
        target.stat().st_mtime < src.stat().st_mtime
    )
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


LIBKMC_SRC = dc.LIBKMC_SRC
LIBULTRA_SRC = dc.LIBULTRA_SRC


def _detect_in_upstream(src_root: Path, placeholder: str, *, recursive: bool) -> bool:
    """True if <placeholder> is defined as a function in `src_root`'s .c files.

    Shared by the libkmc/libultra profile detectors; `recursive` selects rglob
    (libultra's nested io/os/audio tree) vs glob (libkmc's flat src). Matches a
    K&R `int rand()` or a prototype `int rand(void)` at line start.
    """
    if not src_root.exists():
        return False
    pat = re.compile(
        rf"^[A-Za-z_][A-Za-z0-9_ *]*\b{re.escape(placeholder)}\s*\(",
        re.MULTILINE,
    )
    files = src_root.rglob("*.c") if recursive else src_root.glob("*.c")
    for f in files:
        try:
            text = safe_read_text(f)
        except OSError:
            continue
        if pat.search(text):
            return True
    return False


def detect_libkmc_profile(placeholder: str) -> bool:
    """True if <placeholder> appears as a function definition in libkmc upstream.

    libkmc was built with `gcc -O` (not -O2); the project Makefile carves a
    libkmc-only compile profile via LIBKMC_CFLAGS. The loop must mirror that
    to produce ground-truth bytes — see CLAUDE.md "libkmc compile profile is `-O`".
    """
    return _detect_in_upstream(LIBKMC_SRC, placeholder, recursive=False)


def detect_libultra_profile(placeholder: str) -> bool:
    """True if <placeholder> appears as a function definition in ultralib upstream.

    libultra was built with -O3 -funsigned-char -DBUILD_VERSION=VERSION_J
    (ultralib gcc.mk, VERSION_J libgultra_rom). The loop must mirror LIBULTRA_CFLAGS
    to produce ground-truth bytes — affects inlining and delay-slot scheduling.
    """
    return _detect_in_upstream(LIBULTRA_SRC, placeholder, recursive=True)


def compile_candidate(
    placeholder: str, libkmc: bool, libultra: bool = False
) -> tuple[bool, str, Path]:
    """Run `make nonmatching-func FUNC=<placeholder> [LIBKMC=1|LIBULTRA=1]`."""
    current_o = NONMATCHINGS_DIR / placeholder / "current.o"
    cmd = ["make", "nonmatching-func", f"FUNC={placeholder}"]
    if libkmc:
        cmd.append("LIBKMC=1")
    elif libultra:
        cmd.append("LIBULTRA=1")
    proc = subprocess.run(
        cmd,
        cwd=ROOT_DIR,
        capture_output=True,
        text=True,
    )
    ok = (proc.returncode == 0) and current_o.exists()
    combined_log = (proc.stdout or "") + (proc.stderr or "")
    return ok, combined_log, current_o


def objdump_function(obj_path: Path, placeholder: str) -> str:
    """objdump one function from `obj_path` (raw text).

    Flags mirror decomp.me / asm-differ's expected objdump input:
      -drz                  disassemble, show raw, intermix zeroes
      -m mips:4300          force the N64 CPU (objects carry no arch tag)
      --no-show-raw-insn    drop opcode hex so asm-differ keys on mnemonics
      --disassemble-zeroes  keep `.word 0` padding (don't collapse runs)
      --reloc               emit relocations so symbol refs are comparable
      --disassemble=<fn>    only the target function
    """
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
        fail(f"objdump failed on {obj_path}: {capture_stderr(proc)}")
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
    """Distill the asm-differ raw dict into a structured score + top mismatches.

    asm-differ already computes `current_score` (0 = perfect) and `max_score`
    in the raw PythonFormatter dict; mirror those directly. Rows are compared
    by `row["key"]` (canonical pre-normalized form) — when base/current keys
    match, the instructions are equivalent modulo whitespace.
    """
    rows = raw.get("rows", [])
    total = len(rows)
    match_count = 0
    mismatches: list[dict] = []
    for idx, row in enumerate(rows):
        base = row.get("base") or {}
        current = row.get("current") or {}
        if not base and not current:
            continue
        # asm-differ marks identity via matching `key` on both sides.
        is_match = (
            bool(base) and bool(current) and (base.get("key") == current.get("key"))
        )
        if is_match:
            match_count += 1
        elif len(mismatches) < max_mismatches:

            def _flat(d):
                t = d.get("text", []) if isinstance(d, dict) else []
                if isinstance(t, list):
                    return "".join(
                        seg.get("text", "") if isinstance(seg, dict) else str(seg)
                        for seg in t
                    )
                return str(t)

            mismatches.append(
                {
                    "row_idx": idx,
                    "base_text": _flat(base),
                    "current_text": _flat(current),
                }
            )
    current_score = int(raw.get("current_score", -1))
    max_score = int(raw.get("max_score", 0))
    # Prefer asm-differ's own score; fall back to row-counting if unavailable.
    if current_score >= 0:
        score = current_score
        percent = (
            1.0 - (current_score / max_score)
            if max_score > 0
            else (1.0 if total else 0.0)
        )
    else:
        score = 0 if (total > 0 and match_count == total) else (total - match_count)
        percent = (match_count / total) if total else 0.0
    return {
        "score": score,
        "percent": percent,
        "total_rows": total,
        "match_count": match_count,
        "max_score": max_score,
        "top_mismatches": mismatches,
    }


def resolve_profile(profile: str, placeholder: str) -> tuple[bool, bool]:
    """Resolve the compile profile to (libkmc, libultra) flags.

    `auto` detects by upstream-src presence; the explicit choices force a
    profile. Mirrors the Makefile's per-library CFLAGS so the candidate object
    reflects ground-truth bytes (libkmc -O, libultra -O3 -funsigned-char).
    """
    if profile == "libkmc":
        libkmc, libultra = True, False
    elif profile == "libultra":
        libkmc, libultra = False, True
    elif profile == "default":
        libkmc, libultra = False, False
    else:
        libkmc = detect_libkmc_profile(placeholder)
        libultra = (not libkmc) and detect_libultra_profile(placeholder)
    if libkmc:
        log(f"[profile] libkmc (-O) — placeholder found in {LIBKMC_SRC}")
    elif libultra:
        log(
            f"[profile] libultra (-O3 -funsigned-char) — placeholder found in {LIBULTRA_SRC}"
        )
    return libkmc, libultra


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--func", required=True, help="func_XXXXXXXX or curated name")
    parser.add_argument(
        "--score-only",
        action="store_true",
        help="Skip mismatch detail; just report score/percent.",
    )
    parser.add_argument(
        "--profile",
        choices=["auto", "libkmc", "libultra", "default"],
        default="auto",
        help="Compile profile. auto = detect by upstream-src presence; "
        "libkmc = force -O; libultra = force -O3 -funsigned-char; default = force -O2.",
    )
    args = parser.parse_args()

    asmdiff_available()

    placeholder = resolve_placeholder(args.func)
    seg_stem = dc.find_segment(placeholder)
    if seg_stem is None:
        fail(f"no `glabel {placeholder}` found in any asm/*.s file")
    log(f"[resolve] {args.func} -> {placeholder} (segment {seg_stem})")

    reference_o = ensure_reference_object(seg_stem)
    log(f"[reference] {reference_o.relative_to(ROOT_DIR)}")

    libkmc, libultra = resolve_profile(args.profile, placeholder)

    ok, compile_log, current_o = compile_candidate(placeholder, libkmc, libultra)
    if not ok:
        emit(
            {
                "compile_ok": False,
                "placeholder": placeholder,
                "segment": seg_stem,
                "compile_log": compile_log[-4000:],
            }
        )
        sys.exit(0)
    log(f"[compile] {current_o.relative_to(ROOT_DIR)}")

    base_dump = objdump_function(reference_o, placeholder)
    my_dump = objdump_function(current_o, placeholder)
    log("[objdump] both objects disassembled")

    raw = run_asm_differ(base_dump, my_dump)
    scored = score_diff(raw, max_mismatches=0 if args.score_only else 5)
    log(f"[diff] score={scored['score']} percent={scored['percent']:.4f}")

    emit(
        {
            "compile_ok": True,
            "placeholder": placeholder,
            "segment": seg_stem,
            "reference_path": str(reference_o.relative_to(ROOT_DIR)),
            "current_path": str(current_o.relative_to(ROOT_DIR)),
            **scored,
        }
    )


if __name__ == "__main__":
    main()
