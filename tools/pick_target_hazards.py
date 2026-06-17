#!/usr/bin/env python3
"""Hazard taxonomy for pick_target: the Hazard record + its HAZARD_* kind vocabulary.

Stdlib-only leaf extracted from pick_target.py so every pick_target_* module can
import the Hazard type and the HAZARD_* kinds (referenced across the whole ranker)
without a cycle. Mirrors the decomp_asm.py / cpreprocess.py split: this module
imports nothing back from pick_target.
"""
import dataclasses

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
HAZARD_BARE_ASSERT = "bare-assert"  # a non-`#ifdef _DEBUG`-guarded upstream assert() the verbatim
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
HAZARD_ONE_TU = "one-tu"  # every inner fn boundary of a pack is non-16-aligned → ONE .o (the linker
# 16-aligns each .o's .text start, so any second .o begins on a 16 boundary). Confirms a
# single-file-pack structurally even when members are un-named, and signals that a per-fn decompose
# split is mechanically blocked.
HAZARD_CODDOG_FNCOUNT_MISMATCH = "coddog-fncount-mismatch"  # a coddog-mirror file defines FEWER fns
# than the pack holds → a structural fingerprint match, NOT a source attribution; the pack's real
# source is multi-file. Under-count direction only (a true single source can define MORE, via
# version/_DEBUG-gated extras). Also fires when the coddog identity is carried by a TAIL member, not
# only the leader.
HAZARD_CODDOG_STRUCTURAL = "coddog-structural"  # a coddog-mirror hit whose matched source's expected
# compiled size is far below the matched subseg's → a structural fingerprint match, not a source
# attribution. The size-dimension companion to coddog-fncount-mismatch; advisory (does not re-price).
CODDOG_STRUCTURAL_BYTES_PER_LOC = 64  # subseg bytes / source meaningful-LOC above this, on a
# single-identity multi-fn pack, marks the coddog match structural (conservative: ~4x a dense -O2 fn)
HAZARD_COMBINED_SUBSEG = "combined-subseg"
HAZARD_C_COMBINED = "c-combined"
HAZARD_NON16ALIGN = "non16align"
HAZARD_INTRINSIC_LIKELY = "intrinsic-likely"
HAZARD_MAYBE_UPSTREAM = "maybe-upstream"
HAZARD_RODATA_LITERAL = "rodata-literal"
HAZARD_RODATA_JTBL = "rodata-jtbl"  # a switch jump table the mirror re-emits → .rodata sibling carve
HAZARD_DATA_STATIC = "data-static"
HAZARD_TWIN_OF = "twin-of"
HAZARD_REMAINING = "remaining"
HAZARD_CODDOG_MIRROR = "coddog-mirror"  # a coddog compare2 match to an ultralib fn
HAZARD_CODDOG_TWIN = "coddog-twin"  # coddog matched a near-identical TWIN file, but the named members
# name the real source; mirror from the member-named source, not the matched file.
HAZARD_CODDOG_SOURCE_BANKED = "coddog-source-banked"  # a coddog-mirror hit whose project mirror is
# ALREADY banked (0-stub) in-tree — the source is fully decompiled, so the match can't be a fresh
# attribution; necessarily a fingerprint coincidence. Advisory (does not re-price).
HAZARD_CALLER_EVICT = "caller-evict"  # an un-named func_ member a banked C file calls by name
HAZARD_TRAILING_PAD = "trailing-pad"  # trailing nop pad to a higher-aligned next subseg a C mirror can't emit
HAZARD_DROP_STATIC_MIRROR = "drop-static-mirror"  # a coddog-confirmed verbatim mirror whose
# file-static + defines-data + refs-unplaced cluster is ONE drop-to-extern enabler (pure .bss, no
# carve) — re-frames the cluster so the gate prices it as a seed-only N-symbol mirror, not a
# carve/classical spike. See docs/hazards.md#file-static-bss-layout-conflict
HAZARD_HEADER_RENAMES_SYMBOL = "header-renames-symbol"  # a (transitively-)vendored header rewrites the
# candidate's curated symbol via a macro `#define <curated_fn>(...) <other>` (e.g. the os_host.h K->J
# shim `#define __osInitialize_common() osInitialize()`) → needs a `#undef <curated_fn>` enabler in
# the mirror. See docs/hazards.md#header-renames-symbol
HAZARD_WRONG_GHIDRA_NAME = "wrong-ghidra-name"  # ghidra_symbols mislabels the primary vram with a
# macro NAME (e.g. os_motor.h `#define osMotorStop(x) __osMotorAccess(...)`) while the upstream defines
# the macro's RHS (`__osMotorAccess`) at that vram. Bank under the correct (RHS) name via a
# symbol_addrs maintainer-override (a `rom:<off>` qualifier dodges splat's same-rom+segment dup error;
# symbol_addrs is read first so it wins the reference) — NOT the destructive `make sync-names`.
# Detail `<ghidra_name>-><correct_name>@<header>`. See docs/hazards.md#wrong-ghidra-name-override
HAZARD_CODDOG_PARTIAL = "coddog-partial"  # coddog matched >=2 DISTINCT per-fn TWIN files to ONE
# multi-fn subseg, covering only a SUBSET of its fns → a per-fn fingerprint SET, NOT a single-file
# mirror; the un-matched fns need per-fn verification. The multi-twin companion to
# coddog-fncount-mismatch (which fires only at len(distinct)==1). Advisory. See docs/hazards.md#coddog-cross-ref
HAZARD_GAME_REGION_MIRROR = "game-region-mirror"  # a coddog/libultra-source mirror whose vram is BELOW
# the libultra code band is statically linked INTO THE GAME and compiled -O2 (game CFLAGS), NOT -O3
# (LIBULTRA_CFLAGS) — the src/libultra/ path would force -O3 → wrong auto-inlining. Route the mirror
# to a -O2 path (src/mgu/…). Advisory. Detail `0x<vram>`. See docs/hazards.md#game-region-mirror-o2-profile
HAZARD_UPSTREAM_FNCOUNT_MISMATCH = "upstream-fncount-mismatch"  # a pack with ONE named C stem whose
# upstream .c defines FEWER functions than the pack holds → the extra members are a FOREIGN TU bundled
# in the subseg (split it off, mirror only the upstream's fns). The named-symbol analog of
# coddog-fncount-mismatch. Detail `<members>vs<defs>`. Advisory. See docs/hazards.md#upstream-mirror-pattern
HAZARD_DATA_CARVE = "data-carve"  # the upstream .c defines a file-scope INITIALIZED static array
# (`static <type> <name>[…] = <init>;`, NON-const → .data) the verbatim mirror re-emits → needs a
# `.data` sibling carve at the asm-recovered vram. `static` bars cross-file linkage so the array is
# file-PRIVATE. Fired ONLY on the single-file (non c-combined) subset. Detail `<names>`. Advisory.
# See docs/hazards.md#defines-data


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

    def render(self) -> str:
        return self.kind if self.detail is None else f"{self.kind}:{self.detail}"

    @classmethod
    def coddog_mirror(cls, cfile: str, pct: float) -> "Hazard":
        """A coddog-mirror flag, detail encoded as `<file>@<pct>` (2-decimal)."""
        return cls(HAZARD_CODDOG_MIRROR, f"{cfile}@{pct:.2f}")

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
