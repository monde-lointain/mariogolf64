#!/usr/bin/env python3
"""pick_target_score.py — extracted from pick_target.py."""

import dataclasses
import os
import re

from decomp_asm import PRIVILEGED_OPS, subseg_vram
from pick_target_classify import (
    _FUNC_TOKEN_RE,
    _is_static_func_proto,
    defines_data_globals,
    placed_symbols,
    src_func_callers,
)
from pick_target_config import (
    BACKLOG,
    ROOT,
    BAND_WARM_BONUS,
    BIG_FN_BYTES,
    CARRYOVER_PENALTY,
    FILE_STATIC_RE,
    HUGE_FN_BYTES,
    PACK_DECOMPOSE_NFNS,
    SMALL_PACK_BYTES,
    UPSTREAM_BONUS,
    UpstreamSource,
)
from pick_target_hazards import (
    CODDOG_MIRROR_PCT,
    HAZARD_CALLS_UNPLACED,
    HAZARD_DEFINES_DATA,
    HAZARD_FILE_STATIC,
    HAZARD_JAL_FREE,
    HAZARD_NEEDS_DEFINE,
    HAZARD_NEEDS_HEADER,
    HAZARD_ONE_TU,
    HAZARD_PACK,
    HAZARD_REFS_UNPLACED,
    HAZARD_RODATA_STRADDLE,
    HAZARD_SINGLE_FILE_PACK,
    Hazard,
)
from pick_target_index import _coddog_source_banked

def snap_fib(n):
    """Round a raw additive score up to the nearest Fibonacci ladder rung (1,2,3,5,8,13)."""
    for f in (1, 2, 3, 5, 8, 13):
        if n <= f:
            return f
    return 13

# Mirror-path story-point enablers, GROUPED (Phase 1b seed-weight table). Each present group
# adds 1 to the seed base; a group with >1 member kind contributes a SINGLE bump (preserving the
# old `drop`/`recover` OR-pairs: FILE_STATIC + DEFINES_DATA both present is still one `drop`). The
# names are reused by the classical enabler gate. Adding a hazard's mirror-path seed cost is now an
# edit to this table, not to seed_points' body (the Phase-1b de-shotgun of the if-ladder).
_SEED_ENABLER_GROUPS = (
    (
        "drop",
        frozenset({HAZARD_FILE_STATIC, HAZARD_DEFINES_DATA}),
    ),  # → classical fallback
    ("needs_copy", frozenset({HAZARD_NEEDS_HEADER})),  # companion-header copy
    ("needs_define", frozenset({HAZARD_NEEDS_DEFINE})),  # Makefile build-define enabler
    (
        "recover",
        frozenset({HAZARD_REFS_UNPLACED, HAZARD_CALLS_UNPLACED}),
    ),  # asm-data-recovery
)

