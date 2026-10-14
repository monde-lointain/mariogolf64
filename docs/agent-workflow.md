---
type: Guide
title: Mario Golf 64 decomp workflow
description: Routes users to the native decomp skill and on-demand technical guidance.
sources:
  - resource: ../.omp/skills/mg64-decomp/SKILL.md
    title: Native decomp workflow contract
  - resource: ../tools/verify-rom.sh
    title: ROM verification gate
---

# Mario Golf 64 decomp workflow

Use the native Oh My Pi [`mg64-decomp` skill](../.omp/skills/mg64-decomp/SKILL.md) or the `/decomp`
command to select, iterate, bank, and review decompilation work. The root [AGENTS.md](../AGENTS.md)
is the sole always-loaded project context. This guide is a navigation page, not a second procedure.

The ROM SHA-1 against `baserom.z64` is the only match authority; the skill uses
[`tools/verify-rom.sh`](../tools/verify-rom.sh) as the bank gate. Keep candidate experiments isolated
and serialize access to the shared build tree and Ghidra MCP.

## Where to read

| Need | Read |
| --- | --- |
| Select, iterate, bank, review | [Native decomp skill](../.omp/skills/mg64-decomp/SKILL.md) |
| Candidate flag or residual | [Hazard routing](hazard-index.md), then the selected [playbook](hazards.md) |
| Source lever | [C coding style](coding-style.md) and [source levers](levers.md) |
| Tooling for references and campaigns | [Campaign guide](campaigns.md) |
| Bank-aware Ghidra context | [Ghidra guide](ghidra.md) |
| Fan-out and isolated work | [Fan-out rules](workflow/fan-out.md) |
| Prompt authoring | [Prompt style](prompt-style.md) |
| Build setup and supported commands | [Build guide](build.md) and [tooling entrypoints](tooling.md) |
