---
type: Reference
title: "Expressions and instruction selection"
description: "Signed division, constant multiplication, commutative operand order, packed or overlapping values and sqrt under KMC GCC 2.7.2; look here when single instructions differ."
tags: [kmc-gcc, instruction-selection]
---

# Signed division became a shift-and-bias sequence

**Cause:** GCC compiles signed division by a power of two into the sequence that adds a bias for negative values: a shift to extract the sign bit, an add, and an arithmetic shift. A hand-written copy of that pattern usually changes scheduling or register allocation. Shifts written in place of a division come from m2c ([decompilation artefacts](/reference/decompilation-artefacts.md)).

**Try:** write `x / 2` or `x / 0x2000` and let GCC emit the sequence.

# A constant multiplier became shifts and subtracts, or `mult` is missing

**Cause:** GCC replaces a multiplication by a visible constant with shifts and additions. If the target uses `mult`, the multiplier must stay nonconstant until the multiplication pass. In one probe a literal multiplier became shifts and subtracts, while the same value held in a local computed from constants compiled to `li` plus `mult` with no added code.

**Try:**
* Check which form the target uses.
* If the target has `mult`, hold the constant in a local computed from constants.
* Hide a constant only when the diff requires it. Anything added to stop propagation can change the surrounding code.

# Address operands are swapped

**Cause:** equivalent address expressions can reverse the operands of a commutative `addu`. Typed array indexing, field addressing and explicit scaled-address expressions can each differ in the final instruction text.

**Try:** prefer typed access. Keep a different expression shape only when operand order is the only mismatch.

# A `static inline` helper schedules differently from a macro

**Cause:** in one probe, small and loop-containing `static inline` helpers were inlined at `-O2` with no call, yet their instruction order differed from the same expression written as a macro.

**Try:** compare both forms against the target and keep the form whose order matches.

# Several widths of the same storage do not match

**Cause:** the target accesses the same storage at several widths. It is big-endian, so the low byte of a halfword or word is at its highest address. Assigning only a named byte changes codegen. A signed `>> 16` of a word read through a pointer compiles to one halfword load of the high half (`lh` at the same address).

**Try:**
* Model the storage with a union or a small typed view.
* Keep halfword and word assignments where the target uses `sh` or `sw`.
* Keep the word type for a signed `>> 16` read.
* Do not split a packed word in a global struct just to name its bytes. Altered declarations can change alignment and data layout even with unchanged offsets; a union view keeps the original declaration.

# A bare `sqrt` is inlined only under fast-math

**Cause:** files built with `-ffast-math` inline `sqrtf()` without a NaN guard, so bare `sqrt.s` and `sqrt.d` come from that override ([compiler profiles, overrides](/project/compiler-profiles.md#overrides)).

**Try:** a guarded sqrt with a library fallback means the owning file is not built fast-math. Check the override list before writing a bare `sqrt`.