def seed_points(size, upstream, band, nfns, hazards, blocked):
    """Deterministic a-priori story-point seed (Fibonacci 1,2,3,5,8,13) for one increment —
    the v1 story-point system (see VELOCITY.md). MG64's effort axis is PATH (upstream-mirror vs
    classical-loop) + ENABLER LOAD (band warmth, hazards), not raw bytes; the byte gates (768 /
    1536) are dormant in the current <256 B regime and only bite for large/classical fns.
    `hazards` is the list of Hazard objects; `blocked` is True when any needs-header is un-pickable
    (see include_is_blocked). Returns an int, or 'blk' for a blocked (un-pickable) target.
    Display-only — does NOT influence the smallest-first sort. A committed cluster seed is the
    SUM of its files' seeds (banked per-file, not all-or-nothing across the cluster)."""
    if blocked:
        return "blk"  # un-pickable until the -I path is added / header shipped — a DoR reject
    kinds = {h.kind for h in hazards}
    classical = upstream == "none"
    pack = nfns > 1
    # Which enabler groups are present (table-driven). Each contributes one base bump below; the
    # classical gate keys on the curated subset drop/needs_copy/recover.
    present = {name: bool(kinds & ks) for name, ks in _SEED_ENABLER_GROUPS}
    drop, needs_copy, recover = (
        present["drop"],
        present["needs_copy"],
        present["recover"],
    )
    big = size >= BIG_FN_BYTES
    huge = size >= HUGE_FN_BYTES
    # Non-decomposable classical pack: `one-tu` = every inner fn boundary is non-16-aligned, so the
    # pack is ONE .o (shared rodata/data) and a per-fn decompose split is mechanically BLOCKED. The
    # 8-gate's "must decompose" 13 is therefore a FALSE fire — the pack banks atomically as one
    # vertical slice (S148/S150/S152/S153/S154; folds the by-hand small classical pack exemption
    # a1/a2 into the ranker). Price it by SIZE (graded), then add the enabler load, so a decomposed
    # slice reads strictly BELOW its parent pack. The nfns<PACK_DECOMPOSE_NFNS cap is deliberate: a
    # 4+fn pack is a genuine "large pack, decompose" candidate that stays at the gate; a genuinely
    # huge one-tu (>=1536B) still flags 13 (size, not fn-count, is the risk there). See VELOCITY.md
    # + CLAUDE.md ## Story points. Display-only, like the rest of seed_points.
    if classical and HAZARD_ONE_TU in kinds and not huge and nfns < PACK_DECOMPOSE_NFNS:
        base = 8 if big else (5 if size >= SMALL_PACK_BYTES else 3)
        if HAZARD_JAL_FREE in kinds and not big:
            base = min(base, 3)  # 0-jal: no callee-resolution cost (exemption a2, size-agnostic)
        # Enabler load, same groups the mirror path sums (a one-tu pack that ALSO needs a
        # header-copy / data-drop / symbol-recovery is more work than a bare slice).
        base += sum(1 for x in (drop, needs_copy, recover, present["needs_define"]) if x)
        if HAZARD_RODATA_STRADDLE in kinds:
            base += 1  # a >=8-aligned pooled constant makes a decompose hit the align wall (S154)
        return snap_fib(base)
    if huge or (classical and pack):
        return 13  # must decompose; never a 1-increment sprint
    if nfns >= PACK_DECOMPOSE_NFNS:
        return 8  # a large pack must decompose regardless of path (hits the 8-gate)
    if classical and (big or pack or drop or needs_copy or recover):
        return 8  # enabler gate — decompose / scaffold first
    if classical:
        return 5  # small single classical fn — unproven regime, high variance
    base = (
        1 if band == "warm" else 2
    )  # warm = enabler-free; cold = symbol recovery / cold deps
    base += sum(
        present.values()
    )  # one bump per present enabler group (drop/copy/define/recover)
    # Unexplained jal-count-mismatch (NOT a `(version-artifact?)` clean-drop) with NO coddog-mirror
    # structural match is a probable near-verbatim version/reorder divergence, not a clean verbatim
    # cp: the verbatim copy SHA-misses and needs an asm-driven diagnosis + hand-edit. Price it one
    # above the mirror floor so the gate plans the near-verbatim risk instead of exempting it as a
    # clean verbatim. A coddog-mirror match (structural fingerprint) or the (version-artifact?)
    # annotation explains the mismatch → no bump; "no coddog" is itself the divergence tell, since a
    # reorder breaks coddog's fingerprint. (S119 nuContGBPakFread: jal `5vs9` macro-artifact + no
    # coddog, planned 2pt clean mirror, realized 3pt block-reorder near-verbatim.) See
    # docs/hazards.md#near-verbatim-mirror-jal-count-mismatch.
    jal_unexplained = any(h.is_unexplained_jal() for h in hazards)
    # A coddog-mirror SUPPRESSES the near-verbatim bump only when it is a CLEAN structural identity.
    # A coddog-structural / coddog-source-banked hit is the S123/S124 customization-MASK tell: the
    # 99.99 fingerprint matches a game-DIVERGENT body (or an already-banked false source), not a
    # verbatim cp. It must NOT suppress the bump, and on a (single-file-)pack it earns the same
    # game-modified-risk +1 — most-of-the-bodies-diverge, route to classical/mixed, not a seed-only
    # mirror (S123 nusched.c coddog-source-banked@99.99 pack ran as a 10/14 mixed bank). The non-lib
    # `jal func_<vram>` game-callee tell is a stronger signal still, but needs the call list here — a
    # tracked follow-up; until then a masking coddog on a pack carries the risk price. See
    # docs/hazards.md#coddog-cross-ref and CLAUDE.md ## Story points (exemption-GUARD).
    coddog_masks = any(h.is_coddog_mask() for h in hazards)
    has_clean_coddog = any(h.is_coddog_mirror() for h in hazards) and not coddog_masks
    if (jal_unexplained and not has_clean_coddog) or (coddog_masks and pack):
        base += 1  # near-verbatim / game-modified risk (the verbatim cp won't match; plan classical)
    if pack or big:
        base += 1
    return snap_fib(base)

