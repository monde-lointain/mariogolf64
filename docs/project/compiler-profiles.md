---
type: Reference
title: "Compiler profiles"
description: "KMC GCC 2.7.2 flags per source directory and per file, and how to print the effective flags."
tags: [compiler, build]
resource: ../../Makefile
---

C is compiled by KMC GCC 2.7.2 and assembled by KMC `as` (binutils 2.6), through `tools/kmc-cc`. The `COMPILER PROFILES` section of the `Makefile` is the source of truth; this page summarizes it. A candidate that matches under the wrong profile is not a match, so decomp environments and the [permuter](/tools/permuter.md) read the flags of the owning file from `make print-cflags` instead of assuming one.

# Compile pipeline

`tools/kmc-cc` runs three steps: KMC GCC 2.7.2 (`tools/gcc_kmc/gcc -S`) emits assembly, KMC `as -EB -mips2 -G 0 -I include` assembles it, and `strip -N dummy-symbol-name` removes the symbol `dummy-symbol-name`. The ROM build, the decomp environments and the permuter all use this wrapper.

# Printing the flags

```sh
make -s print-cflags SRC=src/main/func_80025D30.c
```

This is the only `make` invocation agents may run; see [ground rules](/project/ground-rules.md) §5. The compile line is `PROFILE_PREPEND`, then the base flags, the optimization level, the include paths, the macros, and `PROFILE_APPEND`.

# Base profile

`-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2`, plus `-D_LANGUAGE_C -D_FINALROM -DINCLUDE_ASM_USE_MACRO_INC`, and `-DNON_MATCHING` when `NON_MATCHING=1`. Include paths: `include`, `include/libultra`, `include/libultra/internal`, `include/libkmc`, `include/libnusys`, `include/libmus`, `include/libnualstl` and `include/libnaudio`.

Code is written C89-style with declarations at the start of blocks. `make tidy` checks with `-std=gnu89`.

# Overrides

| Scope | Changes | Why |
|---|---|---|
| `src/main/*` | `-DF3DEX_GBI_2` | Game code builds F3DEX2 display lists. `src/mgu/` has no override and uses the base profile. |
| `src/overlay_*/*` (including `overlay_gap`) | `-DF3DEX_GBI_2` | Overlays run the same F3DEX2 microcode; their display-list builders only match with F3DEX2 opcodes. |
| Files in `FAST_MATH_SRCS` (Makefile; `src/main` and overlay files) | `-DF3DEX_GBI_2 -ffast-math` | These files inline `sqrtf()`/`sqrt()` as bare `sqrt.s`/`sqrt.d`. Without the flag KMC GCC adds a NaN guard and a library fallback the ROM lacks. The flag applies to every function in the file. |
| `src/libkmc/*` | `-O` | libkmc was built with `gcc -O`. |
| `src/libultra/*` | `-O3`; `-I include/libultra/compiler/gcc` before the base flags; appends `-fsigned-char -DBUILD_VERSION=VERSION_J -DF3DEX_GBI_2 -I include/libultra/PR` | VERSION_J libultra was built with signed `char`: byte loads sign-extend (`lb` plus shifts, not `lbu`/`andi`). Its compiler headers come before `include/libkmc`, so `stdlib.h` resolves to libultra's copy. |
| `src/libnusys/*` | appends `-DUSE_EPI` | Enables the `nuPi*` bodies. |
| `src/libnusys/nuboot.*` | `-O0`; no `-DUSE_EPI` | The game-embedded nusys boot shipped at `-O0` without `USE_EPI`. |
| `src/libnaudio/*` | `-O3`; `-I src/libnaudio` before the base flags; appends `-I include/libultra/PR -DN_MICRO=1` | n_audio_sc with the micro command stream. Its private headers come before the libultra-internal copies of the same names. |
| `src/libmus/*` | `-O3`; `-I src/libmus -I include/libmus/PR` before the base flags; appends `-I include/libultra/PR -DSUPPORT_NAUDIO -U_FINALROM` | The n_audio variant, built without `_FINALROM`: `OSScTask` keeps its trailing timing fields. |

A file-specific rule replaces the directory value of the same variable. `print-cflags` shows this for `src/libnusys/nuboot.c` (`-O0`, no `-DUSE_EPI`) and for the FAST_MATH members.

To change one file, add a rule next to its library profile, for example `$(BUILD_DIR)/src/libultra/io/foo.%: OPT_FLAGS = -O2`. A new FAST_MATH file goes into `FAST_MATH_SRCS` instead. Renaming a file also means updating its override ([rename-file](/procedures/rename-file.md)).

# Assembly and object layout

KMC `as` emits the original `move` encoding and pads each object's `.text` to a 16-byte boundary, so translation-unit boundaries must follow the original objects when splitting or merging files.

| Input | Assembler | Notes |
|---|---|---|
| `src/libultra/**/*.s` | KMC GCC driver (`$(CC)`) with `LIBULTRA_ASFLAGS` | Hand-written libultra assembly. `objcopy` then sets 4-byte section alignment. |
| `src/libkmc/*.s` | KMC `as -EB -mips2 -I src/libkmc` | Soft-float assembly keeps KMC register conventions. |
| `src/entry/entry.s`, `asm/**/*.s` | Host `cpp`, then GNU `as -march=vr4300 -32 --no-pad-sections` | Entry stub and extracted disassembly. `objcopy` then sets 4-byte section alignment. |
| `*.bin` | `objcopy -I binary -O elf32-tradbigmips` | Raw binary data. |
