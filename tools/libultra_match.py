#!/usr/bin/env python3
"""Bounded per-candidate opcode matcher: identify which unmapped `asm` subsegs are libultra.

Deliverable 2 of the "surface remaining libultra targets" work (plan
vivid-snuggling-pearl). pick_target's `--lib libultra` now surfaces the *named* libultra asm
blocks (those whose primary resolves through build_upstream_index / build_asm_tu_index). This
tool closes the gap for the *unnamed* `func_<vram>` blocks: it opcode-matches every function in
every `asm` subseg against the compiled VERSION_J `libgultra_rom.a`, and emits an add-only
`symbol_addrs.txt` worklist for the matches that are still un-named.

Why a bounded matcher instead of coddog `compare-raw`: compare-raw skips every function under 20
instructions (44.7% of the archive — exactly the leaf intrinsics we want) and drops symbols whose
first-20-opcode window collides. This matcher has no length floor and no collision loss: it walks
the ~candidate `asm/<rom>.s` listings (function boundaries + vram + raw words are already in the
file) and compares full opcode sequences.

Normalization: each 32-bit word is decoded by rabbitizer (the same disassembler splat uses) to its
opcode name; the per-function signature is the opcode-name sequence. This ignores all operands, so
relocations (jal targets, %hi/%lo immediates) that differ between the unrelinked archive and the
relocated ROM do not break a match. The trade-off is that two functions with identical
instruction-type sequences but different register/immediate usage can false-match; the worklist is
reviewed at a `/sprint-plan` gate before any symbol is added.

#7 guard: the prebuilt archive was built with a different gcc binary than the project's
`tools/cc/gcc`. Before trusting matches, the tool calibrates against compiled-C libultra functions
that are still present as asm (e.g. osContInit) and confirms they opcode-match the archive. A low
calibration rate means the reference gcc diverges — rebuild the archive from source with the
project gcc + LIBULTRA_CFLAGS before trusting the worklist.

Read-only. Usage: venv/bin/python3 tools/libultra_match.py [--min-insns N] [--all]
"""

import argparse
import functools
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decomp_common  # noqa: E402

decomp_common.reexec_into_venv(os.path.abspath(__file__))

import rabbitizer  # noqa: E402  (after venv re-exec)
import pick_target as pt  # noqa: E402  (parse_subsegs / upstream + asm-tu indices / ROOT)

# --- config (derived from decomp_common's libultra source pin) ------------------------------
# Reference = the UNION of several libgultra_rom builds, not just J. The project pins
# BUILD_VERSION=VERSION_J for macro gating, but the *shipped* libultra in the ROM opcode-matches
# the J+K source variants better than J alone (measured: J 29/50, K 32/44, J+K union 36/50 of the
# compiled-C calibration set — K uniquely recovers the libc/printf family strlen/strchr/_Printf/
# _Litob, J uniquely recovers __osSetTimerIntr). The official n64sdk-J `libgultra_rom.a` is
# byte-equivalent to ultralib's build/J, so it adds nothing; L≈K. Union maximizes recall without
# raising worklist ambiguity. Override with MG_LIBULTRA_ARCHIVES (os.pathsep-separated).
_ULTRALIB_ROOT = decomp_common.LIBULTRA_SRC.parent
_DEFAULT_ARCHIVES = [
    str(_ULTRALIB_ROOT / "build" / v / "libgultra_rom" / "libgultra_rom.a")
    for v in ("J", "K")
]
ARCHIVES = [
    a for a in os.environ.get("MG_LIBULTRA_ARCHIVES", "").split(os.pathsep) if a
] or _DEFAULT_ARCHIVES
# src-tree for member->src mapping: any present build/<v>/libgultra_rom/src (member names match).
SRC_OBJ_TREE = next(
    (
        str(_ULTRALIB_ROOT / "build" / v / "libgultra_rom" / "src")
        for v in ("J", "K", "L", "I")
        if (_ULTRALIB_ROOT / "build" / v / "libgultra_rom" / "src").is_dir()
    ),
    str(_ULTRALIB_ROOT / "build" / "J" / "libgultra_rom" / "src"),
)
OBJDUMP = os.environ.get("MG_MIPS_OBJDUMP", "mips-linux-gnu-objdump")

