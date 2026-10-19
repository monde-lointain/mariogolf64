---
type: Reference
title: "Data, jump tables and section placement"
description: "Switch jump-table merging, symbol boundaries, bss and zero-initialized global placement, and section padding under KMC GCC 2.7.2 and KMC as."
tags: [data, bss, jump-tables, kmc-gcc]
---

# Switch tables

Consecutive switch cases with identical bodies may be merged into one target. An empty case can instead point directly after the switch. Inspect the jump-table data to tell them apart. KMC GCC emits a function's jump tables and literals into `.rodata` itself.

# Symbols and boundaries

Symbol declarations and object boundaries can affect section layout even when bytes look equivalent. Preserve the owning symbol and storage width unless there is strong evidence for a structural change. For bss mismatches, inspect adjacent symbols and alignment: several apparent globals may be one aggregate, and one apparent buffer may need explicit alignment or padding.

# Where globals land

* An explicitly zero-initialized global (`= 0`) goes to `.data`.
* An uninitialized global becomes a `.comm` symbol, which the linker may reorder; the section attribute is ignored.
* `.bss` in source order requires `__attribute__((section(".bss")))` **plus** an initializer. Arrays of 8 bytes or more are 8-byte aligned. Procedure: [match-bss-segment](/procedures/match-bss-segment.md).

# Section padding

KMC `as` pads each object's `.text` to 16 bytes, so keep translation-unit boundaries the same as the original objects when splitting or merging files. A fast-math file applies `-ffast-math` to every function in it ([compiler profiles](/project/compiler-profiles.md)).
