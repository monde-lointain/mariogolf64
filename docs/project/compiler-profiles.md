---
type: Reference
title: "Compiler profiles"
description: "KMC GCC 2.7.2 flags per source directory and per file, and how to print the effective flags."
tags: [compiler, build]
resource: ../../Makefile
---

All C is compiled by KMC GCC 2.7.2 and assembled by KMC binutils 2.6, driven by `tools/kmc-cc`. The `COMPILER PROFILES` section of the `Makefile` is the source of truth; this table summarizes it. A candidate that matches under the wrong profile is not a match, which is why [decomp environments](/tools/decomp-env.md) and the [permuter](/tools/permuter.md) read the owning file's profile instead of assuming one.

Print the effective flags for any file (allowed under the [policy hook](/harness/policy-hook.md)):

```bash
make -s print-cflags SRC=src/main/func_80025D30.c
```

# Base profile

`-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2`, `-D_LANGUAGE_C -D_FINALROM -DINCLUDE_ASM_USE_MACRO_INC`, and the include paths for `include/` and each vendored library header tree. C89; declarations at the start of blocks.

# Overrides

| Scope | Changes | Why |
|---|---|---|
| `src/main/*`, `src/overlay_*` | `-DF3DEX_GBI_2` | Game code and overlays build F3DEX2 display lists for the F3DEX2 microcode. `src/mgu` uses the bare base profile. |
| Listed `FAST_MATH_SRCS` files | adds `-ffast-math` | These files inline `sqrtf()`/`sqrt()` as bare `sqrt.s`/`sqrt.d`. Without it GCC emits a NaN check and a library call. The flag applies to every function in the file. |
| `src/overlay_7/func_ovl7_80206680.c` | adds `-ffast-math` | `func_ovl7_802068B4` is a bare `sqrt.s` in the ROM. Only this file: the rest of `overlay_7` stays on the base profile, without `-DF3DEX_GBI_2`, so it is not in `FAST_MATH_SRCS`. |
| `src/libkmc/*` | `-O` | libkmc was built with `gcc -O`. |
| `src/libultra/*` | `-O3 -fsigned-char -DBUILD_VERSION=VERSION_J -DF3DEX_GBI_2`, libultra compiler headers first | VERSION_J libultra with signed `char`: byte loads sign-extend (`lb` + shifts, not `lbu`/`andi`). |
| `src/libnusys/*` | `-DUSE_EPI` | Enables the `nuPi*` bodies. |
| `src/libnusys/nuboot.*` | `-O0`, no `USE_EPI` | The game-embedded nusys boot shipped at `-O0`. |
| `src/libnaudio/*` | `-O3 -DN_MICRO=1`, `src/libnaudio` headers first | n_audio_sc with the micro command stream. |
| `src/libmus/*` | `-O3 -DSUPPORT_NAUDIO -U_FINALROM`, `src/libmus` headers first | n_audio variant built without `_FINALROM`; `OSScTask` keeps its trailing timing fields. |

File-specific overrides win over directory profiles (longer stems win). To change one file, add an override next to its library profile, for example `$(BUILD_DIR)/src/libultra/io/foo.%: OPT_FLAGS = -O2`; never change a vendored tool. Renaming a file also means updating its override ([rename-file](/procedures/rename-file.md)).

# Assembler behavior that affects layout

KMC `as` pads each object's `.text` to 16 bytes, so translation-unit boundaries must follow the original objects when splitting or merging files. Hand-written libultra and libkmc assembly goes through the KMC driver for the same reason; only the entry stub uses modern GAS.
