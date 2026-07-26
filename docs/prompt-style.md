# Prompt style (on-demand reference)

<role>
The house style for this project's agent-facing prompt surfaces: `AGENTS.md`, `CLAUDE.md`,
`docs/agent-workflow.md`, `docs/hazards.md`, `docs/coding-style.md`, this file,
`.agents/skills/*/SKILL.md`, and `.claude/commands/*.md`. Read it when writing or editing any of
them, and use the checklist at the end as the conformance gate.
</role>

<scope>
This governs prompt text that an agent executes. `BACKLOG.md`, `RETRO.md` and `VELOCITY.md` are
append-only logs, not instruction surfaces: their headings are frozen anchors, but their historical
entry prose is out of scope, and they are excluded from citation harvesting for that reason.

`SPRINT.md` is split. Its `## Method`, `## Definition of Done` and `## Scope` sections are
instruction text -- they are read at every resume and `## Method` is pure procedure -- so they follow
this style, checked advisory-only by `prompt_lint.py check --sprint`. Its standup log, per-sprint
result, PO decisions and suggestion buffer are append-only log. The file is gitignored, so it never
gates the suite: a fresh clone has none.
</scope>

## Prime directive: outcome first, gates explicit

Agent prompts work best when they define the target outcome, success criteria, constraints, available
evidence, validation, and stop conditions. State what good looks like before detailed procedure.
Keep process instructions only when order matters for the ROM oracle, MCP/build safety, or Scrum gate.

This file names no model. The project runs on more than one harness and the surfaces are shared, so a
rule tied to one vendor's model rots on the other and reads as authority it does not have. Where two
harnesses genuinely differ, say so in the harness deltas at the end.

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

## Prompt rules

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
- **Supersede, do not nest.** When a sprint sharpens an existing rule, rewrite the rule and append its
  `S###` to the provenance slot. Reserve a new sub-bullet for a genuinely new case. A fourth bullet
  level means a sharpening was nested under the rule it sharpens instead of replacing it; flatten it.
  `tools/prompt_lint.py` tracks nesting depth for this reason.

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
- De-shout any word capitalized only for emphasis. `tools/deshout.py` does this mechanically and
  asserts `lower(before) == lower(after)`, so no word can be dropped, reordered or respelled.
- Never touch caps that carry meaning: a token inside backticks; a define, macro, type, or `nm` class
  (`VERSION_K`, `N_MICRO`, `IO_WRITE`, `F3DEX_GBI_2`, `SUPPORT_NAUDIO`, `_FINALROM`, `nm T/D/b`); or a
  filename or segment name. The single list of protected terms is
  `tests/tooling/prompt_caps_allow.txt`; add to it rather than keeping a second copy here.
- **A gate word is not emphasis.** When the caps sit on `AND`, `OR`, `BOTH`, `ALL`, `EITHER` or
  `EVERY`, they are usually binding a conjunctive or disjunctive gate, and folding them is
  word-preserving but meaning-losing. Restate the binding in prose instead: "hold integration until
  both the subagent and its permuter have reported" rather than a bolded `AND`. `deshout.py gates`
  enumerates these for review; folding one without restating it is the one way this pass loses a rule.

## The anchor API

Heading text in `docs/hazards.md` generates GitHub `#anchors` that the rest of the repo cites as
grep-keys. A citation resolves when it equals an anchor exactly or is the leading slug of exactly one
heading.

- Treat the **slug** as the frozen API, not the heading's bytes. `hazard_anchors.py` lowercases before
  slugging, so a case-only heading edit is anchor-neutral by construction. Any change to a heading's
  words or punctuation is a deliberate anchor edit: update every citation in lockstep and re-run
  `freeze`.
- `tools/hazard_anchors.py` enforces this: `check` asserts every frozen cited anchor in
  `tests/tooling/hazard_cited_anchors.txt` still resolves, and `make test-tools` runs it.
- A citation's shorthand form stops being unique the moment a sibling section lands. Prefer the
  longest unambiguous form, and never coin a shorthand that a plausible future section would collide
  with. The append-only logs are excluded from the harvest for exactly this reason: a digest written
  when a shorthand was unique is a historical record, not a live pointer, and repairing it would mean
  rewriting history.
