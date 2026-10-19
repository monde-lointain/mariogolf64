---
type: Policy
title: "Code-quality standards"
description: "What counts as an acceptable match: no inline asm or raw display-list words, typed structs not pointer math, no redundant externs, file structure, validation checklist."
tags: [policy, quality]
---

A matching checksum does not make an implementation complete if it violates these rules. Resolve differences through C types, control flow and the proper macros. Checkers: [quality checkers](/tools/quality-checkers.md).

# Prohibited

* Inline assembly: `asm`/`__asm__`, empty barriers, constraints, register bindings. `INCLUDE_ASM`, `INCLUDE_RODATA` and `GLOBAL_ASM` are not allowed in candidate files; the decomp environment `build.sh` rejects them.
* Raw display-list words. Use the F3DEX2 macros from `PR/gbi.h` (look for `gDPLoadTLUT_pal16`, `gDPLoadTextureTile_4b`). Never advance a `Gfx *` by hand; see [decompile-microcode](/procedures/decompile-microcode.md).
* `#define` abuse.
* `void *` where the real type is knowable.
* Manual pointer arithmetic.

# Typed access instead of manual offsets

1. Identify the access: which field or element is read or written, and at what stride.
2. Define the element struct and the container struct that hold it.
3. Check `sizeof` against the stride the assembly uses.

Raw pointer arithmetic is acceptable only as a narrowly justified codegen expression after the underlying storage has a correct type. `tools/check_pointer_arithmetic.py` reports these casts.

# Changing structs

* Search for every other user of the struct before changing it.
* Reuse an existing type where one fits; see [types and layouts](/reference/kmc-gcc/types-and-layouts.md).
* Verify every affected function, then the full ROM.

# Redundant declarations

After adding or moving a function, check that every `extern` you used is not already declared in an included header (`rg "void functionName" include/ src/`). Remove duplicates and rebuild. `tools/fix_redundant_externs.py [--fix]` finds and removes source-local externs that repeat a project header's prototype. Function prototypes in headers belong to the header owned by the defining C file and do not use `extern`; `extern` data declarations are fine.

# File structure

C files are ordered: macro definitions, struct definitions, global variables, function declarations, function implementations. Reorganize toward this proactively. Keep every `INCLUDE_ASM(...)` on one line; [Splat reads the function name from it](/reference/splat-yaml.md).

# Validation checklist

Before declaring C changes complete:

- [ ] No pointer arithmetic with manual offsets.
- [ ] Field accesses use `->` or `.`.
- [ ] No `void *` parameters that should be typed.
- [ ] Struct sizes match the strides and offsets in the assembly.
- [ ] Every `INCLUDE_ASM(...)` is on a single line.
- [ ] No redundant externs.
- [ ] Every affected function is diffed after a fresh build: `python3 tools/asm-differ/diff.py --no-pager <function>`.
- [ ] The full ROM verifies; see [ground rules](/project/ground-rules.md) §1.
