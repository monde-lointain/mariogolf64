# Sprint gates (plan and review)

<role>
The two Scrum gates that bound every sprint, for both harnesses. Read this at the plan gate and at
the review gate; the execution loop between them is `docs/workflow/loop.md`. Everything here is
harness-neutral -- where Claude Code and Codex genuinely differ, the step says so.
</role>

## Gate entry points

- **Claude Code:** `/sprint-plan [scope]`, `/sprint-review`.
- **Codex:** `$mg64-sprint-plan [scope]`, `$mg64-sprint-review`.
- **`sprint-plan [scope]`**: ranks candidates with `tools/pick_target.py` (smallest-first), proposes
  one small increment plus its enablers, gets Product Owner approval, then performs and validates the
  flip, and writes `SPRINT.md`. Execution follows over the committed backlog.
- **`sprint-review`**: verifies the ROM-SHA-1 Definition of Done on the increment, takes scope
  sign-off, and is the single apply-point for the sprint's buffered workflow-improvement edits. It
  appends the `RETRO.md` digest and the `BACKLOG.md` carry-overs.

## Sprint-plan procedure

1. Validate the optional scope argument against `^[a-z][a-z0-9_-]*$`; abort on mismatch.
2. Read `BACKLOG.md` and run `venv/bin/python3 tools/pick_target.py -n 12`. Filter by the kind of
   scope: `--lib <name>` for a library band, `--segment <name>` or `--loose-stubs <name>` for a
   segment such as `main`. Passing a segment to `--lib` returns unrelated overlay and libnusys rows
   instead of erroring. Run `--loose-stubs <seg>` for a segment scope even when another subcommand
   already gave you the ranking: it is the only place the plateau advisory prints, and that advisory
   is what tells you `fresh` is a ceiling rather than a clean pool.
3. Pick the smallest coherent increment, honoring carry-overs, hazards, the 8-point gate, and the
   Definition of Ready below. Run `--carried-check` and `--nested-check` on every hand-mined leaf.
4. Present the goal, committed backlog, gate enablers, snapshot, and story-point estimate to the
   Product Owner. Use the harness's structured-input tool when it has one; otherwise ask directly.
5. After approval, perform only gate enablers: `mariogolf64.yaml` subseg flip/split/path-qualifier
   lines, `symbol_addrs.txt` add-only, and optional `make sync-names`.
6. Validate with `tools/verify-rom.sh --extract`; require exit 0. It derives the expected SHA-1 from
   `mariogolf64.yaml` and asserts the `build/mariogolf64.z64: OK` line before trusting `sha1sum`.
7. Write `SPRINT.md`, then run `venv/bin/python3 tools/prompt_lint.py check --sprint` and de-shout
   whatever it flags in the instruction sections. If the committed regime is classical or mixed, make
   the seed-freeze commit as documented. Keep this gate cheap.
8. Hand off to the execution loop (`docs/workflow/loop.md`). Codex needs an explicit
   `$mg64-decomp-loop` invocation; Claude Code continues inline or via `/decomp-loop`. Do not stop for
   per-function approval: the committed backlog carries standing approval.

## Sprint-review procedure

1. Reject arguments; this gate takes none.
2. Read `SPRINT.md`; abort if no sprint is open.
3. Verify the Definition of Done with `tools/verify-rom.sh`; require exit 0. A hand-rolled
   `make; sha1sum` is what this gate must not do: a failed `make` leaves the previous ROM in place, so
   the hash reads green off a stale build.
4. For each file claimed md5-candidate, run `grep -c 'INCLUDE_ASM' src/<seg>.c` and require `0`.
5. Report the progress delta, descriptive count, quality counter-metric, story points, and scope
   against the goal. Claim nothing the verification output does not show.
6. Ask the Product Owner for scope sign-off, which buffered suggestions to accept, and the push/PR
   decision. Use the harness's structured-input tool when it has one; otherwise ask directly.
