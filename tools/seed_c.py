#!/usr/bin/env python3
"""Generate `nonmatchings/<func>/base.c` for the execution loop.

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

import sys
from pathlib import Path

import decomp_common as dc

dc.reexec_into_venv(__file__)

import argparse
import importlib.util
import re
import subprocess
from dataclasses import dataclass

# Shared constants/helpers live in decomp_common (single source of truth);
# re-bound here so the rest of this module reads unchanged.
SCRIPT_DIR = dc.SCRIPT_DIR
ROOT_DIR = dc.ROOT_DIR
ASM_DIR = dc.ASM_DIR
ASM_DATA_DIR = dc.ASM_DATA_DIR
NONMATCHINGS_DIR = dc.NONMATCHINGS_DIR
PLACEHOLDER_RE = dc.PLACEHOLDER_RE
emit = dc.emit
log = dc.log
safe_read_text = dc.safe_read_text
capture_stderr = dc.capture_stderr

M2CTX_PATH = SCRIPT_DIR / "m2ctx.py"
M2C_PATH = SCRIPT_DIR / "m2c" / "m2c.py"

# seed_c-specific regexes (parent-scan + rodata-slicing).
HI_LO_REF_RE = re.compile(r"%(?:hi|lo)\(([A-Za-z_][\w.]*)\)")
INCLUDE_ASM_RE = re.compile(r"INCLUDE_ASM\([^,]+,\s*([A-Za-z_][\w]*)\s*\)\s*;")
EXTERN_LINE_RE = re.compile(r"^\s*extern\s+[^;]+;\s*$")
DATA_LABEL_RE = re.compile(r"^\s*([A-Za-z_][\w.]*)\s*:\s*$")
DLABEL_RE = re.compile(r"^\s*dlabel\s+([A-Za-z_][\w.]*)\s*$")
ENDDLABEL_RE = re.compile(r"^\s*enddlabel\b")
WORD_SYM_RE = re.compile(r"\.word\s+([A-Za-z_][\w.]*)\s*$")


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


def slice_target_asm(placeholder: str, seg_stem: str, out_dir: Path) -> Path:
    """Extract the formatted slice for <placeholder> into out_dir/target.s."""
    extract_mod = load_extract_functions()
    seg_path = ASM_DIR / f"{seg_stem}.s"
    if not seg_path.exists():
        fail(f"missing asm source: {seg_path}")
    content = safe_read_text(seg_path)
    for func_name, func_text in extract_mod.extract_functions(content):
        if func_name == placeholder:
            target_path = out_dir / "target.s"
            target_path.write_text(func_text + "\n", encoding="utf-8")
            return target_path
    fail(
        f"`{placeholder}` listed in {seg_path} but extract_functions did not return it"
    )
    raise SystemExit(1)


def collect_hi_lo_labels(target_asm_path: Path) -> set[str]:
    """Return all labels referenced via %hi/%lo in the target asm."""
    text = safe_read_text(target_asm_path)
    return set(HI_LO_REF_RE.findall(text))


def slice_rodata(
    labels: set[str], out_path: Path
) -> tuple[Path | None, list[str], set[str]]:
    """Extract each label's contents from asm/data/*.{rodata,data}.s.

    Recognizes splat's `dlabel <name>` ... `enddlabel <name>` form (what
    spimdisasm emits for this project) in addition to C-style `name:` labels.

    Slices `.rodata` content into out_path (for m2c). For `.data` labels,
    records presence but does NOT slice — m2c doesn't want writable data in
    rodata, and the type info we glean from the body is enough.

    Returns (path_or_None, missing_labels, pointer_labels).
      - missing_labels: labels not found in ANY asm/data/*.s file (rodata or
        data). Labels found in `.data` are NOT considered missing.
      - pointer_labels: labels whose body is exactly one `.word <SYMBOL>` —
        a relocation, not a hex immediate. These are almost certainly
        pointer-typed globals, used by auto_externs_for_hi_lo to emit
        `extern void *<sym>;` instead of `extern u32 <sym>;`.
    """
    if not labels:
        return None, [], set()
    if not ASM_DATA_DIR.exists():
        return None, sorted(labels), set()

    found_rodata: dict[str, list[str]] = {}
    found_anywhere: set[str] = set()
    pointer_labels: set[str] = set()

    for data_file in sorted(ASM_DATA_DIR.glob("*.s")):
        is_rodata = ".rodata." in data_file.name
        try:
            lines = safe_read_text(data_file).splitlines()
        except OSError:
            continue
        i = 0
        while i < len(lines):
            line = lines[i]
            dl = DLABEL_RE.match(line)
            cl = DATA_LABEL_RE.match(line) if dl is None else None
            label = (dl or cl).group(1) if (dl or cl) else None
            if label is None or label not in labels:
                i += 1
                continue
            found_anywhere.add(label)
            chunk: list[str] = [line]
            body_lines: list[str] = []
            i += 1
            while i < len(lines):
                nxt = lines[i]
                if ENDDLABEL_RE.match(nxt):
                    chunk.append(nxt)
                    i += 1
                    break
                if DLABEL_RE.match(nxt) or DATA_LABEL_RE.match(nxt):
                    break
                stripped = nxt.lstrip()
                if stripped.startswith(".section") or stripped.startswith("glabel"):
                    break
                chunk.append(nxt)
                body_lines.append(nxt)
                i += 1
            # Pointer detection: exactly one `.word` line in the body, and
            # its operand is a symbol (not a hex immediate).
            word_lines = [b for b in body_lines if ".word" in b]
            if len(word_lines) == 1 and WORD_SYM_RE.search(word_lines[0]):
                pointer_labels.add(label)
            if is_rodata:
                found_rodata[label] = chunk

    missing = sorted(set(labels) - found_anywhere)

    if found_rodata:
        out_lines = [".section .rodata", ""]
        for label in labels:
            if label in found_rodata:
                out_lines.extend(found_rodata[label])
                out_lines.append("")
        out_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")
        return out_path, missing, pointer_labels
    return None, missing, pointer_labels


def slice_rodata_phase(
    target_asm: Path, out_dir: Path, no_rodata: bool
) -> tuple[Path | None, list[str], set[str]]:
    """Best-effort rodata slice for the target's %hi/%lo labels.

    Returns (rodata_path_or_None, missing_labels, pointer_labels). When
    --no-rodata is set or no labels are referenced, yields the empty result.
    """
    rodata_path: Path | None = None
    missing_rodata: list[str] = []
    pointer_labels: set[str] = set()
    if not no_rodata:
        labels = collect_hi_lo_labels(target_asm)
        log(f"[seed] {len(labels)} %hi/%lo labels referenced")
        if labels:
            rodata_path, missing_rodata, pointer_labels = slice_rodata(
                labels, out_dir / "target.rodata.s"
            )
            if rodata_path is not None:
                log(
                    f"[seed] target.rodata.s written ({len(labels) - len(missing_rodata)} labels)"
                )
            if pointer_labels:
                log(f"[seed] pointer-typed globals: {len(pointer_labels)}")
            if missing_rodata:
                log(f"[seed] WARN: {len(missing_rodata)} labels not found in asm/data/")
    return rodata_path, missing_rodata, pointer_labels


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
        log(
            f"[seed] m2ctx.py failed (rc={proc.returncode}); continuing without --context"
        )
        log(f"[seed] stderr: {capture_stderr(proc)}")
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
        log(f"[seed] stderr: {capture_stderr(proc)}")
        out_path.write_text(
            f"/* m2c failed; stderr:\n{capture_stderr(proc, 2000)}\n*/\n",
            encoding="utf-8",
        )
        return False
    out_path.write_text(proc.stdout, encoding="utf-8")
    return True


