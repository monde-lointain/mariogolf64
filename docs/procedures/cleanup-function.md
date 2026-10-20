---
type: Procedure
title: "Clean up a matched function"
description: "Improve names, types and structure of already-matching code without breaking the full-ROM match; shared by the cleanup-matched and fix-low-quality nigel tasks."
tags: [cleanup, quality]
---

The ROM already matches, so every change must keep the build matching. Your job is to make the code read like what the original developers wrote.

# Steps

1. Read the target function and think about its purpose in the game, and how it relates to callers and callees.
2. Make a problems list: placeholder names, `unk` fields, pointer arithmetic, shift-for-arithmetic, copied locals, decompilation artefacts ([list](/reference/decompilation-artefacts.md)), untyped access.
3. Fix them while preserving the match. Expect several iterations. Cross-file refactors are in scope: when shared structs or declarations are wrong or incomplete, refactor the nearby code, and do not stop at a file boundary.
4. Verify with `./tools/build-and-verify.sh`. If it fails, fix it while preserving the cleanup.
5. Commit with a suitable message.

**Give up** after 30 failed attempts: revert all changes so the build stays clean, then exit.

# What to improve

* **Function name**, if it does not reflect the purpose. Leave good names and standard-library names alone.
* **Globals** with generic or misleading names; **struct fields**, especially `unk<offset>` fields whose purpose is clear from this and neighboring functions (`arg0->unk0` to `arg0->assetIndex`); **locals** that do not communicate intent (always safe to rename).
* **Structure and spacing**; proper struct and array accesses and types.
* **Artefacts** that do not break the match.

# Rename correctly

Renames are `symbol_addrs.txt` changes: follow [rename-global](/procedures/rename-global.md), including its rule for C and asm names that disagree.

# Safe refactoring

**Pointer arithmetic to typed access.** Provide the type for `arg0`, then:

```c
*(s16 *)((u8 *)arg0 + 0x28) -= 1;      →   arg0->unk28 -= 1;
temp_a0 = (u8 *)arg0->entries + offset;   →   temp_a0 = (u8 *)&arg0->entries[i];
```

If `offset` is `0x10 * i`, the elements are likely 0x10-byte structs.

**New or changed structs.** A shared struct may be reused where it is wrong, which can be what blocks cleanup. When you create or modify one, verify that nearby functions using the same data still compile and match ([struct changes](/project/code-quality.md#changing-structs)). Never replace a real type with `void *` to satisfy the compiler.
