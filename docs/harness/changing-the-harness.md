---
type: Policy
title: "Changing the harness"
description: "Workflow changes are free; comparison changes need a failing-first test and before/after evidence; protected paths never change; commit harness changes separately."
tags: [policy, harness]
---

The **harness** is everything that builds, scores, selects or polices the work: `tools/` (except the protected paths), `tools/decomp-env-template/`, the `Makefile`, `task-runner/`, `.omp/` (config, skills, policy hook), `AGENTS.md`, `README.md`, and `docs/`. [Code map](/harness/code-map.md) shows where each job lives and which tests cover it.

Any session, including a single-function decomp environment or a nigel candidate, may fix a harness bug or remove friction, and should do so instead of working around it. Examples: a tool crash, a policy-hook false positive, a misleading error message, a missing flag. Keep the fix general and small, then return to the original task.

# Protected, never changed by an agent

The [protected paths](/project/ground-rules.md#2-protected-paths) are the ground truth a match is measured against and are never part of a harness fix. If a fix would need one of them, stop and report it to the user.

# Two classes of change

**Workflow changes**: CLI ergonomics, output, selection and ranking, environment setup, docs, skills, task prompts. Make them, keep the tests green ([code map](/harness/code-map.md#tests)), and update the doc or skill that describes the behavior.

**Comparison changes**: anything that can change what counts as a match or what may be committed:

* build, link and checksum rules: `Makefile`, `tools/build-and-verify.sh`, `tools/git-hooks/pre-commit`
* the scorer: `tools/decomp-env-template/build.sh`, `dist.py`, `normalize_asm.py`, `objdump.py`, `prelude.inc`
* match certification: `tools/nonmatching_match_log.py`, `tools/m2c_one_shot.py`
* `tools/data-differ/`, the [code-quality checkers](/tools/quality-checkers.md), `.omp/hooks/pre/mg64-policy.ts`

These additionally require:

1. a test that **fails before** the change and shows the old behavior was a real false positive or false negative;
2. for scorer or certification changes, the score of at least one existing environment (or a fixture) before and after, quoted in the commit message;
3. for policy-hook changes, the guarded action stays blocked. A fix may narrow a pattern that blocks legitimate commands (for example reading a file, or the read-only `print-cflags` query). It may never remove a rule or a protected path.

A `COMPILER PROFILES` override in the Makefile is a source change, not a comparison change: the checksum verifies it.

Never weaken a check to make a specific function pass. If a check is wrong, fix it for every input and add the test.

# Commits

Commit a harness change by itself, as soon as it is done, staging only its own paths (`git add <paths>`, never `-A`), with the subject prefix `harness:`. This keeps comparison changes bisectable. Under [nigel](/tools/task-runner.md), `reset_command` (`git clean -f; git reset --hard HEAD || true`) wipes uncommitted work after every failed candidate and at startup, and `success_command` (`git add -A && git commit`) would mix an uncommitted fix into the candidate's commit. The pre-commit hook rebuilds the whole tree, so commit while `src/` still builds; in a decomp environment that means before integrating the function into `src/`.

# Docs are part of the harness

This bundle follows [OKF](https://github.com/GoogleCloudPlatform/open-knowledge-format) and is read top-down: `docs/index.md`, a section `index.md`, then single concepts.

* Frontmatter, in this order: `type` (`Overview`, `Policy`, `Guide`, `Procedure`, `Tool` or `Reference`), `name` (skills only, equal to the file name), `title`, `description`, `tags`, and `resource` when the concept documents one repo file (path relative to the concept).
* `description` is one sentence of at most 200 characters that tells a reader whether to open the concept; for skills it is also the omp trigger, so keep it task-shaped ("…; use when …"). Each directory's `index.md` lists every concept and subdirectory as `* [<title>](<file>) - <description>`, with title and description copied verbatim.
* One owner per fact: state a rule, format or flag in one concept and link to it from the others.
* Links between concepts are bundle-absolute (`/tools/decomp-env.md`): skills are read through `.omp/skills/<name>/SKILL.md`, where relative links break. Repo files outside `docs/` are written as code paths, not links.
* Concepts describe current behaviour only. Dates, changes, before/after figures and costs go to `docs/log.md`, newest date first. The log records notable docs and harness changes, never individual function matches.
* Skills are symlinks into `docs/procedures/`; edit the concept, not the symlink. A new skill needs both the concept (with `name:`) and the symlink.