# Ghidra "undefined*" / generic names → ultra64.h equivalents.
# Use the project's typed aliases; never let int/long/etc. through.
GHIDRA_TYPE_MAP = {
    r"\bundefined8\b": "u64",
    r"\bundefined4\b": "u32",
    r"\bundefined2\b": "u16",
    r"\bundefined1\b": "u8",
    r"\bundefined\b": "u8",
    r"\bulonglong\b": "u64",
    r"\blonglong\b": "s64",
    r"\bulong\b": "u32",
    r"\buint\b": "u32",
    r"\bushort\b": "u16",
    r"\bsbyte\b": "s8",
    r"\bbyte\b": "u8",
    r"\bword\b": "u16",
    r"\bdword\b": "u32",
    r"\bqword\b": "u64",
}

# Hardware registers are declared in <PR/rcp.h> (pulled in via <ultra64.h>);
# do not auto-extern these. Conservative list — anything ending in _REG plus a
# few unsuffixed standards.
HW_REG_NAMES = {
    "AI_STATUS_REG",
    "AI_DRAM_ADDR_REG",
    "AI_LEN_REG",
    "AI_CONTROL_REG",
    "AI_DACRATE_REG",
    "AI_BITRATE_REG",
    "DPC_STATUS_REG",
    "DPC_START_REG",
    "DPC_END_REG",
    "DPC_CURRENT_REG",
    "DPC_CLOCK_REG",
    "DPC_BUFBUSY_REG",
    "DPC_PIPEBUSY_REG",
    "DPC_TMEM_REG",
    "SI_STATUS_REG",
    "SI_DRAM_ADDR_REG",
    "SI_PIF_ADDR_RD64B_REG",
    "SI_PIF_ADDR_WR64B_REG",
    "MI_MODE_REG",
    "MI_VERSION_REG",
    "MI_INTR_REG",
    "MI_INTR_MASK_REG",
    "VI_STATUS_REG",
    "VI_DRAM_ADDR_REG",
    "VI_WIDTH_REG",
    "VI_INTR_REG",
    "VI_CURRENT_REG",
    "VI_BURST_REG",
    "VI_V_SYNC_REG",
    "VI_H_SYNC_REG",
    "VI_LEAP_REG",
    "VI_H_START_REG",
    "VI_V_START_REG",
    "VI_V_BURST_REG",
    "VI_X_SCALE_REG",
    "VI_Y_SCALE_REG",
    "PI_STATUS_REG",
    "PI_DRAM_ADDR_REG",
    "PI_CART_ADDR_REG",
    "PI_RD_LEN_REG",
    "PI_WR_LEN_REG",
    "PI_BSD_DOM1_LAT_REG",
    "PI_BSD_DOM1_PWD_REG",
    "PI_BSD_DOM1_PGS_REG",
    "PI_BSD_DOM1_RLS_REG",
    "PI_BSD_DOM2_LAT_REG",
    "PI_BSD_DOM2_PWD_REG",
    "PI_BSD_DOM2_PGS_REG",
    "PI_BSD_DOM2_RLS_REG",
    "SP_STATUS_REG",
    "SP_MEM_ADDR_REG",
    "SP_DRAM_ADDR_REG",
    "SP_RD_LEN_REG",
    "SP_WR_LEN_REG",
    "SP_DMA_FULL_REG",
    "SP_DMA_BUSY_REG",
    "SP_SEMAPHORE_REG",
    "SP_PC_REG",
    "SP_IBIST_REG",
    "AI_STATUS",
    "DPC_STATUS",
    "SI_STATUS",
    "MI_STATUS",  # Ghidra often drops _REG
}

