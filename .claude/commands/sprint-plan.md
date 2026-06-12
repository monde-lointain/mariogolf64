---
description: Plan gate — rank with pick_target.py, propose a sprint goal + small committed backlog, get PO approval, then perform + validate the flip, write SPRINT.md.
args:
  - name: scope
    description: Optional subsystem filter (e.g. `audio`, `libkmc`, `rsp`). Matches `^[a-z][a-z0-9_-]*$`. No arg = whole active phase.
    required: false
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-plan — the plan gate

SCOPE = `$ARGUMENTS`

The **planning gate** of the lightweight Scrum loop (see `## Scrum operating model` +
`## Execution loop` in `CLAUDE.md`). Propose one sprint's worth of work, get the Product
Owner's approval, then **perform and validate** the enablers (subseg flip/split,
`symbol_addrs.txt` additions, `make extract`), and write `SPRINT.md`. Then hand off to the
inline execution loop. Keep it cheap — this is triage, not decompilation.

<scope>
This gate reads `BACKLOG.md` and ranks candidates with `tools/pick_target.py`; performs the
gate enablers — edits `mariogolf64.yaml` (subseg flip/split/path-qualifier lines only) and
`symbol_addrs.txt` (add-only), optionally `make sync-names` to refresh curated names — runs
`make extract && make` to validate, and writes `SPRINT.md`. It does not edit `src/`,
`ghidra_symbols.txt` (except via `make sync-names`), or `mariogolf64.ld`.
</scope>

## Step 0 — Validate argument

If `$ARGUMENTS` is non-empty, anchor-check against `^[a-z][a-z0-9_-]*$`. On mismatch, abort:

```
/sprint-plan: arg `$ARGUMENTS` does not match `^[a-z][a-z0-9_-]*$`. Aborting.
```

Empty `$ARGUMENTS` is valid — plan from the whole active phase.

## Step 1 — Rank the candidates

Read `BACKLOG.md` (active phase, **carry-overs first**, enabler items), then rank the live
candidates smallest-first:

```bash
venv/bin/python3 tools/pick_target.py -n 12        # add `--lib <scope>` when $ARGUMENTS is set
```

The table gives, per candidate: `func`, `rom`, `size`, function count, `kind` (`asm-flip` |
`c-stub`), `upstream` (libultra|libkmc|none), and `hazards` (file-scope static, non-16-align,
multi-fn pack).

## Step 2 — Optional name refresh (gate-only)

If the chosen candidate's curated name isn't yet synced (it shows as `func_<VRAM>` in the
table) and Ghidra has a name for it, refresh once at the gate:

```bash
make sync-names    # Ghidra → ghidra_symbols.txt; gate-only, never mid-sprint
```

A rename breaks in-flight links until `make extract && make`, so this is **gate-only** — never
during the execution middle. Skip it when the name is already curated.

## Step 3 — Refine the slice (lightweight DoR)

Pick the smallest coherent increment, in this order of preference:

