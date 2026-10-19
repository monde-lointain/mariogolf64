---
type: Reference
title: "Expressions and instruction selection"
description: "Signed division, constant multiplication, commutative operand order, and packed or overlapping values under KMC GCC 2.7.2; look here when single instructions differ."
tags: [kmc-gcc, instruction-selection]
---

# Signed division by powers of two

Write signed division naturally (`x / 2`, `x / 0x2000`). GCC emits the add-bias-and-shift sequence needed for negative values (shift right to extract the sign bit, add it, arithmetic shift). Hand-writing that pattern usually changes scheduling or register allocation, and shifts written for division are an m2c artefact.

# Constant multiplication

GCC commonly replaces multiplication by a visible constant with shifts and additions. If the target uses `mult`, the multiplier must stay nonconstant until the multiplication pass. Use this only when the diff requires it; anything added to inhibit propagation can affect surrounding code. In one probe, a literal multiplier was synthesized into shifts and subtracts, while the same value held in a local computed from constants compiled to `li` plus `mult` with no added code; check which form the target uses before hiding a constant.

# Expression-tree operand order

Equivalent address expressions can reverse the operands of a commutative `addu`. Typed array indexing, field addressing and explicit scaled-address expressions may therefore differ in the final instruction text. Prefer typed access, but keep a different expression shape when exact operand order is the only mismatch. A `static inline` helper and a macro with the same text can also schedule differently: in one probe, small and loop-containing `static inline` helpers were inlined at -O2 with no call, yet their instruction order differed from the same expression written as a macro.

# Packed and overlapping values

When assembly accesses the same storage at several widths, model it with a union or small typed view. The target is big-endian: the low byte of a halfword or word is at the highest address within the value. Preserve halfword or word assignments when the target uses `sh` or `sw`; assigning only a named byte changes codegen. A signed `>> 16` of a word read through a pointer compiles to one halfword load of the high half (`lh` at the same address), so the access can stay on the word type.

Do not split a packed word in a global struct merely to name its bytes. Even with unchanged field offsets, altered declarations can change alignment and data layout; a union view preserves the original storage declaration.
