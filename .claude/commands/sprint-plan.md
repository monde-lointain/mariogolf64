---
description: Plan gate — propose a sprint goal + small committed backlog, get PO approval, then perform + validate the flip, write SPRINT.md. MCP-lock-free.
args:
  - name: scope
    description: Optional subsystem filter (e.g. `audio`, `libkmc`, `rsp`). Matches `^[a-z][a-z0-9_-]*$`. No arg = whole active phase.
    required: false
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-plan — the plan gate

SCOPE = `$ARGUMENTS`

The **planning gate** of the lightweight Scrum loop (see `## Scrum operating model` in
`CLAUDE.md`). Propose one sprint's worth of work, get the Product Owner's approval, then
**perform and validate** the enablers (subseg flip/split, `symbol_addrs.txt` additions,
`make extract`), and write `SPRINT.md`. Then hand off to autonomous execution. Keep it cheap —
this is triage, not decompilation.

<scope>
This command is **MCP-lock-free**: it never touches the Ghidra MCP and never acquires
`tools/mcp_lock.py` (the heavy trio `/decomp`, `/decomp-lead`, `/sync-names` own that slot).
It reads `BACKLOG.md` + `docs/decomp_roadmap.md`, performs the gate enablers — edits
`mariogolf64.yaml` (subseg flip/split/path-qualifier lines only) and `symbol_addrs.txt`
(add-only, names already known from the roadmap/upstream so no MCP lookup is needed) — runs
`make extract && make` to validate, and writes `SPRINT.md`. It does not edit `src/`, the
roadmap, `ghidra_symbols.txt`, or `mariogolf64.ld`.
</scope>

## Step 0 — Validate argument

If `$ARGUMENTS` is non-empty, anchor-check against `^[a-z][a-z0-9_-]*$`. On mismatch, abort:

```
/sprint-plan: arg `$ARGUMENTS` does not match `^[a-z][a-z0-9_-]*$`. Aborting.
```

Empty `$ARGUMENTS` is valid — plan from the whole active phase.

## Step 1 — Read the backlog ordering

Read `BACKLOG.md` (active phase, **carry-overs first**, enabler items) and
`docs/decomp_roadmap.md` (the live candidate list + the **file-completion rollup**:
"files N functions from md5-candidate, smallest remaining-work first"). If `$SCOPE` is set,
restrict to that subsystem.

## Step 2 — Freshness check

If `docs/decomp_roadmap.md` is older than the most recent match (`git log -1 --format=%cd -- src/`)
or the PO asks, **advise running `/decomp-lead` first** — it is the grooming step and holds
the MCP lock, so it cannot run concurrently with this gate. Do not silently plan off a stale
roadmap; surface the staleness and let the PO decide.

## Step 3 — Refine the slice (lightweight DoR)

Pick the smallest coherent increment, in this order of preference:

1. A **carry-over** from `BACKLOG.md` being retried (its spike blocker resolved).
2. The **smallest file N functions from md5-candidate** in the rollup — one
   `src/<seg>.c` taken to zero `INCLUDE_ASM` stubs.
3. One **cohesive subseg cluster** when the file is part of a multi-file pack (split into
   singletons at the upstream-file boundary).

Drop: `hasm`-permanent subsegs; data-dominant units (a table-authoring task, not a code
loop). Route any target whose upstream has a **file-scope `static`** to `/decomp` (not
`-libupstream`) per the BSS-layout-conflict rule. Per surviving candidate note: subseg type
(`c` or needs-flip), upstream-mirror availability (a yes/no, not a diff), and hazards
(file-scope static, known BSS-layout-conflict, subseg-alignment).

**Cap small.** Sprint 1 (and until a per-sprint count emerges) = 1 file or ≤ ~3–4 functions,
so the autonomous middle fits a context window and calibrates the unproven classical loop.
Span multiple files only when **siblings** (same upstream source, or one pack split).

## Step 4 — Identify the gate enablers

