---
type: Reference
title: "Control-flow shape"
description: "How equivalent C lays out blocks, calls, loops and conditional assignments under KMC GCC 2.7.2; look here when blocks, delay slots or branch-likely choice differ."
tags: [kmc-gcc, control-flow]
---

# Blocks or branches are laid out in a different order

**Cause:** in a nested `if`/`else` the outer condition is tested first, and the inner alternative is laid out before the outer alternative. Inverting a condition changes fall-through and can decide between a normal and a branch-likely instruction.

**Try:**
* Invert the condition or swap the arms to match the target's fall-through.
* For a few sparse constant cases, use a `switch`. A `default` placed first serves as the fall-through body while the cases keep source order.

# A call is duplicated or shared differently from the target

**Cause:** merging two branches into one call with a selected argument is not codegen-neutral. If the target repeats the call setup in each branch, the calls stay in the branches. If the target shares the setup, one call matches.

**Try:**
* Compare the call setup in the target. For a shared setup, compute the differing value first and make one call.
* A `case` label inside an `else` arm: in one probe the switch jumped into the arm, and the call was emitted once and shared by the other path.

# Loop entry test, back edge or delay slot differs

**Cause:** `for`, `while` and guarded post-test loops differ in entry test, back edge and delay slot even when they are equivalent. Moving an increment between the loop header and the body changes scheduling. A `continue` targets the compiler's continuation point, and a `goto` targets a source label.

**Try:**
* Keep an explicit zero-trip guard when the target shows one.
* Choose the loop form the target shows, and move the increment only when the target does.
* A `goto` to a label before the increment did not change branch selection in one probe: a `for` with `continue` and a `while` with that `goto` compiled to the same branch-likely (`beql`) with the increment in the delay slot. A `goto` in m2c output is usually an artefact ([decompilation artefacts](/reference/decompilation-artefacts.md)).

# A conditional assignment is branchless, or keeps a branch

**Cause:** a local scalar assigned and then conditionally overwritten can become branchless arithmetic (`slti` + `addu`). GCC 2.7.2 sometimes expects a variable written before a branch even when a later assignment overwrites it.

**Try:**
* Direct stores in both arms of an `if`/`else` are more likely to keep a branch when the target has one.
* An ordinary ternary or `if`/`else` often yields the branchless form the target has.
* When the target stores before the branch, initialise the variable before the conditional (for example `result = 0`).
