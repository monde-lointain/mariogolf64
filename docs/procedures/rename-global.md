---
type: Procedure
name: rename-global
title: "Rename a global variable or function"
description: "Renaming a global variable or function requires additional steps in decompilation projects or the build will fail. Use this skill when you want to rename a global variable or function."
tags: [symbols, rename]
---

Global names are bound to addresses in `symbol_addrs.txt` ([format](/reference/symbol-addrs-format.md)). Each symbol appears at most once; the only exceptions are same-named file-static functions in different libraries, which both carry `allow_duplicated:True`. A symbol that is absent gets a default name: `func_<VRAM>` / `D_<VRAM>` (`func_ovl<N>_<VRAM>` / `D_ovl<N>_<VRAM>` in overlays).

# Steps

1. Edit the entry in `symbol_addrs.txt`. Keep existing `size:` and `type:` annotations.
2. Update every C reference, declaration and header in `src/` and `include/`.
3. If the symbol is a function still pulled in with `INCLUDE_ASM("asm/nonmatchings/<dir>/<file>", <name>)`, rename the second argument too; extraction writes `asm/nonmatchings/<dir>/<file>/<new name>.s`.
4. Rebuild: `./tools/build-and-verify.sh`.

# Entry shapes

```text
MyNewGlobal = 0x800B11A0; // size:0x4
MyFunction = 0x800B00C0; // type:func segment:main
MyOverlayGlobal = 0x8024F0B0; // segment:overlay_4
```

Overlays share virtual addresses, so overlay symbols must name their segment from `mariogolf64.yaml` with `segment:`.

# When the C name and the asm name disagree

If asm shows `func_8004E6F8` but C calls it `initPauseMenuDisplayTask`, the C function was renamed without updating `symbol_addrs.txt`. Update the entry to the new name; a task that lists such a function (cleanup candidates, `func_...` names) means the entry is missing or wrong.

# When not to rename

Leave a name alone when it already conveys the purpose. Never rename functions that correspond to standard-library or SDK names; keeping that convention is valuable.