7. **Retire before you add.** Run `venv/bin/python3 tools/prompt_lint.py report`. When a surface is
   within 10% of its budget, or any counter rose this sprint, propose exactly one retirement or
   consolidation in the same turn as the acceptance ask: a superseded sub-bullet to delete, two
   near-duplicate rules to merge, or a case-law paragraph compressed to its rule plus one citation.
   Apply only what the Product Owner accepts, and record the net line delta in the digest. Skip this
   step when the sprint's own net line delta is already negative -- a sprint that retired has no debt
   to pay. Never select a block marked `<!-- load-bearing -->`, the fan-out trigger table and its
   calibration paragraphs, an oracle-table failure-mode cell, or any threshold: those are the
   rationale that makes the surrounding rules apply correctly.
8. Apply only accepted process/tooling edits, then update `VELOCITY.md`, prepend `RETRO.md`, and
   update the `BACKLOG.md` carry-overs and active phase.
9. Perform outward push or PR only if approved.

## Scrum operating model

The tactical match-loop runs inside a thin Scrum cadence (McConnell, *More Effective Agile*).
Artifacts: `BACKLOG.md` (PO-owned ordering rationale, enablers, carry-overs), `SPRINT.md` (gitignored
ephemeral board and resume surface), `RETRO.md` (consolidated digest), `VELOCITY.md` (story-point
dashboard). Target selection is `tools/pick_target.py`, not a stored roadmap.

- **Roles.** Product Owner is the user (backlog priority, goal approval, scope sign-off, retro
  selection). Development Team is the agent (executes the gate enablers after PO approval) plus
  `tools/` plus the KMC-GCC / asm-differ / ROM-SHA-1 oracle.
- **Cycle.** `sprint-plan` (PO approves a goal and small backlog; agent performs and validates the
  flip enablers), then autonomous execution, then `sprint-review` (DoD verify, scope sign-off,
  retro), then repeat.
- **Sprint = one `src/<seg>.c` file to md5-candidate (0 `INCLUDE_ASM` stubs), or one cohesive subseg
  cluster.** The function is the task; the file or cluster is the increment. Cap small early: 1
  upstream file, or about 3 to 4 functions, so the middle fits a context window. For a homogeneous
  sibling set (same upstream pattern, same uniform enabler), fill the 3-to-4 cap, since per-file
  all-or-nothing banking makes the marginal sibling near-free. A heterogeneous batch (mixed hazards)
  trims to the cleanest 2. Review fires when every committed item is banked or spiked/carried.
