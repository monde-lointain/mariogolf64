#!/usr/bin/env python3
"""Asm-scanning primitives for pick_target.py.

The per-subseg `asm/<ROM>.s` listing layer: the path helper, the single
function-body walk every per-function scan filters over, and the scans
themselves (callee/constant signature, jal count, intrinsic shim, unplaced
data/function recovery). Stdlib-only and self-contained — it imports nothing
from pick_target (so the dependency stays one-directional and acyclic).

ROOT is computed the same os.path way pick_target uses (NOT pathlib resolve()):
decomp_common.py's pathlib ROOT_DIR follows symlinks, which could resolve to a
different `asm/` path and shift the ranked rows; keep these two independent.
"""
import os
import re

# tools/decomp_asm.py -> tools/ -> project root (matches pick_target.py exactly).
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)")


def asm_path(rom_off):
    """The asm listing for a subseg: `asm/<ROM>.s` (the file splat emits per ROM offset)."""
    return os.path.join(ROOT, "asm", f"{rom_off:X}.s")


def iter_function_body(rom_off, primary):
    """Yield each body line of `primary` from its asm/<ROM>.s listing.

    Starts after the `glabel <primary>` line; stops at the next `glabel` or a line
    beginning `endlabel` (neither boundary line is yielded). Yields nothing when the
    asm file is absent or the label isn't found. Each per-function asm scan (callee
    set, jal count, intrinsic check, unplaced-call recovery) is a filter over this
    one body walk. (A caller needing to distinguish absent-file from empty-body — see
    _asm_jal_count's None — keeps its own asm_path()/os.path.exists guard.)"""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return
    in_fn = False
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                if in_fn:
                    break
                in_fn = m.group(1) == primary
                continue
            if not in_fn:
                continue
            if line.lstrip().startswith("endlabel"):
                break
            yield line


def iter_subseg_body(rom_off):
    """Yield every body line of asm/<ROM>.s across *all* its functions (the whole subseg).

    Like iter_function_body but un-scoped: skips `glabel`/`endlabel` boundary lines and
    yields the instruction lines of every function in the listing. The asm/<ROM>.s file is
    exactly one subseg = one future C source file = one compiled object, so this is the right
    domain for any scan that concerns the *object's* output rather than a single function's —
    notably the compiler-pooled `.rodata` literals (rodata_literals / rodata_word_refs): a
    `.rodata` sibling subseg places the whole object's `.rodata`, so a sibling function's
    pooled doubles belong to the same split extent as the primary's (S55: guPerspective loads
    0x800D2540..0x2558 that guPerspectiveF does not). Yields nothing when the file is absent."""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return
    with open(path) as f:
        for line in f:
            if GLABEL_RE.match(line) or line.lstrip().startswith("endlabel"):
                continue
            yield line


def asm_functions(rom_off):
    """glabel names defined in asm/<ROM>.s for this subseg (curated if already synced)."""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return []
    names = []
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                names.append(m.group(1))
    return names


# `/* ROM VRAM BYTES */` instruction comment — the second field is the authoritative vram
# splat emitted for that ROM offset. Surfacing it spares the gate a rom→vram derivation for
# the mandatory recover-extern re-confirm (S22: a hand-guessed flat `rom + <const>` resolved
# mid-function and returned a wrong containing fn — read the vram, never guess it).
VRAM_COMMENT_RE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/")


def subseg_vram(rom_off):
    """The target subseg's start vram, read from the first instruction comment in
    asm/<ROM>.s. Returns an int, or None when the asm listing is absent/unreadable."""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        for line in f:
            m = VRAM_COMMENT_RE.search(line)
            if m:
                return int(m.group(1), 16)
    return None


# `/* ROM VRAM BYTES */` with the BYTES (3rd) field captured, for spotting trailing 0x00000000
# nop padding (a higher-alignment gap the next subseg sits at) that a verbatim C mirror can't
# reproduce — the compiler only 16-aligns its `.text`. See code_end_rom + pick_target trailing-pad.
PAD_INSN_RE = re.compile(r"/\*\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]{8}\s+([0-9A-Fa-f]{8})\s*\*/")


def code_end_rom(rom_off):
    """ROM offset just past the last non-nop instruction in asm/<ROM>.s (the real code end,
    excluding trailing 0x00000000 nop padding). Returns None when the listing is absent/empty.

    Used to price a trailing higher-alignment pad: splat extracts the whole subseg slot (function
    + the pad nops up to the next, higher-aligned subseg), but a flipped C mirror's compiler only
    16-aligns its `.text`, so a pad beyond 16B is lost → ROM short → SHA-miss in execution. Reading
    where the real code ends lets the gate price the pad-subseg split (see
    pick_target HAZARD_TRAILING_PAD + docs/hazards.md#trailing-alignment-pad-after-a-c-mirror).
    Approximate by up to one 16B step when a function's real final instruction is a delay-slot nop
    (counted as pad); the flag is advisory and the gate confirms the .o end by compiling."""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return None
    last = None
    with open(path) as f:
        for line in f:
            m = PAD_INSN_RE.search(line)
            if not m:
                continue
            if m.group(2).lower() != "00000000":
                last = int(m.group(1), 16)
    if last is None:
        return None
    return last + 4


