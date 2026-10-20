---
type: Reference
title: "Decompilation artefacts"
description: "Patterns m2c and permuters leave in C (do-while loops, gotos, duplicated temporaries, shifts, false returns); open when a decompiled C shape may be the cause of a mismatch."
tags: [m2c, cleanup, kmc-gcc]
---

Remove these when the target still matches: procedure in [cleanup-function](/procedures/cleanup-function.md), method in [matching method](/procedures/matching-method.md) phase 2.

# Artefacts

**`for` loops.** The compiler pulls the condition out so it can bail early if never met, and the comparison may change (`<` becomes `<=`). A loop

```c
for (i = 0; i < 10; i++) { /* stuff */ }
```

often decompiles as

```c
i = 0;
if (i < 10) {
    do { /* stuff */ i++; } while (i < 10);
}
```

**`goto`s.** Developers rarely write them; branches in assembly represent many kinds of control flow. Assume a `goto` is an artefact.

**Duplicated variables.** A single reused variable is far more likely than many temporaries. Literal decompilation and offline permuter output both create reassignments that hurt the match rate.

**Shifts instead of arithmetic.** For unsigned values, `x >> 2` is `x / 4` and `x << 2` is `x * 4`. For negative signed values, `>>` rounds toward negative infinity and `/` rounds toward zero, so write `/` and let GCC emit the bias ([expressions and instruction selection](/reference/kmc-gcc/expressions-and-instruction-selection.md)).

**False returns.** An explicit return in assembly may be a fall-through:

```c
if (condition) {
    /* main work */
} else {
    return X;      /* alternative path */
}
return Y;          /* fall-through from the if branch */
```