- **Definition of Ready.**
  - Subseg flippable (not `hasm`); coarse size known; upstream-mirror availability noted; hazards
    flagged.
  - **Size fresh-pack leaves from the extracted `.s` headers, not vram gaps (S247).** In a
    multi-fn asm-flip pack, curated-named fns interleave the `func_<vram>` ones, so a leaf's size
    computed from adjacent-vram deltas is wrong (S247 mis-sized `func_8009232C`/`per_hole_...`/
    `func_8009351C` as tiny when they were 0x4CC-0xAE4, and missed the real tiny leaves
    `func_8008FF14`/`func_800959F8`/`func_800934CC`). Either (a) commit the backlog as "N smallest-first
    leaves TBD at extract" and re-sort by `head -1 asm/nonmatchings/<seg>/<f>/<f>.s` (`nonmatching <f>,
    0x<size>`) once the flip is done, or (b) if you must name leaves at the gate, flip+extract first
    then size from the `.s`. (Fold into the `carried-wall`/sizing ranker follow-ups in `BACKLOG.md`.)
  - **For a c-stub continuation (an already-`c` file), grep the target `src/<file>.c` for pre-existing
    near-match / carry comments on each candidate leaf before committing it (S232).** `pick_target.py`'s
    smallest-first sort and any FP/jal tell-filter do not see the in-file wall comments a prior sprint
    wrote above a carried `INCLUDE_ASM` stub, so the smallest remaining leaves are often exactly the
    documented walls. `grep -nE 'INCLUDE_ASM|/\*' src/<file>.c` (or read the stub's preceding comment):
    a leaf with a fully-RE'd near-match comment is a carried wall, not a fresh tractable leaf. Committing
    it is fine if the goal is a crack-attempt slice (compiler-source fan-out, S232 cracked 3/3) — but
    label it as such, do not price it as a clean leaf. (A `pick_target.py` `carried-wall:<fn>` detector
    that reads the in-file comment is a tracked ranker follow-up; see `BACKLOG.md`.)
    - **A compiler-jtbl leaf is an all-or-nothing single slice, not a gate-flip-then-iterate (S265).**
      A leaf whose `.s` carries `jtbl_<vram>`/`jr $v0`/`.word .L` dispatches through a compiler jump
      table whose `.rodata` carve is a bank-time action: the jtbl only exists as a symbol once the C
      `switch` regenerates it, so carving it while the fn is still `INCLUDE_ASM` breaks the still-asm
      reference (`undefined reference to jtbl_<vram>`). You cannot probe the body against a still-asm
      baseline first — the carve + the full C reconstruction commit together. Price such a leaf as a
      full vertical slice (not a quick getter), and do not pre-carve it at the plan gate. See
      `docs/hazards.md#switch-jtbl-dispatch` (S265 func_8005CF78).
    - **A prior sprint's wall often lives only in `BACKLOG.md ## Carry-overs` or a
      `docs/wip/<fn>.*.md` note, not as an in-file comment, so the in-file grep above misses it and
      the leaf re-surfaces as "fresh" (S239, S240, S268; 8 recurrences).
      `venv/bin/python3 tools/pick_target.py --carried-check <fn>...` is the check: it prints
      `CARRIED-WALL`/`fresh` per fn against the union of both sources and exits non-zero if any is a
      wall. Run it on every hand-mined leaf at the plan gate.** A flagged leaf is either skipped or
      labelled a crack-attempt/deepen slice, never priced as a fresh leaf. Write new wall
      characterizations to `docs/wip/<fn>.near-match.md` at discovery, not only into the retro digest,
      so the next sprint's DoR finds them. (The `carried-wall:<fn>` in-row ranker tag remains a
      follow-up; the detector is a gate command, not yet a ranked-row column.)
      - **`--carried-check` unions the BACKLOG names with every `docs/wip/<fn>.*.md` by stem, so a
        scoping note reads as a wall (S303).** The suffix does not matter: a reconstruction-in-progress
        note flags carried-wall exactly like a terminal one, and S303's `func_8002CDA8` is at the
        ROM's exact instruction count with its display list verified, not a wall. Read the doc the
        flag names before pricing the leaf. (A wall-vs-scoped state in the detector is a ranker
        follow-up; see `BACKLOG.md`.)
      - **Companion tool (S269): `venv/bin/python3 tools/pick_target.py --nested-check <fn>...`**
        flags a GCC nested function among a fresh pack's smallest leaves — its `.s` prologue spills an
        incoming `$v0` static chain (`sw $v0,K($sp)` + `addu $reg,$v0,$zero`) instead of taking its
        arg in `$a0`. Such a leaf is not standalone-bankable: it banks inside its (often still-asm)
        parent, so price it coupled-to-parent (a carry), not a fresh smallest-first leaf. Exits
        non-zero if any is nested. S269's two smallest post-getter leaves (`func_8002BE78` ->
        `draw_ground_shadow_decals`, `func_8002DAC0` -> `render_frame`) were both nested children this
        flags; each otherwise cost a caller-disasm to classify. Run it with `--carried-check` on every
        hand-mined leaf. See `#nested-function-static-chain-spill` and the memory
        `docs/levers.md` (nested function banks the parent too). (The in-row `nested-child:<parent>` ranker tag is a
        follow-up, same as `carried-wall`.)
      - **A leaf that is `fresh` + `standalone` + no-prior-doc can still be an at-attempt wall of a
        class the size+FP sort cannot see (S275).** All four S275 committed low-FP leaves passed
        `--carried-check`/`--nested-check` clean yet every one walled (jtbl-carve-align,
        reorg-delay-slot-coin, two raw-DL-word store-giv emitters). The carried/nested checks catch
        only RE-surfaced or nested walls, not first-encounter ones. `--loose-stubs <seg>` runs
        `wall_class_tell` and tags `JTBL-DISPATCH` (a `jtbl_<vram>` ref -> bank-time rodata carve,
        walls unless 8-aligned both edges), which drops out of `fresh`, and `RAW-DL-EMITTER` (>= 6 DL
        command words), which is advisory and counted in `fresh` since S293 — S258 retired that wall
        verdict and S293 banked the class's three smallest members 3/3, no permuter. **So a `0 fresh`
        count is not proof of a crack-slice-only pool: re-check each excluding tag's own verdict at a
        plateau gate.** Even so, fresh is a ceiling — the FP-scheduler / value-select-branch-likely /
        register-permutation walls are `.s`-undetectable and read `fresh`. Read a candidate's `.s`
        (jtbl/`bnel`/`mflo`/heavy-FP tells) before pricing it a clean leaf. See the S275/S293 retros.
      - **A `.s`-tell rejection is a guess; record it, and stop repeating it unattempted (S289).**
        `func_8005C038` was gate-rejected by S287 and again by S288 on the same tell (10
        branch-likely instructions -> value-select class). Attempted in S289, every `beql` reproduced
        from ordinary C on the first build and the real residual was a callee-saved colouring
        equilibrium. Name the tell beside the rejected leaf in `SPRINT.md ## Committed backlog`, and
        read "rejected twice, never attempted" as the tell being unproven, not confirmed: at the
        third encounter either attempt the leaf or drop it, rather than re-rejecting it.
  - **A compiler-source dive's "proven wall" conclusion is a hypothesis too (S292).** A dive's pass
    citation is usually right; its closing claim that *no source form reaches the other side* is a
    negative claim over a space the dive could not measure before `tools/allocno_report.py` existed.
    Two of S292's three targets had exactly that claim disproved: S210/S213 called `func_8005D334` a
    register-pressure tie (it banked) and said `func_80056060`'s `cs` "always colours `$s0`" (one
    `do {} while (0)` moved it). Re-derive the numbers before inheriting the verdict; keep the
    citation, drop the conclusion.
  - **A carried wall's near-match doc can be wrong about the function's semantics, not just its
    verdict — re-derive behaviour from the `.s` before accepting a stated residual (S258).** S252
    recorded `func_800824E4` as a three-argument packer with `b = arg2` on the negative path and
    built a terminal delay-slot-coin verdict on top of that reading; `$a2` is written in the entry
    branch's delay slot before any read, so there is no third argument and `b = 0xFF` is an ordinary
    shared statement before the if-chain (which is precisely why it fills the delay slot). Corrected,
    the function was 20/20 instructions with one differing operand and banked the same day. S254's
    `func_80088A90` verdict likewise named two "unreachable from faithful C" features that both fell
    out of source once the emit block used the SDK macro. So at the start of a crack-attempt slice,
    read the target's `.s` end to end and re-derive the signature and dataflow first, then read the
    doc's residual. A register written in a branch delay slot before its first read is a shared
    pre-branch statement, not an argument. Extends the memory `revalidate-old-carries-stale-wall`
    from stale builds to stale RE.
    - **The doc's stated residual class is itself a hypothesis, not just its verdict (S268).**
      Beyond semantics: re-derive the residual from a fresh `mips-linux-gnu-objdump -d` of the
      current-build object diffed against the `.s`, before trusting the doc's named divergence
      class. S268 re-opened two carries and both had a mis-stated residual: `func_8005CEE0`'s doc
      claimed an "a0<->v1 register-role swap" that was not present (the roles already matched; the
      real residual was a fold-canonical load-order coin coupled to the idx-index coloring), and
      `func_8005B0B4`'s doc framed the divergence as "branch-direction BB-layout" when the dominant
      issue was an accumulator-role + delay-slot-fill divergence a structural lever fixed, leaving a
      3-register allocno permutation. A mis-stated class sends the crack attempt at the wrong lever.
      Spend the first iteration re-deriving {instruction-count match?, which registers differ, which
      ordering differs} from the object, then map that to a lever — do not inherit the doc's class.
  - Enablers (subseg flip plus `make extract`, multi-file split, `symbol_addrs.txt` additions) are
    performed by the agent at the plan gate after the PO approves the goal/scope, and validated
    there: `make extract && make` must still produce the green baserom ROM with the new stubs. This
    gate build-check is load-bearing: it once surfaced a missing-`cpp` toolchain regression that was
    silently emptying every asm object.
  - Run `make sync-names` only at the gate; a mid-sprint rename breaks in-flight links until the next
    `make extract && make`.