# Ops C can't emit (or won't schedule into a leaf delay slot): CP0 register moves
# and a bare FPU sqrt. A tiny leaf whose only work is one of these is really `hasm`,
# not a classical target — flag it so smallest-first stops surfacing it as the pick.
INTRINSIC_OPS = {
    "mfc0", "mtc0", "dmfc0", "dmtc0", "cfc0", "ctc0",
    "sqrt.s", "sqrt.d",
}
INSN_RE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s*\*/\s+(\S+)")


def intrinsic_likely(rom_off, primary):
    """True if the primary fn is a register/FPU intrinsic shim (mfc0/mtc0/sqrt + jr),
    not C-expressible. Reads the fn's body in asm/<ROM>.s: a leaf (no `jal`) whose
    work instructions (excluding jr/nop) are all CP0-moves/sqrt, or one spimdisasm
    tagged `handwritten`."""
    mnemonics = []
    handwritten = False
    for line in iter_function_body(rom_off, primary):
        if "handwritten" in line:
            handwritten = True
        insn = INSN_RE.search(line)
        if insn:
            mnemonics.append(insn.group(1))
    if "jal" in mnemonics:          # a wrapper/has calls → decompilable
        return False
    work = [m for m in mnemonics if m not in ("jr", "nop")]
    if work and all(m in INTRINSIC_OPS for m in work):
        return True
    return handwritten             # spimdisasm-tagged handwritten leaf (no jal)


# Privileged ops gcc never emits from C: TLB writes/probes, CP0 register moves (32/64-bit),
# FPU control-register moves, the `cache` op, and `eret`. A function whose compiled body
# contains ANY of these is definitively a hand-asm TU (zero false-positive risk on a genuine
# classical target — the C compiler never generates them), so it is an asm-mirror candidate,
# NOT something to decompile. This is broader than intrinsic_likely, which only catches a
# *pure* shim (whole body = CP0-moves/sqrt + jr, no jal): a TU that does real work around the
# privileged op — branches/loads (osMapTLB/osUnmapTLB) or even calls (exception dispatch) —
# slips past intrinsic_likely yet is just as much hand-asm. (S70 #1.)
PRIVILEGED_OPS = {
    "tlbwi", "tlbwr", "tlbp", "tlbr",
    "mfc0", "mtc0", "dmfc0", "dmtc0", "cfc0", "ctc0",
    "cfc1", "ctc1",
    "cache", "eret",
}


def privileged_asm(rom_off, primary):
    """True if `primary`'s body (in asm/<ROM>.s) contains a privileged op C cannot emit
    (TLB/CP0/FPU-control/cache/eret) → a hand-asm TU even when it has surrounding control flow
    or calls. Broader than intrinsic_likely; used to flag asm-mirror candidates the pure-shim
    test misses (osMapTLB/osUnmapTLB: a tlbwi + branch/lw logic; an exception dispatcher: a
    privileged op + jals)."""
    for line in iter_function_body(rom_off, primary):
        insn = INSN_RE.search(line)
        if insn and insn.group(1) in PRIVILEGED_OPS:
            return True
    return False


# Asm-side of the signature matcher (the C-side _c_signature + the IDF scoring live in
# pick_target.py). CALL_INSN_RE/IMM_INSN_RE pull the target's callee + constant sets from
# the asm; _FRAME_IMMS is shared with _c_signature (imported back there) so both sides drop
# stack-frame offsets identically.
CALL_INSN_RE = re.compile(r"\bjal\s+([A-Za-z_]\w+)")
# Immediate-bearing ALU/load-imm ops; the operand we want is the trailing immediate literal.
IMM_INSN_RE = re.compile(
    r"\b(?:addiu|ori|andi|xori|lui|li|slti|sltiu|daddiu)\b.*?(-?0x[0-9A-Fa-f]+)\s*$")
# Stack-frame / register-save offsets dominate addiu/sw immediates and carry no identity;
# ignore them so the constant signal isn't pure noise. (Kept tiny — callees do the real work.)
_FRAME_IMMS = {"0x10", "0x14", "0x18", "0x1c", "0x20", "0x24", "0x28", "-0x10",
               "-0x14", "-0x18", "-0x1c", "-0x20", "-0x24", "-0x28"}