GHIDRA_HEADER_RE = re.compile(r"^/\*\s*\[MM\d+\][^*]*\*+/\s*$", re.MULTILINE)


def sanitize_ghidra_body(text: str, placeholder: str) -> str:
    """Make a Ghidra decompile suitable for KMC GCC compilation.

    - Map Ghidra synthetic types (`undefined8`, `byte`, `ulonglong`, …) to the
      project's ultra64.h aliases (`u64`, `u8`, `s64`, …).
    - Rename the declared function symbol to <placeholder> (Ghidra often emits
      a curated name like `returns_0` — decomp is authoritative, but only AFTER
      the match lands; until then base.c needs the placeholder so symbol
      resolution against build/asm/<seg>.o lines up).
    - Strip the leading `/* [MMxx] copied from ELF … */` Ghidra audit header.
    """
    # Strip Ghidra audit header(s).
    text = GHIDRA_HEADER_RE.sub("", text)

    # Type substitutions.
    for pattern, replacement in GHIDRA_TYPE_MAP.items():
        text = re.sub(pattern, replacement, text)

    # Rename the function symbol. Look for `<retty> <name>(...)` (1-line decl)
    # or `<retty>\n<name>(...)` (Ghidra's 2-line style).
    # The placeholder MUST appear as the function name in base.c so that
    # objdump --disassemble=<placeholder> finds it.
    def _rename(match: re.Match[str]) -> str:
        return match.group(1) + placeholder + match.group(3)

    # Match: identifier( before optional whitespace + opening paren, where the
    # identifier is preceded by a return type (last whitespace-separated token
    # on the previous non-empty stretch). Conservative: only rename if the
    # candidate name is followed by `(` and the line looks like a function
    # definition (not a call).
    func_def_re = re.compile(
        r"((?:^|\n)\s*(?:[A-Za-z_][\w\s\*]*?\s|\*\s*))"  # return-type group
        r"([A-Za-z_]\w*)"  # candidate name
        r"(\s*\([^;]*?\)\s*\{)",  # (...) {
        re.MULTILINE,
    )
    text = func_def_re.sub(_rename, text, count=1)
    return text.strip() + "\n"


