---
type: Policy
title: "Docs conventions"
description: "How the docs bundle is authored: frontmatter, directory indexes, bundle-absolute links, one owner per fact, current-behaviour-only text and the log; read before adding or moving a concept."
tags: [policy, docs]
---

The bundle follows the [Open Knowledge Format](https://github.com/GoogleCloudPlatform/open-knowledge-format) v0.2. Root `docs/index.md` is the entry point; each directory's `index.md` routes into its concepts.

# Frontmatter

Keys appear in this order:

| Key | Rule |
|---|---|
| `type` | `Overview`, `Policy`, `Guide`, `Procedure`, `Tool` or `Reference`. |
| `name` | Skills only. Equal to the file name without `.md`. |
| `title` | Human-readable title; copied verbatim into the directory index. |
| `description` | One sentence, at most 200 characters, saying whether a reader should open the concept. |
| `tags` | Short lowercase keywords. |
| `resource` | Only when the concept documents one repo file. Path relative to the concept, e.g. `../../tools/progress.py`. |

A skill's `description` is also its omp trigger, so keep it task-shaped: `…; use when …`.

# Directory indexes

* `index.md` has no frontmatter. The root file carries only `okf_version`.
* Sections are headed; each entry is `* [<title>](<file>) - <description>`, with title and description copied verbatim from the concept's frontmatter.
* Procedures entries append ` *(skill)*` for skills.
* Every concept and subdirectory is listed.
* Adding, moving or retitling a concept updates its directory index in the same change.

# Links and ownership

* One owner per fact. State a rule, format or flag once and link to it from everywhere else. Owners by kind:

| Fact | Owner |
|---|---|
| Non-negotiables | [ground rules](/project/ground-rules.md) |
| What acceptable C looks like | [code-quality standards](/project/code-quality.md) |
| Compiler flags per file | [compiler profiles](/project/compiler-profiles.md) |
| What C shape yields what assembly | [KMC GCC reference](/reference/kmc-gcc/) |
| Splat and `symbol_addrs.txt` formats | [splat config](/reference/splat-yaml.md), [symbol_addrs format](/reference/symbol-addrs-format.md) |
| A script's flags, inputs, outputs and exit codes | its concept in [tools](/tools/); procedures give steps and exact command lines, then link |
| nigel tasks and their commands | [task-runner](/tools/task-runner.md) |
| Where harness code lives, its class and tests | [code map](/harness/code-map.md) |
| What the policy hook blocks | [policy hook](/harness/policy-hook.md) |

* Links between concepts are bundle-absolute (`/tools/decomp-env.md`). Skills are read through `.omp/skills/<name>/SKILL.md`, where relative links break.
* Repo files outside `docs/` are backticked code paths, not links.
* Cite code by file and function, option or symbol name, never by line number; line numbers go stale with the next edit.

# Content

* Concepts describe current behaviour only. Dates, changes, before/after figures and costs go to `docs/log.md`.
* Bodies are terse and structural: headings, short lists, tables, fenced code. Procedures are imperative.
* Do not open a body by restating the description.

# Skills

* `.omp/skills/<name>/SKILL.md` is a symlink into `docs/procedures/`. Edit the concept, never the symlink.
* A new skill needs both the concept (with `name:`) and its symlink.

# Log

`docs/log.md` is the only place for history. It is newest date first: a `## YYYY-MM-DD` heading, then `* **Update**: <text>` lines. It records notable docs and harness changes, never individual function matches.

# Scratch knowledge

* Generic, reusable compiler lessons go into [/reference/kmc-gcc/](/reference/kmc-gcc/) under its [editing rules](/reference/kmc-gcc/editing-rules.md).
* Per-function findings stay in that function's workspace `NEAR_MISS.md` (and `LEARNINGS.md`) until the `consolidate-learnings` nigel task folds the generic ones in ([list-unconsolidated-learnings](/tools/list-unconsolidated-learnings.md)).