def carry_over_names():
    """Function names genuinely PARKED in BACKLOG.md ## Carry-overs (de-ranked from the
    default ranker; build_rows skips a row whose primary is in here unless --include-stuck).

    A parked carry-over is correctly absent from `pick_target` output BY DESIGN — retrieve it
    with `--include-stuck` or by reading the BACKLOG (e.g. pimgr is found via the carry-over, not
    the ranker, and that is the intended path, not a filter bug).

    Two scope guards narrow an over-scoop, where every backticked prose token (a macro `STACK`, a
    name-dropped banked callee, a tool name) was carried — so a *still-asm* primary could be
    silently dropped just by being prose-mentioned:
      (1) stop at the first historical `- **Sprint NN: … BANKED` paragraph — that append-only
          banked-sprint archive lives under the same heading but is NOT parked work; and
      (2) keep only tokens that are real symbols (placed in a name file, or a `func_<vram>`
          placeholder) — drops macro/tool/keyword noise.
    A genuine parked leader (osMotorStop, osCreateScheduler) still de-ranks; the archive's
    name-drops of still-asm functions no longer do."""
    names = set()
    if not os.path.exists(BACKLOG):
        return names
    with open(BACKLOG) as f:
        # Anchor on the HEADING (start-of-line), not a mid-line prose mention of
        # "## Carry-overs" (several digest paragraphs reference it in backticks) — a string split
        # would start the region mid-archive and truncate the genuine carry-overs.
        parts = re.split(r"(?m)^## Carry-overs", f.read(), maxsplit=1)
    if len(parts) <= 1:
        return names
    region = parts[1]
    archive = re.search(
        r"^- \*\*Sprint \d+\b", region, re.M
    )  # guard (1): bound the live region
    if archive:
        region = region[: archive.start()]
    placed = placed_symbols()  # guard (2): real symbols only (names file ∪ func_<vram>)
    # NOTE (S271): this scan deliberately OVER-scoops toward false-POSITIVE. A `func_<vram>` named
    # only as a mid-prose callee of another wall ("... via `func_X`/`func_Y` ...") de-ranks even
    # though it is not itself parked (S271 func_80029A30/func_80029A6C were flagged CARRIED-WALL this
    # way, yet were trivial fresh glue). That is the SAFE direction: a false-positive costs one asm
    # read to clear, while a false-negative re-surfaces a known wall as a fresh leaf (the 8x-recurring
    # miss this tool exists to prevent). Do NOT tighten with a lead-line/subject heuristic — it un-parks
    # genuine carries documented only on continuation/prose lines (func_8003E004, func_80050710) and,
    # for a carry with no wip doc, turns --carried-check into a false-negative. Clear a suspected
    # false-positive by reading the .s, per the DoR.
    for tok in re.findall(r"`([A-Za-z_]\w+)`", region):
        if tok in placed or _FUNC_TOKEN_RE.fullmatch(tok):
            names.add(tok)
    return names

_WIP_DIR = os.path.join(ROOT, "docs", "wip")
_WIP_FUNC_RE = re.compile(r"func_[0-9A-Fa-f]{8}")


def carried_wall_names():
    """Union of the BACKLOG ## Carry-overs parked names (carry_over_names) AND every function
    that owns a `docs/wip/<fn>.*.md` near-match / wall note.

    The DoR "is this leaf a documented wall?" check. Recurred 8x through S268: a hand-mined leaf
    from an already-`c` PARTIAL pack reads "fresh" (the ranker is c-continuation-blind and only
    emits `blk`/asm-flip rows for such a segment) yet is a carried wall recorded ONLY in a wip
    doc or the BACKLOG carry block. `carry_over_names()` alone misses the wip-doc-only walls (a
    wall characterized at discovery into `docs/wip/` but never parked into BACKLOG). This union
    is the authoritative "already-characterized" set; surface it at the plan gate via
    `pick_target.py --carried-check <fn>...`. See docs/agent-workflow.md ## Definition of Ready."""
    names = set(carry_over_names())
    if os.path.isdir(_WIP_DIR):
        for fname in os.listdir(_WIP_DIR):
            if not fname.endswith(".md"):
                continue
            stem = fname.split(".", 1)[0]  # func_<vram>[-func_<vram>] before the first dot
            funcs = _WIP_FUNC_RE.findall(stem)  # a paired doc names >1 member
            if funcs:
                names.update(funcs)
            else:
                names.add(stem)  # curated-name wip doc (no func_<vram> in the stem)
    return names


