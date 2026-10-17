---
type: Agent Instructions
title: "Mario Golf 64 project instructions"
description: Routes agents to the sticky project invariants, the native decomp skill, and the documentation index.
---

# Mario Golf 64 project instructions

The project invariants live in [.omp/RULES.md](.omp/RULES.md): Oh My Pi applies them on every
request and forwards them to task subagents, which do not receive this file. Invoke `/decomp` or
the `mg64-decomp` skill for the decompilation workflow; every other reference is routed from
[documentation navigation](docs/index.md).
