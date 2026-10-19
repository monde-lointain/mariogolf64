---
type: Policy
title: "Code-quality standards"
description: "What counts as an acceptable match: no inline asm or raw display-list words, typed structs not pointer math, no redundant externs, file structure, validation checklist."
tags: [policy, quality]
---

A matching checksum does not make an implementation complete if it violates these rules. Resolve differences through C types, control flow and the proper macros. Checkers: [quality checkers](/tools/quality-checkers.md).

# Prohibited

* **Inline assembly of any kind**: no `asm`/`__asm__`, empty barriers, constraints, or explicit register bindings (`register T x asm("$s0")`). Also `INCLUDE_ASM`, `INCLUDE_RODATA` and `GLOBAL_ASM` in candidate files (the env's `build.sh` rejects them).
* **Raw display-list words or hand-encoded opcodes.** Use the F3DEX2 macros from `PR/gbi.h` and named constants; look for aggregate macros such as `gDPLoadTLUT_pal16` and `gDPLoadTextureTile_4b` first. Never advance the `Gfx *` cursor by hand. See [decompile-microcode](/procedures/decompile-microcode.md).
* **`#define` abused as a codegen or struct-access hack.** Use it for constants only.
* **`void *` where the real type is knowable**, and "make everything `void *`" fixes.
* **Manual pointer arithmetic** such as `*(type *)((u8 *)ptr + offset)` or `(type *)ptr + offset`.

# Typed access instead of manual offsets

1. Identify the access: the offset (`0xC` is field 12), whether it is an array element (`arg1 * 36` means a 36-byte struct), and which field inside the element (`+ 0xA` is offset 10).
2. Define the element struct with the right size and offsets, then the container with the pointer at the right offset. Use meaningful names, or `unk<Offset>` while the purpose is unknown.
3. Check `sizeof` against the stride in the assembly.

Raw pointer arithmetic is acceptable only as a narrowly justified codegen expression after the underlying storage has a correct type. `tools/check_pointer_arithmetic.py` reports these casts.

# Changing structs

* Search the whole codebase for other users of the struct and for accesses at nearby offsets.
* Reuse an existing type with the same size, offsets and ownership before defining a padded local view; see [types and layouts](/reference/kmc-gcc/types-and-layouts.md).
* Verify every affected function and the full ROM: a field insertion can shift later fields, change alignment, or change address formation elsewhere.

# Redundant declarations

After adding or moving a function, check that every `extern` you used is not already declared in an included header (`rg "void functionName" include/ src/`). Remove duplicates and rebuild. `tools/fix_redundant_externs.py [--fix]` finds and removes source-local externs that repeat a project header's prototype. Function prototypes in headers belong to the header owned by the defining C file and do not use `extern`; `extern` data declarations are fine.

# File structure

C files are ordered: macro definitions, struct definitions, global variables, function declarations, function implementations. Reorganize toward this proactively. Keep every `INCLUDE_ASM(...)` on one line; see [Splat reads the function name from it](/reference/splat-yaml.md).

# Validation checklist

Before declaring C changes complete:

- [ ] No pointer arithmetic with manual offsets
- [ ] Field accesses use `->` or `.`
- [ ] No `void *` parameters that should be typed
- [ ] Struct sizes match the strides and offsets in the assembly
- [ ] Every `INCLUDE_ASM(...)` is on a single line
- [ ] No redundant externs
- [ ] Every affected function diffed (`python3 tools/asm-differ/diff.py --no-pager <function>`, after a fresh build)
- [ ] `./tools/build-and-verify.sh` succeeds
