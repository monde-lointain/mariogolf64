---
type: Guide
title: "Matching method"
description: "How to approach a function that will not match: reconstruct plausible source first, escape plateaus with a clean rewrite, classify near misses, handle large functions and data layout."
tags: [decompile, strategy]
---

Used from inside a [decomp environment](/tools/decomp-env.md) after `base.c` compiles. The attempt loop and stop rules live in [decompile-function](/procedures/decompile-function.md); compiler behavior in [KMC GCC learnings](/reference/kmc-gcc/index.md); cleanup of m2c output in [decompilation artefacts](/reference/decompilation-artefacts.md).

# General approach

* Think about what the function does in the game and structure the code to fulfill that purpose. Callers and callees are evidence.
* Control flow and data modeling first. Once opcodes and branches align, treat stack and register differences as evidence about source types, local lifetimes, aliases and expression structure, not as noise.
* Make one small source-shape change at a time; several at once interact.
* A relocation whose name differs but whose resolved VRAM is equal is not an instruction difference ([decomp-env](/tools/decomp-env.md)). Do not distort the C to chase names. If the names still differ after that, the layout in `symbol_addrs.txt` is wrong; fix the layout.

# Phase 1: reconstruct the simplest plausible source

Before any compiler-shaping trick:

* Resolve every referenced address to its enclosing typed object, including fields inside arrays or larger symbols. Compare relocations and byte offsets against known structs and inspect neighboring users rather than trusting the nearest symbol name.
* Model repeated fixed-stride data with typed arrays or records. When a callee receives pointers to adjacent stack words, consider whether the source used one array or struct in the observed address order.
* Inspect structurally paired functions (In/Out, Open/Close, Init/Update) for shared types and a shared control-flow skeleton.
* Prefer natural `for` loops, `switch` statements, direct field access and the fewest live temporaries.

# Phase 2: escape plateaus with a clean rewrite

If several consecutive attempts give the same score or mismatch region, keep the best attempt and start an independent one from the function's behavior.

Remove decompiler-created pointers, copied loop bounds, cached selectors, redundant casts and masks, `volatile` or `register` hints, fake unions, identity arithmetic and manual unrolling. Re-establish the candidate in canonical typed C before more register tuning. A high score is not evidence that coercion artifacts are plausible original source.

# Phase 3: classify very close matches (99% or better)

Classify every normalized difference before editing:

* control flow
* instruction selection
* memory offset or type
* stack layout
* register allocation
* relocation-label-only noise (ignore)

A stack-frame mismatch points to extra locals, spills or wrong lifetimes. Repeated target loads favor direct global or field access; a value held in one register may favor a local. If only registers differ, freeze the matched algorithm and map each target register to its logical value and live range. First remove redundant aliases and temporaries, use original parameters or destination fields directly, and inline copied constants or bounds. Test one localized equivalent expression at a time.

# Large functions

1. Get it to compile first: fill in missing types and undefined functions.
2. Focus on control flow. m2c's control flow can be convoluted and misleading; make a checklist of differences and work through it.
3. Large structs are easy: there are often big gaps between fields, so concentrate on getting the accesses right.

# Display lists

Functions that write 8-byte command pairs through a `Gfx *` cursor build F3DEX2 display lists. Use [decompile-microcode](/procedures/decompile-microcode.md) and the `gbi.h` macros.

# Data layout

The symbols (data, rodata, bss) a function uses may not reflect the true layout. Many contiguous symbols accessed sequentially are probably one array; data that otherwise looks like it rolls up into a common struct probably is one. Update `symbol_addrs.txt` to reflect the true layout, even if that means a wider refactor ([format](/reference/symbol-addrs-format.md)).