_ASM_ROOT = os.path.join(ROOT, "asm")
# Scan the WHOLE prologue for the $v0-chain-home tell, not a fixed 8-insn window: the static-chain
# spill/copy is emitted AFTER the callee-saved register saves (sw ra/s0-s8, sdc1 f20-f30), so for a
# fn with many saved regs it lands 15-20 instructions in (S279 func_8007A40C: `addu $s1,$v0,$zero` +
# `sw $v0,0x10($sp)` at instruction ~15, past the old 8-insn window -> a MISS that re-priced two
# nested children as fresh). The home ALWAYS precedes real work, so scan up to the first `jal`,
# capped at _PROLOGUE_CAP as a backstop against a $v0-reusing body false-positive.
_PROLOGUE_CAP = 32
_V0_SPILL_RE = re.compile(r"\bsw\b\s+\$v0,\s*0x[0-9A-Fa-f]+\(\$sp\)")
_V0_COPY_RE = re.compile(r"\baddu\b\s+\$\w+,\s*\$v0,\s*\$zero")
_ASM_INSN_RE = re.compile(r"/\*[^*]*\*/\s+(\S.*)")
_JAL_RE = re.compile(r"\bjal\b")


def _find_asm_s(fn):
    import glob

    # the .s lives under its PACK dir (asm/nonmatchings/<seg>/<pack>/<fn>.s), not a per-fn dir,
    # so match the file anywhere under the asm root, not a dir named <fn>.
    hits = glob.glob(os.path.join(_ASM_ROOT, "**", fn + ".s"), recursive=True)
    return hits[0] if hits else None


def nested_child_tell(fn):
    """Heuristic DoR check: does <fn> look like a GCC nested function whose argument is homed via
    the static chain in $v0 (STATIC_CHAIN_REGNUM = $2), rather than a standalone leaf?

    A standalone leaf receives its first arg in $a0 and never reads an incoming $v0 (v0 is
    caller-saved / the return register, not an argument slot). A nested child's prologue instead
    spills the incoming static chain and copies it to a working register:
        sw   $v0, K($sp)            (dead spill of the chain — even a chain that goes unused)
        addu $<reg>, $v0, $zero     (copy the incoming chain into a saved/arg reg)
    Both appear within the first few prologue instructions. Such a leaf is NOT standalone-bankable:
    it banks as a nested function inside its (often still-asm) parent, so the plan gate must price
    it coupled-to-parent (a carry), not as a fresh smallest-first leaf.

    Returns True on the tell. Reads only <fn>'s own `.s` (cheap). The confirming caller tell
    (`addiu $v0,$sp,K` in the jal delay slot = address of a parent local) is left to a manual
    disasm. See docs/hazards.md#nested-function-static-chain-spill and the memories
    nested-function-banks-the-parent-too / func-80041e8c-v0-arg-convention-wall (S269 flagged
    func_8002BE78 -> draw_ground_shadow_decals and func_8002DAC0 -> render_frame; S279 widened the
    scan window after it MISSED func_8007A40C / func_8007A10C, whose chain-home sits ~15 insns in)."""
    path = _find_asm_s(fn)
    if not path:
        return False
    with open(path) as f:
        insns = _ASM_INSN_RE.findall(f.read())
    # Prologue = everything up to (and including the delay slot of) the first `jal`, capped.
    end = _PROLOGUE_CAP
    for i, insn in enumerate(insns[:_PROLOGUE_CAP]):
        if _JAL_RE.search(insn):
            end = i + 2  # include the branch delay slot
            break
    prologue = "\n".join(insns[:end])
    return bool(_V0_SPILL_RE.search(prologue) and _V0_COPY_RE.search(prologue))


_FRAME_ALLOC_RE = re.compile(r"\baddiu\b\s+\$sp,\s*\$sp,\s*-(0x[0-9A-Fa-f]+)")
_ARGP_RE = re.compile(r"\baddiu\b\s+\$(s[0-7]|fp),\s*\$sp,\s*(0x[0-9A-Fa-f]+)")


