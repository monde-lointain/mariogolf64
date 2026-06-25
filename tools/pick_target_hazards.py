#!/usr/bin/env python3
"""Hazard taxonomy for pick_target: the Hazard record + its HAZARD_* kind vocabulary.

Stdlib-only leaf extracted from pick_target.py so every pick_target_* module can
import the Hazard type and the HAZARD_* kinds (referenced across the whole ranker)
without a cycle. Mirrors the decomp_asm.py / cpreprocess.py split: this module
imports nothing back from pick_target.
"""

import dataclasses
from typing import ClassVar

# Coddog match-confidence threshold (percent): a >= hit re-prices an un-named
# candidate as a libultra mirror.
CODDOG_MIRROR_PCT = 99.0


# --- Hazards ---------------------------------------------------------------
# A hazard is a (kind, optional detail) pair, not a formatted string: the kind is
# one of the constants below (the shared vocabulary producers emit and seed_points
# classifies on), and the detail carries the kind-specific payload (the symbol
# list, the count pair, the upstream short-list). render() is the ONLY place a
# hazard becomes a string — `kind` for a bare flag, `kind:detail` otherwise — so
# the printed/JSON form stays exactly what the gate + the hazard index read.
HAZARD_FILE_STATIC = "file-static"
HAZARD_DEFINES_DATA = "defines-data"
HAZARD_BARE_ASSERT = (
    "bare-assert"  # a non-`#ifdef _DEBUG`-guarded upstream assert() the verbatim
)
# mirror compiles in (this build defines neither NDEBUG nor _DEBUG, so <assert.h> expands to a live
# __assert) while the ROM's release libultra stripped asserts → SHA-miss; wrap it in `#ifdef _DEBUG`.
# Advisory, a cheap finalize edit. See docs/hazards.md#assert-strip
HAZARD_REFS_UNPLACED = "refs-unplaced"
HAZARD_CALLS_UNPLACED = "calls-unplaced"
HAZARD_NEEDS_HEADER = "needs-header"
HAZARD_STALE_HEADER = "stale-header"
HAZARD_NEEDS_DEFINE = "needs-define"
HAZARD_JAL_COUNT_MISMATCH = "jal-count-mismatch"
HAZARD_PACK = "pack"
HAZARD_SINGLE_FILE_PACK = "single-file-pack"
HAZARD_ONE_TU = (
    "one-tu"  # every inner fn boundary of a pack is non-16-aligned → ONE .o (the linker
)
# 16-aligns each .o's .text start, so any second .o begins on a 16 boundary). Confirms a
# single-file-pack structurally even when members are un-named, and signals that a per-fn decompose
# split is mechanically blocked.
HAZARD_CODDOG_FNCOUNT_MISMATCH = (
    "coddog-fncount-mismatch"  # a coddog-mirror file defines FEWER fns
)
# than the pack holds → a structural fingerprint match, NOT a source attribution; the pack's real
# source is multi-file. Under-count direction only (a true single source can define MORE, via
# version/_DEBUG-gated extras). Also fires when the coddog identity is carried by a TAIL member, not
# only the leader.
HAZARD_CODDOG_STRUCTURAL = (
    "coddog-structural"  # a coddog-mirror hit whose matched source's expected
)
# compiled size is far below the matched subseg's → a structural fingerprint match, not a source
# attribution. The size-dimension companion to coddog-fncount-mismatch; advisory (does not re-price).
CODDOG_STRUCTURAL_BYTES_PER_LOC = (
    64  # subseg bytes / source meaningful-LOC above this, on a
)
# single-identity multi-fn pack, marks the coddog match structural (conservative: ~4x a dense -O2 fn)
HAZARD_COMBINED_SUBSEG = "combined-subseg"
HAZARD_C_COMBINED = "c-combined"
HAZARD_NON16ALIGN = "non16align"
HAZARD_INTRINSIC_LIKELY = "intrinsic-likely"
HAZARD_MAYBE_UPSTREAM = "maybe-upstream"
HAZARD_RODATA_LITERAL = "rodata-literal"
HAZARD_RODATA_JTBL = (
    "rodata-jtbl"  # a switch jump table the mirror re-emits → .rodata sibling carve
)
HAZARD_DATA_STATIC = "data-static"
HAZARD_TWIN_OF = "twin-of"
HAZARD_REMAINING = "remaining"
HAZARD_CODDOG_MIRROR = "coddog-mirror"  # a coddog compare2 match to an ultralib fn
HAZARD_CODDOG_TWIN = (
    "coddog-twin"  # coddog matched a near-identical TWIN file, but the named members
)
# name the real source; mirror from the member-named source, not the matched file.
HAZARD_CODDOG_SOURCE_BANKED = (
    "coddog-source-banked"  # a coddog-mirror hit whose project mirror is
)
# ALREADY banked (0-stub) in-tree — the source is fully decompiled, so the match can't be a fresh
# attribution; necessarily a fingerprint coincidence. Advisory (does not re-price).
HAZARD_CALLER_EVICT = (
    "caller-evict"  # an un-named func_ member a banked C file calls by name
)
HAZARD_TRAILING_PAD = "trailing-pad"  # trailing nop pad to a higher-aligned next subseg a C mirror can't emit
HAZARD_DROP_STATIC_MIRROR = (
    "drop-static-mirror"  # a coddog-confirmed verbatim mirror whose
)
# file-static + defines-data + refs-unplaced cluster is ONE drop-to-extern enabler (pure .bss, no
# carve) — re-frames the cluster so the gate prices it as a seed-only N-symbol mirror, not a
# carve/classical spike. See docs/hazards.md#file-static-bss-layout-conflict
HAZARD_HEADER_RENAMES_SYMBOL = (
    "header-renames-symbol"  # a (transitively-)vendored header rewrites the
)
# candidate's curated symbol via a macro `#define <curated_fn>(...) <other>` (e.g. the os_host.h K->J
# shim `#define __osInitialize_common() osInitialize()`) → needs a `#undef <curated_fn>` enabler in
# the mirror. See docs/hazards.md#header-renames-symbol
HAZARD_WRONG_GHIDRA_NAME = (
    "wrong-ghidra-name"  # ghidra_symbols mislabels the primary vram with a
)
# macro NAME (e.g. os_motor.h `#define osMotorStop(x) __osMotorAccess(...)`) while the upstream defines
# the macro's RHS (`__osMotorAccess`) at that vram. Bank under the correct (RHS) name via a
# symbol_addrs maintainer-override (a `rom:<off>` qualifier dodges splat's same-rom+segment dup error;
# symbol_addrs is read first so it wins the reference) — NOT the destructive `make sync-names`.
# Detail `<ghidra_name>-><correct_name>@<header>`. See docs/hazards.md#wrong-ghidra-name-override
HAZARD_CODDOG_PARTIAL = (
    "coddog-partial"  # coddog matched >=2 DISTINCT per-fn TWIN files to ONE
)
# multi-fn subseg, covering only a SUBSET of its fns → a per-fn fingerprint SET, NOT a single-file
# mirror; the un-matched fns need per-fn verification. The multi-twin companion to
# coddog-fncount-mismatch (which fires only at len(distinct)==1). Advisory. See docs/hazards.md#coddog-cross-ref
HAZARD_GAME_REGION_MIRROR = (
    "game-region-mirror"  # a coddog/libultra-source mirror whose vram is BELOW
)
# the libultra code band is statically linked INTO THE GAME and compiled -O2 (game CFLAGS), NOT -O3
# (LIBULTRA_CFLAGS) — the src/libultra/ path would force -O3 → wrong auto-inlining. Route the mirror
# to a -O2 path (src/mgu/…). Advisory. Detail `0x<vram>`. See docs/hazards.md#game-region-mirror-o2-profile
HAZARD_GAME_EMBEDDED = (
    "game-embedded"  # a coddog-mirror that is NOT a standalone-carvable object: it is
)
# game-region-mirror (below the libultra band, -O2) AND covers only a SUBSET of its subseg's fns
# (coddog-fncount-mismatch / coddog-structural / coddog-partial) → the upstream file's fns are compiled
# INTO a larger game audio TU, tight-packed with game fns at non-16 boundaries, so a standalone carve
# SHA-misses (KMC `as` 16-pads the .o .text, shifting the tail; #non16align). Plan a MIXED game-region
# carve at 16-aligned bounds (bank the lib fns as a mirror + the adjacent game fns classical), NOT a
# seed-only verbatim mirror. The synthesis of game-region-mirror + a coddog-subset signal (S128
# audio_mgr.c: nualstl3 embedded in a 27KB game audio grab-bag). Detail `0x<vram>`. Advisory. See
# docs/hazards.md#game-region-mirror-o2-profile
HAZARD_UPSTREAM_FNCOUNT_MISMATCH = (
    "upstream-fncount-mismatch"  # a pack with ONE named C stem whose
)
# upstream .c defines FEWER functions than the pack holds → the extra members are a FOREIGN TU bundled
# in the subseg (split it off, mirror only the upstream's fns). The named-symbol analog of
# coddog-fncount-mismatch. Detail `<members>vs<defs>`. Advisory. See docs/hazards.md#upstream-mirror-pattern
HAZARD_DATA_CARVE = (
    "data-carve"  # the upstream .c defines a file-scope INITIALIZED static array
)
# (`static <type> <name>[…] = <init>;`, NON-const → .data) the verbatim mirror re-emits → needs a
# `.data` sibling carve at the asm-recovered vram. `static` bars cross-file linkage so the array is
# file-PRIVATE. Fired ONLY on the single-file (non c-combined) subset. Detail `<names>`. Advisory.
# See docs/hazards.md#defines-data
HAZARD_BLOCK_REORDER_SIBLING = (
    "block-reorder-sibling"  # a libnusys candidate carrying the
)
# block-reorder tell (an unexplained jal-mismatch + NO coddog-mirror; the reorder breaks coddog's
# fingerprint) whose upstream-file family has an ALREADY-BANKED block-reorder mirror — MG64's per-file
# nusys revision swaps two source blocks vs every archived SDK (the GBPak F-variants run the RAM-enable
# block before nuContGBPakCheckConnector). The verbatim cp needs the SAME swap the sibling needed, so
# the gate applies it UP-FRONT instead of rediscovering it via a re-attempt (S119 nucontgbpakfread ->
# S120 nucontgbpakfwrite, first-build clean). Detail `<sibling.c>`. Advisory. See
# docs/hazards.md#near-verbatim-mirror-jal-count-mismatch
HAZARD_UNATTRIB_LEAF = (
    "unattrib-leaf"  # within a c-combined pack (≥2 distinct C upstream files), a
)
# `func_<addr>=?` member attributed to NEITHER file that STRADDLES a file boundary — the nearest named-C
# members before AND after it resolve to DIFFERENT upstream stems. The file-boundary split must assign it
# to one side; a silent `?` could ride into the wrong singleton (S120 func_800A2780, between
# nucontgbpakfwrite and nusimgr). Front/trailing `?` (within one file) do NOT fire. Fired only for a
# LONE straddler (a clean 2-file split with one stray leaf, not a whole interleaved foreign TU).
# Detail `0x<vram>` (comma list). Advisory. See docs/hazards.md#multi-function-segment-splitting-pack
HAZARD_BODY_DIVERGENCE_SUSPECT = (
    "body-divergence-suspect"  # a SUB-100 coddog-mirror (near-verbatim
)
# but NOT byte-exact, e.g. @99.99) can mask a GAME-MODIFIED body, not just a block-reorder/compiler
# artifact: an extra branch/store the literal upstream lacks. Flags the row for a body-store-value
# diagnosis (read the target's store SEQUENCE + VALUES) BEFORE declaring a clean mirror OR a compiler
# wall. S121 contRmbControl @99.99 was a game-modified FORCESTOP (state=STOPPED on osMotorInit error),
# misread as a #cross-jump-tail-merge wall for 5 sprints; resolved S127 with a one-branch fix. Detail
# `<file>@<pct>`. Advisory. See docs/hazards.md#cross-jump-tail-merge