def _asm_signature(rom_off, primary):
    """(callee set, constant set) for the target fn from asm/<ROM>.s. Callees exclude internal
    `func_*`/`.L*` targets (only symbolic SDK-ish names carry cross-build identity)."""
    callees, consts = set(), set()
    for line in iter_function_body(rom_off, primary):
        cm = CALL_INSN_RE.search(line)
        if cm and not cm.group(1).startswith("func_"):
            callees.add(cm.group(1))
        im = IMM_INSN_RE.search(line)
        if im and im.group(1).lower() not in _FRAME_IMMS:
            consts.add(im.group(1).lower())
    return callees, consts


def _asm_jal_count(rom_off, primary):
    """Count of `jal <name>` in `primary`'s body in asm/<ROM>.s (incl. func_ targets), or None
    when the asm is absent (e.g. the subseg was already flipped to c)."""
    # Absent-file MUST return None (not 0): call_divergence branches on it, and an
    # asm-present 0-jal fn (jal-count-mismatch:Nvs0) is observably distinct from absent.
    if not os.path.exists(asm_path(rom_off)):
        return None
    n = 0
    for line in iter_function_body(rom_off, primary):
        if CALL_INSN_RE.search(line):
            n += 1
    return n


# splat emits `D_<8-hex-vaddr>` auto-labels for any referenced data address WITHOUT a name in
# the two name files; a *placed* symbol shows its real name instead. So every `D_<addr>` in a
# subseg's asm is, by construction, an unplaced data reference — and the recovered vram is the
# label itself (no MCP `disassemble_function` needed). The fallback `lui reg,0xHHHH` + `addiu`
# pair (raw immediate, not %hi(...)) catches a non-D_ materialization.
DATA_LABEL_RE = re.compile(r"\bD_([0-9A-Fa-f]{8})\b")
RAW_HILO_RE = re.compile(
    r"\blui\s+\$\w+,\s*0x([0-9A-Fa-f]{4})\b.*?\b(?:addiu|lw|sw|lh|sh|lhu|lbu|lb|sb)\s"
    r"[^\n]*?(-?0x[0-9A-Fa-f]+)",
    re.DOTALL,
)


def recover_unplaced_vram(rom_off):
    """Candidate vrams of the unplaced data this subseg references, read from `asm/<ROM>.s`
    deterministically (the asm-data-recovery the execution middle would otherwise do via MCP).
    Returns a sorted list of distinct addresses. The caller binds a name→vram only when the
    mapping is unambiguous (exactly one unplaced upstream name ∩ one candidate) — the
    osYieldThread floor case; multi-extern stays a hint. The gate still confirms before any
    symbol_addrs add, so a stray local-rodata D_ over-listing is harmless."""
    path = asm_path(rom_off)
    if not os.path.exists(path):
        return []
    try:
        text = open(path, errors="ignore").read()
    except OSError:
        return []
    addrs = {int(h, 16) for h in DATA_LABEL_RE.findall(text)}
    for hi, lo in RAW_HILO_RE.findall(text):
        addrs.add(((int(hi, 16) << 16) + int(lo, 16)) & 0xFFFFFFFF)
    return sorted(addrs)


# splat labels a callee whose vram is in NEITHER name file `func_<8-hex>` in the asm jal; a
# *placed* callee shows its real name. So every `jal func_<addr>` is, by construction, an
# unplaced *function* reference — the function dual of the unnamed-data `D_<addr>` label.
FUNC_JAL_RE = re.compile(r"\bjal\s+func_([0-9A-Fa-f]{8})\b")


# A compiler-pooled FP constant load: `ldc1/lwc1 $fX, %lo(D_<addr>)($reg)` — an *anonymous*
# (`D_`-labelled, so absent from both name files) double/float literal GCC emits into the C file's
# `.rodata`. Overlay subsegs label it `D_ovl<N>_<addr>`; the trailing 8 hex is the vram either way.
# A named float global would show its curated name (not `D_`), so it is a placed/recover-extern
# case, not a literal — excluded by requiring the `D_` prefix.
RODATA_LITERAL_RE = re.compile(
    r"\b(?:ldc1|lwc1)\b[^\n]*%lo\(D_(?:ovl\d+_)?([0-9A-Fa-f]{8})\)"
)


