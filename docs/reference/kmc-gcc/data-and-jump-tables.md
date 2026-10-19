---
type: Reference
title: "Data, jump tables and section placement"
description: "Jump-table merging, symbol boundaries, bss and zero-initialized global placement, and section padding; look here for data or section-layout mismatches."
tags: [data, bss, jump-tables, kmc-gcc]
---

# Switch tables

Consecutive switch cases with identical bodies may be merged into one target. An empty case can instead point directly after the switch. Inspect the jump-table data to tell them apart. KMC GCC emits a function's jump tables and literals into `.rodata` itself.

# Symbols and boundaries

Symbol declarations and object boundaries can affect section layout even when bytes look equivalent. Preserve the owning symbol and storage width unless there is strong evidence for a structural change. For bss mismatches, inspect adjacent symbols and alignment: several apparent globals may be one aggregate, and one apparent buffer may need explicit alignment or padding. A constant subtracted from an index folds into the displacement of the table's own symbol: `table[id - 0x80]` on an `s32` table compiled to a shift of `id` and a load with a negative displacement from `table`, so a disassembler can show the table relative to a neighbouring symbol. Keep the real table symbol.

# Where globals land

* An explicitly zero-initialized global (`= 0`) goes to `.data`.
* An uninitialized global becomes a `.comm` symbol, which the linker may reorder; the section attribute is ignored.
* `.bss` in source order requires `__attribute__((section(".bss")))` **plus** an initializer. Arrays of 8 bytes or more are 8-byte aligned. Procedure: [match-bss-segment](/procedures/match-bss-segment.md).

# Section padding

Object padding and translation-unit boundaries are covered in [assembly and object layout](/project/compiler-profiles.md#assembly-and-object-layout).
