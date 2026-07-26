# Prompt style (on-demand reference)

<role>
The house style for this project's agent-facing prompt surfaces: `AGENTS.md`, `CLAUDE.md`,
`docs/agent-workflow.md`, `docs/hazards.md`, `docs/coding-style.md`, this file,
`.agents/skills/*/SKILL.md`, and `.claude/commands/*.md`. Read it when writing or editing any of
them, and use the checklist at the end as the conformance gate.
</role>

<scope>
This governs prompt text that an agent executes. The living-state artifacts (`BACKLOG.md`,
`RETRO.md`, `VELOCITY.md`, `SPRINT.md`) are append-only logs, not instruction surfaces: their
headings are frozen anchors, but their historical entry prose is out of scope.
</scope>

## Prime directive: outcome first, gates explicit

Gpt-5.5 works best when prompts define the target outcome, success criteria, constraints, available
evidence, validation, and stop conditions. State what good looks like before detailed procedure.
Keep process instructions only when order matters for the ROM oracle, MCP/build safety, or Scrum gate.

Separate three things that tend to fuse together: the **rule** (what to do now), the **rationale**
(why), and the **provenance** (which sprint, `S###`). State the rule first and plainly; put rationale
second; quarantine provenance to its own slot. If a section's rule lives only in its title or a
trailing parenthetical, lift it into a real sentence.

## The hazard entry template

Every `docs/hazards.md` section uses this ordered shape. Short principle notes carry only the labels
that apply.

- **Rule.** One sentence. The invariant, as a positive imperative, with no `S###`. This is the claim
  the reader acts on.
- **Trigger.** The exact `pick_target.py` flag token (in backticks) or the asm symptom that routes
  here. One sentence, or a short list of flag variants.
- **Procedure.** Numbered imperative steps to a green ROM SHA-1. No `S###` in step text.
- **Sub-cases.** Optional. Each variant as its own `**bold lead-in.**`, or a `###` child when it needs
  its own grep-anchor.
- **Caveats.** Optional. False tells, edge conditions, and "do not read X as Y" traps.
- **Provenance.** The `S###` tags, semicolon-separated, each with a 2-to-4-word gloss of what that
  sprint contributed.

## Gpt-5.5 prompt rules

- Prefer concise, outcome-first prompts over process-heavy stacks.
- Use absolute rules only for true invariants: safety, file mutation limits, required output fields,
  required validation, and things that must never happen.
- For judgment calls, write decision rules: when to search, ask, stop, retry, escalate, or fall back.
- Add retrieval budgets for research-heavy prompts. Search again only when required evidence, a
  source, or a precise parameter is missing.
- Add validation loops where tools can check the result: targeted tests, build, ROM SHA-1, anchor
  checks, or skill validation.
- Use short preambles for long/tool-heavy work so the user sees the first step.
- Keep formatting proportional. Use bullets and headers when they improve scanning; otherwise prefer
  short paragraphs.

## Provenance and sprint references

- **Quarantine `S###` out of the rule.** No sprint number appears in a Rule, Trigger, or Procedure
  line. One rule, specialized by doc type: `hazards.md` entries collect sprint tags in the
  `**Provenance.**` slot; prose surfaces use a trailing citation only when it is useful evidence.
- **Keep an evidence citation inline.** The exception is a concrete worked example inside a step that
  names the function or case a sprint pinned, e.g. `compute lo last to drop its live range 27->26
  (S158 func_80068308)`. Keep it when deleting it would lose a pointer to evidence.
- **Cap Provenance density.** When a section cites six or more distinct sprints, group them by
  contribution (`established: S158; codec-triage tell: S164; source-side levers: S166`) rather than a
  flat list.

## Conditions are logic

Never soften or drop a scope-limiter or a threshold. Words that gate an action (`only`, `not`,
`every`, `all`, `before`, `unless`) and every numeric threshold (`>= 0.97`) carry the rule's logic.
De-shout the glyph if it is shouted, but keep the condition and its binding intact. A conjunctive gate
with several conditions that must all hold stays one joined gate; do not split it into separate
sentences that lose the "all must hold jointly" binding.

## Prose

- One idea per sentence. Break run-ons into ordered steps or short sentences, except where a
  conjunctive gate must stay joined.
- At most one level of parenthetical. A second aside becomes its own sentence.
- Positive imperative: say what to do. Reserve a prohibition for a genuine footgun.
- Match the surrounding surface's structure rather than inventing a new one.

## Emphasis

- Prefer structure to shouting. Bold a lead-in, or split a clause into its own sentence, instead of
  All-caps.
- De-shout words used only for emphasis: `NOT`, `ONLY`, `NEVER`, `ALWAYS`, `MUST`, `DUPLICATE`,
  `DEAD`, `ZERO`, `ONE`, `TWO`, `BUNDLED`, `STATICALLY`, `PERVASIVELY`, `EXACT`.
- Never touch caps that carry meaning: a token inside backticks; a define, macro, type, or `nm` class
  (`VERSION_K`, `N_MICRO`, `IO_WRITE`, `F3DEX_GBI_2`, `SUPPORT_NAUDIO`, `_FINALROM`, `nm T/D/b`); or a
  filename or segment name.

## The anchor API

Heading text in `docs/hazards.md` generates GitHub `#anchors` that the rest of the repo cites as
grep-keys. A citation resolves when it equals an anchor exactly or is the leading slug of exactly one
heading.

- Treat cited anchors as a frozen API. Freeze a heading line byte-for-byte unless the change is a
  deliberate, verified anchor edit.
- `tools/hazard_anchors.py` enforces this: `check` asserts every frozen cited anchor in
  `tests/tooling/hazard_cited_anchors.txt` still resolves, and `make test-tools` runs it. After a
  deliberate anchor change, update every citation in lockstep and re-run `freeze`.

## Diff discipline

Match the existing ~100-column wrap and change words, not untouched line breaks. Re-wrapping an
untouched paragraph turns `git diff` into whole-paragraph churn and hides the real change from both
the token-diff check and a reviewer. Reflow only the lines whose content actually changed.

## Conformance checklist

Run this before committing a prompt-surface edit.

- [ ] The prompt states outcome, success criteria, constraints, validation, and stop conditions.
- [ ] Every rule reads standalone; no rule lives only in a title or trailing parenthetical.
- [ ] No `S###` in a Rule / Trigger / Procedure line.
- [ ] Every scope-limiter and threshold is preserved; no conjunctive gate was split apart.
- [ ] all-caps is gone except backticked / define / filename tokens.
- [ ] One idea per sentence; at most one parenthetical level.
- [ ] Cited anchors unchanged, or changed with citations updated in lockstep and `freeze` re-run.
- [ ] `make test-tools` passes (`tools/hazard_anchors.py check` green).
- [ ] The diff changes words, not untouched line wraps.