- **Definition of Done (binary; the oracle already exists).**
  - Per-function: score 0, byte-`cmp` spot-check, inlined, clang-format, full `make` to
    `build/mariogolf64.z64: OK`, SHA-1 == baserom, and committed.
  - Per-sprint: every fn in the file inlined (0 stubs), ROM SHA-1 matches, committed.
  - The ROM SHA-1 is green at every commit (un-decompiled parts fill from extracted asm), so at
    review the `make` + SHA-1 paste is a regression guard; the value delta is the matched-count /
    md5-candidate-files number. Never commit a non-matching fn.
- **Spike + carry-over.** A function that blocks its file's DoD (stuck-far below 0.97, needs permuter,
  BSS-layout-conflict, subseg-alignment) is a spike: note it, carry its file or cluster to the next
  sprint, and count credit at the function level. Hold the Definition of Done firm; a spike is
  carried, not banked. The loop's own carry action is in
  `docs/workflow/loop.md ## Execution loop`.
- **Quality counter-metric.** Track stuck-far + permuter-escalated + carried + re-opened per sprint,
  reported next to the match count so the count cannot be gamed by premature spiking.
- **Process changes are retro-gated.** The "Suggested workflow improvements" the execution loop emits
  are recorded into `SPRINT.md` during the sprint and applied at `sprint-review` only, not
  mid-sprint, so the sprint's matching behavior stays fixed and its measurement stays clean. The
  retro is the single apply-point for edits to `AGENTS.md`, `CLAUDE.md`, `docs/*`, `tools/*.py`,
  `.agents/skills/*`, and the Claude command files. A PO-directed out-of-band rework is the exception, by direct
  request.