HAZARD_STATIC_NAME_COLLISION = (
    "static-name-collision"  # a coddog-mirror upstream defines a FILE-STATIC whose
)
# verbatim name is ALREADY a curated symbol placed at a DIFFERENT vram (a non-micro twin holds the
# name): naming THIS subseg's instance as that global would multiply-define the label at the gate
# scaffold, so the gate keeps it file-local instead of adding a colliding symbol_addrs entry. Detail
# `<name>@<existing-addr>`. Advisory. S135 n_load _decodeChunk (0x8009FA14) vs the placed
# _decodeChunk @0x800A4E3C. See docs/hazards.md#static-name-collision


@dataclasses.dataclass
class Hazard:
    """One flagged hazard: a `kind` constant + an optional `detail` payload.

    `detail` holds everything after the kind's colon verbatim — a comma list
    (`defines-data:a,b`), a count pair (`jal-count-mismatch:2vs1`), a bracketed
    pack (`pack:2fn[a=foo]`), or a colon-bearing short-list (`maybe-upstream:
    libultra:f1,f2`). render() emits it verbatim. The handful of callers that
    need a FIELD back out of the string go through the named accessors below
    (the coddog `<file>@<pct>` and twin `<stem>!=<...>` encodings), so that
    parse/encode knowledge lives here, not scattered across the ranker."""

    kind: str
    detail: str | None = None

    # Semantic kind groups: one source of truth for the "is this kind in group X"
    # tests the ranker used to spell out inline (ClassVar -> not dataclass fields).
    CARVE_SIGNALS: ClassVar[frozenset[str]] = frozenset(
        {HAZARD_RODATA_LITERAL, HAZARD_DATA_STATIC, HAZARD_RODATA_JTBL}
    )
    CODDOG_MASKS: ClassVar[frozenset[str]] = frozenset(
        {HAZARD_CODDOG_STRUCTURAL, HAZARD_CODDOG_SOURCE_BANKED}
    )
    RODATA_OWNERS: ClassVar[frozenset[str]] = frozenset(
        {HAZARD_RODATA_JTBL, HAZARD_RODATA_LITERAL}
    )
    # A coddog hit covering only a SUBSET of a multi-fn subseg (the matched upstream file's fns are
    # interspersed with un-matched/game fns) — the game-embedded synthesis keys off this.
    CODDOG_SUBSET_SIGNALS: ClassVar[frozenset[str]] = frozenset(
        {HAZARD_CODDOG_FNCOUNT_MISMATCH, HAZARD_CODDOG_STRUCTURAL, HAZARD_CODDOG_PARTIAL}
    )

    def render(self) -> str:
        return self.kind if self.detail is None else f"{self.kind}:{self.detail}"

    # --- semantic predicates (centralize the scattered kind-knowledge) ----------------------
    def is_coddog_mirror(self) -> bool:
        return self.kind == HAZARD_CODDOG_MIRROR

    def is_file_static(self) -> bool:
        return self.kind == HAZARD_FILE_STATIC

    def is_carve_signal(self) -> bool:
        """A `.rodata`/`.data` carve signal (rodata-literal/jtbl, data-static)."""
        return self.kind in self.CARVE_SIGNALS

    def is_coddog_mask(self) -> bool:
        """A coddog hit that masks divergence (structural / already-banked source)."""
        return self.kind in self.CODDOG_MASKS

    def is_rodata_owner(self) -> bool:
        """A pooled-rodata owner (literal or switch jtbl) the sibling carve places."""
        return self.kind in self.RODATA_OWNERS

    def is_game_region_mirror(self) -> bool:
        """A libultra/SDK-source mirror linked into the game (below the libultra band, -O2)."""
        return self.kind == HAZARD_GAME_REGION_MIRROR

    def is_coddog_subset_signal(self) -> bool:
        """A coddog hit covering only a SUBSET of the subseg (fncount-mismatch/structural/partial)."""
        return self.kind in self.CODDOG_SUBSET_SIGNALS

    def is_unexplained_jal(self) -> bool:
        """A jal-count mismatch NOT annotated as a known artifact: a per-version wrapper (S118
        libnusys `osSetIntMask` pair) or a single-real-call macro expansion (S136 n_synthesizer's
        `alHeapAlloc`->`alHeapDBAlloc`, 5 invocations = the whole 3vs8 gap). Either annotation means
        the surplus jals are accounted for, not a logic divergence."""
        d = self.detail or ""
        return (
            self.kind == HAZARD_JAL_COUNT_MISMATCH
            and "(version-artifact?)" not in d
            and "(macro-artifact?)" not in d
        )

    def is_jal_artifact(self) -> bool:
        """A jal-count mismatch ANNOTATED as a known artifact (version wrapper / macro expansion).
        The complement of is_unexplained_jal, restricted to the jal-count kind."""
        return self.kind == HAZARD_JAL_COUNT_MISMATCH and not self.is_unexplained_jal()

    @classmethod
    def coddog_mirror(cls, cfile: str, pct: float) -> "Hazard":
        """A coddog-mirror flag, detail encoded as `<file>@<pct>` (2-decimal)."""
        return cls(HAZARD_CODDOG_MIRROR, f"{cfile}@{pct:.2f}")

    @classmethod
    def body_divergence_suspect(cls, cfile: str, pct: float) -> "Hazard":
        """A sub-100 coddog-mirror that may mask a game-modified body; detail `<file>@<pct>`."""
        return cls(HAZARD_BODY_DIVERGENCE_SUSPECT, f"{cfile}@{pct:.2f}")

    @classmethod
    def game_embedded(cls, vram: int) -> "Hazard":
        """A game-embedded coddog-mirror (not standalone-carvable); detail `0x<vram>`."""
        return cls(HAZARD_GAME_EMBEDDED, f"0x{vram:08X}")

    @classmethod
    def game_embedded_for(cls, hazards, vram):
        """Synthesize game-embedded from an assembled hazard set, or None.

        A coddog-mirror that is ALSO a game-region-mirror AND covers only a SUBSET of its subseg's
        fns (a coddog-subset signal) is compiled INTO a game TU, tight-packed with game fns, so a
        standalone carve SHA-misses on the non-16 tail. The row should be planned as a mixed
        16-aligned game-region carve, not a seed-only verbatim mirror (S128 audio_mgr.c)."""
        if vram is None:
            return None
        if (
            any(h.is_coddog_mirror() for h in hazards)
            and any(h.is_game_region_mirror() for h in hazards)
            and any(h.is_coddog_subset_signal() for h in hazards)
        ):
            return cls.game_embedded(vram)
        return None

    # --- detail-format factories ------------------------------------------------------------
    # One source of truth for each kind's `detail` encoding; render() output is byte-identical to
    # the old inline f-strings (the goldens + test_hazard_factories.py pin every format). ALL
    # construction goes through a factory (no bare `Hazard(KIND, ...)` at call sites): the bare
    # no-detail kinds and the pass-through-detail kinds get trivial factories below so the kind
    # constant is referenced in exactly one place.

    @classmethod
    def one_tu(cls) -> "Hazard":
        """A single hand-asm TU subseg (no detail)."""
        return cls(HAZARD_ONE_TU)

    @classmethod
    def non16align(cls) -> "Hazard":
        """A non-16-aligned inner boundary (no detail)."""
        return cls(HAZARD_NON16ALIGN)

    @classmethod
    def file_static(cls) -> "Hazard":
        """A file-scope static the verbatim mirror must place (no detail)."""
        return cls(HAZARD_FILE_STATIC)

    @classmethod
    def combined_subseg(cls, detail: str) -> "Hazard":
        """A multi-TU asm subseg; `detail` is pre-assembled by the caller."""
        return cls(HAZARD_COMBINED_SUBSEG, detail)

    @classmethod
    def intrinsic_likely(cls, detail: str) -> "Hazard":
        """An intrinsic-likely asm-mirror TU; `detail` is pre-assembled by the caller."""
        return cls(HAZARD_INTRINSIC_LIKELY, detail)

    @classmethod
    def needs_define(cls, detail: str) -> "Hazard":
        """A missing `-D` define gating the mirror; `detail` is pre-assembled by the caller."""
        return cls(HAZARD_NEEDS_DEFINE, detail)

    @classmethod
    def rodata_literal(cls, detail: str) -> "Hazard":
        """A pooled `.rodata` literal needing a sibling carve; `detail` pre-assembled by the caller."""
        return cls(HAZARD_RODATA_LITERAL, detail)

    @classmethod
    def pack(cls, kind: str, n_fns: int, members) -> "Hazard":
        """pack / single-file-pack: `<n>fn[member=alias,...]` (caller picks the kind constant)."""
        return cls(kind, f"{n_fns}fn[" + ",".join(members) + "]")

    @classmethod
    def upstream_fncount_mismatch(cls, n_members: int, n_defs: int) -> "Hazard":
        """`<members>vs<defs>` — pack has more fns than its single named .c defines."""
        return cls(HAZARD_UPSTREAM_FNCOUNT_MISMATCH, f"{n_members}vs{n_defs}")

    @classmethod
    def c_combined(cls, c_stems) -> "Hazard":
        """`<n>file[stem|stem|...]` — ≥2 distinct C upstream files in one asm subseg."""
        return cls(
            HAZARD_C_COMBINED, f"{len(c_stems)}file[" + "|".join(sorted(c_stems)) + "]"
        )

    @classmethod
    def jal_count_mismatch(
        cls,
        n_c: int,
        n_asm: int,
        version_artifact: bool = False,
        macro_artifact: bool = False,
    ) -> "Hazard":
        """`<c>vs<asm>` jal-count divergence. An artifact suffix marks the surplus jals as accounted
        for (not a logic divergence), an advisory hint that smallest-first should not be deterred from
        a clean near-verbatim drop: `(version-artifact?)` for a known per-version wrapper (libnusys
        cont/RMB family's nusys-2.05+ `osSetIntMask` pair vs the pre-2.05 leaf asm, S118);
        `(macro-artifact?)` when the asm surplus is EXACTLY the count of single-real-call macros the C
        invokes (each emits one `jal` but `_c_jal_count` drops it as a macro -> the C side under-counts;
        S136 n_synthesizer 3vs8 = 5 `alHeapAlloc` calls). version_artifact takes precedence if both."""
        suffix = (
            "(version-artifact?)"
            if version_artifact
            else "(macro-artifact?)" if macro_artifact else ""
        )
        return cls(HAZARD_JAL_COUNT_MISMATCH, f"{n_c}vs{n_asm}{suffix}")

    @classmethod
    def maybe_upstream(cls, lib: str, bases) -> "Hazard":
        """`<lib>:<name,...>` signature-hint match."""
        return cls(HAZARD_MAYBE_UPSTREAM, f"{lib}:" + ",".join(bases))

    @classmethod
    def trailing_pad(cls, residual: int, align: int) -> "Hazard":
        """`<n>B@<align>` nop-pad after a short C mirror."""
        return cls(HAZARD_TRAILING_PAD, f"{residual}B@{align}")

    @classmethod
    def remaining(cls, n: int) -> "Hazard":
        """`<n>` residual count."""
        return cls(HAZARD_REMAINING, str(n))

    @classmethod
    def rodata_jtbl(cls, addrs) -> "Hazard":
        """Comma list of `0x%08X` switch-jtbl vrams (sorted)."""
        return cls(HAZARD_RODATA_JTBL, ",".join(f"0x{a:08X}" for a in sorted(addrs)))

    @classmethod
    def data_static(cls, addrs) -> "Hazard":
        """Comma list of `0x%08X` file-static data vrams (sorted)."""
        return cls(HAZARD_DATA_STATIC, ",".join(f"0x{a:08X}" for a in sorted(addrs)))

    @classmethod
    def defines_data(cls, names) -> "Hazard":
        """Comma list of file-scope data symbol names."""
        return cls(HAZARD_DEFINES_DATA, ",".join(names))

    @classmethod
    def refs_unplaced(cls, refs) -> "Hazard":
        """Comma list of un-placed extern data refs (each may carry `@<vram>`)."""
        return cls(HAZARD_REFS_UNPLACED, ",".join(refs))

    @classmethod
    def calls_unplaced(cls, names) -> "Hazard":
        """Comma list of un-placed callee names."""
        return cls(HAZARD_CALLS_UNPLACED, ",".join(names))

    @classmethod
    def data_carve(cls, arrays) -> "Hazard":
        """Comma list of file-scope initialized-array names (.data sibling carve)."""
        return cls(HAZARD_DATA_CARVE, ",".join(arrays))

    @classmethod
    def block_reorder_sibling(cls, sibling: str) -> "Hazard":
        """`<sibling.c>` — the banked same-family block-reorder mirror whose swap fix replays here."""
        return cls(HAZARD_BLOCK_REORDER_SIBLING, sibling)

    @classmethod
    def unattrib_leaf(cls, addrs) -> "Hazard":
        """Comma list of `0x%08X` vrams of inter-file `?` leaves straddling a c-combined file boundary."""
        return cls(HAZARD_UNATTRIB_LEAF, ",".join(f"0x{a:08X}" for a in sorted(addrs)))

    @classmethod
    def needs_header(cls, tagged) -> "Hazard":
        """Comma list of needs-header entries (each may carry a `(...)` adapt tag)."""
        return cls(HAZARD_NEEDS_HEADER, ",".join(tagged))

    @classmethod
    def header_renames_symbol(cls, primary: str, header: str) -> "Hazard":
        """`<primary>@<header>` — a vendored header macro rewrites the curated symbol."""
        return cls(HAZARD_HEADER_RENAMES_SYMBOL, f"{primary}@{header}")

    @classmethod
    def wrong_ghidra_name(cls, primary: str, correct: str, header: str) -> "Hazard":
        """`<ghidra>-><correct>@<header>` — ghidra_symbols mislabels the vram with a macro name."""
        return cls(HAZARD_WRONG_GHIDRA_NAME, f"{primary}->{correct}@{header}")

    @classmethod
    def stale_header(cls, missing) -> "Hazard":
        """`os_version.h(<token,...>)` — header resolves but is missing referenced VERSION_* tokens."""
        return cls(HAZARD_STALE_HEADER, "os_version.h(" + ",".join(missing) + ")")

    @classmethod
    def bare_assert(cls, n: int) -> "Hazard":
        """`<n>` un-guarded upstream assert() count."""
        return cls(HAZARD_BARE_ASSERT, str(n))

    @classmethod
    def twin_of(cls, stem: str) -> "Hazard":
        """`<stem>` of a banked sibling sharing the same ld-section carve."""
        return cls(HAZARD_TWIN_OF, stem)

    @classmethod
    def drop_static_mirror(cls, n: int) -> "Hazard":
        """`<n>bss` (or bare `bss`) drop-to-extern enabler."""
        return cls(HAZARD_DROP_STATIC_MIRROR, f"{n}bss" if n else "bss")

    @classmethod
    def caller_evict(cls, evicts) -> "Hazard":
        """`;`-joined `<fn>@<header,...>` caller-eviction entries."""
        return cls(HAZARD_CALLER_EVICT, ";".join(evicts))

    @classmethod
    def coddog_twin(cls, cstem: str, member_stems) -> "Hazard":
        """coddog-twin: `<stem>!=<alt,...>` (sorted alts; paired with twin_file())."""
        return cls(HAZARD_CODDOG_TWIN, f"{cstem}!={','.join(sorted(member_stems))}")

    @classmethod
    def coddog_fncount_mismatch(cls, matched_n: int, n_fns: int) -> "Hazard":
        """`<matched>vs<pack>` — coddog source defines fewer fns than the pack holds."""
        return cls(HAZARD_CODDOG_FNCOUNT_MISMATCH, f"{matched_n}vs{n_fns}")

    @classmethod
    def coddog_structural(cls, cfile: str, pct: float) -> "Hazard":
        """coddog-structural: `<file>@<pct>` (paired with coddog_source()/coddog_pct())."""
        return cls(HAZARD_CODDOG_STRUCTURAL, f"{cfile}@{pct:.2f}")

    @classmethod
    def coddog_partial(cls, matched: int, total: int) -> "Hazard":
        """`<m>of<n>fn` — per-fn twin set covering only a subset of a multi-fn pack."""
        return cls(HAZARD_CODDOG_PARTIAL, f"{matched}of{total}fn")

    @classmethod
    def coddog_source_banked(cls, basename: str) -> "Hazard":
        """The coddog source's basename (already banked)."""
        return cls(HAZARD_CODDOG_SOURCE_BANKED, basename)

    @classmethod
    def game_region_mirror(cls, vram, off: int) -> "Hazard":
        """`0x%08X` of the vram (or hex(off) fallback) for a game-region (-O2) mirror."""
        return cls(
            HAZARD_GAME_REGION_MIRROR, f"0x{vram:08X}" if vram is not None else hex(off)
        )

    @classmethod
    def static_name_collision(cls, name: str, existing_addr: str) -> "Hazard":
        """`<name>@<existing-addr>` — an upstream file-static whose name already names a DIFFERENT
        placed vram (keep this instance file-local, no colliding symbol_addrs add)."""
        return cls(HAZARD_STATIC_NAME_COLLISION, f"{name}@{existing_addr}")

    def coddog_file(self) -> str:
        """The `<file>` half of a coddog `<file>@<pct>` detail."""
        return self.detail.split("@", 1)[0]

    def coddog_pct(self) -> float:
        """The `<pct>` half of a coddog `<file>@<pct>` detail, or 0.0 if malformed."""
        try:
            return float(self.detail.split("@", 1)[1])
        except (IndexError, ValueError):
            return 0.0

    def coddog_source(self) -> str:
        """The source path of a coddog detail, tolerating an `@` in the path (rsplit)."""
        return self.detail.rsplit("@", 1)[0]

    def twin_file(self) -> str:
        """The primary `<stem>` of a coddog-twin `<stem>!=<alt,...>` detail."""
        return self.detail.split("!=", 1)[0]

    def mark_owner_per_member(self) -> None:
        """Append the `;owner-per-member` marker once, in place (idempotent)."""
        if "owner-per-member" not in (self.detail or ""):
            self.detail = f"{self.detail or ''};owner-per-member"
