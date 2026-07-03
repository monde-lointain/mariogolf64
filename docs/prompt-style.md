# Prompt style (on-demand reference)

<role>
The house style for this project's agent-facing prompt surfaces: `CLAUDE.md`, `docs/hazards.md`,
`docs/coding-style.md`, this file, and `.claude/commands/*.md`. It applies `PROMPT_GUIDELINES.md`
(the vendored canonical guide, Opus-4.8 section included) to these specific surfaces. Read it when
writing or editing any of them, and use the checklist at the end as the conformance gate.
</role>

<scope>
This governs prompt text that an agent executes. `PROMPT_GUIDELINES.md` is the upstream authority;
when the two disagree, the guide wins and this file gets corrected. The living-state artifacts
(`BACKLOG.md`, `RETRO.md`, `VELOCITY.md`, `SPRINT.md`) are append-only logs, not instruction
surfaces: their headings are frozen anchors, but their entry prose is out of scope. `PROMPT_GUIDELINES.md`
itself is vendored and is never hand-edited here.
</scope>

## Prime directive: the rule reads standalone

Opus 4.8 follows instructions literally. A reader must be able to act on the rule without first
reconstructing which sprint discovered it or untangling a nested aside. So separate three things that
tend to fuse together: the **rule** (what to do now), the **rationale** (why), and the **provenance**
(which sprint, `S###`). State the rule first and plainly; put rationale second; quarantine provenance
to its own slot. If a section's rule lives only in its title or a trailing parenthetical, lift it into
a real sentence.

## The hazard entry template

Every `docs/hazards.md` section uses this ordered shape (the preamble already claims it; this makes it
mandatory). Short principle notes carry only the labels that apply.

- **Rule.** One sentence. The invariant, as a positive imperative, with no `S###`. This is the claim
  the reader acts on.
- **Trigger.** The exact `pick_target.py` flag token (in backticks) or the asm symptom that routes
  here. One sentence, or a short list of flag variants.
- **Procedure.** Numbered imperative steps to a green ROM SHA-1. No `S###` in step text.
- **Sub-cases.** (optional) Each variant as its own `**bold lead-in.**`, or a `###` child when it
  needs its own grep-anchor.
- **Caveats.** (optional) False tells, edge conditions, and the "do not read X as Y" traps.
- **Provenance.** The `S###` tags, semicolon-separated, each with a 2-to-4-word gloss of what that
  sprint contributed.

## Provenance and sprint references

- **Quarantine `S###` out of the rule.** No sprint number appears in a Rule, Trigger, or Procedure
  line. One rule, specialized by doc type: `hazards.md` entries collect sprint tags in the
  `**Provenance.**` slot; the prose surfaces (`CLAUDE.md`, the commands) have no per-entry slot, so a
  sprint tag moves to a trailing citation on its sentence, e.g. `... (S148)`.
- **Keep an evidence citation inline.** The one exception is a concrete worked example inside a step
  that names the function or case a sprint pinned, e.g. `compute lo last to drop its live range 27->26
  (S158 func_80068308)`. Keep it when deleting it would lose a pointer to evidence; move it to
  Provenance when it only records who decided.
- **Cap Provenance density.** When a section cites six or more distinct sprints, group them by
  contribution (`established: S158; codec-triage tell: S164; source-side levers: S166`) rather than a
  flat eleven-item list, so the slot does not become a new run-on.

## Conditions are logic, not emphasis (load-bearing)

Never soften or drop a scope-limiter or a threshold. Words that gate an action (`only`, `not`,
`every`, `all`, `before`, `unless`) and every numeric threshold (`>= 0.97`) carry the rule's logic.
De-shout the glyph if it is shouted, but keep the condition and its binding intact. A gate with
several conditions that must all hold (an all-of-(a, b, c) rule like the verbatim-mirror exemption)
stays one joined gate; do not split it into separate sentences that lose the "all must hold jointly"
binding.

## Prose

- One idea per sentence. Break a run-on into ordered steps or short sentences, except where a
  conjunctive gate must stay joined (above).
