# Selecting what applies

Pick the concepts that match the mismatch you see in the diff; each states a generic, durable behavior of KMC GCC 2.7.2 and what to try.

* [Control-flow shape](control-flow.md) - How equivalent C lays out blocks, calls, loops and conditional assignments under KMC GCC 2.7.2; look here when blocks, delay slots or branch-likely choice differ.
* [Expressions and instruction selection](expressions-and-instruction-selection.md) - Signed division, constant multiplication, commutative operand order, and packed or overlapping values under KMC GCC 2.7.2; look here when single instructions differ.
* [Registers, scheduling and stack frames](registers-and-scheduling.md) - How lifetimes, declaration and statement order, and locals drive register allocation, delay slots and frame size; look here when only registers, spills or frame size differ.
* [Types and layouts](types-and-layouts.md) - Reusing canonical types, inferring layouts from all consumers, and the blast radius of shared-struct changes; look here when offsets or access widths differ.
* [Data, jump tables and section placement](data-and-jump-tables.md) - Jump-table merging, symbol boundaries, bss and zero-initialized global placement, and section padding; look here for data or section-layout mismatches.

# Adding to this reference

* [Editing rules for the KMC GCC reference](editing-rules.md) - What may be added to the KMC GCC learnings: generic reusable compiler and build properties only; per-function findings go to the workspace NEAR_MISS.md.

# Related

* Per-directory and per-file flags: [compiler profiles](/project/compiler-profiles.md)
* How to work through a diff: [matching method](/procedures/matching-method.md)
* What may never be used to force a match: [code-quality standards](/project/code-quality.md)
* m2c cleanup patterns: [decompilation artefacts](/reference/decompilation-artefacts.md)
