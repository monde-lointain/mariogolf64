---
type: Reference
title: "Decompilation artefacts"
description: "Patterns m2c and permuters leave in C (do-while loops, gotos, duplicated temporaries, shifts, false returns) and the GCC 2.7.2 codegen patterns not to fight."
tags: [m2c, cleanup, kmc-gcc]
---

Literal decompilation leaves artefacts. Remove them when the target still matches ([cleanup-function](/procedures/cleanup-function.md), [matching method](/procedures/matching-method.md) phase 2).

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

# GCC 2.7.2 codegen to recognize, not fight

* **Signed division** by a power of two compiles to shift-and-bias; write `x / 4` ([details](/reference/kmc-gcc/expressions-and-instruction-selection.md)).
* **Branchless operations** (`slti` + `addu`) often come from an ordinary ternary or `if`/`else`.
* **Dead stores.** GCC 2.7.2 sometimes expects a variable written before a branch even if overwritten later; an initialization such as `result = 0` before a conditional can fix a mismatch.
* **Bare `sqrt.s`/`sqrt.d`.** Files built with `-ffast-math` inline `sqrtf()` without a NaN guard ([overrides](/project/compiler-profiles.md#overrides)). A guarded sqrt with a library fallback means the owning file is not fast-math.
