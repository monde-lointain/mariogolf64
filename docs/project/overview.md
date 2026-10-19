---
type: Overview
title: "Project overview"
description: "Mario Golf 64 (N64, US revision 0) matching decompilation; goal and non-goals."
tags: [project]
---

This project decompiles Mario Golf 64 into C that rebuilds the original ROM. The goal, the pieces that produce the ROM, and the non-goals are below.

# Goal

Produce C source that, compiled with the original toolchain, yields a ROM byte-identical to Mario Golf 64 (Nintendo 64, North America, revision 0).

| Property | Value |
|---|---|
| Base ROM | `baserom.z64`, SHA1 `e2c4e7a905b29529b49a1619a401fe699224829b` (same value in `mariogolf64.sha1`) |
| Compiler | KMC GCC 2.7.2 with KMC binutils 2.6, via `tools/kmc-cc` (see [compiler profiles](/project/compiler-profiles.md)) |
| ROM extraction | [Splat](https://github.com/ethteck/splat), configured by [`mariogolf64.yaml`](/reference/splat-yaml.md) |
| Microcode | F3DEX2 (`gspF3DEX2.fifo`) |

# How the pieces fit

1. `make extract` runs Splat over `baserom.z64` and writes `asm/` (disassembly and per-function `INCLUDE_ASM` sources), `assets/`, and the linker script.
2. C files under `src/` replace assembly one function at a time; unmatched functions stay as `INCLUDE_ASM(...)` stubs.
3. The build links the C objects and extracted data into `build/mariogolf64.z64`. The full ROM matches when its SHA1 equals `mariogolf64.sha1`.

# Non-goals

* No game assets, and no ROM-extracted output (`asm/`, `assets/`, `build/`), are committed. Hand-written library and entry assembly under `src/` is.
* An existing copy of the game is required (`baserom.z64`).
* Not a port to PC or any other platform.
* Not for commercial use.

# Next

* Where code lives: [repository layout](/project/repository-layout.md).
* Set up a checkout and build: [setup and build](/project/setup-and-build.md).
* Constraints that apply to every change: [ground rules](/project/ground-rules.md).
* Match a function: [decompile-function](/procedures/decompile-function.md).
