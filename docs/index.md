---
okf_version: "0.2"
---

# Start and operate

- [Local build and verification](build.md) — Sets up the local toolchain, extracts the supported ROM, and distinguishes matching builds from WIP artifacts.
- [Decomp tooling entrypoints](tooling.md) — Selects the existing command for candidate discovery, matching iteration, and documentation maintenance.
- [Mario Golf 64 decomp workflow](agent-workflow.md) — Routes to the native decomp skill and on-demand technical references.
- [Workflow phases](workflow/index.md) — Links to candidate selection, execution details, and fan-out guidance.

# Diagnose and match

- [Hazard flag lookup](hazard-index.md) — Routes ranker flags and assembly symptoms to the applicable hazard playbook.
- [Matching residual lookup](hazard-symptoms.md) — Routes observed instruction, register, scheduling, and data-layout residuals to matching playbooks.
- [Hazard playbooks](hazards.md) — Provides symptom-specific recovery and matching procedures with stable section anchors.
- [Source-level code-generation levers](levers.md) — Evidence-backed source transformations and compiler behaviors for steering Mario Golf 64's generated MIPS toward the ROM.

# Author and maintain

- [C coding and matching style](coding-style.md) — Practical conventions for hand-authored C that maintain readability without overriding byte-exact decompilation requirements.
- [Agent prompt style](prompt-style.md) — Authoring rules for concise, checkable project instructions.
- [Crack-subagent prompt template](fanout-prompt.md) — Provides the isolated-work contract and deliverables for delegated function-cracking work.
- [Documentation structure and maintenance](documentation.md) — Defines the OKF bundle boundary, progressive reading routes, metadata, and documentation validation rules.

# Evidence and updates

- [Sprint history and reconstruction evidence](history.md) — Distinguishes current workflow authority from time-scoped sprint records and function reconstruction notes.
- [Function reconstruction evidence](wip/index.md) — Select an active note or the banked-evidence archive.
- [Documentation changes](log.md) — Bundle updates, newest first.