- Cross-document citations name their file and heading in one backticked span, so
  `tools/prompt_lint.py` can resolve them. A reference written as prose ("`CLAUDE.md` holds the hazard
  index") is unverifiable and rots silently; two such references did. Write a placeholder with angle
  brackets rather than a plausible filename, or the checker resolves it and fails.

## Verification calibration

The ROM SHA-1 against `baserom.z64` is an external oracle: a match cannot be asserted, only measured.
Current models also verify their own work without being asked, so re-check layers stacked on top of
the oracle buy nothing and cost tokens.

- State each tool's authority and failure mode once, in the oracle table, rather than restating it as
  an imperative at every use site.
- One binding gate per outcome. For a bank that gate is `tools/verify-rom.sh`, and nothing else.
- Do not add a re-check for something the oracle already settles. "Confirm", "double-check" and
  "re-verify" belong in a prompt only when they name a tool whose failure mode is documented.
- Write a tool's failure mode as a fact with its evidence, not as a prohibition. "`diff.py` reads
  `build/*.map`, which an incremental build does not refresh" outperforms "do not trust `diff.py`",
  and it survives being lowercased.

## Subagent control

Current models delegate readily, and delegation multiplies cost while serializing at integration.

- Give a trigger, not an exhortation. The fan-out doctrine holds the trigger table; a prompt surface
  points at it rather than re-arguing the case.
- Derive the head count, never write a literal. One agent per independently-compilable unit, plus at
  most one rule-out agent.
- Never delegate a task finishable in a handful of tool calls, context gathering, or verification of
  your own work.
- A fan-out prompt is a file to fill in, not prose to remember. Remembered boilerplate does not
  survive a fresh context window.
- Every agent-facing contract clause names the harness it applies to.

## Effort and thinking

Effort is a session setting on both harnesses and is not settable from a repo file, so a surface
states a recommendation and never claims to set one.

| Phase | Effort | Why |
| --- | --- | --- |
| `sprint-plan`, `sprint-review` | low or medium | ranking, reading, a gate build; the tools carry the judgment |
| mirror branch | low or medium | a verbatim copy plus a ROM gate |
| classical iterate, crack slice, permuter triage | high or above | the reasoning is the work |

This table is current practice, not an evaluated result: it was inherited from a skill file's aside.
Re-run an effort sweep against real sprints before treating it as calibration.

Prefer a general instruction over a prescribed reasoning chain. "Re-derive the residual from a fresh
object" outperforms "spend the first iteration doing X, then Y, then Z": a prescribed chain becomes
wrong the moment the situation differs.

## Narration and verbosity

Default responses and written documents run long, and effort does not reliably shorten them, so ask
for length explicitly.

- Report at bank boundaries, not per tool call. One `SPRINT.md` standup line per banked function.
- Lead the final response with the outcome -- banked or carried, and the ROM verdict -- then detail.
- A `docs/wip/<fn>.near-match.md` records the measured residual, the levers ruled out with their
  `file:line` citations, and the verdict. It is not a narrative of the attempt.
- Claim nothing the verification output does not show.

## Harness deltas

Everything above is harness-neutral. These are the differences worth stating.

| | Claude Code | Codex |
| --- | --- | --- |
| Always-loaded surface | `CLAUDE.md` | `AGENTS.md` (kept short for project-doc loading limits) |
| Gate invocation | `/sprint-plan`, `/sprint-review` in `.claude/commands/` | `$mg64-sprint-plan`, `$mg64-decomp-loop`, `$mg64-sprint-review` in `.agents/skills/` |
| Subagent mechanism | `.claude/agents/*.md`, or a mesh with `SendMessage` | mesh agents |
| Hand-off contract | mesh: send the verdict before idling. Task-tool: the return value is the verdict | mesh contract applies |
| Effort control | session setting | session or CLI setting |
| Shared source | `docs/agent-workflow.md`, `docs/hazards.md` | the same files |

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
- [ ] Emphasis caps are gone except backticked / define / filename tokens.
- [ ] Any gate word that was de-shouted had its binding restated in prose.
- [ ] One idea per sentence; at most one parenthetical level.
- [ ] Cited anchors unchanged, or changed with citations updated in lockstep and `freeze` re-run.
- [ ] No model name appears outside a harness-delta row.
- [ ] Every verification imperative names a tool with a documented failure mode.
- [ ] No literal subagent head count.
- [ ] Every cross-document reference is a checkable file-and-heading citation, and resolves.
- [ ] `make test-tools` passes (`hazard_anchors.py check` and `prompt_lint.py check` green).
- [ ] The diff changes words, not untouched line wraps.
