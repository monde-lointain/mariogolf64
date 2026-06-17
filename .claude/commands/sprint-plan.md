---
description: Plan gate. Rank with pick_target.py, propose a sprint goal + small committed backlog, get PO approval, then perform + validate the flip, write SPRINT.md.
args:
  - name: scope
    description: Optional subsystem filter (e.g. `audio`, `libkmc`, `rsp`). Matches `^[a-z][a-z0-9_-]*$`. No arg means the whole active phase.
    required: false
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-plan, the plan gate

SCOPE = `$ARGUMENTS`

<role>
You are the planning gate of the lightweight Scrum loop (see `## Scrum operating model` and
`## Execution loop` in `CLAUDE.md`). Propose one sprint's worth of work, get the Product Owner's
approval, then perform and validate the enablers, and write `SPRINT.md`. Then hand off to the inline
execution loop. Keep it cheap: this is triage, not decompilation, so a lower effort setting fits.
</role>

<scope>
This gate reads `BACKLOG.md` and ranks candidates with `tools/pick_target.py`; performs the gate
enablers (edits `mariogolf64.yaml` subseg flip/split/path-qualifier lines only, and
`symbol_addrs.txt` add-only, optionally `make sync-names` to refresh curated names); runs
`make extract && make` to validate; and writes `SPRINT.md`. It does not edit `src/`,
`ghidra_symbols.txt` (except via `make sync-names`), or `mariogolf64.ld`. The read-only context
gathering in Steps 1 and 3 may run in parallel; the build and edit steps stay serial.
</scope>

<steps>

## Step 0, validate argument

If `$ARGUMENTS` is non-empty, anchor-check against `^[a-z][a-z0-9_-]*$`. On mismatch, abort:

```
/sprint-plan: arg `$ARGUMENTS` does not match `^[a-z][a-z0-9_-]*$`. Aborting.
```

Empty `$ARGUMENTS` is valid: plan from the whole active phase.

## Step 1, rank the candidates

Read `BACKLOG.md` (active phase, carry-overs first, enabler items), then rank the live candidates
smallest-first:

```bash
venv/bin/python3 tools/pick_target.py -n 12        # add `--lib <scope>` when $ARGUMENTS is set
```

The table gives, per candidate: `func`, `rom`, `vram`, `size`, `nfns`, `pts`, `kind`
(`asm-flip` or `c-stub`), `upstream` (libultra|libkmc|libnusys|none), `band` (warm|cold), and
`hazards`. Each hazard maps to a `docs/hazards.md` section via the hazard index in `CLAUDE.md`. Read
the matching section when you plan around one.

## Step 2, optional name refresh (gate-only)

If the chosen candidate shows as `func_<VRAM>` and Ghidra has a name for it, refresh once here:

```bash
make sync-names    # Ghidra to ghidra_symbols.txt; gate-only, never mid-sprint
```

A rename breaks in-flight links until `make extract && make`, so keep this at the gate. Skip it when
the name is already curated.

## Step 3, refine the slice (lightweight DoR)

Pick the smallest coherent increment, in this order of preference:

