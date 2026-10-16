#!/usr/bin/env python3
"""pick_target_score.py — extracted from pick_target.py."""

import dataclasses
import functools
import os
import re

import yaml
from cpreprocess import _strip_comments
from decomp_asm import PRIVILEGED_OPS, subseg_vram
from decomp_common import ASM_DATA_DIR, ASM_DIR
from pick_target_classify import (
    _FUNC_TOKEN_RE,
    _is_static_func_proto,
    defines_data_globals,
    placed_symbols,
    src_func_callers,
)
from pick_target_config import (
    BACKLOG,
    BAND_WARM_BONUS,
    BIG_FN_BYTES,
    CARRYOVER_PENALTY,
    FILE_STATIC_RE,
    HUGE_FN_BYTES,
    PACK_DECOMPOSE_NFNS,
    ROOT,
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
    """Backlog-mentioned symbols used to park whole assembly packs.

    This deliberately broad prose scan is not function-specific wall evidence.
    Stop before the historical sprint archive and retain only real symbols.
    Individual C-host leaves use WIP notes for exclusion and expose these
    backlog mentions separately for review.
    """
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
    # Explicit retry and next-target lists do not park whole packs.
    region = re.sub(
        r"(?ms)^- \*\*\([^)]*NEAR-FREE RETRY.*?(?=^- \*\*|\Z)", "", region
    )
    region = re.sub(
        r"(?ms)\*\*The vein, smallest-first.*?(?=^\s*\*\*|^- \*\*|\Z)", "", region
    )
    placed = placed_symbols()  # Known symbols or func_<vram> placeholders only.
    # Preserve continuation-line carries for whole-pack selection; a mention
    # alone must not exclude a loose stub or establish compiler impossibility.
    for tok in re.findall(r"`([A-Za-z_]\w+)`", region):
        if tok in placed or _FUNC_TOKEN_RE.fullmatch(tok):
            names.add(tok)
    return names

_WIP_DIR = os.path.join(ROOT, "docs", "wip")
_WIP_FUNC_RE = re.compile(r"func_[0-9A-Fa-f]{8}")


def carried_wall_names():
    """Functions named by existing per-function WIP notes, not BACKLOG prose.

    A note records characterization, not proof that matching is impossible.
    Paired notes retain both members; curated names retain their file stem.
    """
    names = set()
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


_ASM_ROOT = str(ASM_DIR)
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
    """Recognize an incoming static-chain spill, including a chain-unused child.

    A copied chain retains the existing prologue tell. Without a copy, the spill
    must be the first mention of $v0: spilling a locally computed return value is
    not evidence of nesting. Confirm the caller's chain setup before reconstruction.
    """
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
    prologue = insns[:end]
    if not any(_V0_SPILL_RE.search(insn) for insn in prologue):
        return False
    if any(_V0_COPY_RE.search(insn) for insn in prologue):
        return True
    first_v0 = next((insn for insn in prologue if "$v0" in insn), "")
    return bool(_V0_SPILL_RE.search(first_v0))


_FRAME_ALLOC_RE = re.compile(r"\baddiu\b\s+\$sp,\s*\$sp,\s*-(0x[0-9A-Fa-f]+)")
_ARGP_RE = re.compile(r"\baddiu\b\s+\$(s[0-7]|fp),\s*\$sp,\s*(0x[0-9A-Fa-f]+)")
# `addiu $v0,$sp,K` = STATIC_CHAIN_REGNUM loaded with the address of a caller local (S310).
_CHAIN_SETUP_RE = re.compile(r"\baddiu\b\s+\$v0,\s*\$sp,\s*(?:0x)?[0-9A-Fa-f]+")


def _static_chain_callees(insns):
    """Find direct calls with a nearby live chain setup, including the delay slot."""
    callees = set()
    for i, insn in enumerate(insns):
        call = re.search(r"\bjal\b\s+(\w+)", insn)
        if not call:
            continue
        if i + 1 < len(insns) and _CHAIN_SETUP_RE.search(insns[i + 1]):
            callees.add(call.group(1))
            continue
        for previous in reversed(insns[max(0, i - 3):i]):
            if _CHAIN_SETUP_RE.search(previous):
                callees.add(call.group(1))
                break
            if _JAL_RE.search(previous) or "$v0" in previous:
                break
    return callees


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
    return bool(_static_chain_callees(insns))


def nested_parents_of(fn):
    """Find callers that set up <fn>'s static chain, including jal delay slots.

    Extracted assembly may outlive its stub; confirm live source before pricing.
    """
    import glob

    hits = []
    root = os.path.join(_ASM_ROOT, "nonmatchings")
    for path in glob.glob(os.path.join(root, "**", "*.s"), recursive=True):
        name = os.path.splitext(os.path.basename(path))[0]
        if name == fn:
            continue
        try:
            with open(path) as f:
                text = f.read()
        except OSError:
            continue
        insns = _ASM_INSN_RE.findall(text)
        if fn in _static_chain_callees(insns):
            hits.append(name)
    return sorted(set(hits))


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
      - "dl-emitter": >= _DL_CMD_MIN display-list command words materialised as `lui`
        immediates (0xE7/0xED/0xFC/0xE3/... top bytes). A PRICING tag, not a wall class: the caller
        counts these as `fresh`. It says the leaf needs the DL reconstruction recipe (find the gbi.h
        composite first, re-derive every word from the `.s` store trace), nothing more.
        The wall framing this tag carried as `raw-dl-emitter` from S275 is RETIRED (S294): S258 had
        already retired the underlying verdict, and S293+S294 banked the seven smallest members of
        the class 7/7 with zero permuter runs and zero compiler-source dives, at 154-307
        instructions.
        The S294 gate tried to SPLIT the tag into composite-mappable (a one-to-three-build leaf) and
        custom-packing (the genuine store-giv carry, func_8007624C's >>2-quantized colour). There is
        no `.s` tell that separates them: measured over the seven banked composites plus
        func_8007624C, the bitfield-assembly counts overlap completely (a composite expanding
        run-time arguments emits MORE `andi`/`sll`/`or` than the custom packing does -- func_8006A84C
        banked with andi=43/sll=38 against func_8007624C's andi=4/sll=23). Do not re-attempt the
        split on those counts. The separating signal that DOES exist is `dl_twin_pairs` below.
        See [[mg64-glyph-emitter-dl-family]], [[sdk-composite-macro-before-dl-reconstruction]].

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
        return "dl-emitter"
    return ""


_JAL_TARGET_RE = re.compile(r"\bjal\b\s+([A-Za-z_][A-Za-z_0-9]*)")
_SINGLE_CALLEE_MIN = 4  # below this a single target says nothing about the leaf's shape


def single_callee_tell(fn):
    """DoR DEWEIGHT: does ONE callee account for every `jal` in <fn>'s body? Returns the callee name
    or "".

    A high `jal` count reads as size-like risk in the ranker, but when every call goes to one target
    the leaf is call glue over a single helper, which is the cheapest shape the segment has: the
    helper's signature is already known (usually already `c`), so the whole body is a dispatch
    skeleton with constant arguments and there is no per-call type derivation. S311's
    `func_80095DE0` was 611 instructions with 39 `jal`, all to the already-`c` `func_80095D70`, and
    banked on the second build; the one-line histogram said so before any C was written.

    Requires >= _SINGLE_CALLEE_MIN calls: one or two calls to one target is the ordinary case and
    carries no shape information. Reads only <fn>'s own `.s` (cheap)."""
    path = _find_asm_s(fn)
    if not path:
        return ""
    try:
        with open(path) as f:
            targets = _JAL_TARGET_RE.findall(f.read())
    except OSError:
        return ""
    if len(targets) < _SINGLE_CALLEE_MIN or len(set(targets)) != 1:
        return ""
    return targets[0]


_TEMPLATE_ADDR_RE = re.compile(r"%hi\((D_[0-9A-Fa-f]{8})\)")
_TEMPLATE_MIN_PAIRS = 3  # >= this many lw/sw pairs after the address => a block move, not a read


def pool_blocked_tell(fn):
    """DoR check: would <fn>'s C body have to EMIT a `.rodata` template that the ROM keeps in a pool
    shared with still-asm siblings? Returns "pool-blocked" or "".

    The blocked shape is a local array initializer: gcc block-moves it from a compiler-generated
    rodata template, so the C body necessarily emits its own copy, and one object's `.rodata` is
    contiguous. If the ROM's copy of that template sits in a generic (uncarved) rodata blob rather
    than in the host file's own `.rodata` carve, the emitted copy lands at the wrong address and
    shifts every following data symbol -- a byte-exact body that reddens the gate build. It banks
    only once the pool-owning siblings are also C. S300 `func_8007C5D8` (byte-exact 590/590,
    22.1 MB of ROM shift when integrated); S279 `func_80079EBC` is the same class.

    The tell: a `%hi(D_<addr>)` whose symbol is defined in an `asm/data/*.rodata.s` generic blob,
    followed within a short window by >= _TEMPLATE_MIN_PAIRS `lw`/`sw` pairs targeting `sp`. Reads
    only <fn>'s own `.s` plus the data blobs (cheap).

    SCOPE LIMIT, deliberate. This catches the block-move case only. A leaf blocked because an FP
    literal it must spell would duplicate a pool double (S279 `func_80079EBC`, where referencing the
    constant extern instead perturbs a sched coin) has no `.s` tell at all: the ROM's instruction
    stream is identical either way. So a clean result here is not proof the pool is safe -- check the
    host's `.rodata` carve against the constants the body will emit before pricing a partial-bank
    leaf as clean."""
    path = _find_asm_s(fn)
    if not path:
        return ""
    try:
        with open(path) as f:
            lines = f.readlines()
    except OSError:
        return ""
    for i, line in enumerate(lines):
        m = _TEMPLATE_ADDR_RE.search(line)
        if not m:
            continue
        window = "".join(lines[i : i + 24])
        n_lw = len(re.findall(r"\blw\b\s+\$\w+,\s*0x[0-9A-Fa-f]+\(\$\w+\)", window))
        n_sw = len(re.findall(r"\bsw\b\s+\$\w+,\s*0x[0-9A-Fa-f]+\(\$sp\)", window))
        if min(n_lw, n_sw) < _TEMPLATE_MIN_PAIRS:
            continue
        if _sym_in_generic_rodata_blob(m.group(1)):
            return "pool-blocked"
    return ""


_JTBL_SYM_RE = re.compile(r"\b(jtbl_[0-9A-Fa-f]{8})\b")
# `/* ACAD0 800D16D0 80073B74 */ .word .L80073B74` — the rom offset + vram splat writes on every
# data line. The third column is the datum, absent on an `.asciz`, so it is not captured.
_BLOB_LINE_RE = re.compile(r"/\*\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]{8})\b")
_ASCIZ_RE = re.compile(r'\.asciz\s+"((?:[^"\\]|\\.)*)"')


@functools.lru_cache(maxsize=1)
def _rodata_blob_index():
    """{symbol: {rom, vram, words, asciz}} for every `dlabel` block in an `asm/data/*.rodata.s` blob.

    These blobs are the rodata splat did NOT attribute to a C file, so every symbol here is data a
    C body must either reference `extern` or carve. `words` is the block's `.word` count (its size in
    words), `asciz` the first string literal in it, or None. Read once per process; the blobs only
    change on the CMake extraction target."""
    index = {}
    data_dir = str(ASM_DATA_DIR)
    try:
        names = sorted(os.listdir(data_dir))
    except OSError:
        return index
    for name in names:
        if not name.endswith(".rodata.s"):
            continue
        try:
            with open(os.path.join(data_dir, name)) as f:
                lines = f.readlines()
        except OSError:
            continue
        sym = None
        for line in lines:
            stripped = line.strip()
            if stripped.startswith("dlabel "):
                sym = stripped.split()[1]
                index[sym] = {"rom": None, "vram": None, "words": 0, "asciz": None}
                continue
            if stripped.startswith("enddlabel"):
                sym = None
                continue
            if sym is None:
                continue
            entry = index[sym]
            m = _BLOB_LINE_RE.search(line)
            if m and entry["rom"] is None:
                entry["rom"] = int(m.group(1), 16)
                entry["vram"] = int(m.group(2), 16)
            if ".word" in stripped:
                entry["words"] += 1
            if entry["asciz"] is None:
                a = _ASCIZ_RE.search(stripped)
                if a:
                    entry["asciz"] = a.group(1)
    return index


def _jtbl_syms(fn):
    """The `jtbl_<vram>` symbols <fn>'s `.s` dispatches through, in first-reference order."""
    path = _find_asm_s(fn)
    if not path:
        return []
    try:
        with open(path) as f:
            body = f.read()
    except OSError:
        return []
    seen = []
    for sym in _JTBL_SYM_RE.findall(body):
        if sym not in seen:
            seen.append(sym)
    return seen


# `- [0xACBE0, .rodata, main/func_80071370]`. Parsed here rather than through
# pick_target_yaml.parse_subsegs, whose type group is `[a-z]+` and therefore never matches a
# LEADING-DOT section name: every `.rodata`/`.data` carve line is silently dropped from that parse.
# Widening the shared regex would change what the rodata-range helpers see, which is a behaviour
# change for the mirror-path carve advice and belongs in its own change, not in this tell.
_DOT_RODATA_RE = re.compile(r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*\.rodata\s*,\s*([^\]]+?)\s*\]")
_ANY_ROW_RE = re.compile(r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,")


@functools.lru_cache(maxsize=1)
def _dot_rodata_carves():
    """{path-qualifier: rom_offset} for every `.rodata, <path>` carve line in the yaml."""
    from pick_target_yaml import YAML

    carves = {}
    try:
        with open(YAML) as f:
            for line in f:
                m = _DOT_RODATA_RE.match(line)
                if m:
                    carves[m.group(2)] = int(m.group(1), 16)
    except OSError:
        pass
    return carves


def _host_rodata_carve_rom(cfile):
    """ROM offset of the `.rodata, <path>` subseg already carved for the C file `cfile`
    (`src/main/func_80071370.c` -> the `main/func_80071370` path qualifier), or None."""
    stem = os.path.splitext(cfile)[0]
    if stem.startswith("src/"):
        stem = stem[len("src/") :]
    return _dot_rodata_carves().get(stem)


def _rodata_row_ends():
    """{rom_offset: next_row_rom_offset} over every rodata-ish yaml row, in file order.

    A carve's END is the next row's start, and `_dot_rodata_carves` records only starts. Needed to
    tell whether a jump table sits immediately after a host's existing carve (extendable) or behind
    foreign rodata (not)."""
    from pick_target_yaml import YAML

    offs = []
    try:
        with open(YAML) as f:
            for line in f:
                m = _ANY_ROW_RE.match(line)
                if m:
                    offs.append(int(m.group(1), 16))
    except OSError:
        return {}
    return {a: b for a, b in zip(offs, offs[1:])}


def _blob_syms_between(lo, hi):
    """Names of uncarved-blob rodata symbols whose ROM offset lies in [lo, hi), sorted by offset.

    Every symbol in `asm/data/*.rodata.s` is data no C body emits, so one lying in a gap a carve
    would have to span makes that carve impossible: splat stops emitting the bytes and no object
    supplies them."""
    if hi <= lo:
        return []
    hits = []
    for sym, entry in _rodata_blob_index().items():
        rom = entry.get("rom")
        if rom is not None and lo <= rom < hi:
            hits.append((rom, sym))
    return [sym for _, sym in sorted(hits)]


def _jtbl_owner(sym):
    """The still-asm function whose `.s` dispatches through `sym`, or "". Names the cohort member a
    pool-cohort verdict is blocked on."""
    import glob

    # nonmatchings only: the whole-subseg listings under asm/ (asm/4C770.s) reference every jtbl in
    # their range, so globbing the asm root returns the subseg stem instead of the owning function.
    root = os.path.join(_ASM_ROOT, "nonmatchings")
    for path in glob.glob(os.path.join(root, "**", "*.s"), recursive=True):
        try:
            with open(path) as f:
                if sym in f.read():
                    return os.path.splitext(os.path.basename(path))[0]
        except OSError:
            continue
    return ""


def jtbl_carve_tell(fn, cfile):
    """DoR check for a `jtbl-dispatch` leaf: can its jump table actually be carved on its own?

    S265 recorded one condition and S307 measured a second. Returns "" when <fn> dispatches through
    no jump table, else one of:

      - "jtbl-carve-blocked": the table is not 8-aligned on BOTH edges, so the carve cannot be
        expressed as a subseg boundary at all ([[jtbl-carve-both-edge-8align]]; `blend_terrain_color`
        is the shape, 21 entries ending at 0x800CAA7C).
      - "pool-cohort:<fn>,...": the table is carveable in isolation, but the HOST OBJECT ALREADY OWNS
        a `.rodata` carve further along, with other rodata in between. One object emits one
        contiguous `.rodata`, so the leaf can only bank together with whoever owns the rodata in the
        gap, and the carve then moves to the earlier offset spanning all of it. S307
        `func_8007399C`: `jtbl_800D16D0` at 0xACAD0 is 8-aligned both edges, but the host already
        carves 0xACBE0 (`func_800760CC`'s table) and `jtbl_800D1738` (still-asm `func_800754BC`)
        sits between them, so the two leaves bank as one cohort at `[0xACAD0, ...]`, size 0x178.
      - "carve-pool-blocked:<sym>,...": the host object's `.rodata` carve PRECEDES the table and
        uncarved-blob rodata sits in the gap. One object emits one contiguous `.rodata`, so
        extending the existing carve forward to reach the table would also stop splat emitting
        those bytes, and no C body supplies them (S310 `spawn_terrain_effect`: host carve
        [0xACD10, 0xACD60), `jtbl_800D1990` at 0xACD90, and `D_800D1960`..`D_800D1988` --
        constants of two still-asm heavy-FP siblings -- in between). The leaf banks only once the
        constants' owners are C. Both edges 8-aligned is necessary, not sufficient.
      - "jtbl-carveable": every condition passes, so the bank-time carve is a single subseg line. S307
        `kSetMultiTLB` was this shape and banked on the first build — the first `jtbl-dispatch` bank
        in `main` since the class was excluded at S265 — which is why the caller counts this verdict
        as `fresh`.

    Reads <fn>'s `.s`, the extracted rodata blobs and the yaml (all cheap, all cached)."""
    syms = _jtbl_syms(fn)
    if not syms:
        return ""
    index = _rodata_blob_index()
    carve_rom = _host_rodata_carve_rom(cfile)
    cohort = []
    for sym in syms:
        entry = index.get(sym)
        if not entry or entry["rom"] is None or not entry["words"]:
            # The table is already attributed to a C file's own carve, or the blob is unreadable:
            # nothing to verify, and nothing to claim.
            continue
        start, end = entry["vram"], entry["vram"] + 4 * entry["words"]
        if start % 8 or end % 8:
            return "jtbl-carve-blocked"
        if carve_rom is None:
            continue
        rom_end = entry["rom"] + 4 * entry["words"]
        if carve_rom <= entry["rom"]:
            # The host's carve PRECEDES the table. It can be extended forward only if nothing
            # foreign sits in the gap; a blob symbol there is data no C body emits, so extending
            # over it removes bytes nobody supplies (S310, `spawn_terrain_effect`: host carve
            # [0xACD10, 0xACD60), table at 0xACD90, six doubles owned by two still-asm siblings in
            # between). Both edges being 8-aligned is necessary and not sufficient.
            carve_end = _rodata_row_ends().get(carve_rom)
            if carve_end is not None:
                blockers = _blob_syms_between(carve_end, entry["rom"])
                if blockers:
                    return "carve-pool-blocked:" + ",".join(blockers[:3])
            continue
        if carve_rom == rom_end:
            # The host's carve starts exactly where this table ends: contiguous, nothing to absorb.
            continue
        for other, oentry in index.items():
            if other == sym or not other.startswith("jtbl_") or oentry["rom"] is None:
                continue
            if rom_end <= oentry["rom"] < carve_rom:
                owner = _jtbl_owner(other)
                if owner and owner != fn and owner not in cohort:
                    cohort.append(owner)
    if cohort:
        return "pool-cohort:" + ",".join(cohort)
    return "jtbl-carveable"


_STRING_REF_RE = re.compile(r"%hi\((D_[0-9A-Fa-f]{8})\)")
_STRING_MAX = 2
_STRING_CHARS = 40


def string_refs(fn):
    """The `.asciz` literals <fn>'s `.s` references, up to _STRING_MAX, truncated.

    A stub's own strings are the cheapest provenance signal there is and the ranker was not reading
    them: S307's `func_8005342C` printed `kSetMultiTLB : Invalid Page Mode\\n`, which named the
    function, its signature and its page-mode table before a single build, and no upstream copy of
    that KMC routine exists to coddog against. Returns a list of strings (possibly empty)."""
    path = _find_asm_s(fn)
    if not path:
        return []
    try:
        with open(path) as f:
            body = f.read()
    except OSError:
        return []
    index = _rodata_blob_index()
    out = []
    for sym in _STRING_REF_RE.findall(body):
        entry = index.get(sym)
        if not entry or not entry["asciz"]:
            continue
        text = entry["asciz"]
        if len(text) > _STRING_CHARS:
            text = text[:_STRING_CHARS] + "..."
        if text not in out:
            out.append(text)
        if len(out) >= _STRING_MAX:
            break
    return out


def _sym_in_generic_rodata_blob(sym):
    """Is <sym> defined in an `asm/data/*.rodata.s` blob? Those blobs are the rodata splat did NOT
    attribute to a C file, so a C body emitting its own copy of that data duplicates it."""
    data_dir = str(ASM_DATA_DIR)
    try:
        names = os.listdir(data_dir)
    except OSError:
        return False
    needle = "dlabel " + sym
    for name in names:
        if not name.endswith(".rodata.s"):
            continue
        try:
            with open(os.path.join(data_dir, name)) as f:
                if needle in f.read():
                    return True
        except OSError:
            continue
    return False


# Full 32-bit form of the same command-word `lui`, for the twin signature below.
_DL_CMD_WORD_RE = re.compile(r"\blui\b\s+\$\w+,\s*\(0x([0-9A-Fa-f]{8})\s*>>\s*16\)")


def dl_word_signature(fn):
    """Sorted multiset of <fn>'s display-list command words, as a tuple, or () if it has fewer than
    _DL_CMD_MIN of them.

    Two emitters with an EQUAL signature emit the same DL commands with the same constant operands,
    which in practice means the same body shape with different globals and guards. Banking one makes
    the other a near-mechanical replay, so the pair is worth ordering adjacently at the plan gate.

    S294 provenance: func_80031450 (0x458) and func_8009351C (0x4CC) sit in different files and have
    an equal signature. The first took 2 iterations from scratch; the second took 2 iterations for
    307 instructions with its whole structure already written, and its only real residual was one
    aliased global read. The size sort alone put 5 unrelated leaves between them."""
    path = _find_asm_s(fn)
    if not path:
        return ()
    try:
        with open(path) as f:
            body = f.read()
    except OSError:
        return ()
    words = [w.upper() for w in _DL_CMD_WORD_RE.findall(body) if int(w[:2], 16) >= 0xC8]
    return tuple(sorted(words)) if len(words) >= _DL_CMD_MIN else ()


def dl_twin_pairs(stubs):
    """Annotate each row of `stubs` with `dl_twin`: another row's fn emitting an equal
    `dl_word_signature`, or "". Mutates and returns `stubs`."""
    by_sig = {}
    for s in stubs:
        sig = dl_word_signature(s["fn"])
        s["dl_twin"] = ""
        if sig:
            by_sig.setdefault(sig, []).append(s)
    for group in by_sig.values():
        if len(group) < 2:
            continue
        names = [g["fn"] for g in group]
        for g in group:
            g["dl_twin"] = ",".join(n for n in names if n != g["fn"])
    return stubs


_FP_ANY_RE = re.compile(
    r"\b(lwc1|swc1|mtc1|mfc1|cvt\.[sdw]\.[sdw]|trunc\.[wl]\.[sd]|bc1[tf]l?"
    r"|c\.[a-z]+\.[sd]|(?:add|sub|mul|div|neg|abs|sqrt|mov)\.[sd])\b"
)
_FP_TOINT_RE = re.compile(r"\b(trunc\.[wl]\.[sd]|cvt\.[wl]\.[sd])\b")
_FP_ARITH_RE = re.compile(r"\b(?:add|sub|mul|div|neg|abs|sqrt)\.[sd]\b")
_FP_SWC1_RE = re.compile(r"\bswc1\b")
_FP_MFC1_RE = re.compile(r"\bmfc1\b")
_FP_MIN = 8  # below this many FP mnemonics the body is not FP-shaped enough to classify
_FP_SCHED_SWC1_MIN = 8  # float stores at/above this, with no to-int conversion, = a stored-FP body


def fp_class_tell(fn):
    """DoR check: <fn>'s FP mnemonics classified by what CONSUMES them. Returns "" / "fp-coord" /
    "fp-sched" / "fp-mixed".

    S290 fixed how to MEASURE FP here (count mnemonics, never `grep '$f[0-9]'`, which returns 0 on a
    heavily-FP function). S295 is why the count alone is not the class. Its gate read `fp=31` on
    func_800880A0 as a risk tell and designated it the pack's first drop; every one of those 31
    mnemonics was rectangle-corner conversion inside a textbook composite emitter, and it banked
    byte-exact on the first build with zero iterations. What the floats FEED is the class:

      - "fp-coord": floats exist only to become integers. `trunc.w.s`/`cvt.w.s` present, `mfc1`
        present (the result crosses to a GPR to become a DL word or an integer argument), few float
        stores, and no more float arithmetic than conversions. Cheap -- the FP is 10.2 fixed-point
        coordinate conversion, not a compute body. S295 func_800880A0: 31 total, toint=6, mfc1=6,
        swc1=2, arith=6.
      - "fp-sched": floats are computed and STORED -- no to-int conversion at all and >= 8 `swc1`.
        This is the S276/S277/S290 FP-scheduler class (a walked float table). S290 func_8005D3B8:
        161 total, toint=0, swc1=36, cvt.s.w=22, arith=37, dropped at the gate on exactly this shape.
      - "fp-mixed": real float arithmetic that is neither purely converted out nor purely stored.
        Unclassified, so price it as risk. S295 rejected func_8006AEA4 (arith=14 > toint=8) and
        func_80083AC8 (arith=13 > toint=3) land here.

    ADVISORY ONLY, and deliberately so: it does not feed `_is_fresh`. It is a first-sprint heuristic
    calibrated on six functions, two of them with a known outcome, which is thin -- and the S275 tag
    is the standing lesson about what over-trusting a fresh `.s` heuristic costs. Read it as a
    pricing hint at the plan gate, then read the `.s`. Reads only <fn>'s own `.s` (cheap)."""
    path = _find_asm_s(fn)
    if not path:
        return ""
    try:
        with open(path) as f:
            body = f.read()
    except OSError:
        return ""
    total = len(_FP_ANY_RE.findall(body))
    if total < _FP_MIN:
        return ""
    toint = len(_FP_TOINT_RE.findall(body))
    swc1 = len(_FP_SWC1_RE.findall(body))
    mfc1 = len(_FP_MFC1_RE.findall(body))
    arith = len(_FP_ARITH_RE.findall(body))
    if toint and mfc1 and swc1 <= toint and arith <= toint:
        return "fp-coord"
    if not toint and swc1 >= _FP_SCHED_SWC1_MIN:
        return "fp-sched"
    return "fp-mixed"


_STUB_FN_RE = re.compile(r'\bINCLUDE_ASM\s*\(\s*"[^"]*"\s*,\s*([A-Za-z0-9_]+)\s*\)')
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
    """List stubs in the requested YAML code bank's C hosts, including libraries."""
    with open(os.path.join(ROOT, "mariogolf64.yaml")) as f:
        segments = yaml.safe_load(f)["segments"]
    bank = next(
        (
            segment
            for segment in segments
            if isinstance(segment, dict)
            and segment.get("type") == "code"
            and segment.get("name") == seg
        ),
        None,
    )
    if bank is None:
        raise ValueError(f"unknown YAML code bank: {seg}")

    hosts = set()
    for subsegment in bank.get("subsegments", []):
        if isinstance(subsegment, list):
            if len(subsegment) < 2 or subsegment[1] != "c":
                continue
            name = subsegment[2] if len(subsegment) > 2 else f"{subsegment[0]:X}"
        elif isinstance(subsegment, dict) and subsegment.get("type") == "c":
            name = subsegment.get("name") or f"{subsegment['start']:X}"
        else:
            continue
        hosts.add(os.path.join(ROOT, "src", bank.get("dir", ""), name + ".c"))

    walls = carried_wall_names()
    backlogged = carry_over_names()
    out = []
    for cpath in sorted(hosts):
        try:
            with open(cpath) as f:
                text = f.read()
        except OSError:
            continue
        rel = os.path.relpath(cpath, ROOT)
        for fn in _STUB_FN_RE.findall(_strip_comments(text)):
            out.append(
                {
                    "fn": fn,
                    "size": _stub_size(fn),
                    "file": rel,
                    "carried": fn in walls,
                    "backlogged": fn in backlogged,
                    "nested": nested_tell(fn),
                    "intrinsic": intrinsic_stub_tell(fn),
                    "wall_class": wall_class_tell(fn),
                    "jtbl_carve": jtbl_carve_tell(fn, rel),
                    "strings": string_refs(fn),
                    "pool_blocked": pool_blocked_tell(fn),
                    "fp_class": fp_class_tell(fn),
                    "single_callee": single_callee_tell(fn),
                }
            )
    out.sort(key=lambda r: (r["size"] if r["size"] is not None else 1 << 30, r["fn"]))
    dl_twin_pairs(out)
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