- **Two-gate close.** Review accepts the product (DoD plus scope); retro improves the process (the
  single apply-point for tooling edits).
- **Resume protocol.** On a fresh mid-sprint session: read `SPRINT.md`, reconcile banked work via
  `git log` since the snapshot, and verify Ghidra MCP connectivity (`list_instances`, port 8089)
  before resuming. `docs/workflow/loop.md ## Execution loop` states the persistence rule that makes
  this work.

## Story points

Lightweight estimation over the Scrum cadence. Scale: Fibonacci 1, 2, 3, 5, 8, 13. Current phase
`regime: mirror`. `VELOCITY.md` is the committed dashboard with the full rules and anchors; this is
the summary.

- **Deterministic seed (v1).** `pick_target.py`'s `pts` column is the a-priori seed (a pure function
  of `size`, `upstream`, `band`, `nfns`, `hazards`). A cluster seed is the sum of its files' seeds.
  `pts` is display-only: it does not change the smallest-first sort. A `blk` seed is an un-pickable
  needs-header (a DoR reject); swap the increment rather than commit it.
- **The 8-point decompose gate (v1).** Route a seed of 8 or 13 through the decompose gate before it
  runs as a 1-increment sprint: decompose it (split the subseg at the upstream-file or function
  boundary) or pull a scaffolding enabler as the goal instead. Applied at the `sprint-plan` gate. This prevents an all-or-nothing
  bank stall. Expect it to fire once the mirror band is mined out and classical units dominate.
  - **Small classical pack exemption (now folded into the ranker).** A `one-tu`
    classical pack is mechanically non-decomposable (non-16-aligned inner boundaries, or the fns share
    a TU's rodata/data, so you cannot independently compile half a one-tu), so the 8-gate's "must
    decompose" verdict is a false fire: it banks atomically as one vertical slice. **The ranker folds
    this into the `pts` column** (`tools/pick_target.py seed_points`): a one-tu classical pack of
    `nfns<4` now seeds 3/5/8 by size (tiny <256 B → 3, mid → 5, big >=768 B → 8; deweight to 3 if
    `jal-free`; +1 if `rodata-straddle`), not the old flat 8/13 (S155). So the gate no longer fires
    on these, and this bullet is now the banking-behavior note (bank atomically, or a quick spike;
    no permuter-class control flow) rather than a pts workaround. The residual manual override the
    ranker still leaves: a big/huge or 4+fn one-tu pack the ranker prices 8/13 that you nonetheless
    know banks atomically (it can't decompose) — run it seed-only and record it in
    `SPRINT.md ## Estimate`. The historical a1 (`<256B AND <=2fn`) / a2 (`0-jal size-agnostic`) branches
    are the empirical anchors the ranker was calibrated to (see VELOCITY.md ## Seed rubric, S155
    re-anchor). S148 `overlay_10/func_ovl10_801F4A40` (2fn, 176B, one-tu) and S150 `main/func_80029250`
    (`cfb_setup`+`cfb_set_num`, 2fn, 496B, one-tu, 0-jal) both banked seed-only; both now seed 3/5 (not
    13) automatically.
  - **Verbatim-mirror exemption.** A seed-8/13 increment may run as a normal
    1-increment sprint when all of these hold:
    - (a) `regime: mirror` plus a verbatim copy of a single upstream file. A drop-def mirror
      qualifies: the function bodies are byte-verbatim, and only the file-scope data defs become
      `extern` decls, which emit nothing, so it banks atomically exactly like a pure `cp` (S86
      `os/timerintr.c`, pts-8 single-file-pack, banked first-try seed-only). A `.data`-carve mirror
      qualifies even more directly: the data defs stay defined and the `.data` is carved to its placed
      vram, so it is a pure verbatim `cp` of the whole file (S116 `nucontgbpakmgr.c`, pts-8
      single-file-pack, banked first-try seed-only, the first libnusys `.data` carve).
    - (b) The decompose path is mechanically blocked: the increment is a `single-file-pack` (every
      member fn comes from one upstream `.c`), so there is no inter-file boundary to split at and an
      intra-file split cannot be independently mirrored. This holds regardless of inner-boundary
      16-alignment: S64 `lookathil`'s inner boundary was `non16align`, S69 `lookat`'s was 16-aligned,
      and both were decompose-blocked (you cannot mirror half a source file). `pick_target.py`'s
      `single-file-pack:<n>fn[…]` tag is the signal (S67); the old `non16align`-on-the-inner-boundary
      test was one mechanical case of it, not the rule.
    - (c) Every callee is placed and all names are curated (the "no residual variance" condition).

    The gate's all-or-nothing concern guards classical iteration stalls; a verbatim single-file mirror
    banks atomically (or is a quick spike), so a size-only 8/13 is a false fire, the same false-flag
    class the hazard detectors keep retiring. Document the exemption in `SPRINT.md ## Estimate`; the
    increment stays seed-only (S64 `gu/lookathil.c` + S69 `gu/lookat.c`, both pts-13, banked
    first-try). The exemption never covers classical or multi-file packs (a `pack` or `c-combined` of
    2 or more distinct upstream files decomposes at the file boundary as usual).
    - **Sub-100 coddog hedge.** A `single-file-pack` with a sub-100 coddog score
      (e.g. `@99.99`, the same near-verbatim tell that flags block-reorders) still qualifies for the
      exemption, but budget a body-divergence diagnosis pass: the "banks atomically or is a quick
      spike" assumption can be violated by a per-fn divergence (block-reorder, or a **game-modified
      body** — an extra branch/store the literal upstream lacks) that turns the "quick spike" into a
      partial bank (S121 `nucontrmbmgr.c`: 8/9 banked C, 1 carried as `INCLUDE_ASM`). Hedge the estimate
      "<=1 re-attempt" and expect a per-file score of 0 pt if the file ends partial (the matched-fn
      count is the value signal, not the file point). **Prove the body emits the target's exact
      stores + values before concluding "compiler wall":** the S121 carry was misframed as an
      unbankable `#cross-jump-tail-merge` for 5 sprints, then banked S127 with a one-branch force-stop
      fix (`state = STOPPED` on `osMotorInit` error). See `#cross-jump-tail-merge`.
    - **coddog 99.99 == structure, not bytes; the exemption-guard.** A `coddog-mirror:<f>@99.99`
      can mask a heavily game-customized file where most bodies diverge, not just a block-reorder
      (S123 `nusched.c`: a game scheduler that shares only the nusys skeleton). The verbatim-mirror
      exemption does not apply when the pack carries a customization tell: a `jal` to a non-lib
      `func_<vram>` game callee (a callee that is not `os*`/`nuSc*`/`al*`/lib), or a large
      `jal-count-mismatch` not explained by a known macro/version artifact. Those signal pervasive
      per-fn divergence -> route to the classical track with the mixed bank-stock-carry-custom
      plan (next bullet), not a seed-only atomic mirror. Verify bodies before trusting a 99.99 row:
      diff the asm against the upstream for the heavy functions, and run the nusys/libultra version
      triage (`docs/hazards.md#upstream-mirror-pattern`) before concluding the divergence is custom.
      (pick_target.py automation to price the non-lib-`func_`-callee tell is a tracked follow-up;
      until then the gate applies this guard by reading the pack's asm callees.)
    - **Mixed mirror+INCLUDE_ASM partial bank is first-class** (S121 generalized to S123). A
      `coddog-mirror` file can be partially stock: some fns byte-match the upstream, others are
      game-customized. The right play is bank-stock-carry-custom — write the stock fns as C and
      keep the customized fns as `INCLUDE_ASM` in the same `src/<seg>.c` (the ROM stays green; the
      file is partial / not md5-candidate until the customized fns are classically decompiled). Plan
      such a file as a `regime: mixed` increment, not a seed-only mirror: per-file all-or-nothing
      means the partial file banks 0 pt, and the matched-fn count is the value signal (S123 nusched.c:
      10/14 banked C, 4 carried, 0 pt, +10 matched).
      - **Extends to a classical one-tu with shared rodata (S169).** A one-tu classical pack can
        partial-bank too: write the matched fns as C and keep the unmatched fns as `INCLUDE_ASM` in the
        same `src/<seg>.c`, **as long as the TU's shared rodata (strings, FP-double pool constants)
        stays in the extracted blob and every fn references it `extern`** (no `.rodata` carve). The ROM
        stays green off the matched fns. So a hard FP fn does not block banking its tractable siblings,
        and the one-tu is not strictly atomic-or-nothing when the rodata is referenced extern rather
        than emitted as literals. S169 `func_80076640.c` banked `func_80076778` (+1) as C while
        `func_80076640` (score-25 reg-swap) and `func_8007680C` (S158-class FP) stayed `INCLUDE_ASM`,
        all referencing the shared `ACAD0` rodata blob. (Emitting the rodata as source literals would
        force the carve and re-impose atomicity, so prefer extern refs for a partial one-tu; see
        `docs/hazards.md#rodata-sibling-yaml-pattern`.)
- **Per-file all-or-nothing banking.** Points bank per file: a spiked/carried file scores 0 pt, a
  banked sibling still counts. This is a separate ledger from the function-level quality
  counter-metric.
- **An enabler sprint scores 0 pt and leaves the velocity series.** A sprint whose goal is scaffolding
  or tooling banks no function by design, so it is excluded from the velocity series and from every
  trailing-mean window. Without that exclusion it reads as a throughput trough and, worse, poisons any
  threshold computed from a trailing mean. The 8-point decompose gate already sanctions this shape as
  one of its two outcomes ("pull a scaffolding enabler as the goal instead"), so no separate gate
  exemption is needed -- this is a ledger rule only.
- **Realized tier + residual (v2, active since Sprint 11).** Scored at review on the classical track
  only: start at the seed band, +1 per {stuck-far / permuter / re-attempt / novel bank-gotcha /
  mid-sprint split / carry-or-reopen}, and -1 for a verbatim first-try with at most 1 fix-iteration;
  per-file, then summed. Residual = realized minus seed. A classical/mixed increment uses the
  two-pass freeze (plan-time seed committed before `src/`, realized in a second commit). The mirror
  track stays seed-only (a point mass with no residual variance), so a `regime: mirror` sprint logs
  banked = seed. The realized tier is agent-scored and subjective, so the real anti-gaming guards stay
  the per-file all-or-nothing bank plus the quality counter-metric, not the freeze. Full rules,
  rolling-5, and re-anchor in `VELOCITY.md`.

Velocity is a planning indicator, not a performance target (McConnell Ch. 19), reported next to the
quality counter-metric, which (with per-file all-or-nothing banking) is the real anti-gaming guard.
