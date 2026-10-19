---
type: Tool
title: "split-asm-segments"
description: "Turn one assembly-only subsegment of mariogolf64.yaml into a C file of INCLUDE_ASM stubs, verify the ROM still matches, and optionally commit that split alone."
tags: [splat, asm, subsegment, split]
resource: ../../tools/split_asm_segments.py
---

```bash
tools/split_asm_segments.py --list
tools/split_asm_segments.py --offset 0x19AC0 [--commit]
tools/split_asm_segments.py --all [--commit]
```

Code that is still raw assembly appears in `mariogolf64.yaml` as `- [0xOFF, asm]` lines. Changing one to `- [0xOFF, c, <segment>/<name>]` makes splat generate `src/<segment>/<name>.c` with one `INCLUDE_ASM` per function in the range. The bytes stay the same and no function is decompiled, so the split is a pure bookkeeping change.

For each subsegment the tool:

1. rewrites only that yaml line and refuses if `src/<segment>/<name>.c` already exists;
2. runs [build-and-verify](build-and-verify.md), which re-extracts the ROM and checks its SHA1;
3. keeps the change if the build passed and the file exists. Otherwise it restores the yaml bytes exactly and removes the C file this attempt created. The build log is kept and its last lines are printed.

If the process is interrupted mid-build, the yaml is restored before the exception propagates.

## Naming

The name follows the existing `c` entries: VRAM is `vram + (offset - start)` of the owning segment, printed as eight hex digits. `func_<VRAM>` for segments without `symbol_name_format`, such as `main/func_8003E6C0`. For overlays the format is substituted, as in `overlay_8/func_ovl8_801F55A0`.

## Commit

`--commit` commits only `mariogolf64.yaml` and the new C file (`git commit --only`), so other staged work is left alone. The message is generated from the split. `--all` splits in file order and keeps going past failures; it prints a `kept` or `FAILED` line per subsegment and exits non-zero if any failed.

Generated `asm/` is gitignored, so it is not part of the commit. A fresh checkout re-extracts it.

Run `--all` only in a dedicated worktree (see [new-worktree](new-worktree.md)). Each attempt rebuilds the ROM, and the tool rewrites tracked files in the checkout it runs in.