1. A **carry-over** from `BACKLOG.md` being retried (its spike blocker resolved).
2. The **smallest candidate** from `pick_target.py` — one `src/<seg>.c` taken to zero
   `INCLUDE_ASM` stubs (an `asm-flip` candidate, or a `c-stub` file's remaining functions).
3. One **cohesive subseg cluster** when the file is part of a multi-file pack (split into
   singletons at the upstream-file boundary).

Drop: `hasm`-permanent subsegs; data-dominant units (a table-authoring task, not a code loop).
A candidate whose upstream has a **file-scope `static`** (the `file-static` hazard) routes to
the **classical loop**, not the upstream mirror (BSS-layout-conflict rule). Per surviving
candidate note: subseg type (`c` or needs-flip), upstream-mirror availability (the `upstream`
column), and hazards.

**Cap small.** Sprint 1 (and until a per-sprint count emerges) = 1 file or ≤ ~3–4 functions,
so the autonomous middle fits a context window and calibrates the unproven classical loop.
Span multiple files only when **siblings** (same upstream source, or one pack split).

## Step 4 — Identify the gate enablers

List the enablers the chosen increment needs (the **agent** performs these in Step 7, after the
PO approves the goal/scope): flip the subseg (`asm` → `c`); split a multi-file pack at the
upstream-file boundary; add the target's curated name to `symbol_addrs.txt` (add-only). The
name comes from the `pick_target.py` table / the upstream source — no MCP lookup at this gate.

## Step 5 — Propose the sprint goal (Commander's Intent)

Write the goal vividly (2–4 lines): what banking this file/cluster means, the seed/upstream
path to try first, and the explicit success predicate —
*"every fn inlined (0 INCLUDE_ASM stubs) + full `make` ROM SHA-1 == baserom + committed;
`src/<seg>.c` flips to md5-candidate."* Snapshot match progress as the pre-sprint baseline
(matched count + md5-candidate file count + date — from `git log` + a `pick_target.py` count).

## Step 5b — Estimate the sprint (story points) + the 8-point gate

Story-point velocity is **active (v1)** — Fibonacci 1,2,3,5,8,13, `regime: mirror` scope. See
`VELOCITY.md` for the rules + anchors. For the chosen increment:

1. **Seed** = the `pts` column from the Step 1 `pick_target.py` table (deterministic). For a
   **cluster**, the committed seed is the **sum** of its files' `pts`. A `blk` seed = an
   un-pickable needs-header (unindexed `-I` / system header) — a DoR reject; **swap the
   increment**, do not commit it.
2. **8-point gate.** A committed **8 or 13 must NOT be committed as a normal 1-increment
   sprint** — **decompose** it (split the subseg at the upstream-file/function boundary) or pull
   a scaffolding **enabler** as the sprint goal instead. This prevents an all-or-nothing bank
   stall. (Most remaining classical units seed 8/13 — expect this gate to fire once the mirror
   band is mined out.)
3. **Adjust** (optional, ±1 ladder step) by comparing the increment's profile to the
   `VELOCITY.md` anchor rows. This is the **committed estimate** — surfaced to the PO in Step 6.
   **No freeze commit in v1** (the seed is deterministic / re-derivable); it is logged to
   `VELOCITY.md` at `/sprint-review`, not here.

## Step 6 — PO approval gate

Present the proposed **goal** + **committed backlog** (the increment, its enablers, the
upstream path, the snapshot) + the **committed story-point estimate** (seed → adjusted; whether
the 8-gate fired) to the PO with `AskUserQuestion`: approve as-is / edit the goal /
swap the increment. **Do not proceed until approved.** On approval, the **agent** performs the
enablers in Step 7 — the PO approves the goal and scope; it no longer hand-edits the yaml or
`symbol_addrs.txt`.

## Step 7 — Perform + validate the flip at the gate

Once the PO approves, **the agent performs the enablers**, then confirms the scaffold still
builds the real ROM:

1. **Add the symbol** (only if the target's curated name isn't already in
   `ghidra_symbols.txt`/`symbol_addrs.txt`): append `<curated_name> = 0x<vram>; // type:func`
   to `symbol_addrs.txt` (add-only). This must land **before** `make extract` so splat
   scaffolds the curated name, not `func_<VRAM>`.
2. **Flip the subseg** in `mariogolf64.yaml`: `[0x<off>, asm]` → `[0x<off>, c, <path>]` (split a
   multi-file pack at the upstream-file boundary first, if needed).
3. **Validate**:

   ```bash
   make extract && make
   ```

   The build must end `build/mariogolf64.z64: OK` and `sha1sum build/mariogolf64.z64` must
   equal `e2c4e7a905b29529b49a1619a401fe699224829b` (now with the new `INCLUDE_ASM` stubs).
   Confirm the scaffold stub is named the curated name, not `func_<VRAM>`. If it fails, the flip
   is bad (wrong offset, or a non-16-aligned subseg per the execution loop's alignment guard) —
   fix it before writing DoR-ok. This catches a bad flip **at the gate**, not mid-sprint.

## Step 8 — Write SPRINT.md and hand off

On a green validation, write `SPRINT.md` (gitignored — ephemeral scratch + resume surface):

```markdown
# Sprint <N> — <goal one-liner>

## Goal (Commander's Intent)
<vivid 2-4 lines + the success predicate>

## Committed backlog
- [ ] <file or cluster>  (<N> fns, ~<instrs> instrs, upstream: <libultra|libkmc|none>)  DoR: ok (flip validated)
- [x] enabler (agent, at gate): <flip / split / symbol_addrs — performed + validated>

## Estimate (story points)
committed <N>pt  (seed <S from pts; sum for clusters> → adjusted <N>; 8-gate: <fired/clear>; regime: mirror)

## DoR notes
- subseg: <c | flipped>; upstream: <path | none>; hazards: <static / BSS-conflict / none>

## Pre-sprint snapshot
matched <X>/<total> fns (<pct>%); md5-candidate files <n>/<m>   (<date>)

## Standup log
(execution appends: <fn> — <Match|stuck-far|spiked> — next)

## Suggestion buffer (recorded, applied at /sprint-review — NOT mid-sprint)
(the execution loop appends its numbered "Suggested workflow improvements" here)
```

Then hand off to **autonomous execution**: run the **`## Execution loop`** (`CLAUDE.md`) inline
over the committed backlog — one function at a time, **no per-function PO stop**, no MCP lock.
Each banked function appends a standup line + its suggestion buffer to `SPRINT.md` and applies
**no** tooling edits (those wait for the retro). Run `/sprint-review` when every committed item
is **banked or spiked/carried**.