1. A carry-over from `BACKLOG.md` being retried (its spike blocker resolved).
2. The smallest candidate from `pick_target.py`: one `src/<seg>.c` taken to zero `INCLUDE_ASM` stubs
   (an `asm-flip` candidate, or a `c-stub` file's remaining functions).
3. One cohesive subseg cluster when the file is part of a multi-file pack (split into singletons at
   the upstream-file boundary).

Drop `hasm`-permanent subsegs and data-dominant units (a table-authoring task, not a code loop). A
candidate whose upstream has a file-scope `static` (the `file-static` hazard) routes to the classical
loop, not the upstream mirror (see `docs/hazards.md#file-static-bss-layout-conflict`). Per surviving
candidate, note the subseg type (`c` or needs-flip), upstream-mirror availability, and hazards.

Cap small. Sprint 1 (and until a per-sprint count emerges) is 1 file or about 3 to 4 functions, so
the autonomous middle fits a context window. Span multiple files only when they are siblings (same
upstream source, or one pack split).

## Step 4, identify the gate enablers

List the enablers the chosen increment needs (the agent performs these in Step 7, after the PO
approves): flip the subseg (`asm` to `c`); split a multi-file pack at the upstream-file boundary; add
the target's curated name to `symbol_addrs.txt` (add-only). The name comes from the `pick_target.py`
table or the upstream source; no MCP lookup at this gate. If a `refs-unplaced` or `calls-unplaced`
hazard is flagged, the symbol recovery is a gate enabler too: follow
`docs/hazards.md#recover-extern-refs-unplaced` or `#calls-unplaced-function-callee-dual`.

## Step 5, propose the sprint goal (Commander's Intent)

Write the goal vividly (2 to 4 lines): what banking this file or cluster means, the seed/upstream
path to try first, and the explicit success predicate: *"every fn inlined (0 INCLUDE_ASM stubs) +
full `make` ROM SHA-1 == baserom + committed; `src/<seg>.c` flips to md5-candidate."* Snapshot match
progress as the pre-sprint baseline (matched count + md5-candidate file count + date, from `git log`
plus a `pick_target.py` count).

## Step 5b, estimate the sprint (story points) + the 8-point gate

Story-point velocity is live: the v1 seed everyday, plus the v2 realized tier on the classical track
since Sprint 11 (mirror sprints stay seed-only). Fibonacci 1, 2, 3, 5, 8, 13; current
`regime: mirror`. See `VELOCITY.md` for the rules and anchors. For the chosen increment:

1. **Seed** is the `pts` column from the Step 1 table (deterministic). For a cluster, the committed
   seed is the sum of its files' `pts`. A `blk` seed is an un-pickable needs-header (a DoR reject):
   swap the increment, do not commit it.
2. **8-point gate.** A committed 8 or 13 must not run as a normal 1-increment sprint: decompose it
   (split the subseg at the upstream-file or function boundary) or pull a scaffolding enabler as the
   sprint goal instead. This prevents an all-or-nothing bank stall. (Expect this gate to fire once the
   mirror band is mined out and classical units dominate.) The verbatim-mirror exemption in
   `CLAUDE.md ## Story points` lets a single-file-pack mirror run seed-only despite an 8/13 seed.
3. **Adjust** (optional, plus or minus one ladder step) by comparing the increment's profile to the
   `VELOCITY.md` anchor rows. This is the committed estimate, surfaced to the PO in Step 6. A mirror
   increment has no freeze commit (seed-only, re-derivable), logged to `VELOCITY.md` at
   `/sprint-review`. A classical/mixed increment uses the v2 two-pass freeze (commit the plan-time
   seed before `src/`, realized in a second commit); see `VELOCITY.md`.

## Step 6, PO approval gate

Present the proposed goal + committed backlog (the increment, its enablers, the upstream path, the
snapshot) + the committed story-point estimate (seed to adjusted; whether the 8-gate fired) to the PO
with `AskUserQuestion`: approve as-is, edit the goal, or swap the increment. Do not proceed past this
gate until the PO approves. On approval, the agent performs the enablers in Step 7.

## Step 7, perform + validate the flip at the gate

Once the PO approves, perform the enablers, then confirm the scaffold still builds the real ROM:

1. **Add the symbol** (only if the target's curated name isn't already in
   `ghidra_symbols.txt`/`symbol_addrs.txt`): append `<curated_name> = 0x<vram>; // type:func` to
   `symbol_addrs.txt` (add-only). This must land before `make extract` so splat scaffolds the curated
   name, not `func_<VRAM>`.
2. **Flip the subseg** in `mariogolf64.yaml`: `[0x<off>, asm]` becomes `[0x<off>, c, <path>]` (split a
   multi-file pack at the upstream-file boundary first, if needed). **Text only at the gate.** An
   ld-section sibling (`.data`/`.rodata`/`.bss` carved from the C compile) is NOT a gate enabler: an
   `INCLUDE_ASM` stub emits no such section, so a sibling added here carves an empty range and shifts
   every following data/rodata byte, failing the green-ROM check below. The ld-section split lands
   during execution with the real body (S68; see `docs/hazards.md#defines-data` for the `.data` carve
   and `#rodata-sibling-yaml-pattern` for `.rodata`). At the gate, leave the data/rodata region as its
   existing generic subseg; the stub's `%lo(D_<vram>)` resolves from it.
3. **Validate**:

   ```bash
   make extract && make
   ```

   The build must end `build/mariogolf64.z64: OK` and `sha1sum build/mariogolf64.z64` must equal
   `e2c4e7a905b29529b49a1619a401fe699224829b` (now with the new `INCLUDE_ASM` stubs). Confirm the
   scaffold stub is named the curated name, not `func_<VRAM>`. If it fails, the flip is bad (wrong
   offset, or a non-16-aligned subseg; see `docs/hazards.md#non16align`); fix it before writing
   DoR-ok. This catches a bad flip at the gate, not mid-sprint.

## Step 8, write SPRINT.md and hand off

On a green validation, write `SPRINT.md` (gitignored: ephemeral scratch and resume surface). Then run
the `## Execution loop` (`CLAUDE.md`) inline over the committed backlog, one function at a time, no
per-function PO stop. Each banked function appends a standup line + its suggestion buffer to
`SPRINT.md` and applies no tooling edits (those wait for the retro). Run `/sprint-review` when every
committed item is banked or spiked/carried.

</steps>

<example>
A completed `SPRINT.md` for a single recover-extern mirror sprint:

```markdown
# Sprint 41, bank __osEPiRawWriteIo (libultra pi IO_WRITE mirror)

## Goal (Commander's Intent)
Bank `__osEPiRawWriteIo` into `src/libultra/nintendo/pi/epirawwrite.c`, a verbatim IO_WRITE
mirror in the warm pi band. Recover the macro-hidden `__osCurrentHandle` extern before the flip,
copy upstream verbatim, prove via full-make ROM SHA-1.
Success: 0 INCLUDE_ASM stubs in the file + ROM SHA-1 == baserom + committed, so md5-candidate.

## Committed backlog
- [ ] src/libultra/nintendo/pi/epirawwrite.c  (1 fn, ~120 instrs, upstream: libultra)  DoR: ok (flip validated)
- [x] enabler (agent, at gate): symbol_addrs += __osCurrentHandle = 0x800C7E90; // size:0x8, recovered from lui/lo16, validated

## Estimate (story points)
committed 2pt  (seed 2 from pts to adjusted 2; 8-gate: clear; regime: mirror)

## DoR notes
- subseg: flipped; upstream: libultra/nintendo/pi/epirawwrite; hazards: macro-hidden recover-extern (EPI_SYNC to __osCurrentHandle)

## Pre-sprint snapshot
matched 312/1180 fns (26.4%); md5-candidate files 47/130   (2026-06-13)

## Standup log
(execution appends: <fn>, <Match|stuck-far|spiked>, next)

## Suggestion buffer (recorded, applied at /sprint-review, NOT mid-sprint)
(the execution loop appends its numbered "Suggested workflow improvements" here)
```
</example>
