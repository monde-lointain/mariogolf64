---
type: Reference
title: "KMC GCC expressions and instruction selection"
description: "Signed division, constant multiplication, commutative operand order, and packed or overlapping values under KMC GCC 2.7.2."
tags: [kmc-gcc, instruction-selection]
---

# Signed division by powers of two

Write signed division naturally (`x / 2`, `x / 0x2000`). GCC emits the add-bias-and-shift sequence needed for negative values (shift right to extract the sign bit, add it, arithmetic shift). Hand-writing that pattern usually changes scheduling or register allocation, and shifts written for division are an m2c artefact.

# Constant multiplication

GCC commonly replaces multiplication by a visible constant with shifts and additions. If the target uses `mult`, the multiplier must stay nonconstant until the multiplication pass. Use this only when the diff requires it; anything added to inhibit propagation can affect surrounding code.

# Expression-tree operand order

Equivalent address expressions can reverse the operands of a commutative `addu`. Typed array indexing, field addressing and explicit scaled-address expressions may therefore differ in the final instruction text. Prefer typed access, but keep a different expression shape when exact operand order is the only mismatch.

# Packed and overlapping values

When assembly accesses the same storage at several widths, model it with a union or small typed view. The target is big-endian: the low byte of a halfword or word is at the highest address within the value. Preserve halfword or word assignments when the target uses `sh` or `sw`; assigning only a named byte changes codegen.

Do not split a packed word in a global struct merely to name its bytes. Even with unchanged field offsets, altered declarations can change alignment and data layout; a union view preserves the original storage declaration.