def parent_has_real_c(parent: Path) -> bool:
    """True if the parent .c holds C beyond #includes and INCLUDE_ASM stubs.

    A freshly-flipped 1-stub file (the common classical-loop start) has no type
    context for m2ctx to extract, so running cpp over it only yields a noisy
    failure — detect it and skip m2ctx, falling to the Ghidra seed cleanly.
    """
    if parent is None or not parent.exists():
        return False
    in_block_comment = False
    for raw in safe_read_text(parent).splitlines():
        line = raw.strip()
        if in_block_comment:
            if "*/" in line:
                in_block_comment = False
            continue
        if not line or line.startswith("//"):
            continue
        if line.startswith("/*"):
            if "*/" not in line:
                in_block_comment = True
            continue
        if line.startswith("#"):  # preprocessor (incl. #include)
            continue
        if INCLUDE_ASM_RE.search(line):  # an asm stub, not real C
            continue
        return True  # anything else is real C
    return False


def collect_parent_externs(parent: Path) -> tuple[list[str], list[str]]:
    """Return (extern_decls, include_asm_names) from the parent .c."""
    if parent is None or not parent.exists():
        return [], []
    externs: list[str] = []
    asm_names: list[str] = []
    for line in safe_read_text(parent).splitlines():
        if EXTERN_LINE_RE.match(line):
            externs.append(line.rstrip())
            continue
        m = INCLUDE_ASM_RE.search(line)
        if m:
            asm_names.append(m.group(1))
    return externs, asm_names


