---
type: Reference
title: "Types and layouts"
description: "Reusing canonical types, inferring layouts from all consumers, and the blast radius of shared-struct changes; look here when offsets or access widths differ."
tags: [kmc-gcc, types, layout]
---

# A payload embeds a partial copy of the owning type

**Cause:** task payloads and allocation records often embed a complete renderer, transform, model or session type, even when one callback touches only a few fields.

**Try:**
* Use one canonical owning type across allocation, initialisation, rendering and cleanup.
* Use a callback-specific prefix type only when the payload is really a prefix or an alternate view. Using fewer fields alone is not a reason.
* For the steps when a shared struct changes, see [changing structs](/project/code-quality.md#changing-structs).

# Access widths or offsets differ between modes or consumers

**Cause:** a plausible field name is not evidence that two allocations share a type. Overlay game modes can reuse one address for different layouts. When several meanings occupy the same bytes, the bytes need a union or distinct types.

**Try:**
* Infer the layout from all consumers together: allocation sizes, element strides, field widths, nearby offsets, callers, callbacks and cleanup paths.
* For several meanings in the same bytes, use an offset-preserving union or distinct mode-owned types.
* Do not borrow an unrelated common type just because one accessed offset agrees.

# Offsets or sizes are hand-written

**Cause:** hand-written offsets are not checked against the declared type.

**Try:** use typed access, and check `sizeof` and the field offsets against the assembly. Procedure: [typed access](/project/code-quality.md#typed-access-instead-of-manual-offsets).

# A shared struct, prototype or global change moves other code

**Cause:**
* Parameter signedness is codegen. A `short` argument is sign-extended at the call site (`sll` then `sra`), while an `unsigned short` argument is masked (`andi`).
* A `volatile` qualifier on a shared global is semantic. In one probe, two stores through a `volatile` pointer global stayed two stores, and the same stores through a non-`volatile` global reduced to one.

**Try:**
* Keep each prototype's signedness as the original declares it.
* Give a shared declaration the `volatile` qualifier that each translation unit needs.
* For the change steps, see [changing structs](/project/code-quality.md#changing-structs).
