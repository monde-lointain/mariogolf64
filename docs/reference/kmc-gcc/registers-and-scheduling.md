---
type: Reference
title: "KMC GCC registers, scheduling and stack frames"
description: "How variable lifetime, declaration order, statement order and locals affect register allocation, delay-slot filling and frame size under KMC GCC 2.7.2."
tags: [kmc-gcc, registers, scheduling]
---

# Variable lifetime and scope

Block-scoped temporaries have shorter live ranges and are less likely to take callee-saved registers. Keep a temporary close to its use when it need not survive calls. Hoisting or caching a subexpression deliberately raises register pressure and shifts allocation.

Declaration order and expression decomposition affect allocation: an otherwise unnecessary temporary can change which values are reloaded, retained, or placed in argument and temporary registers. Use such changes only when they leave readable, meaningful C.

# Statement order

The scheduler fills load-use and branch-delay slots with independent instructions. Source statement order decides which instructions are available in a scheduling region, so splitting a compound expression or moving a pointer load can change emitted order without changing behavior.

# Stack frames

Declaration order affects stack offsets, and alignment can make a small local change alter the frame by more than that local's size. The assembler may remove unused stack locals. If the target needs stack space the source never seems to use, look for the real object (a local whose address escapes, a struct or array) rather than padding; do not add `volatile` to retain space.

# Not available

These are **prohibited** by the [code-quality standards](/project/code-quality.md), so they are not matching tools: fixed-register locals (`register T x asm("$s0")`), empty or narrow inline-assembly barriers, assembly constraints, and `volatile`/`register` hints added only for codegen. When a register or scheduling difference seems to need one, the usual cause is a wrong type, lifetime, alias or expression shape; see [matching method](/procedures/matching-method.md) phases 2 and 3. A `do { } while (0)` wrapper can also influence allocation and scheduling without changing semantics; keep it as a last resort.