def auto_externs_for_hi_lo(
    labels: set[str],
    placeholder: str,
    sibling_asm_names: list[str],
    parent_externs: list[str],
    pointer_labels: set[str] | None = None,
) -> list[str]:
    """Emit fallback `extern` decls for %hi/%lo labels not yet declared.

    Skips: the placeholder itself, sibling INCLUDE_ASM names (already declared
    elsewhere in this seed), names already in parent externs, known hardware
    registers (declared by <PR/rcp.h>), and rodata jtbl_* labels emitted
    inline.

    `pointer_labels` carries the output of slice_rodata's pointer-detection
    pass: labels whose body is a single `.word <SYMBOL>`. These get
    `extern void *<sym>;` (no TODO comment — pointer-ness is solid signal).
    Everything else still gets `extern u32 <sym>; // TODO: refine ...`.
    """
    pointer_labels = pointer_labels or set()
    already = set(sibling_asm_names) | {placeholder} | HW_REG_NAMES
    for line in parent_externs:
        for tok in re.findall(r"\b([A-Za-z_]\w*)\b", line):
            already.add(tok)

    out: list[str] = []
    for label in sorted(labels):
        if label in already:
            continue
        if label.startswith("jtbl_"):
            # Jump tables: declare as void* table; agent likely needs to add
            # an extern_syms entry instead, but this at least compiles.
            out.append(f"extern void *{label}[]; // TODO: confirm jtbl type")
        elif label.startswith("func_") and re.match(r"^func_[0-9A-Fa-f]{8}$", label):
            out.append(f"extern void {label}(void); // TODO: refine signature from MCP")
        elif label.startswith("D_") and re.match(r"^D_[0-9A-Fa-f]{8}$", label):
            if label in pointer_labels:
                out.append(f"extern void *{label};")
            else:
                out.append(f"extern u32 {label}; // TODO: refine type from MCP")
        else:
            out.append(f"extern char {label}[]; // TODO: refine from MCP/parent header")
    return out


@dataclass
class BaseCSpec:
    """The assembled inputs for stitch_base_c (groups what main accumulates over
    its rodata/context/extern phases), replacing an 8-argument call."""

    out_dir: Path
    placeholder: str
    externs: list[str]
    sibling_asm_names: list[str]
    auto_externs: list[str]
    m2c_path: Path
    ghidra_path: Path
    missing_rodata: list[str]