def nested_parent_tell(fn):
    """Heuristic DoR check: does <fn> CONTAIN GCC nested inline helpers (the S280 tell), the inverse
    of nested_child_tell?

    A nested helper that references the outer function's params/locals as FREE VARIABLES forces those
    variables put_var_into_stack (address-taken), and on MIPS gcc-2.7.2 (ARG_POINTER_REGNUM == $zero,
    fixed) the outer prologue materialises an ARG POINTER = frame top into a saved reg and re-reads its
    own incoming params from their home slots through it:
        addiu $sp, $sp, -K          (allocate frame K)
        addiu $<sreg>, $sp, K       (arg pointer = sp + framesize; a plain fn NEVER computes sp+K)
        sw    $<sreg>, M($sp)        (home the arg pointer)
        ... later ...  lw $aN, 0(<sreg>) / 4(<sreg>)   (re-read own params x/z/out as MEMs)
    Such a leaf is NOT a plain standalone body: it must be reconstructed WITH the nested `inline`
    helpers, and banking it uses the GNU nested-function extension (which also disables import.py /
    the permuter for the whole TU). So the plan gate must price it as a nested-function slice, not a
    fresh smallest-first leaf, and must NOT mis-read it as a register-pressure / spill wall.

    Returns True on the tell (frame K allocated AND an `addiu $sreg,$sp,K` arg-pointer for the same K).
    Reads only <fn>'s own `.s` (cheap). S280 flagged detect_terrain_collision, which --nested-check
    (child-only) had returned `standalone` for. See docs/hazards.md#nested-function-static-chain-spill
    and the memory argpointer-params-home-slots-nested-function-tell."""
    path = _find_asm_s(fn)
    if not path:
        return False
    with open(path) as f:
        insns = _ASM_INSN_RE.findall(f.read())
    frame = None
    for insn in insns:
        m = _FRAME_ALLOC_RE.search(insn)
        if m:
            frame = m.group(1).lower()
            break
    if frame is None:
        return False
    frame_val = int(frame, 16)
    for insn in insns:
        m = _ARGP_RE.search(insn)
        if m and int(m.group(2), 16) == frame_val:
            return True
    return False


def nested_tell(fn):
    """True if <fn> is EITHER a nested child (static-chain $v0 home) or a nested parent (contains
    nested helpers => arg-pointer home). Both are coupled/non-plain-standalone slices the plan gate
    must not price as a fresh smallest-first leaf."""
    return nested_child_tell(fn) or nested_parent_tell(fn)


_INSN_MNEMONIC_RE = re.compile(r"([a-z][a-z0-9.]*)")


def intrinsic_stub_tell(fn):
    """DoR check: is <fn>'s body a coprocessor / privileged-op intrinsic that GCC 2.7.2 has NO
    plain-C form for (FCSR/CP0 control moves: cfc1/ctc1/mfc0/mtc0/dmfc0/dmtc0/tlb*/cache/eret)?

    Such a leaf (e.g. an FCSR read-modify-write via cfc1/ctc1 $31) is NOT classically bankable: any
    C body needs inline asm, which is a `hasm` decision, not a match. Without this tell it reads as a
    fresh tiny loose-stub leaf and re-surfaces every sprint (S273 declined func_80029C00, but the skip
    lived only in ephemeral SPRINT.md, so S274 re-committed it). Reuses decomp_asm.PRIVILEGED_OPS (the
    same set `privileged_asm` uses for asm-mirror detection). Reads only <fn>'s own `.s` (cheap).

    Returns True on the tell. See the retired-wall note docs/wip/func_80029C00.near-match.md."""
    path = _find_asm_s(fn)
    if not path:
        return False
    with open(path) as f:
        for insn in _ASM_INSN_RE.findall(f.read()):
            m = _INSN_MNEMONIC_RE.match(insn)
            if m and m.group(1) in PRIVILEGED_OPS:
                return True
    return False


_JTBL_RE = re.compile(r"\bjtbl_[0-9A-Fa-f]+\b")
# a raw display-list command word materialised as the FULL-32-bit-word form
# `lui $reg, (0xHHHHHHHH >> 16)` (splat's spelling for a lui+ori that rebuilds a known 32-bit
# constant); the top byte HH is an RDP/RSP command opcode (G_SETOTHERMODE_H=0xE3, G_SETSCISSOR=0xED,
# G_SETCOMBINE=0xFC, G_RDPPIPESYNC=0xE7, G_TEXRECT=0xE4, G_RDPHALF_1/2=0xE1/0xF1, G_SETCIMG=0xFF, ...
# span 0xC8..0xFF). Requiring the full 8-digit `>> 16` form excludes a bare 16-bit `lui $reg,0xFFFF`
# negative-constant HI, which would otherwise false-positive on the top byte alone.
_DL_CMD_LUI_RE = re.compile(r"\blui\b\s+\$\w+,\s*\(0x([0-9A-Fa-f]{2})[0-9A-Fa-f]{6}\s*>>\s*16\)")
_DL_CMD_MIN = 6  # >= this many DL-command-word luis => a raw-DL-word emitter (S275 wall class)


