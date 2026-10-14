---
type: Agent Instructions
title: "Mario Golf 64 project instructions"
description: Defines the always-loaded source-edit, matching, and shared-resource invariants.
---

# Mario Golf 64 project instructions

This is the sole always-loaded project context. Invoke `/decomp` or the `mg64-decomp` skill for the
decompilation workflow; consult [documentation navigation](docs/index.md) for on-demand references.

## Invariants

- Complete the YAML `main` segment, including its linked libraries, before selecting or dispatching
  overlay decompilation. The [native skill's main-completion gate](.omp/skills/mg64-decomp/SKILL.md#main-before-overlays)
  defines scope and completion; an empty shortlist or a carried wall does not unlock overlays.
- `tools/verify-rom.sh` is the sole bank authority. Never treat an isolated compile, object diff, or
  direct hash as a bank verdict.
- Never commit a non-matching function. Keep unfinished candidates isolated under `nonmatchings/`.
- Serialize builds, Ghidra MCP, YAML changes, and source edits; the build tree and MCP are shared.
- Preserve compiler-profile fidelity between isolated candidate builds and the ROM build.
- Never hand-edit generated extraction artifacts, including `mariogolf64.ld`,
  `undefined_syms_auto.txt`, `undefined_funcs_auto.txt`, `mariogolf64.d`, `ctx.c`, `ctx.c.m2c`,
  `undefined_funcs.txt`, or `undefined_syms.txt`.
- Edit `mariogolf64.yaml` only for required subsegment flip/split/path-qualifier changes; additions
  to `symbol_addrs.txt` are add-only. Do not directly edit `ghidra_symbols.txt` except through the
  supported name-sync workflow, or edit `checksum.sha1`, `symbol_addrs_manual.txt`,
  `re_tracking/*.yml`, or `reloc_addrs.txt`.

## On-demand references

- Select candidates, iterate, bank, or review: [native decomp skill](.omp/skills/mg64-decomp/SKILL.md).
- Candidate flags or residual symptoms: [hazard routing](docs/index.md), then only the selected
  [hazard playbook](docs/hazards.md).
- Source lever: [docs/levers.md](docs/levers.md); C naming and style:
  [docs/coding-style.md](docs/coding-style.md).
- Delegation: [docs/workflow/fan-out.md](docs/workflow/fan-out.md).
- Prompt authoring: [docs/prompt-style.md](docs/prompt-style.md).

