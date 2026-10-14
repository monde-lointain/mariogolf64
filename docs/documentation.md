---
type: Reference
title: Documentation structure and maintenance
description: Defines the OKF bundle boundary, progressive reading routes, metadata, and documentation validation rules.
sources:
  - resource: https://github.com/GoogleCloudPlatform/open-knowledge-format/blob/main/SPEC.md
    title: Open Knowledge Format specification
---

# Documentation structure and maintenance

## Bundle boundary

`docs/` is the project's OKF v0.2 knowledge bundle. Start at [index.md](index.md), choose a task
or symptom, then open only the selected concept or heading. Narrower directory indexes expose
[workflow decisions](workflow/index.md), [function evidence](wip/index.md), and
[banked evidence](wip/banked/index.md); they are listings, not additional workflow instructions.

Root project guides, native Oh My Pi skills and commands, sprint records, and container/dependency
guides live outside this recursive bundle. Root guides and records use compatible concept metadata;
skills and commands retain their native invocation contracts. Vendored tools, virtual environments,
generated output, and dependency documentation are not bundle concepts. Links may leave the bundle
to reference implementation or evidence.

## Concept metadata

Every `.md` file in the bundle except reserved `index.md` and `log.md` files begins with parseable
YAML frontmatter containing a nonempty string `type`. Use a descriptive `title` and a one-sentence
`description` that the directory index can reuse. A concept answers one coherent question; split
unrelated operational detail rather than growing another command catalog or workflow copy.

```yaml
---
type: Reference
title: A specific concept
description: Explains the decision this reference supports.
sources:
  - resource: ../tools/example.py
    title: Relevant implementation
---
```

`sources` names actual evidence. Trust fields such as `verified`, `generated`, and `status` are
optional: omit them rather than inventing authorship, timestamps, or verification. Reconstruction
notes are evidence at the time of their sprint, not promises about today's source state.

`index.md` contains grouped Markdown links and short descriptions, not concept frontmatter or
procedures. Every immediate child concept and child directory must be discoverable there. Only
the bundle-root index may declare `okf_version: "0.2"`; nested indexes have no frontmatter.
`log.md` has no frontmatter: it is a flat list of updates under ISO `YYYY-MM-DD` date headings,
newest first. Existing sprint records retain their names and frozen bodies; they are typed
concepts outside the bundle, not OKF `log.md` files.

## Authority and discovery

- [Build](build.md) owns setup, profiles, outputs, and verification commands; implementation is
  authoritative when prose and code disagree. [Tooling](tooling.md) is a command selector, not
  another build or execution guide.
- The [native skill](../.omp/skills/mg64-decomp/SKILL.md) owns select/iterate/bank/review.
  [Execution details](workflow/loop.md) own technical decisions and oracle limits; delegated
  analysis has its own [resource-ownership contract](workflow/fan-out.md).
- Candidate/build hazards and observed matching residuals have separate lookup concepts. Both
  link to the detailed playbook section; the detail does not belong in the routing table.
- `hazards.md` remains a section-addressable reference because its frozen heading anchors are used
  by the existing tooling and sprint evidence. Select a family or flag before reading a section.
  Do not load its complete body as an onboarding step.
- Current workflow rules live in the guides, not in [historical records](history.md). Historical
  numbers and retired advice are explicitly time-scoped evidence.

## Changes and validation

Follow [prompt style](prompt-style.md) when editing instructions. Preserve hazard-heading slugs:
they are consumed by tooling, the frozen manifest, and reconstruction evidence. An intentional
anchor change requires migrating live citations and the manifest together. Relative Markdown
links work in both the repository browser and this bundle; include the full generated heading slug,
not a prefix accepted only by the hazard diagnostic. Ignore links demonstrated inside code fences.

Before delivering a documentation change:

1. Parse every concept's frontmatter; require a nonempty string `type`. Check that the native skill
   still has its required `name` and `description`, without imposing OKF metadata on the command.
2. Check reserved index/log structure and immediate-child index coverage at every bundle directory.
3. Resolve local Markdown links, exact heading fragments, and local `sources` paths. Include root
   guides and the skill when checking links into or out of the bundle.
4. Run `uv run --locked python tools/hazard_anchors.py check` for the frozen hazard-anchor contract.
   This is not a general Markdown link checker: it also accepts unique prefix citations that do
   not navigate to actual headings.
5. Follow each changed command through its CLI implementation and exercise safe representative
   invocations. Read-only documentation checks do not establish a ROM match, start Ghidra, or
   authorize concurrent builds.
6. Report only checks actually run. The tooling suite and full ROM gate have separate ownership
   in [the build guide](build.md#tooling-checks); a documentation-only edit needs neither merely
   to claim that navigation works.

Update [log.md](log.md) only after the parent's smoke proof. Do not rewrite sprint history to normalize
obsolete prose.
