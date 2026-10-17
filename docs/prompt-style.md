---
type: Reference
title: Agent prompt style
description: Authoring rules for concise, checkable project instructions.
---

# Prompt style (on-demand reference)

Use this guide when editing agent-facing instructions such as `AGENTS.md`, `.omp/RULES.md`, the Oh My Pi skill and
command, or shared workflow references. It governs prompt wording; use the
[native decomp skill](../.omp/skills/mg64-decomp/SKILL.md) for execution and
[C coding style](coding-style.md) for source conventions.

## Outcome-first instructions

- State the target outcome and success criteria before procedure.
- Name the relevant constraints, evidence, validation, and stop conditions.
- Keep process instructions only where order matters for matching, the ROM oracle, or shared-resource
  safety.
- Use absolute rules for actual invariants. For judgment calls, give a decision rule.
- Prefer concise standalone rules and positive imperatives. Reserve prohibitions for real hazards.
- Keep rationale and historical provenance separate from the instruction itself.
- Use a structure proportional to the task; avoid duplicate procedures across surfaces.

## Source and generated-file boundaries

Keep the existing hazard-heading anchors stable. `tools/hazard_anchors.py` and
`tests/tooling/hazard_cited_anchors.txt` preserve cited headings; update citations and the frozen
anchor list together only when an intentional heading change is required. Do not edit
`docs/hazards.md` headings or content during general prompt cleanup.

Use relative Markdown links with complete heading slugs for navigation. Preserve meaningful scope
conditions and thresholds when wording changes. Do not turn an iteration signal into a match claim:
`tools/verify-rom.sh` is the sole ROM bank authority.

## Diff discipline

Change only the wording needed and retain untouched line wrapping. Before delivering, review every
constraint and cross-document link affected by a move or consolidation. Follow the repository's
requested verification ownership; do not claim checks that were not run.
