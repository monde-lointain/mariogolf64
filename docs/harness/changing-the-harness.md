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
* the candidate gate: [verify-candidate](/tools/verify-candidate.md)
* `tools/data-differ/`, the [code-quality checkers](/tools/quality-checkers.md), `.omp/hooks/pre/mg64-policy.ts`

These additionally require:

1. a test that **fails before** the change and shows the old behavior was a real false positive or false negative;
2. for scorer or certification changes, the score of at least one existing environment (or a fixture) before and after, quoted in the commit message;
3. for policy-hook changes, the guarded action stays blocked. A fix may narrow a pattern that blocks legitimate commands (for example reading a file, or the read-only `print-cflags` query). It may never remove a rule or a protected path.

A `COMPILER PROFILES` override in the Makefile is a source change, not a comparison change: the checksum verifies it.

Never weaken a check to make a specific function pass. If a check is wrong, fix it for every input and add the test.

# Commits

Commit a harness change by itself, as soon as it is done, staging only its own paths (`git add <paths>`, never `-A`), with the subject prefix `harness:`. This keeps comparison changes bisectable. Under [nigel](/tools/task-runner.md), `reset_command` wipes uncommitted work after every failed candidate and at startup, while the default `success_command` runs `git add -A`, so an uncommitted fix would be swept into the candidate's commit. The [pre-commit hook](/harness/git-hook-and-ci.md) rebuilds the whole tree, so commit while `src/` still builds; in a decomp environment that means before integrating the function into `src/`.

# Docs are part of the harness

The authoring rules for this bundle (frontmatter, directory indexes, links, the log and skill symlinks) are in [docs conventions](/harness/docs-conventions.md). Read them before editing any concept.
