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
| Compiler | KMC GCC 2.7.2 with KMC binutils 2.6, via `tools/kmc-cc` (see [compiler profiles](compiler-profiles.md)) |
| ROM extraction | [Splat](https://github.com/ethteck/splat), configured by [`mariogolf64.yaml`](/reference/splat-yaml.md) |
| Microcode | F3DEX2 (`gspF3DEX2.fifo`) |

# How the pieces fit

1. `make extract` runs Splat over `baserom.z64` and writes `asm/` (disassembly, per-function `INCLUDE_ASM` sources), `assets/`, and the linker script.
2. C files under `src/` replace assembly one function at a time; unmatched functions stay as `INCLUDE_ASM(...)` stubs.
3. The build links C objects plus extracted data into `build/mariogolf64.z64`. A match means its SHA1 equals `mariogolf64.sha1`.

# Non-goals

* No game assets or assembly are committed; an existing copy of the game is required.
* Not a port to PC or any other platform.
* Not for commercial use.

# Next

* Set up a checkout: [setup and build](setup-and-build.md).
* Constraints that apply to every change: [ground rules](ground-rules.md).
* Match a function: [decompile-function](/procedures/decompile-function.md).
