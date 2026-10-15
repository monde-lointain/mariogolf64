---
type: Reference
title: C coding and matching style
description: Practical conventions for hand-authored C that maintain readability without overriding byte-exact decompilation requirements.
---

# Coding guidelines (on-demand reference)

These rules govern classical / hand-authored C. For upstream mirrors under `src/libultra/`,
`src/libkmc/`, `src/libnusys/`, `src/libnaudio/`, `src/libmus/`, and `src/mgu/`, preserve upstream
logic and names. Apply only the formatting required by the mirror workflow; never refactor it.

## Matching takes priority

This is a maintainability guide, not a mandate to reshape code that already matches. In a
decompilation, source structure, types, declarations, and control flow can change emitted MIPS even
when behavior is equivalent. Preserve the ROM's instruction order, register allocation, delay slots,
frame, and bytes; prefer the source form demonstrated by a measured match over generic style rules.
Use [Source levers](levers.md) for evidence-backed code-generation adjustments and
[Hazard index](hazard-index.md) for procedures with a playbook.

The project uses KMC GCC 2.7.2 and KMC binutils. The base flags include `-O2` and `-mips3`,
but effective flags vary by library, tree, and translation unit. Treat
[`tools/build_profiles.json`](../tools/build_profiles.json) as the compiler-profile authority; use
the [build guide](build.md#compiler-profiles-and-incremental-work) for current build commands.
Do not assume a modern compiler, C++ features, or a single flag set.

Keep code understandable, but treat complexity guidance as a heuristic, not a reason to change a
matching source form. Simplify only when the generated output remains correct for the target and the
change is consistent with the ROM.

## Naming

Use the project convention below as the default for new identifiers. Choose descriptive names, with
short names for short-lived local values; `i`/`j`/`k` are suitable only for simple loops. Prefer
positive boolean names and consistent opposites. Do not rename an identifier in a matched body
without checking the resulting output.

For newly authored code, keep each statement and declaration on its own line when that does not
conflict with the match.

## Variables and data

- Keep variables local and their live ranges short where the ROM permits; both scope and lifetime can
  affect register allocation.
- GCC 2.7.2 requires C89 declaration placement. Narrow declarations into existing control-flow
  bodies where matching permits; do not add standalone scope blocks solely to hide locals.
- Initialize values at the point required by the target output; do not move declarations or
  initialization mechanically.
- Use project types from `ultra64.h` (`s32`, `u64`, `vu32`, `f32`, and related types) where appropriate.
  Avoid host-width assumptions such as raw `long` for target data.
- Preserve signedness, width, casts, and expression shape needed by the target instructions. Do not
  replace literal values or arithmetic with named constants if that alters matching.

```c
s32 count;
u8 *bytes;
```

## Functions

Use a function when it names a useful abstraction or matches the target's structure. Keep signatures,
return types, call boundaries, and side effects aligned with the ROM; apparently redundant code may
be required for exact output. Do not impose arbitrary limits on function length or parameter count.

## Modules and data abstraction

Use the existing project structures and interfaces. New abstractions are useful when they clarify
ownership or remove duplication, but avoid changing data layout, symbol visibility, or access
patterns in a matched region without confirming the emitted result.

### Recovered main-bank layouts

Keep cross-file contracts in their existing owner header; keep genuinely private records in their
translation unit. These headers describe observed target layouts, not host-native serialized data:

| Header | Shared records |
| --- | --- |
| [`animation.h`](../src/main/animation.h) | Character state, model-resource prefix, keyframe tracks, texture-animation records |
| [`club.h`](../src/main/club.h) | Live `ClubShot` and separate `ClubDistancePreset` table rows |
| [`controller.h`](../src/main/controller.h) | Processed controller state and six-byte repeat slots |
| [`fog.h`](../src/main/fog.h) | Fog/color presets, light records, lens-flare-cap prefix |
| [`rom_asset.h`](../src/main/rom_asset.h) | ROM read cursor and its enclosing compressed-asset load record |
| [`session.h`](../src/main/session.h) | Player/scenario state, saved mode records, per-scenario statistics |
| [`terrain.h`](../src/main/terrain.h) | Terrain attributes and scenery collision cylinders |
| [`vector.h`](../src/main/vector.h) | Shared `Vec3f` position, rotation, scale, and velocity values |

Name fields from multiple consumers, preserving target width, signedness, units, and sentinel values.
Use `unknown_XX` for an unestablished meaning instead of promoting a plausible guess. In particular,
the statistics buckets select characters, not holes; a scenery cylinder's sprite index is not a
boolean active flag; and a ROM asset's decode envelope is not a second interchangeable read slot.

Interior linker labels do not imply independent allocations. For example, the player score/status
labels belong to `PlayerState`, `polychara_state` names `CharacterState.resource_index`, and the sky
position/texture-coordinate labels belong to the same `Vtx` array. Prefer the enclosing record or
actual member array when it matches.

GCC 2.7.2 distinguishes component accesses, scalar pointer views, and address roots. A measured
exception may therefore use a scalar walk rooted at `&records[0].member`, or an explicitly documented
address projection, while retaining the canonical record definition and its stride. Preserve the
required induction/control shape; do not introduce duplicate padded types or pretend an interior
label owns separate storage just to recover codegen. Isolated comparisons guide iteration;
`tools/verify-rom.sh` remains the bank authority.

## Control flow

Write control flow that is clear and defined, then compare it with the target. Equivalent `if`,
`switch`, loop, and boolean-expression forms can compile differently; branch shape, loop form, and
join points are code-generation choices here. Preserve required conditions and behavior, and consult
[Source levers](levers.md) before changing structure to chase a particular instruction sequence.

Prefer measured structured alternatives over blanket claims that a `goto` is mandatory. The safe
magnitude helpers match with ordered, nested exit tests in `while (1)`, and the CRC bit loop matches
as a counted `for` with its counter declared at function scope. Conversely, decoder dispatch edges,
one-shot loop notes, shared scratch values, and score-comparison exits can still determine allocation
or branch layout. Retain a non-idiomatic form only with a nearby explanation of the measured constraint.

## Defensive programming

Keep behavior defined and preserve the target's boundary behavior. Add checks, assertions, or error
paths only when required by the code's actual contract; extra branches can change the match. Do not
introduce C++-specific practices into this C codebase.

## Comments and layout

- Prefer clear names and straightforward control flow. Comments explain intent, surprising behavior,
  or why a deliberate oddity must remain; keep them accurate and next to the code.
- Keep layout consistent with the surrounding file; preserve upstream mirror logic and names.
- The root `.clang-format` uses Google style and sorts includes. The library trees (`src/libultra/`,
  `src/libkmc/`, `src/libnusys/`, `src/libnaudio/`, `src/libmus/`, `src/mgu/`, `src/main/`, and
  audio-library include trees) use local `.clang-format` files with `SortIncludes: Never` because
  include order can be significant. Preserve required include order; configure the directory rather
  than adding a per-file `clang-format off` guard. See the [root format configuration](../.clang-format)
  and [main profile](../src/main/.clang-format).

## Writing, evolving, and verifying code

Understand the target and existing implementation before editing. For a non-trivial change, identify
the intended behavior and relevant ROM instructions first; make a focused source change, then use the
project's documented matching workflow to establish whether it reproduces the target. A behaviorally
equivalent rewrite is not a successful match by itself.

When diagnosing a difference, use the observed assembly/object output to form a hypothesis, change
one relevant source choice at a time, and compare again. Avoid speculative cleanup or optimization:
the compiler's result, not source-level elegance, determines whether a candidate is useful.

---

# C naming conventions

| Identifier | Convention | Example |
| --- | --- | --- |
| Struct and enum types | `CamelCase` | `PacketHeader`, `ConnectionState` |
| Enum constants and macros | `UPPER_CASE` | `CONNECTION_CONNECTED`, `BUFFER_SIZE` |
| Functions, variables, parameters, globals, statics | `lower_case` | `initialize_system`, `frame_count` |
| Global/static-duration constants | `UPPER_CASE` | `MAX_CONNECTIONS` |
| Local constants | `lower_case` | `max_connections` |

Struct members use `lower_case`. Do not add a `k` prefix. This convention guides new code; preserve
the identifier spelling in matched or mirrored code when renaming changes output or upstream fidelity.

The m2c seed body is compiled from `base.c`, so rename synthetic `temp_*`, `local_*`, and `arg_*`
identifiers before promoting when matching permits. Use target-width types from `ultra64.h`, not raw
host-width `int`/`long long`/`volatile unsigned long` for target values.

## Library near-implementations: keep the `gu*` name vs. rename as custom

Preserve upstream names for verbatim mirrors. For a library-like candidate, distinguish an upstream
libultra routine from custom game code using its address region and matching evidence; opcode
similarity alone is not proof of identity. The matcher reports opcode-signature candidates, so treat
its output as evidence to investigate, not an automatic naming decision. See
[the candidate matcher](../tools/libultra_match.py).
