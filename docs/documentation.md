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

`docs/` is the project's OKF v0.2 knowledge bundle. [index.md](index.md) is its discovery root;
[workflow/index.md](workflow/index.md), [wip/index.md](wip/index.md), and
[wip/banked/index.md](wip/banked/index.md) disclose narrower scopes. A reader selects a task or concept
before opening detailed procedure or historical evidence.

Root project guides, native Oh My Pi skills and commands, sprint records, and container/fingerprint
guides use compatible concept metadata but live outside this recursive bundle. Third-party tool
submodules, local virtual environments, generated output, and dependency documentation are not part of it.
Links may leave the bundle to point at implementation or history.

## Concept metadata

Each non-reserved `.md` file in the bundle begins with parseable YAML frontmatter containing a
nonempty `type`. Use a descriptive `title` and a one-sentence `description` that the directory index
can reuse. Skill frontmatter follows the native Oh My Pi skill contract; do not maintain
harness-specific wrapper copies.

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

`index.md` contains grouped Markdown links and descriptions, not concept frontmatter. Only the
bundle-root index may declare `okf_version: "0.2"`. `log.md` is a newest-first list grouped by
ISO `YYYY-MM-DD` headings. Existing sprint logs retain their names and frozen bodies; they are typed
concepts, not OKF `log.md` files.

## Authority and discovery

- Build targets, flags, output paths, and CI behavior come from their implementation. Document a
  command once in its canonical guide; other pages link to it.
- Workflow phases have separate concepts. Their section navigation supports selective reading
  without importing the whole workflow directory.
- Candidate/build hazards and observed matching residuals have separate lookup concepts. Both link
  to the detailed playbook section; the detail does not belong in the routing table.
- `hazards.md` remains a section-addressable reference because its frozen heading anchors are used
  by the existing tooling and sprint evidence. Select a family or flag before reading a section.
  Do not load its complete body as an onboarding step.
- Current workflow rules live in the guides, not in [historical records](history.md). Historical
  numbers and retired advice are explicitly time-scoped evidence.

## Changes and validation

For instruction edits, follow [prompt style](prompt-style.md). Keep existing hazard-heading slugs
stable; intentional anchor changes require migrating live citations and updating the anchor manifest
together. Use relative Markdown links with the target's complete generated slug.

Before delivering a documentation change:

1. Parse frontmatter for every bundle concept and require a nonempty `type`.
2. Check that directory indexes expose their child concepts and directories.
3. Resolve local Markdown links and full heading fragments; ignore example links inside code fences.
4. Follow verification ownership and report only checks actually run.
5. Exercise any changed command against its actual CLI or implementation; do not claim a ROM match
   from documentation checks.

Update [log.md](log.md) only after the parent's smoke proof. Do not rewrite sprint history to normalize
obsolete prose.
