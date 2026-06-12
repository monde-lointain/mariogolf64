---
description: Review gate — verify the ROM-SHA-1 DoD on the increment, PO scope sign-off, apply buffered suggestions, write the RETRO.md digest.
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-review — the review gate

The **review gate** of the lightweight Scrum loop (see `## Scrum operating model` in
`CLAUDE.md`). Fires when every committed item in `SPRINT.md` is **banked (DoD met) or
spiked/carried**. The ROM-SHA-1 oracle already guarantees correctness, so the PO step here is
**not** accept/reject of code — it is **scope sign-off + knowledge capture + reprioritize**.
This gate is the **single apply-point** for the sprint's process/tooling edits.

<scope>
Never touches the Ghidra MCP. Runs `make` to verify the DoD; reads `SPRINT.md`; on close
writes `RETRO.md`, `BACKLOG.md`, and the PO-selected tooling/`CLAUDE.md`/command files. Takes
no arguments.
</scope>

## Step 0 — Reject arguments

If `$ARGUMENTS` is non-empty, abort: `/sprint-review takes no arguments.`

## Step 1 — Read the sprint

Read `SPRINT.md`: the goal, committed backlog, DoR notes, pre-sprint snapshot, standup log,
and suggestion buffer. If `SPRINT.md` is absent, abort — there is no open sprint to review.

## Step 2 — Verify the DoD on the increment (paste the output)

```bash
make
sha1sum build/mariogolf64.z64
```

The build must end `build/mariogolf64.z64: OK` and the SHA-1 must equal
`e2c4e7a905b29529b49a1619a401fe699224829b`. Then, for each banked file:

```bash
grep -c 'INCLUDE_ASM' src/<seg>.c   # must be 0 for a file claimed md5-candidate
```

Paste the **real output**. Claim nothing the commands don't show. The SHA-1 is the
**regression guard** (it is green at every commit); the `grep`-zero is the **value signal**
(the file is now fully C). Do not declare an increment that fails these.

## Step 3 — Report the increment

- **Progress delta:** matched count + md5-candidate files now vs the `SPRINT.md` snapshot
  (the snapshot + `git log`, with `venv/bin/python3 tools/pick_target.py` for the live count).
- **Descriptive count:** files banked + functions matched this sprint
  (`git log --oneline <snapshot>..HEAD -- src/`).
- **Quality counter-metric:** stuck-far + permuter-escalated + carried + re-opened, read from
  the `SPRINT.md` standup log — the balance metric so the count isn't gamed by premature spikes.
- **Scope vs goal:** did we bank the increment? List any **carry-overs** (spiked files + the
  blocking function + cost).

## Step 4 — PO sign-off + retro selection

With `AskUserQuestion`, ask the PO to:
1. sign off on **scope** (goal met / partial / re-plan),
2. pick which of the sprint's buffered **"Suggested workflow improvements"** to apply,
3. decide whether to **push/PR** now (the only outward, gated op — per-function commits
   already happened autonomously during execution).

## Step 5 — Apply + retro digest (the single apply-point)

This is the **only** place the sprint's process/tooling edits land:

1. Apply the PO-selected suggestions to their named files (`tools/*.py`, `CLAUDE.md`, the
   commands, `include/`).
2. Prepend one entry to `RETRO.md` in its documented format (increment, quality
   counter-metric, what helped, friction, the `Applied (N of M)` audit line, carry-overs).
3. Move carry-overs to `BACKLOG.md ## Carry-overs`; add an `## Active phase` BANKED paragraph
   for the increment.

Durable lessons stay in `RETRO.md`; the **PO promotes them into the memory dir manually** —
do not auto-write memory.

## Step 6 — Outward ops + close

If the PO approved push/PR, do it now (and only now). Leave `SPRINT.md` for the next
`/sprint-plan` to overwrite. Suggest running `/sprint-plan` for the next sprint.

Optionally close with a short numbered **"Suggested Scrum-process improvements"** list (about
the overlay itself — gates, artifacts, cadence). These buffer the same way matching
suggestions do and are applied at the next review.
