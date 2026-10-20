---
type: Reference
title: "Registers, scheduling and stack frames"
description: "How lifetimes, declaration and statement order, and locals drive register allocation, delay slots and frame size; look here when only registers, spills or frame size differ."
tags: [kmc-gcc, registers, scheduling]
---

# Variable lifetime and scope

Block-scoped temporaries have shorter live ranges and are less likely to take callee-saved registers. Keep a temporary close to its use when it need not survive calls. Hoisting or caching a subexpression deliberately raises register pressure and shifts allocation.

Declaration order and expression decomposition affect allocation: an otherwise unnecessary temporary can change which values are reloaded, retained, or placed in argument and temporary registers. Use such changes only when they leave readable, meaningful C.

A load from a `const`-qualified global is treated as loop-invariant: GCC hoists it out of a loop that stores to memory and emits it before the loop in order of first use. A load from a non-`const` global that the loop also writes stays inside the loop and is reloaded each iteration. Declaring the global `const` is the way to get the hoisted form; a local copy made before the loop gave a different load order in one probe.

# Statement order

The scheduler fills load-use and branch-delay slots with independent instructions. Source statement order decides which instructions are available in a scheduling region, so splitting a compound expression or moving a pointer load can change emitted order without changing behavior.

# Stack frames

Declaration order decides stack offsets: swapping the declaration order of two local arrays swaps their offsets. Alignment can make a small local change alter the frame by more than that local's size: adding a 1-byte local to a frame holding one 8-byte array grew the frame by 8 bytes. Unused local arrays and aggregates still reserve frame space; unused scalar locals do not. Block scope does not share slots: two arrays in sibling blocks take the same frame as two function-scope arrays, so block scope does not control frame size (it can still avoid a callee-saved register save). The frame size comes from the compiler's `.frame` and `subu $sp` output, not from the assembler. If the target needs stack space the source never seems to use, look for the real object (a local whose address escapes, a struct or array) rather than padding; do not add `volatile` to retain space. One real cause with no local behind it: a GCC nested function stores its incoming static chain (`$v0`) in its first frame slot on entry (`expand_function_start` in GCC's `function.c`, `[INFERENCE]`: not checked against this toolchain), so a leaf with a dead 8-byte frame and `sw $v0,0($sp)`, whose caller loads `$v0` with an address in its own frame before the `jal`, was written nested inside that caller. Until that parent is decompiled, write the nested function standalone with an uninitialized local standing in for the chain, usually `s32 r; volatile s32 unused = r;` (a chain-pointer variant reads the parent's locals through it), and a comment naming the parent. This is the only allowed uninitialized or `volatile` local, and it is a match, not a reason to give up.

When the frame size is the only remaining difference and no real object fits the space, declare an unused local array of the missing size where its offset lands, with the comment `/* unused: original frame space */`. Apart from the static-chain stand-in above, it is the only allowed unused local.

# Not available

Inline assembly and explicit register bindings are **not matching tools**; [code-quality](/project/code-quality.md) prohibits them. Only C types, control flow and the proper macros resolve differences ([code-quality](/project/code-quality.md), first paragraph), plus the two unused locals in [stack frames](#stack-frames). `volatile` or `register` hints added only to change codegen are decompiler artefacts to remove ([matching method](/procedures/matching-method.md), phase 2); keep a `volatile` the original source needs ([types and layouts](/reference/kmc-gcc/types-and-layouts.md)). When a register or scheduling difference seems to need one, the usual cause is a wrong type, lifetime, alias or expression shape; see [matching method](/procedures/matching-method.md) phases 2 and 3. A `do { } while (0)` wrapper can also influence allocation and scheduling without changing semantics; keep it as a last resort.