def wall_class_tell(fn):
    """DoR check: does <fn>'s body carry an S275 wall-class `.s` tell the size+FP sort cannot see?

    Returns a short class string, or "" for none:
      - "jtbl-dispatch": the `.s` references a compiler jump table (`jtbl_<vram>`). Its `.rodata` carve
        is a BANK-TIME action that only banks when the table is 8-aligned on BOTH edges with no
        interleaved still-asm sibling rodata; a 4-aligned trailing edge walls it (S275 func_800985B4,
        [[jtbl-carve-both-edge-8align]]).
      - "raw-dl-emitter": >= _DL_CMD_MIN raw display-list command words materialised as `lui`
        immediates (0xE7/0xED/0xFC/0xE3/... top bytes). These glistp++ / unrolled per-entry emitters
        are the store-giv + sched/reg-permutation CARRY class (S243/S275 func_800318A8, func_80075E48;
        [[mg64-glyph-emitter-dl-family]]).

    Such a leaf reads `fresh` + `standalone` + no-prior-doc yet walls at attempt, so a smallest-first
    main slice should de-prioritise it. Reads only <fn>'s own `.s` (cheap). See the S275 retro."""
    path = _find_asm_s(fn)
    if not path:
        return ""
    try:
        with open(path) as f:
            body = f.read()
    except OSError:
        return ""
    if _JTBL_RE.search(body):
        return "jtbl-dispatch"
    n_cmd = sum(1 for hh in _DL_CMD_LUI_RE.findall(body) if int(hh, 16) >= 0xC8)
    if n_cmd >= _DL_CMD_MIN:
        return "raw-dl-emitter"
    return ""


_STUB_FN_RE = re.compile(r'INCLUDE_ASM\("[^"]+",\s*([A-Za-z0-9_]+)\)')
_STUB_SIZE_RE = re.compile(r"0x([0-9A-Fa-f]+)")


def _stub_size(fn):
    """Instruction-blob byte size of <fn> from its `.s` header (`nonmatching <fn>, 0x<size>`), or
    None if the `.s` is missing. Cheap: reads only the first line."""
    path = _find_asm_s(fn)
    if not path:
        return None
    try:
        with open(path) as f:
            first = f.readline()
    except OSError:
        return None
    m = _STUB_SIZE_RE.search(first)
    return int(m.group(1), 16) if m else None


def loose_stubs(seg):
    """Enumerate the still-`INCLUDE_ASM` stubs sitting inside ALREADY-`c` files of a segment's src
    tree, smallest-first, each tagged carried-wall / nested-child.

    Why this is not build_rows: the whole-subseg ranker prices only asm-flip PACKS, so a fresh leaf
    that persists as one stub inside a partial `c` file is invisible to it — `--segment main` can
    read "no candidates" while genuinely fresh standalone leaves remain (S273 mined
    pause_audio/unload_active_overlay/func_80029EEC this way, all after the pack ranker went empty).
    This walks src/<seg>/*.c for INCLUDE_ASM fn names, sizes each from its `.s`, and applies the same
    DoR tells as --carried-check (carried_wall_names) and --nested-check (nested_child_tell), so the
    gate sees the actionable fresh+standalone leaves without a hand-grep.

    Returns a list of dicts {fn, size, file, carried, nested} sorted by (size, fn). A stub whose `.s`
    is missing sorts last (size None -> a large sentinel) so it never masquerades as tiny.
    """
    import glob

    tree = os.path.join(ROOT, "src", seg)
    walls = carried_wall_names()
    out = []
    for cpath in sorted(glob.glob(os.path.join(tree, "*.c"))):
        try:
            with open(cpath) as f:
                text = f.read()
        except OSError:
            continue
        rel = os.path.relpath(cpath, ROOT)
        for fn in _STUB_FN_RE.findall(text):
            out.append(
                {
                    "fn": fn,
                    "size": _stub_size(fn),
                    "file": rel,
                    "carried": fn in walls,
                    "nested": nested_tell(fn),
                    "intrinsic": intrinsic_stub_tell(fn),
                    "wall_class": wall_class_tell(fn),
                }
            )
    out.sort(key=lambda r: (r["size"] if r["size"] is not None else 1 << 30, r["fn"]))
    return out


def _file_scope_static_count(cpath):
    """Number of file-scope static *variable* declarations (the uninitialized .bss family
    FILE_STATIC_RE matches; static function protos excluded). One matching line == one .bss symbol,
    counted without fragile declarator extraction (a `STACK(name, size)` macro hides the name). Used
    for the drop-static-mirror count; a func-local uninitialized static is a known under-count — it
    has no file-scope line and is recovered at the gate alongside the rest."""
    n = 0
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return 0
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith(("//", "*")):
            continue
        if FILE_STATIC_RE.match(line) and not _is_static_func_proto(line):
            n += 1
    return n

