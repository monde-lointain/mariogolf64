# Selecting what applies

Pick the concepts that match the mismatch you see in the diff; each states a generic, durable behavior of KMC GCC 2.7.2 and what to try.

* [Control-flow shape](control-flow.md) - branch and block order, shared vs duplicated calls, loop forms, branchless conditionals; look here when blocks, delay slots or branch-likely choice differ
* [Expressions and instruction selection](expressions-and-instruction-selection.md) - signed division, constant multiplication, operand order, packed values; look here when single instructions differ
* [Registers and scheduling](registers-and-scheduling.md) - variable lifetime, statement order, stack frames; look here when only registers, spills or frame size differ
* [Types and layouts](types-and-layouts.md) - canonical types, inferring layouts, manual offsets, shared-type changes; look here when offsets or access widths differ
* [Data and jump tables](data-and-jump-tables.md) - switch tables, symbol boundaries, bss and zero-initialized globals; look here for data or section-layout mismatches

# Related

* [Editing rules](editing-rules.md) - what may be added here (generic, reusable behavior only)
* Per-directory flags: [compiler profiles](/project/compiler-profiles.md)
* How to work through a diff: [matching method](/procedures/matching-method.md)
* m2c cleanup patterns: [decompilation artefacts](/reference/decompilation-artefacts.md)
