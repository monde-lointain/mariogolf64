---
type: Policy
title: "Editing rules for the KMC GCC reference"
description: "What may be added to the KMC GCC learnings: generic reusable compiler and build properties only; per-function findings go to the workspace NEAR_MISS.md."
tags: [policy, docs]
---

These concepts are a curated reference of generic KMC GCC 2.7.2 and build behavior, not a work log.

* Add an entry only for a **generic, reusable** property of the compiler or the build that no concept here already covers. Put it in the matching concept and keep it short.
* Never record function or symbol names, ROM or VRAM addresses, match percentages, attempt histories, commits, or one-off layout discoveries.
* Test before adding: would this lesson help match an unrelated function tomorrow? If not, leave it out.
* A lesson that contradicts a [code-quality standard](/project/code-quality.md) (for example an inline-assembly trick) is not a lesson; do not add it.
* Per-function [decomp-env](/tools/decomp-env.md) sessions treat this reference as read-only and write findings to the workspace `NEAR_MISS.md`.