def drop_static_mirror_hazard(mirror_path, hazards):
    """Re-frame a coddog-confirmed verbatim mirror's .bss-family hazard cluster as ONE
    drop-to-extern enabler, or None. When a >=CODDOG_MIRROR_PCT non-audio `coddog-mirror` identity is
    on the row AND a `file-static` is present AND NO carve signal is (no rodata-literal / data-static /
    rodata-jtbl), the file-static + defines-data + refs-unplaced flags are NOT a carve/classical spike:
    every uninitialized file-scope static/global is dropped to a sized `extern` placed at its main_bss
    vram (pure .bss = no ROM bytes = no carve). Returns a `drop-static-mirror:<n>bss` Hazard so the
    gate prices a seed-only N-symbol mirror instead of the scary 4-flag cluster. The co-listed
    file-static/defines-data/refs-unplaced stay (seed_points + the gate's per-symbol recovery read
    them); this tag is the leading verdict that they are one enabler.

    Advisory + graceful: a candidate with a NONZERO-initialized global (real .data, not modelled as a
    distinct flag) that slips the condition degrades to the documented carve playbook when the gate
    SHA misses — never a silent wrong bank."""
    if not any(h.is_file_static() for h in hazards):
        return None
    if any(h.is_carve_signal() for h in hazards):
        return None  # a carve signal disqualifies the pure-.bss drop
    definitive = any(
        h.is_coddog_mirror()
        and not h.coddog_file().startswith("src/audio")
        and h.coddog_pct() >= CODDOG_MIRROR_PCT
        for h in hazards
    )
    if not definitive:
        return None
    n = (
        _file_scope_static_count(mirror_path) + len(defines_data_globals(mirror_path))
        if mirror_path and os.path.isfile(mirror_path)
        else 0
    )
    return Hazard.drop_static_mirror(n)

@dataclasses.dataclass
class Candidate:
    """A ranked subseg's resolved identity + path/band verdict, assembled in build_rows once its
    upstream/coddog branches settle, then passed as ONE argument to score_row (Phase 2a: collapses
    score_row's former 10-parameter list to 3 and retires the (off, primary, up_lib, ...) data
    clump). `fns` is the full member list (len == nfns); `up_lib` is None for a classical target."""

    off: int
    primary: str
    kind: str
    fns: list
    size: int
    up_lib: "str | None"
    band: str
    blocked: bool

def score_row(cand, hazards, carried):
    """Assemble the ranked row dict (score folds size + upstream/warm/carry bonuses)."""
    score = -cand.size + (UPSTREAM_BONUS if cand.up_lib else 0)
    if cand.band == "warm":
        score += BAND_WARM_BONUS  # prefer enabler-free warm-band siblings over equally-small cold ones
    if cand.primary in carried:
        score -= CARRYOVER_PENALTY
    hz = ",".join(h.render() for h in hazards) or "-"
    vram = subseg_vram(cand.off)
    return {
        "func": cand.primary,
        "rom": f"0x{cand.off:X}",
        "vram": f"0x{vram:08X}" if vram is not None else "?",
        "size": cand.size,
        "nfns": len(cand.fns),
        "pts": seed_points(
            cand.size,
            cand.up_lib or "none",
            cand.band,
            len(cand.fns),
            hazards,
            cand.blocked,
        ),
        "kind": cand.kind,
        "upstream": cand.up_lib or "none",
        "band": cand.band,
        "hazards": hz,
        "score": score,
    }

def _append_caller_evict(fns, hazards):
    """Flag each un-named `func_<vram>` member a banked C file calls BY NAME: the gate's curated-symbol
    rename EVICTS it (splat renames the member -> the caller's hard-coded name fails to link), so the
    one-line fixup is priced up-front, not discovered as a build-check link error. Display-only
    (seed_points ignores HAZARD_CALLER_EVICT)."""
    callers = src_func_callers()
    evicts = [
        f"{fn}@{','.join(callers[fn])}"
        for fn in fns
        if fn.startswith("func_") and fn in callers
    ]
    if evicts:
        hazards.append(Hazard.caller_evict(evicts))