# asm/<rom>.s instruction line: /* ROM VRAM WORD */  mnemonic ...
_ASM_LINE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]{8})\s*\*/")
_GLABEL = re.compile(r"^\s*glabel\s+(\S+)")
# objdump -d: member header / function header / instruction line.
_OD_MEMBER = re.compile(r"^(\S+\.o):\s+file format")
_OD_FUNC = re.compile(r"^[0-9a-f]+ <([^>]+)>:")
_OD_INSN = re.compile(r"^\s+[0-9a-f]+:\t([0-9a-f]{8})\s")


def opcode_sig(words):
    """Opcode-name sequence for a list of 32-bit instruction words (operands ignored)."""
    return tuple(rabbitizer.Instruction(w).getOpcodeName() for w in words)


# --- reference side: libultra archive -------------------------------------------------------
def build_reference(archives=None):
    """Union over archives: name -> [(sig, member)], sig -> [(name, member)] (distinct names)."""
    archives = archives or ARCHIVES
    by_name, by_sig = {}, {}
    seen_sig_name = (
        set()
    )  # dedup (sig,name) so the same fn across versions isn't double-listed
    for archive in archives:
        proc = subprocess.run([OBJDUMP, "-d", archive], capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit(f"objdump failed on {archive}:\n{proc.stderr}")
        member, name, words = None, None, []

        def flush():
            nonlocal member, name, words
            if name is not None and words:
                sig = opcode_sig(words)
                by_name.setdefault(name, []).append((sig, member))
                if (sig, name) not in seen_sig_name:
                    seen_sig_name.add((sig, name))
                    by_sig.setdefault(sig, []).append((name, member))

        for line in proc.stdout.splitlines():
            m = _OD_MEMBER.match(line)
            if m:
                flush()
                member, name, words = m.group(1), None, []
                continue
            m = _OD_FUNC.match(line)
            if m:
                flush()
                name, words = m.group(1), []
                continue
            m = _OD_INSN.match(line)
            if m and name is not None:
                words.append(int(m.group(1), 16))
        flush()
    return by_name, by_sig


@functools.cache
def _src_index():
    """Archive-member basename (rotate.o) -> its src relpath (gu/rotate.o), built once by walking
    SRC_OBJ_TREE."""
    index = {}
    for dirpath, _, files in os.walk(SRC_OBJ_TREE):
        for f in files:
            if f.endswith(".o"):
                index.setdefault(
                    f, os.path.relpath(os.path.join(dirpath, f), SRC_OBJ_TREE)
                )
    return index


def member_src(member):
    """Map an archive member basename (rotate.o) to its src relpath (gu/rotate), best-effort."""
    rel = _src_index().get(member)
    return os.path.splitext(rel)[0] if rel else os.path.splitext(member)[0]


# --- candidate side: the project's asm/<rom>.s listings -------------------------------------
def parse_asm_functions(rom_off):
    """[(name, vram, words)] for every glabel'd function in asm/<rom>.s."""
    path = os.path.join(pt.ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return []
    out, name, vram, words = [], None, None, []

    def flush():
        if name is not None and words:
            out.append((name, vram, words))

    with open(path, errors="ignore") as f:
        for line in f:
            g = _GLABEL.match(line)
            if g:
                flush()
                name, vram, words = g.group(1), None, []
                continue
            m = _ASM_LINE.search(line)
            if m and name is not None:
                if vram is None:
                    vram = int(m.group(1), 16)  # first instr's vram = function start
                words.append(int(m.group(2), 16))
    flush()
    return out


def existing_names_and_addrs():
    """(names, vrams) already claimed in symbol_addrs.txt + ghidra_symbols.txt (disjointness)."""
    names, addrs = set(), set()
    for sf in decomp_common.SYMBOL_FILES:
        if not os.path.exists(sf):
            continue
        with open(sf, errors="ignore") as f:
            for line in f:
                m = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)", line)
                if m:
                    names.add(m.group(1))
                    addrs.add(int(m.group(2), 16))
    return names, addrs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--min-insns",
        type=int,
        default=5,
        help="min instruction count for a worklist match (FP guard; default 5)",
    )
    ap.add_argument(
        "--all",
        action="store_true",
        help="also list matches that land in already-`c` subsegs (sanity)",
    )
    args = ap.parse_args()

    present = [a for a in ARCHIVES if os.path.exists(a)]
    if not present:
        sys.exit(
            "no reference archive found in:\n  "
            + "\n  ".join(ARCHIVES)
            + "\n(set MG_LIBULTRA_ARCHIVES, os.pathsep-separated)"
        )

    print("reference (union):")
    for a in present:
        print(f"  {a}")
    by_name, by_sig = build_reference(present)
    print(
        f"  {len(by_name)} archive functions, {len(by_sig)} distinct opcode signatures"
    )

    upstream = pt.build_upstream_index()
    subs = pt.parse_subsegs()
    claimed_names, claimed_addrs = existing_names_and_addrs()

    # Walk every candidate function once, partition by subseg type. libultra is resident in the
    # boot/main segment; it is NEVER in a relocatable overlay, so overlay functions matching a
    # short libultra opcode-skeleton are false positives (osSetTime "matching" four ovl stubs).
    # Drop them up front (vram in the overlay range, or a func_ovl* name).
    OVERLAY_VRAM = 0x80100000
    asm_funcs, c_funcs = [], []  # (rom_off, name, vram, sig, nins)
    overlay_skipped = 0
    for off, typ, _path in subs:
        for nm, vram, words in parse_asm_functions(off):
            if nm.startswith("func_ovl") or (vram is not None and vram >= OVERLAY_VRAM):
                overlay_skipped += 1
                continue
            rec = (off, nm, vram, opcode_sig(words), len(words))
            (asm_funcs if typ == "asm" else c_funcs).append(rec)

    # Candidate-side signature frequency: a sig appearing at many candidate addresses is a
    # generic skeleton (FP-prone for short funcs), independent of reference ambiguity.
    cand_sig_freq = {}
    for _off, _nm, _vram, sig, _nins in asm_funcs:
        cand_sig_freq[sig] = cand_sig_freq.get(sig, 0) + 1

    # --- calibration (#7): compiled-C libultra funcs still in asm, vs the archive ----------
    # Two failure buckets: same-length opcode swaps = compiler/flag divergence (a faithful
    # rebuild with tools/cc/gcc + LIBULTRA_CFLAGS would fix); different-length = a different
    # source version / impl (rebuild of the same source won't reconcile). Hand-asm (.s) funcs
    # are excluded — they involve no compiler, so they can't probe gcc fidelity.
    cal_ok = cal_total = 0
    cal_flag, cal_impl = [], []
    for off, nm, vram, sig, nins in asm_funcs:
        if nm in upstream and nm in by_name:
            cal_total += 1
            arch_sigs = [rsig for rsig, _ in by_name[nm]]
            if sig in arch_sigs:
                cal_ok += 1
            elif any(len(rsig) == len(sig) for rsig in arch_sigs):
                cal_flag.append((nm, off))  # same length, opcodes differ → flag/codegen
            else:
                cal_impl.append((nm, off))  # different length → different impl/version
    print("\n=== calibration (#7: prebuilt-archive gcc vs the ROM) ===")
    if cal_total:
        print(
            f"  {cal_ok}/{cal_total} compiled-C libultra funcs (still asm) opcode-match the archive"
        )
        print(
            f"  {len(cal_flag)} same-length opcode-swaps (compiler/flag divergence — e.g. -funsigned-char lb/lbu)"
        )
        print(
            f"  {len(cal_impl)} different-length (different source version/impl in the prebuilt tree)"
        )
        for label, lst in (("flag", cal_flag), ("impl", cal_impl)):
            for nm, off in lst[:8]:
                print(f"    [{label}] {nm} @ 0x{off:X}")
        if cal_ok / cal_total < 0.8:
            print(
                "  WARNING: <80% calibration — the prebuilt build/J archive is only a partial reference."
            )
            print(
                "  Trust matches below (full opcode-seq equality is strong); treat NON-matches as"
            )
            print(
                "  'not in this reference', not 'not libultra'. For a complete worklist, rebuild the"
            )
            print(
                "  reference from ultralib source with tools/cc/gcc + LIBULTRA_CFLAGS (plan #7 fallback)."
            )
    else:
        print(
            "  no compiled-C libultra funcs available to calibrate (matcher pipeline unverified for gcc fidelity)"
        )

    # --- match every asm-subseg function against the reference ------------------------------
    validation, worklist, conflicts, notes = [], [], [], []
    for off, nm, vram, sig, nins in asm_funcs:
        hits = by_sig.get(sig)
        if not hits:
            continue
        ref_names = sorted({h[0] for h in hits})
        member = hits[0][1]
        src = member_src(member)
        ambig = len(ref_names) > 1
        row = (off, nm, vram, ref_names, src, nins, ambig)
        if not nm.startswith("func_"):
            if nm in ref_names:
                validation.append(row)  # already named correctly
            else:
                conflicts.append(row)  # named X, opcode-matches libultra Y
        else:
            worklist.append(row)  # unnamed func_ -> a libultra match

    # matches that fell in already-decompiled `c` subsegs (sanity only)
    if args.all:
        for off, nm, vram, sig, nins in c_funcs:
            if sig in by_sig:
                notes.append(
                    (
                        off,
                        nm,
                        vram,
                        sorted({h[0] for h in by_sig[sig]}),
                        None,
                        nins,
                        False,
                    )
                )

    def fmt(row):
        off, nm, vram, ref_names, src, nins, ambig = row
        vs = f"0x{vram:08X}" if vram is not None else "?"
        tag = " [AMBIG]" if ambig else ""
        return (
            f"  0x{off:X} {vs} {nm:<26} -> {','.join(ref_names)} ({src}, {nins}i){tag}"
        )

    print(
        f"\n=== validation: named asm blocks confirmed libultra by opcode ({len(validation)}) ==="
    )
    for row in sorted(validation):
        print(fmt(row))

    print(
        f"\n=== CONFLICTS: named asm block opcode-matches a DIFFERENT libultra fn ({len(conflicts)}) ==="
    )
    if conflicts:
        print(
            "  (likely a name-collision false-positive in pick_target's labeling — verify)"
        )
    for row in sorted(conflicts):
        print(fmt(row))

    # worklist: unnamed func_ matches, filtered for disjointness + min length
    emitted = 0
    print(
        f"\n=== symbol_addrs.txt worklist: unnamed libultra blocks (add-only, gate-applied) ==="
    )
    print(
        "# review before applying; opcode-only match — confirm at the gate. min-insns=%d"
        % args.min_insns
    )
    for off, nm, vram, ref_names, src, nins, ambig in sorted(worklist):
        if nins < args.min_insns:
            continue
        name = ref_names[0]
        flags = []
        if ambig:
            flags.append("AMBIG:" + "|".join(ref_names))
        freq = cand_sig_freq.get(sig, 1)
        if freq > 1:
            flags.append(f"common-skeleton x{freq}")
        if name in claimed_names:
            flags.append("name-claimed")
        if vram in claimed_addrs:
            flags.append("addr-claimed")
        skip = "name-claimed" in flags or "addr-claimed" in flags
        suffix = ("   # " + " ".join(flags)) if flags else ""
        if skip:
            print(f"# SKIP {name} = 0x{vram:08X}; // type:func ({src}){suffix}")
        else:
            print(
                f"{name} = 0x{vram:08X}; // type:func  ({src}, was {nm}, {nins}i){suffix}"
            )
            emitted += 1
    short = sum(1 for r in worklist if r[5] < args.min_insns)
    print(
        f"\n# {emitted} ready to add; {short} unnamed matches below min-insns ({args.min_insns}) withheld;"
        f" {overlay_skipped} overlay candidates excluded (libultra is never in an overlay)."
    )
    print(
        "# AMBIG / common-skeleton flags mark short opcode-only matches that are not unique — verify those."
    )
    print(
        "# Leaf intrinsics under 20i are NOT a coddog blind spot here — this matcher includes them;"
    )
    print(
        "# any still un-named are simply not opcode-unique enough to auto-name (raise --min-insns to trim)."
    )


if __name__ == "__main__":
    main()