def rodata_literals(rom_off):
    """VRAMs of the anonymous compiler-pooled FP constants the *whole subseg* loads via `ldc1/lwc1
    %lo(D_<addr>)` in asm/<ROM>.s. A verbatim mirror's compiler re-emits these into the C object's
    `.rodata`, which must then be placed by a dot-prefixed `.rodata` sibling subseg at the
    constant's ROM offset (docs/hazards.md#rodata-sibling-yaml-pattern, S38/S48). The scan spans
    every function in the subseg, not just the primary: the sibling places the whole object's
    `.rodata`, so a pack sibling's pooled literals belong to the same split extent (S55:
    guPerspective's 0x800D2540..0x2558 — the per-primary scan undercounted 4 of 8). Pre-noting them
    at the gate turns a finalize-time SHA-miss into a planned DoR enabler. Anonymous (`D_`) only:
    a named float global is a placed / recover-extern case, not a literal. Such a load is by
    construction a rodata access (absolute %hi/%lo addressing), so the address always lies outside
    the subseg's own text band. Returns a sorted list of distinct addresses."""
    addrs = set()
    for line in iter_subseg_body(rom_off):
        for h in RODATA_LITERAL_RE.findall(line):
            addrs.add(int(h, 16))
    return sorted(addrs)


# Integer loads of an anonymous pooled constant: `lw/lwu/ld $r, %lo(D_<addr>)($reg)`. GCC moves a
# pooled `double` into an FP register pair with two `lw`s + `mtc1`s (not `ldc1`), so the 2nd (+ any
# further) word of a rodata literal block is invisible to RODATA_LITERAL_RE's FP-only scan.
RODATA_WORD_RE = re.compile(
    r"\b(?:lw|lwu|ld)\b[^\n]*%lo\(D_(?:ovl\d+_)?([0-9A-Fa-f]{8})\)"
)


def rodata_word_refs(rom_off):
    """VRAMs the *whole subseg* loads via integer `lw/lwu/ld %lo(D_<addr>)` in asm/<ROM>.s. The
    caller band-filters these (keeping only code-segment `.rodata` addresses) and unions them with
    the FP literals from rodata_literals to size the full `.rodata` sibling-split extent — GCC loads
    a pooled `double` via an `lw` pair + `mtc1`, so its 2nd word is invisible to the FP-only scan
    (S52: lookatref's 1.0 double at 0x800D2518 was such an `lw` pair). Scans every function in the
    subseg for the same whole-object reason as rodata_literals (S55). Data-segment hits are not
    rodata literals (they are placed/recover-extern data refs) and the caller drops them. Returns a
    sorted list of distinct addresses."""
    addrs = set()
    for line in iter_subseg_body(rom_off):
        for h in RODATA_WORD_RE.findall(line):
            addrs.add(int(h, 16))
    return sorted(addrs)


# A compiler switch jump table: the function loads it via `lui $at, %hi(jtbl_<addr>)` /
# `lw $rd, %lo(jtbl_<addr>)($at)` then an indexed `jr`. spimdisasm names the table `jtbl_<addr>`
# (overlay subsegs `jtbl_ovl<N>_<addr>`); it sits in the code segment's autogenerated `.rodata` and
# its `.word` entries are `.L<addr>` labels INSIDE the function. The jump-table analog of
# RODATA_LITERAL_RE (FP pools) — a `%lo(jtbl_…)` form distinct from `%lo(D_…)`, so the literal scan
# never sees it.
RODATA_JTBL_RE = re.compile(r"%lo\(jtbl_(?:ovl\d+_)?([0-9A-Fa-f]{8})\)")


def rodata_jtbls(rom_off):
    """VRAMs of the switch jump tables the *whole subseg* references via `%lo(jtbl_<addr>)` in
    asm/<ROM>.s. A verbatim mirror's compiler re-emits the table into the C object's `.rodata`,
    which a dot-prefixed `.rodata` sibling subseg must place at the table's ROM offset
    (docs/hazards.md#rodata-sibling-yaml-pattern, S76 __osDevMgrMain). The table's `.word` entries
    are the function's own internal `.L<addr>` labels, so flipping the subseg text->C deletes those
    labels and the still-asm rodata jtbl link-breaks with undefined-`.L<addr>` refs — a break the
    gate's text-only green-ROM check cannot catch by construction (the jtbl stays valid asm until
    the C body lands). Scans every function in the subseg for the same whole-object reason as
    rodata_literals (the sibling places the whole object's `.rodata`). Returns a sorted list of
    distinct table VRAMs."""
    addrs = set()
    for line in iter_subseg_body(rom_off):
        for h in RODATA_JTBL_RE.findall(line):
            addrs.add(int(h, 16))
    return sorted(addrs)


def recover_unplaced_call_vram(rom_off, primary):
    """Addresses of `primary`'s *unnamed* jal targets (`jal func_<hex>`) in asm/<ROM>.s — the
    callees splat couldn't name because they're absent from both name files. The function dual of
    recover_unplaced_vram (data). Returns a sorted list of distinct addresses; the caller binds
    name→vram only when unambiguous (one unplaced upstream call ∩ one unnamed jal). The gate
    confirms before any symbol_addrs add."""
    addrs = set()
    for line in iter_function_body(rom_off, primary):
        for h in FUNC_JAL_RE.findall(line):
            addrs.add(int(h, 16))
    return sorted(addrs)
