---
type: Prompt
title: Crack-subagent prompt template
description: Provides the required isolated-work contract and deliverables for a dispatched function-cracking agent.
---

# Crack-subagent prompt template

<role>
Fill in this contract for each dispatched crack agent; do not paraphrase it.
`{{FN}}`: function. `{{CLASS}}`: prior divergence hypothesis. `{{PROFILE}}`: source-specific compiler
profile from `tools/build_profiles.json`. `{{LEVERS}}`: family-specific [source levers](levers.md).
Use the [fan-out trigger table](workflow/fan-out.md#when-to-fan-out) to decide dispatch and sizing.
</role>

---

You are cracking one function in a Mario Golf 64 (N64) decompilation: `{{FN}}`.

Measure matches; the ROM SHA-1 against `baserom.z64` is the sole arbiter. Work in isolation.

**Toolchain profile.** Use the configured build toolchain and the same source profile as the ROM build.
The available KMC compiler and binutils are under `<binary>/toolchain`; do not substitute a host
compiler. Root-cause residuals to a diverging compiler pass with a `file:line` citation when source
evidence is available.

**Step 0, before trusting any prior document.** Run
`<binary>/venv/bin/python3 tools/pick_target.py --refresh-residual {{FN}}`. A carry doc's instruction
count, residual class, and semantics are hypotheses; re-derive from a fresh object. A register written
in a branch delay slot before its first read is a shared pre-branch statement, not an argument.

Documented class, to be confirmed or refuted rather than assumed: `{{CLASS}}`.

**Isolation.** Iterate with
`<binary>/venv/bin/python3 tools/decomp_loop.py --func {{FN}} --profile {{PROFILE}}`. This compiles
the function in its isolated candidate directory using the selected profile. Read the generated
current object with `<binary>/toolchain/bin/mips-linux-gnu-objdump -dz`; do not use a linked-build diff
as a fresh-object oracle.

**Levers to try.** `{{LEVERS}}`

**Boundaries.** Never use the shared build tree or Ghidra MCP concurrently with another worker. Never
edit anything under `src/` or another function's `nonmatchings/` directory.

**`nonmatchings/{{FN}}/STATUS` plus a best-so-far `base.c` is the hand-off contract.** Append one line
per material iteration (`38/38 count, residual=uniform reg rotation, form=u32 b=src[i]`), keep
`base.c` at its best form and never at a disproven probe, and end `STATUS` with
`RESULT: <mine>/<rom> | frame <ok|delta> | <residual> | <form> | collisions: <list|none>`, nulls
below it. Do not leave a background permuter running.

**Deliverable, either of these two.** A byte-exact C body plus the struct and extern additions it
needs. Or a terminal verdict with a `file:line` citation into the compiler source, written to
`docs/wip/{{FN}}.near-match.md`. A cited verdict preserves the evidence for future work.

**What isolation hides; report both.** Your build never sees the host file and never links, so two
defects surface only at the gate.
- *Extern collisions.* `grep -n '<sym>' <host .c>` for every `extern` you need and list those that
  exist, with their spelling: the host may declare one at a different type (S295 `D_800E2158`,
  scalar `void*` in the host and indexed in the new body, correct on both sides).
- *A false relocation.* A delta is one only if `objdump -r` says so; `lui $at` carries `%hi` and
  float-constant halves alike (S299).
- *A duplicate literal pool.* Run `mips-linux-gnu-objdump -s -j .rodata` on your object. Any
  emitted `.rodata` reddens the gate build behind a byte-exact instruction stream; fix it per
  `docs/hazards.md#duplicate-literal-pool-shared-rodata` and report what you referenced.

**Standing policy.** An empty `__asm__ __volatile__("")` barrier may characterize a wall; it is
never a bank candidate. A body of raw command words is a result to challenge: decode each word
against the SDK header and re-verify (S297).