def stitch_base_c(spec: BaseCSpec) -> Path:
    """Assemble base.c per Section 4 step 6 of the plan."""
    out_dir = spec.out_dir
    placeholder = spec.placeholder
    externs = spec.externs
    sibling_asm_names = spec.sibling_asm_names
    auto_externs = spec.auto_externs
    m2c_path = spec.m2c_path
    ghidra_path = spec.ghidra_path
    missing_rodata = spec.missing_rodata
    lines: list[str] = []
    lines.append("// Generated by tools/seed_c.py for the execution loop.")
    lines.append("// Edit freely; this is a seed, not the final answer.")
    lines.append("")
    lines.append("#include <ultra64.h>")
    lines.append(
        "// DLIST_HINT: if this function builds/walks Gfx*, add `#include <PR/gbi.h>`"
    )
    lines.append(
        "//             and run gfxdis on any static dlists in target.rodata.s."
    )
    lines.append("")

    if missing_rodata:
        lines.append("// SEED WARNING: rodata labels not found in asm/data/:")
        for lbl in missing_rodata[:10]:
            lines.append(f"//   - {lbl}")
        if len(missing_rodata) > 10:
            lines.append(f"//   ...and {len(missing_rodata) - 10} more")
        lines.append("")

    if externs or sibling_asm_names or auto_externs:
        lines.append("// === Externs from parent file ===")
        for e in externs:
            lines.append(e)
        for name in sibling_asm_names:
            if name == placeholder:
                continue
            lines.append(
                f"extern void {name}(void); // TODO: refine signature from MCP"
            )
        if auto_externs:
            lines.append("// --- auto-extern (refine types during iteration) ---")
            for e in auto_externs:
                lines.append(e)
        lines.append("")

    if m2c_path.exists():
        lines.append(
            "/* === m2c reference (do not compile) =========================== */"
        )
        lines.append("#if 0")
        for ml in safe_read_text(m2c_path).splitlines():
            lines.append(ml)
        lines.append("#endif")
        lines.append(
            "/* === end m2c reference ========================================= */"
        )
        lines.append("")

    lines.append("// === Function body ============================================")
    if ghidra_path.exists():
        body = safe_read_text(ghidra_path)
        body = sanitize_ghidra_body(body, placeholder).rstrip()
        lines.append(body)
    else:
        lines.append(
            f"// TODO: agent — paste Ghidra MCP `decompile_function` output for {placeholder}"
        )
        lines.append(
            "// here, then rename m2c synthetics per CLAUDE.md naming conventions."
        )
        lines.append(f"void {placeholder}(void) {{")
        lines.append("    /* unimplemented */")
        lines.append("}")
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
        fail(
            f"--func must be a func_XXXXXXXX placeholder or a valid C identifier; got {args.func!r}."
        )
    placeholder = args.func

    out_dir = NONMATCHINGS_DIR / placeholder
    out_dir.mkdir(parents=True, exist_ok=True)
    log(f"[seed] working dir: {out_dir.relative_to(ROOT_DIR)}")

    seg_stem = dc.find_segment(placeholder)
    if seg_stem is None:
        fail(f"no `glabel {placeholder}` found in any asm/*.s file")
    log(f"[seed] segment: {seg_stem}")

    target_asm = slice_target_asm(placeholder, seg_stem, out_dir)
    log("[seed] target.s written")

    rodata_path, missing_rodata, pointer_labels = slice_rodata_phase(
        target_asm, out_dir, args.no_rodata
    )

    # m2ctx (best-effort)
    ctx_path = out_dir / "ctx.c"
    has_ctx = False
    if args.parent:
        parent_path = Path(args.parent)
        if not parent_path.exists():
            log(f"[seed] --parent {args.parent} not found; skipping m2ctx")
        elif not parent_has_real_c(parent_path):
            log(
                "[seed] parent has only INCLUDE_ASM stubs; skipping m2ctx "
                "(no type context) — relying on the Ghidra seed"
            )
        else:
            has_ctx = run_m2ctx(parent_path, ctx_path)

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
            log(
                f"[seed] parent externs: {len(externs)}  sibling INCLUDE_ASM: {len(sibling_asm_names)}"
            )

    # Ghidra body (written by slash command, not this script)
    ghidra_path = out_dir / "ghidra.c"

    # Auto-extern any %hi/%lo label not already covered by parent externs or
    # sibling INCLUDE_ASM. Type defaults to `char` — agent refines.
    hi_lo_labels = collect_hi_lo_labels(target_asm)
    auto_externs = auto_externs_for_hi_lo(
        hi_lo_labels, placeholder, sibling_asm_names, externs, pointer_labels
    )
    if auto_externs:
        log(f"[seed] auto-extern: {len(auto_externs)} %hi/%lo label(s)")

    base_path = stitch_base_c(
        BaseCSpec(
            out_dir=out_dir,
            placeholder=placeholder,
            externs=externs,
            sibling_asm_names=sibling_asm_names,
            auto_externs=auto_externs,
            m2c_path=m2c_path,
            ghidra_path=ghidra_path,
            missing_rodata=missing_rodata,
        )
    )
    log(f"[seed] {base_path.relative_to(ROOT_DIR)} written")

    emit(
        {
            "ok": True,
            "placeholder": placeholder,
            "segment": seg_stem,
            "out_dir": str(out_dir.relative_to(ROOT_DIR)),
            "base_c": str(base_path.relative_to(ROOT_DIR)),
            "target_s": str(target_asm.relative_to(ROOT_DIR)),
            "target_rodata_s": str(rodata_path.relative_to(ROOT_DIR))
            if rodata_path
            else None,
            "missing_rodata_labels": missing_rodata,
            "ctx_c": str(ctx_path.relative_to(ROOT_DIR)) if has_ctx else None,
            "m2c_c": str(m2c_path.relative_to(ROOT_DIR)),
            "ghidra_c_expected": str(ghidra_path.relative_to(ROOT_DIR)),
            "ghidra_c_present": ghidra_path.exists(),
            "extern_count": len(externs),
            "sibling_asm_count": len(sibling_asm_names),
        }
    )


if __name__ == "__main__":
    main()
