# Decompilation Learnings

This is a compact reference for durable behavior observed with KMC GCC 2.7.2 (game code `-O2 -mips3`; libraries use the profiles in the Makefile's COMPILER PROFILES section). It is not a work log: function names, match scores, attempts, commits, and one-off layout discoveries do not belong here. Add to it only when a lesson is a generic, reusable property of the compiler or the build.

## Matching Strategy

- Start from natural C that expresses the behavior of the assembly. Correct control flow and types usually matter more than early register tuning.
- Compare control flow, instruction selection, scheduling, register allocation, and stack layout separately. Fix the largest structural difference first.
- Make one small source-shape change at a time and validate it with the local matching harness.
- Treat fixed-register locals, empty inline assembly, and narrow inline assembly as last-resort code-generation controls. Keep their scope as small as possible.
- A function-level match is not sufficient after changing shared types or data. Rebuild and verify the complete ROM.

## Project Build Facts

- Each C file is compiled with the profile of its directory (and any file-specific override) in the Makefile; `make -s print-cflags SRC=<file>` prints it. A candidate that matches under the wrong profile is not a match.
- Files whose functions inline `sqrtf()`/`sqrt()` as bare `sqrt.s`/`sqrt.d` are compiled with `-ffast-math`; without it KMC GCC emits a NaN check and a library call. A fast-math TU applies the flag to every function in it.
- libultra is `-O3 -DBUILD_VERSION=VERSION_J` with signed `char`; libkmc is `-O`; libnaudio and libmus are `-O3`, and libmus is built without `_FINALROM`.
- KMC `as` pads each object's `.text` to 16 bytes. When splitting or merging translation units, keep the boundaries the original objects had.
- KMC GCC places an explicitly zero-initialized global in `.data`, leaves an uninitialized global as a `.comm` symbol (ignoring a section attribute), and only emits `.bss` in source order for `__attribute__((section(".bss")))` plus an initializer.
- Keep every `INCLUDE_ASM(...)`/`INCLUDE_RODATA(...)` on one line: Splat reads the function name from the macro's second argument when it decides which assembly to extract.
- Matched source relies on GNU C extensions that KMC GCC accepts (nested functions, lvalue casts, MIPS register asm names); host compilers such as clang reject some of them, so clang diagnostics are advisory only.

## Control-Flow Shape

### Branch and block order

Equivalent C constructs can produce different block layouts. In nested `if`/`else` statements, the outer condition is tested first and the inner alternative is generally laid out before the outer alternative. Inverting a condition changes fall-through behavior and can determine whether GCC emits a normal or branch-likely instruction.

For a few sparse constant cases, a `switch` can provide more control over comparison and block order than nested conditionals. A `default` placed first can serve as the fall-through body, while cases remain in source order.

### Shared versus duplicated calls

Combining two branches into one call with a selected argument is not code-generation-neutral. If the target repeats call setup in each branch, keep the calls in the branches. If the target shares call setup, compute the differing value first and use one call.

### Loop form

`for`, `while`, and guarded post-test loops can emit different entry tests, back edges, and delay slots even when they are semantically equivalent. Preserve explicit zero-trip guards when they are visible in the target. Moving an increment between the loop header and body can also change scheduling.

A `continue` targets the compiler-generated loop continuation point, while a `goto` targets a source label. This distinction can affect branch-likely selection and delay-slot filling.

### Simple conditional assignments

GCC may turn a local scalar assignment followed by a conditional overwrite into branchless arithmetic. Direct stores in both sides of an `if`/`else` are more likely to preserve a branch when the target has one.

## Expressions and Instruction Selection

### Signed division by powers of two

Write signed division naturally, such as `x / 2` or `x / 0x2000`. GCC emits the required add-bias-and-shift sequence for negative values. Hand-writing the assembly pattern usually changes scheduling or register allocation.

### Constant multiplication

GCC commonly replaces multiplication by a visible constant with shifts and additions. If the target uses `mult`, the multiplier must remain nonconstant until the multiplication pass. Use this only when evidence from the diff requires it; barriers added to inhibit propagation can affect surrounding code.

### Expression-tree operand order

Equivalent address expressions can reverse the operands of a commutative `addu`. Typed array indexing, field addressing, and explicit scaled-address expressions may therefore differ in the final instruction text. Prefer typed access, but preserve a different expression shape when exact operand order is the only mismatch.

### Packed and overlapping values

When assembly accesses the same storage at multiple widths, model the storage with a union or a small typed view. On the big-endian target, the low byte of a halfword or word is at the highest address within that value. Preserve halfword or word assignments when the target uses `sh` or `sw`; assigning only a named byte changes code generation.

Do not split a packed word in a global struct merely to name individual bytes. Even when field offsets appear unchanged, altered field declarations can change alignment and data layout. A union view preserves the original storage declaration.

## Register Allocation and Scheduling

### Variable lifetime and scope

Block-scoped temporaries have shorter live ranges and are less likely to consume callee-saved registers. Keep a temporary close to its use when the value does not need to survive calls. Conversely, hoisting or caching a subexpression can deliberately increase register pressure and shift allocation.

Declaration order and expression decomposition affect allocation. An otherwise unnecessary temporary can change which values are reloaded, retained, or assigned to argument and temporary registers. Use such changes only when they also leave readable, meaningful C.

### Statement order

The scheduler uses independent instructions to fill load-use and branch-delay slots. Source statement order determines which instructions are available in a scheduling region, so splitting a compound expression or moving a pointer load can change the emitted order without changing behavior.

An empty inline-assembly barrier can create a scheduling boundary or keep a value live. Use the narrowest possible barrier with accurate constraints; broad barriers tend to disturb unrelated allocation and scheduling.

### Fixed-register locals

KMC GCC accepts local register bindings, but host syntax checking may reject MIPS register names. When a fixed register is unavoidable, provide a host-check fallback and restrict the binding to a small block. Function-wide bindings often cause more mismatches than they solve.

### Stack frames

Unused stack locals may be removed by the assembler. Declaration order affects stack offsets, and alignment can make a small local change alter the frame by more than the local's size. If the target genuinely requires otherwise-unused stack space, a carefully placed volatile object can retain it, but meaningful local objects are preferable.

## Types and Layouts

### Reuse canonical types

Before defining a padded local view, search for an existing type with the same size, offsets, and ownership. Task payloads and allocation records often embed complete renderer, transform, model, or session types even when one callback touches only a few fields.

Prefer one canonical owning type across allocation, initialization, rendering, and cleanup. A callback-specific prefix type is appropriate only when the payload is genuinely a prefix or alternate view, not merely because the callback uses fewer fields.

### Infer layouts from all consumers

Use allocation sizes, element strides, field widths, nearby offsets, callers, callbacks, and cleanup paths together. A plausible field name is not evidence that two unrelated allocations share a type. Overlayed game modes can reuse the same address for different layouts.

When several meanings occupy the same bytes in different modes, use an offset-preserving union or distinct mode-owned types. Do not borrow an unrelated common type solely because one accessed offset happens to agree.

### Avoid manual offsets

Represent repeated strides and fixed offsets with structs, arrays, fields, and unions. Verify `sizeof` and field offsets against the assembly. Raw pointer arithmetic is acceptable only as a narrowly justified code-generation expression after the underlying storage has a correct type.

### Shared-type changes have broad effects

Before changing a shared struct, find every consumer and inspect accesses near the modified region. Verify affected functions and the complete build because a correct-looking field insertion can shift later fields, alter data alignment, or change address formation elsewhere.

## Data and Jump Tables

- Consecutive switch cases with identical bodies may be merged to one target. An empty case can instead point directly after the switch; inspect jump-table data to distinguish them.
- Symbol declarations and object boundaries can affect section layout even when bytes appear equivalent. Preserve the owning symbol and storage width unless there is strong evidence for a structural change.
- For BSS mismatches, inspect adjacent symbols and alignment. Multiple apparent globals may be one aggregate, while one apparent buffer may require explicit alignment or padding.

## Validation

For any code or layout change:

1. Confirm field accesses are typed and struct sizes match observed strides and offsets.
2. Check for existing declarations and canonical types before adding new ones.
3. Diff every affected function, especially other users of modified shared structs.
4. Run `./tools/build-and-verify.sh` and require a successful full-ROM verification.