- At most one level of parenthetical. A second aside becomes its own sentence.
- Positive imperative: say what to do. Reserve a prohibition for a genuine footgun.
- Match the surrounding surface's structure (XML section tags, tables, fenced examples) rather than
  inventing a new one.

## Emphasis

- Prefer structure to shouting. Bold a lead-in, or split a clause into its own sentence, instead of
  ALL-CAPS.
- De-shout these words when they are used for emphasis: `NOT, ONLY, NEVER, ALWAYS, MUST, DUPLICATE,
  DEAD, ZERO, ONE, TWO, BUNDLED, STATICALLY, PERVASIVELY, EXACT`.
- Never touch caps that carry meaning: a token inside backticks; a define, macro, type, or `nm` class
  (`VERSION_K`, `N_MICRO`, `IO_WRITE`, `F3DEX_GBI_2`, `SUPPORT_NAUDIO`, `_FINALROM`, `nm T/D/b`); or a
  filename or segment name.

## The anchor API

Heading text in `docs/hazards.md` generates GitHub `#anchors` that the rest of the repo cites as
grep-keys: `CLAUDE.md`'s hazard index, the commands, the state docs, the tools, and hazards.md's own
cross-references. A citation resolves when it equals an anchor exactly or is the leading slug of
exactly one heading (the shorthand form, e.g. `#file-static` for `file-static-bss-layout-conflict`).

- Treat cited anchors as a frozen API. Freeze a heading line byte-for-byte unless the change is a
  deliberate, verified anchor edit.
- The only sanctioned heading edits in the rework are de-narrating the seven `S###`-titled headings
  (dropping the uncited sprint gloss, which the leading slug does not depend on).
- `tools/hazard_anchors.py` enforces this: `check` asserts every frozen cited anchor (in
  `tests/tooling/hazard_cited_anchors.txt`) still resolves, and `make test-tools` runs it. After a
  deliberate anchor change, update every citation in lockstep and re-run `freeze`.

## Diff discipline

Match the existing ~100-column wrap and change words, not line breaks. Re-wrapping an untouched
paragraph turns `git diff` into whole-paragraph churn and hides the real change from both the
token-diff check and a reviewer. Reflow only the lines whose content actually changed.

## Examples

A hazard entry, before and after. The rule was buried in sprint narration and shouted; the rewrite
leads with a standalone rule and quarantines the sprint tags.

```
Before:
## widget-reload-tell (the S140 dead-reload playbook)
A mirror global with a dead reload after `x++` (the ROM reloads the store) is the volatile tell
(S140); it ALSO shows up as recompute-not-CSE of `a-b` (S142), so do NOT treat the reload as a bug —
mark the global `vu32` and it matches (S140 func_80010abc, S142 func_80010def).

After:
## widget-reload-tell
**Rule.** Mark a mirror global `vu32` when the ROM reloads it after a store; the reload is the
volatile tell, not a compiler bug.
**Trigger.** A dead reload after `x++`, or `a-b` recomputed where CSE would reuse it.
**Procedure.**
1. Retype the fixed global as `vu32` in its header.
2. Rebuild; the reload and the recompute both reappear and the ROM SHA-1 matches.
**Provenance.** dead-reload tell: S140 (func_80010abc); recompute-not-CSE variant: S142 (func_80010def).
```

## Conformance checklist

Run this before committing a prompt-surface edit (the `/sprint-review` apply-point points here).

- [ ] Every rule reads standalone; no rule lives only in a title or trailing parenthetical.
- [ ] No `S###` in a Rule / Trigger / Procedure line (Provenance slot or trailing citation instead).
- [ ] Every scope-limiter and threshold is preserved; no conjunctive gate was split apart.
- [ ] ALL-CAPS is gone except backticked / define / filename tokens.
- [ ] One idea per sentence; at most one parenthetical level.
- [ ] Cited anchors unchanged, or changed with citations updated in lockstep and `freeze` re-run.
- [ ] `make test-tools` passes (`tools/hazard_anchors.py check` green).
- [ ] The diff changes words, not untouched line wraps.
