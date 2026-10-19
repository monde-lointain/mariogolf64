---
type: Reference
title: "Types and layouts"
description: "Reusing canonical types, inferring layouts from all consumers, and the blast radius of shared-struct changes; look here when offsets or access widths differ."
tags: [types, structs, layout]
---

# Reuse canonical types

Task payloads and allocation records often embed a complete renderer, transform, model or session type even when one callback touches only a few fields; [changing structs](/project/code-quality.md#changing-structs) states the reuse rule.

Prefer one canonical owning type across allocation, initialization, rendering and cleanup. A callback-specific prefix type is right only when the payload really is a prefix or alternate view, not merely because the callback uses fewer fields.

# Infer layouts from all consumers

Use allocation sizes, element strides, field widths, nearby offsets, callers, callbacks and cleanup paths together. A plausible field name is not evidence that two unrelated allocations share a type. Overlay game modes can reuse one address for different layouts.

When several meanings occupy the same bytes in different modes, use an offset-preserving union or distinct mode-owned types. Do not borrow an unrelated common type just because one accessed offset agrees.

# Avoid manual offsets

Verify `sizeof` and field offsets against the assembly; the procedure is in [typed access](/project/code-quality.md#typed-access-instead-of-manual-offsets).

# Shared-type changes have broad effects

The steps for a shared struct are in [changing structs](/project/code-quality.md#changing-structs). Parameter signedness is codegen too: at a call site a `short` argument is sign-extended (`sll` then `sra`), while an `unsigned short` argument is masked (`andi`), so keep each prototype's signedness as the original declares it. A `volatile` qualifier on a shared global is semantic as well: in one probe, two stores through a `volatile` pointer global remained two stores, while the same stores through a non-volatile global reduced to one, so a shared declaration should keep the qualifier each translation unit needs.
