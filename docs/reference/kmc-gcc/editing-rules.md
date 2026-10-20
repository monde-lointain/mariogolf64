---
type: Policy
title: "Editing rules for the KMC GCC reference"
description: "Rules for adding KMC GCC lessons: generic, probe-verified compiler and build behavior only; per-function findings stay in the workspace NEAR_MISS.md."
tags: [policy, docs]
---

* Add a lesson only when it is a **generic, reusable** property of KMC GCC 2.7.2 or of the build that no concept here already covers. Put it in the matching concept, in that file's style.
* Verify each lesson with a probe: `tools/gcc_kmc/gcc` with the base profile flags from [compiler profiles, base profile](/project/compiler-profiles.md#base-profile). Write probe outputs under `/tmp`.
* Never record function or symbol names, ROM or VRAM addresses, match percentages, attempt histories, commits, or one-off layout discoveries.
* Test before adding: would this lesson help match an unrelated function tomorrow? If not, leave it out.
* A lesson that conflicts with [code-quality](/project/code-quality.md) or [matching method](/procedures/matching-method.md) is not a lesson; do not add it. When a note refutes text here, correct the text.
* Per-function [decomp-env](/tools/decomp-env.md) sessions treat this reference as read-only and write their findings to the workspace `NEAR_MISS.md`.
