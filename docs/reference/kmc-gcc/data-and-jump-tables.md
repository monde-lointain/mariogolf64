---
type: Reference
title: "Data, jump tables and section placement"
description: "Jump-table merging, symbol boundaries, bss and zero-initialized global placement, and section padding; look here for data or section-layout mismatches."
tags: [kmc-gcc, data, bss, jump-tables]
---

# Jump-table entries merge or point past the switch

**Cause:** consecutive switch cases with identical bodies can merge into one target. An empty case can point directly after the switch. KMC GCC emits a function's jump tables and literals into `.rodata` itself.

**Try:** inspect the jump-table data to tell the two shapes apart, and match the one the target shows.

# A table or buffer looks like part of a neighbour

**Cause:** a constant subtracted from an index folds into the displacement of the table's own symbol. `table[id - 0x80]` on an `s32` table compiled to a shift of `id` and a load with a negative displacement from `table`, so a disassembler can show the table relative to a neighbouring symbol.

**Try:** keep the real table symbol rather than a neighbour's.

# Bss or data boundaries differ from the target

**Cause:** symbol declarations and object boundaries affect section layout even when the bytes look equivalent. Several apparent globals may be one aggregate, and one apparent buffer may need explicit alignment or padding.

**Try:**
* Preserve the owning symbol and storage width unless there is strong evidence for a structural change.
* For bss mismatches, inspect adjacent symbols and their alignment. Procedure: [match-bss-segment](/procedures/match-bss-segment.md).

# A global lands in the wrong section

**Cause:**
* A global explicitly initialised to `= 0` goes to `.data`.
* An uninitialised global becomes a `.comm` symbol, which the linker may reorder. The section attribute is ignored.
* `.bss` in source order needs `__attribute__((section(".bss")))` **and** an initializer.
* Arrays of 8 bytes or more are 8-byte aligned.

**Try:** choose the declaration form that gives the target's section, as described above.

# Section padding differs at a translation-unit boundary

**Cause and fix:** object padding and translation-unit boundaries are covered in [assembly and object layout](/project/compiler-profiles.md#assembly-and-object-layout).
