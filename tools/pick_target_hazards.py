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

    # --- detail-format factories ------------------------------------------------------------
    # One source of truth for each kind's `detail` encoding; render() output is byte-identical to
    # the old inline f-strings (the goldens + test_hazard_factories.py pin every format). The four
    # kinds whose detail is a pre-assembled variable (combined-subseg, intrinsic-likely,
    # rodata-literal, needs-define) and the bare no-detail kinds (one-tu, non16align, file-static)
    # carry no format literal and are constructed directly at their call sites.

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
        return cls(HAZARD_C_COMBINED, f"{len(c_stems)}file[" + "|".join(sorted(c_stems)) + "]")

    @classmethod
    def jal_count_mismatch(cls, n_c: int, n_asm: int, version_artifact: bool = False) -> "Hazard":
        """`<c>vs<asm>` jal-count divergence. `version_artifact` appends `(version-artifact?)` when
        the surplus C calls are a known per-version wrapper rather than a logic divergence (libnusys
        cont/RMB family's nusys-2.05+ `osSetIntMask` pair vs the pre-2.05 leaf asm, S118) — an
        advisory hint that smallest-first should not be deterred from a clean near-verbatim drop."""
        suffix = "(version-artifact?)" if version_artifact else ""
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
        return cls(HAZARD_GAME_REGION_MIRROR, f"0x{vram:08X}" if vram is not None else hex(off))

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