def _append_coddog_aux(hazards, cod_srcs):
    """Three advisory coddog passes over the resolved hazard set: flag a match to an ALREADY-BANKED
    source (the mirror is fully decompiled, so the hit is a fingerprint coincidence, not a fresh
    attribution); flag a `c-combined-undercount` when coddog fingerprints MORE upstream files than the
    named-symbol c-combined sees (extra files' members are un-named); and flag a SUB-100 coddog-mirror
    as body-divergence-suspect (it may mask a game-modified body; the gate runs a store-value diagnosis
    before declaring a clean mirror or a compiler wall, S127)."""
    for s in sorted({c for c in cod_srcs if _coddog_source_banked(c)}):
        hazards.append(Hazard.coddog_source_banked(os.path.basename(s)))
    # De-weight the body-divergence hedge on a SINGLE-coddog row whose only divergence signal is a
    # jal-count-mismatch ALREADY explained as an artifact (version wrapper / single-real-call macro
    # expansion): the surplus jals are accounted for, so the sub-100 coddog is a literal/rodata-carve
    # near-match, not a game-modified body — the rodata-literal/jtbl flags carry the real first-build
    # signal. Keep the hedge for a MULTI-coddog pack (genuinely suspect, most bodies diverge) and for
    # an UNexplained mismatch (a real corroborator). S136 n_synthesizer: 3vs8(macro-artifact?) + the
    # lone n_synthesizer.c coddog -> suppressed; banked atomically as a clean mirror + rodata carve.
    cod_files = {h.coddog_file() for h in hazards if h.is_coddog_mirror()}
    single_cod = len(cod_files) == 1
    # c-combined-undercount: the named-symbol c-combined file count is BELOW the distinct coddog-mirror
    # file count → the pack spans more upstream files than the named index sees (the extra files'
    # members are `func_<addr>`, un-named, so c-combined misses them but coddog fingerprints them). The
    # FILE analog of the S131 coddog-fncount-mismatch under-count guard: tells the gate to decompose at
    # MORE boundaries than c-combined lists, and to hand-trace the extra file's boundary from its named
    # member fns. S139 func_8009E4B0: c-combined saw n_auxbus|n_drvrNew (2), coddog saw +n_env (3) ->
    # `2vs3`. Advisory (display-only). The per-file vram-boundary + carve-extent pricing the gate still
    # hand-traces is a tracked follow-up (it needs the member asm %hi/%lo refs at pricing time).
    cc = next((h for h in hazards if h.is_c_combined()), None)
    if cc is not None and len(cod_files) > cc.c_combined_count():
        hazards.append(Hazard.c_combined_undercount(cc.c_combined_count(), len(cod_files)))
    suppress_body_div = single_cod and any(h.is_jal_artifact() for h in hazards)
    # Also suppress on an n_audio_sc N_MICRO clean single-source row: a single coddog hit on a
    # libnaudio / n_audio_sc source whose row is a single-file-pack OR a plain single-fn file (no
    # pack hazard at all). The sub-100 (e.g. @99.99) on these is a STRUCTURAL artifact of N_MICRO
    # macro / inc-include expansion diverging from the non-micro upstream coddog fingerprint, NOT a
    # game-modified body — it fired FALSE on 9 consecutive n_audio_sc mirrors (S133-S139, all banked
    # atomically as clean verbatim mirrors + carves; S139 added n_auxbus + n_drvrNew x2 after a
    # c-combined DECOMPOSE). Keying on `up_lib == libnaudio` + a clean single-source SHAPE (rather
    # than the single-file-pack shape alone) is load-bearing: a c-combined pack DECOMPOSED at the gate
    # becomes per-file single-fn rows (n_auxbus is 1 fn → no pack hazard), which the old
    # single-file-pack-only key would re-flag. `single_cod` already excludes a still-combined
    # multi-coddog pack (n != 1 distinct coddog files → hedge kept, the S123 customization guard). The
    # libnaudio restriction is load-bearing the other way: a libnusys single-file-pack @99.99 CAN be a
    # real game-modified body (S121/S127 contRmbControl FORCESTOP), so it keeps the hedge. The build
    # SHA-1 oracle still catches any real divergence; this only drops a 9x-false up-front warning. See
    # docs/hazards.md#coddog-cross-ref.
    if not suppress_body_div and single_cod:
        clean_single_source = any(
            h.kind == HAZARD_SINGLE_FILE_PACK for h in hazards
        ) or not any(h.kind in (HAZARD_PACK, HAZARD_SINGLE_FILE_PACK) for h in hazards)
        naudio_clean = (
            bool(cod_srcs)
            and all(("n_audio_sc" in s or "libnaudio" in s) for s in cod_srcs)
            and clean_single_source
        )
        suppress_body_div = naudio_clean
    if not suppress_body_div:
        for cf in sorted(
            {
                h.coddog_file()
                for h in hazards
                if h.is_coddog_mirror() and h.coddog_pct() < 100.0
            }
        ):
            pct = max(
                h.coddog_pct()
                for h in hazards
                if h.is_coddog_mirror() and h.coddog_file() == cf
            )
            hazards.append(Hazard.body_divergence_suspect(cf, pct))

