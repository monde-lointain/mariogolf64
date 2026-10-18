---
name: rename-global-variable-or-function
description: Renaming a global variable or function requires additional steps in decompilation projects or the build will fail. Use this skill when you want to rename a global variable or function.
---

# Rename a global variable or function

## Instructions

Global variables and function names are mapped to specific addresses in memory through the symbol_addrs.txt file at the root of the project. Each symbol should only appear at most once. If a symbol is not present it will be given a default name (usually starting with `func_` or `D_`). `ghidra_symbols.txt` holds older imported names; `symbol_addrs.txt` loads after it and wins for the same address, so add or change names in `symbol_addrs.txt` and remove a stale duplicate from `ghidra_symbols.txt`.

So at a minimum this file will need to be updated when attempting to rename a variable. However additional steps may also be needed:

- Update every C reference, declaration, and header in `src/` and `include/`.
- If the symbol is a function whose assembly is still included with `INCLUDE_ASM("asm/nonmatchings/<dir>/<file>", <name>)`, rename the second argument too: extraction writes `asm/nonmatchings/<dir>/<file>/<new name>.s`.

### Global Variables

Global variables in the main segment have a default name format that looks like: `D_<memory address>`. For example `D_800B11A0`. To rename `D_800B11A0`, for example, to `MyNewGlobal` we would do:

```
MyNewGlobal = 0x800B11A0; // size:0x4
```

Note the structure of `<new name> = <memory address>; // <annotations>`.

Overlays share virtual addresses, so overlay symbols (default names such as `D_ovl4_8024F0B0`) must name their segment from `mariogolf64.yaml`:

```
MyOverlayGlobal = 0x8024F0B0; // segment:overlay_4
```

You may see additional annotations for size and type. These should be preserved if they exist.

### Functions

A similar process is followed for functions. A function will by default have a name structure of `func_<memory address>` (or `func_ovl<N>_<memory address>` in overlays). For example: `func_800B00C0`. To rename this function to `MyFunction` we would do:

```
MyFunction = 0x800B00C0; // type:func segment:main
```

## Validation

To validate your name change, rebuild the project:

```
./tools/build-and-verify.sh
```
