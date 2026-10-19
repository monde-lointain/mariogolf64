---
type: Procedure
name: rename-file
title: "Rename a C file or header"
description: "Renaming a C file or associated header (.h) file requires additional steps in decompilation projects or the build will fail. Use this skill when you want to rename a C file and/or a header file."
tags: [rename, segments]
---

C files map to ROM ranges through Splat segments in `mariogolf64.yaml` ([format](/reference/splat-yaml.md)). A file has a code segment and possibly data and rodata segments; the third field of the data and rodata entries must equal the code segment's name, directory included.

```yaml
- [0x5A40, c, main/func_8002A640]
...
- [0xA5D30, .rodata, main/func_8002A640]
```

To rename `src/main/func_8002A640.c` to `src/main/sky.c`:

1. Change every entry's name: `- [0x5A40, c, main/sky]` and `- [<rom>, .rodata, main/sky]`. Renaming an already-named segment means changing each occurrence of the name.
2. Change every `INCLUDE_ASM("asm/nonmatchings/main/func_8002A640", ...)` in the file to `asm/nonmatchings/main/sky`: extraction writes per-function assembly under the new name.
3. Update file-specific Makefile rules that name the file (for example the `FAST_MATH_SRCS` list and any override in `COMPILER PROFILES`; see [compiler profiles](/project/compiler-profiles.md)).
4. Move or rename the matching header, then update every `#include` of it.
5. `git rm` the old file when the new one is a separate path.
6. Run `./tools/build-and-verify.sh`.
