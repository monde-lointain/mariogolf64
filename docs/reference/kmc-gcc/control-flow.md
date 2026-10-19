---
type: Reference
title: "KMC GCC control-flow shape"
description: "How equivalent C constructs lay out blocks, calls, loops and conditional assignments differently under KMC GCC 2.7.2."
tags: [kmc-gcc, control-flow]
---

# Branch and block order

Equivalent C constructs can produce different block layouts. In nested `if`/`else`, the outer condition is tested first and the inner alternative is generally laid out before the outer alternative. Inverting a condition changes fall-through behavior and can decide whether GCC emits a normal or a branch-likely instruction.

For a few sparse constant cases a `switch` gives more control over comparison and block order than nested conditionals. A `default` placed first can serve as the fall-through body while cases stay in source order.

# Shared versus duplicated calls

Merging two branches into one call with a selected argument is not codegen-neutral. If the target repeats call setup in each branch, keep the calls in the branches. If the target shares call setup, compute the differing value first and use one call. A `case` label may sit inside an `else` arm: the switch jumps into the arm, and a call there is emitted once and shared by the other path rather than duplicated (in one probe).

# Loop form

`for`, `while` and guarded post-test loops can differ in entry test, back edge and delay slot even when semantically equivalent. Preserve explicit zero-trip guards when they are visible in the target. Moving an increment between loop header and body can change scheduling.

A `continue` targets the compiler-generated continuation point, while a `goto` targets a source label. In one probe, a `for` loop with `continue` and a `while` loop with a `goto` to a label before the increment compiled to the same branch-likely (`beql`) with the increment in the delay slot, so the goto form did not change branch selection. (A `goto` in m2c output is usually an artefact; see [artefacts](/reference/decompilation-artefacts.md).)

# Simple conditional assignments

GCC may turn a local scalar assignment followed by a conditional overwrite into branchless arithmetic (for example `slti` + `addu`). Direct stores in both arms of an `if`/`else` are more likely to preserve a branch when the target has one; an ordinary ternary or `if`/`else` often yields the branchless form the target has.
