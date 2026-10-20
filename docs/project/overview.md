---
type: Overview
title: "Project overview"
description: "Mario Golf 64 (N64, US revision 0) matching decompilation; goal and non-goals."
tags: [project]
---

# Goal

Produce C source that, compiled with the original toolchain, yields a ROM byte-identical to Mario Golf 64 (Nintendo 64, North America, revision 0).

| Property | Value |
|---|---|
| Base ROM | `baserom.z64`, SHA1 `e2c4e7a905b29529b49a1619a401fe699224829b` (same value in `mariogolf64.sha1`) |
| Compiler | KMC GCC 2.7.2 with KMC binutils 2.6, via `tools/kmc-cc` (see [compiler profiles](/project/compiler-profiles.md)) |
| ROM extraction | [Splat](https://github.com/ethteck/splat), configured by [`mariogolf64.yaml`](/reference/splat-yaml.md) |
| Microcode | F3DEX2 (`gspF3DEX2.fifo`, extracted as the `rsp/gspF3DEX2_fifo_text` and `rsp/gspF3DEX2_fifo_data` bins) |

# How the pieces fit

1. `make extract` runs Splat over `baserom.z64` and writes `asm/` (disassembly, plus per-function `asm/nonmatchings/` files that `INCLUDE_ASM` stubs pull in), `assets/`, and the linker script `mariogolf64.ld`.
2. C files under `src/` replace assembly one function at a time; an unmatched function stays an `INCLUDE_ASM(...)` stub.
3. The build compiles each C file with its [compiler profile](/project/compiler-profiles.md), links the objects and extracted data into `build/mariogolf64.z64`, and the ROM matches when its SHA1 equals `mariogolf64.sha1`.

# Non-goals

* No game assets or ROM-extracted output (`asm/`, `assets/`, `build/`) are committed. The only committed assembly is hand-written: the entry stub and the libultra and libkmc routines written in assembly (`src/**/*.s`).
* An existing copy of the game is required (`baserom.z64`).
* Not a port to PC or any other platform.
* Not for commercial use.

# Next

* Where code lives: [repository layout](/project/repository-layout.md).
* Set up a checkout and build: [setup and build](/project/setup-and-build.md).
* Constraints that apply to every change: [ground rules](/project/ground-rules.md).
* Match a function: [decompile-function](/procedures/decompile-function.md).