List the enablers the chosen increment needs (the **agent** performs these in Step 7, after
the PO approves the goal/scope): flip the subseg (`asm` → `c`); split a multi-file pack at the
upstream-file boundary; add the target's curated name to `symbol_addrs.txt` (add-only — the
name is already known from the roadmap/upstream, so no MCP lookup at this gate). `/sync-names`
is **gate-only** — name it here only if a broader name-sync is needed, never mid-sprint (a
rename breaks in-flight links until `make extract && make`).

## Step 5 — Propose the sprint goal (Commander's Intent)

Write the goal vividly (2–4 lines): what banking this file/cluster means, the seed/upstream
path to try first, and the explicit success predicate —
*"every fn inlined (0 INCLUDE_ASM stubs) + full `make` ROM SHA-1 == baserom + committed;
`src/<seg>.c` flips to md5-candidate."* Snapshot match progress from the roadmap as the
pre-sprint baseline (matched count + md5-candidate file count + date).

## Step 6 — PO approval gate

Present the proposed **goal** + **committed backlog** (the increment, its enablers, the
upstream path, the snapshot) to the PO with `AskUserQuestion`: approve as-is / edit the goal
/ swap the increment. **Do not proceed until approved.** On approval, the **agent** performs
the enablers in Step 7 — the PO approves the goal and scope; it no longer hand-edits the yaml
or `symbol_addrs.txt`.

## Step 7 — Perform + validate the flip at the gate

Once the PO approves, **the agent performs the enablers**, then confirms the scaffold still
builds the real ROM:

1. **Add the symbol** (only if the target's curated name isn't already in
   `ghidra_symbols.txt`/`symbol_addrs.txt`): append `<curated_name> = 0x<vram>; // type:func`
   to `symbol_addrs.txt` (add-only; the name comes from the roadmap/upstream — no MCP lookup).
   This must land **before** `make extract` so splat scaffolds the curated name, not
   `func_<VRAM>`.
2. **Flip the subseg** in `mariogolf64.yaml`: `[0x<off>, asm]` → `[0x<off>, c, <path>]` (split
   a multi-file pack at the upstream-file boundary first, if needed).
3. **Validate**:

   ```bash
   make extract && make
   ```

   The build must end `build/mariogolf64.z64: OK` and `sha1sum build/mariogolf64.z64` must
   equal `e2c4e7a905b29529b49a1619a401fe699224829b` (now with the new `INCLUDE_ASM` stubs).
   Confirm the scaffold stub is named the curated name, not `func_<VRAM>`. If it fails, the
   flip is bad (wrong offset, or a non-16-aligned subseg per `/decomp` Step-9's guard) — fix it
   before writing DoR-ok. This catches a bad flip **at the gate**, not mid-sprint.

## Step 8 — Write SPRINT.md and hand off

On a green validation, write `SPRINT.md` (gitignored — ephemeral scratch + resume surface):

```markdown
# Sprint <N> — <goal one-liner>

## Goal (Commander's Intent)
<vivid 2-4 lines + the success predicate>

## Committed backlog
- [ ] <file or cluster>  (<N> fns, ~<instrs> instrs, upstream: <libultra|libkmc|none>)  DoR: ok (flip validated)
- [x] enabler (agent, at gate): <flip / split / symbol_addrs — performed + validated>

## DoR notes
- subseg: <c | flipped>; upstream: <path | none>; hazards: <static / BSS-conflict / none>

## Pre-sprint snapshot
matched <X>/<total> fns (<pct>%); md5-candidate files <n>/<m>   (docs/decomp_roadmap.md, <date>)

## Standup log
(execution appends: <fn> — <Match|stuck-far|spiked> — next)

## Suggestion buffer (recorded, applied at /sprint-review — NOT mid-sprint)
(each /decomp run appends its numbered "Suggested workflow improvements" here)
```

Then hand off to **autonomous execution**: chain the per-function runs via the Skill tool —
`/decomp <target>` or `/decomp-libupstream <target>` — serial under the MCP lock, **no
per-function PO stop**. Each run appends a standup line + its suggestion buffer to `SPRINT.md`
and applies **no** tooling edits (those wait for the retro). Run `/sprint-review` when every
committed item is **banked or spiked/carried**.
