---
description: Review gate. Verify the ROM-SHA-1 DoD on the increment, PO scope sign-off, apply buffered suggestions, write the RETRO.md digest.
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-review, the review gate

<role>
You are the review gate of the lightweight Scrum loop (see `## Scrum operating model` in
`CLAUDE.md`). Fire when every committed item in `SPRINT.md` is banked (DoD met) or spiked/carried.
The ROM-SHA-1 oracle already guarantees correctness, so the PO step here is not accept/reject of
code: it is scope sign-off + knowledge capture + reprioritize. This gate is the single apply-point
for the sprint's process/tooling edits.
</role>

<scope>
Never touches the Ghidra MCP. Runs `make` to verify the DoD; reads `SPRINT.md`; on close writes
`RETRO.md`, `BACKLOG.md`, `VELOCITY.md`, and the PO-selected tooling / `CLAUDE.md` / `docs/*` /
command files. Takes no arguments.
</scope>

<steps>

## Step 0, reject arguments

If `$ARGUMENTS` is non-empty, abort: `/sprint-review takes no arguments.`

## Step 1, read the sprint

Read `SPRINT.md`: the goal, committed backlog, DoR notes, pre-sprint snapshot, standup log, and
suggestion buffer. If `SPRINT.md` is absent, abort: there is no open sprint to review.

## Step 2, verify the DoD on the increment (paste the output)

```bash
make
sha1sum build/mariogolf64.z64
```

The build must end `build/mariogolf64.z64: OK` and the SHA-1 must equal
`e2c4e7a905b29529b49a1619a401fe699224829b`. Then, for each banked file:

```bash
grep -c 'INCLUDE_ASM' src/<seg>.c   # must be 0 for a file claimed md5-candidate
```

Paste the real command output and claim nothing the commands don't show (a grounding guard against a
stale-`.z64` false-positive). The SHA-1 is the regression guard (it is green at every commit); the
`grep`-zero is the value signal (the file is now fully C). Do not declare an increment that fails
these.

## Step 3, report the increment

- **Progress delta:** matched count + md5-candidate files now vs the `SPRINT.md` snapshot (the
  snapshot plus `git log`, with `venv/bin/python3 tools/pick_target.py` for the live count).
- **Descriptive count:** files banked + functions matched this sprint
  (`git log --oneline <snapshot>..HEAD -- src/`).
- **Quality counter-metric:** stuck-far + permuter-escalated + carried + re-opened, read from the
  `SPRINT.md` standup log. This is the balance metric so the count isn't gamed by premature spikes.
- **Story points:** the committed seed from `SPRINT.md ## Estimate`, with per-file banking (a spiked
  file scores 0 pt; a banked sibling still counts). For a **mirror** sprint, log seed-only (the mirror
  track is a point mass): `committed <N>pt; banked <B>pt; regime mirror`. For a **classical/mixed**
  sprint, also score the v2 realized tier + residual on the classical track (active since Sprint 11):
  start at the seed band, +1 per {stuck-far / permuter / re-attempt / novel bank-gotcha / mid-sprint
  split / carry-or-reopen}, and -1 for a verbatim first-try with at most 1 fix-iteration; report
  `seed <S>; realized <R>; residual <R minus S>; regime <classical|mixed>`. See `VELOCITY.md`.
- **Scope vs goal:** did we bank the increment? List any carry-overs (spiked files + the blocking
  function + cost).

## Step 4, PO sign-off + retro selection

With `AskUserQuestion`, ask the PO to:
1. sign off on scope (goal met / partial / re-plan),
2. pick which of the sprint's buffered "Suggested workflow improvements" to apply,
3. decide whether to push/PR now (the only outward, gated op; per-function commits already happened
   autonomously during execution).

## Step 5, apply + retro digest (the single apply-point)

This is the only place the sprint's process/tooling edits land:

1. Apply the PO-selected suggestions to their named files (`tools/*.py`, `CLAUDE.md`, `docs/*`, the
   commands, `include/`). A hazard playbook edit goes in `docs/hazards.md`; an every-sprint rule
   change goes in `CLAUDE.md`; keep the `CLAUDE.md` hazard index in sync if a `docs/hazards.md`
   section is added or renamed. New or edited prompt text follows `PROMPT_GUIDELINES.md` at the
   project root. Treat prompt-surface headings as an API: `pick_target.py` / `pick_target_hazards.py`,
   the `CLAUDE.md` hazard index, and `BACKLOG`/`RETRO`/`VELOCITY` cite `docs/hazards.md#<anchor>`
   strings and `CLAUDE.md` section headings by exact text, so re-list or re-wrap content but keep
   cited headings and `#anchor` strings unchanged; when a heading genuinely must change, update every
   reference in lockstep.
2. **Log the story-point row to `VELOCITY.md`** under the anchors/log. For a mirror sprint, the v1
   seed is deterministic (no separate plan-gate freeze): append `Sp <N> | <increment> | seed <S>;
   banked <B>pt; regime mirror`. For a classical/mixed sprint, also fill the v2 realized tier +
   residual in the documented format (the plan-gate seed was already frozen pre-`src/`).
3. Prepend one entry to `RETRO.md` in its documented format (increment, quality counter-metric, the
   `Seed:` line, what helped, friction, the `Applied (N of M)` audit line, carry-overs).
4. Move carry-overs to `BACKLOG.md ## Carry-overs` (follow the two-kind format in that section's
   header: a **spike** records its DoD blocker; a **near-free retry** uses the 5-point completeness
   checklist so the next gate is a mechanical replay); add an `## Active phase` BANKED paragraph for
   the increment.

Durable lessons stay in `RETRO.md`; the PO promotes them into the memory dir manually. Do not
auto-write memory.

## Step 6, outward ops + close

If the PO approved push/PR, do it now (and only now). Leave `SPRINT.md` for the next `/sprint-plan` to
overwrite. Suggest running `/sprint-plan` for the next sprint.

Optionally close with a short numbered "Suggested Scrum-process improvements" list (about the overlay
itself: gates, artifacts, cadence). These buffer the same way matching suggestions do and are applied
at the next review.

</steps>

<example>
A `RETRO.md` entry + the matching `VELOCITY.md` row for a clean single-mirror sprint:

```markdown
## Sprint 41, __osEPiRawWriteIo (libultra pi IO_WRITE mirror)
- Increment: src/libultra/nintendo/pi/epirawwrite.c banked (1 fn). matched 312 to 313; md5-candidate 47 to 48.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear).
- What helped: macro-hidden recover-extern surfaced at the gate via pick_target.py's 1-level macro
  expansion (EPI_SYNC to __osCurrentHandle), so no mid-execution link failure.
- Friction: none.
- Applied (2 of 2): #1 docs note on macro-hidden externs; #2 size-rule clarification for handle arrays.
- Carry-overs: none.
```

```text
Sp 41 | epirawwrite (libultra pi) | seed 2; banked 2pt; regime mirror
```
</example>
