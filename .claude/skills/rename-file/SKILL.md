---
name: rename-a-c-file-or-header-file
description: Renaming a C file or associated header (.h) file  requires additional steps in decompilation projects or the build will fail. Use this skill when you want to rename a C file and/or a header file.
---

# Rename a C file or a Header File

## Instructions

C files and their associated header files are mapped to specific ROM addresses via the Splat project configuration in `mariogolf64.yaml`.

A file will typically have a code segment, such as:

```
- [0x5A40, c, main/func_8002A640]
```

It may also have data and/or rodata segments. Data segment(s) are mapped to the code via the third argument. The name specified in the third argument must match the name of the code segment, including its directory. For example:

```
- [0xA5D30, .rodata, main/get_tile_attribute]
```

To rename src/main/func_8002A640.c to src/main/sky.c, we would need to do the following:

```
- [0x5A40, c, main/sky]

...

- [<rom>, .rodata, main/sky]
```

Every `INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", ...)` line in the renamed file must also change to `asm/nonmatchings/main/sky`, because the per-function assembly is extracted under the new name.

Naturally, the reverse is also true. To rename a segment that has already been named, you must change each instance of that name to the new name.

## Custom Make Rules

Some C files have custom compiler flags (for example the `FAST_MATH_SRCS` list and the per-library profiles in the Makefile's COMPILER PROFILES section). Be sure to check the project `Makefile` and update the C file's name there also.
