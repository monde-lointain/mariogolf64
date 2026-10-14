---
type: Workflow
title: Candidate selection and review
description: Provides lightweight candidate selection and completion guidance for decompilation work.
---

# Candidate selection and review

The [native decomp skill](../../.omp/skills/mg64-decomp/SKILL.md) owns the end-to-end workflow.
This page keeps the selection and review checkpoints; it does not impose sprint approvals, story
points, or ceremonial process gates.

## Select work

1. Read `BACKLOG.md` for carry-overs and context, then rank candidates with
   `python3 tools/pick_target.py -n 12`. Use `--lib`, `--segment`, or `--loose-stubs` for the relevant
   candidate set. `--lib` is not a segment selector.
2. Check hand-selected candidates with `--carried-check` and `--nested-check`. Read any named WIP
   evidence before pricing a candidate: a scoping note is not necessarily a wall.
3. Read the extracted `.s` and source-side comments. Ranker output is a ceiling on tractable work,
   not proof a candidate is fresh or easy. Size candidates from their `.s` headers, not adjacent
   VRAM deltas in multi-function packs.
4. Treat compiler jump-table leaves as coupled source-and-rodata work. The generated table symbol
   exists only when the C switch regenerates it, so do not carve it while the function remains
   `INCLUDE_ASM`.

Candidate judgment remains empirical: a `.s` tell or prior compiler-source diagnosis is a hypothesis
until the candidate is built and its residual measured. Re-derive the signature, dataflow, declarations,
instruction count, and residual class from current source and object evidence before carrying an old
verdict forward.

## Review completion

- A banked function has a successful `tools/verify-rom.sh` result. A score, byte comparison, or
  isolated compile is not a substitute for the ROM SHA-1 gate.
- A file increment is complete only when all of its `INCLUDE_ASM` stubs are removed. Empty extracted
  leaves may already be C: count functions as well as remaining stubs.
- Record unresolved measured residuals in a self-contained `docs/wip/<fn>.near-match.md`; retain
  useful carry-over evidence in `BACKLOG.md`. Do not label an unverified candidate banked.
- On resume, reconcile the current work state with commits and the tracked backlog before continuing.
