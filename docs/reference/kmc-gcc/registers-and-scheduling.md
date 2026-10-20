---
type: Reference
title: "Registers, scheduling and stack frames"
description: "How lifetimes, declaration and statement order, and locals drive register allocation, delay slots and frame size; look here when only registers, spills or frame size differ."
tags: [kmc-gcc, registers, scheduling]
---

# Registers differ: lifetime and declaration

**Cause:** block-scoped temporaries have shorter live ranges and are less likely to take callee-saved registers. Hoisting or caching a subexpression raises register pressure. Declaration order and expression decomposition change which values are reloaded or retained and which argument or temporary registers they use.

**Try:**
* Keep a temporary close to its use when it need not survive calls.
* Use an extra temporary or a different decomposition only when the result is still readable C with a real meaning.

# A load from a global is hoisted or reloaded in a loop

**Cause:** GCC treats a load from a `const`-qualified global as loop-invariant. It hoists the load out of a loop that stores to memory and emits it before the loop, in order of first use. A load from a non-`const` global that the loop also writes stays inside the loop and is reloaded on each iteration. In one probe, a local copy made before the loop gave a different load order.

**Try:** declare the global `const` to get the hoisted form. For a shared declaration, see [types and layouts](/reference/kmc-gcc/types-and-layouts.md).

# Instructions come out in a different order

**Cause:** the scheduler fills load-use and branch-delay slots with independent instructions. Source statement order decides which instructions are available in a scheduling region. Splitting a compound expression or moving a pointer load changes the emitted order without changing behavior.

**Try:**
* Change the statement order, or split a compound expression, to change which instructions share a region.
* A `do { } while (0)` wrapper can also change allocation and scheduling without changing semantics. Use it only as a last resort.

# A register difference seems to need a hint

**Cause:** the usual cause is a wrong type, lifetime, alias or expression shape. Hints are prohibited ([code-quality, prohibited](/project/code-quality.md#prohibited)).

**Try:** diagnose with [matching method](/procedures/matching-method.md) phases 2 and 3.

# Stack frames

**Cause:**
* Declaration order sets the offsets: swapping the declarations of two local arrays swaps their offsets.
* Alignment can change the frame by more than the size of the local that changed it.
* Unused local arrays and aggregates still reserve frame space. Unused scalar locals do not.
* Block scope does not share slots. Two arrays in sibling blocks take the same frame as two function-scope arrays. Block scope can still avoid a callee-saved register save, but it does not control frame size.
* The frame size comes from the compiler's `.frame` and `subu $sp` output, not from the assembler.

**Try:**
* If the target reserves stack the source never seems to use, find the real object (a local whose address escapes, a struct, or an array) instead of padding.
* If the frame is the only difference and no real object fits, declare an unused local array of the missing size where its offset lands, with the comment `/* unused: original frame space */`. The rule for unused locals is in [code-quality, prohibited](/project/code-quality.md#prohibited).

## A nested function's frame starts with a static-chain store

**Cause:** a GCC nested function stores its incoming static chain (`$v0`) in its first frame slot on entry (`expand_function_start` in GCC's `function.c`, `[INFERENCE]`: not checked against this toolchain). The symptom is a leaf with a dead 8-byte frame and `sw $v0,0($sp)`, whose caller loads `$v0` with an address in its own frame before the `jal`.

**Try:** until the parent is decompiled, write the nested function standalone with an uninitialised local standing in for the chain, usually `s32 r; volatile s32 unused = r;`. A chain-pointer variant reads the parent's locals through it. Add a comment naming the parent. The rule for this local is in [code-quality, prohibited](/project/code-quality.md#prohibited).
