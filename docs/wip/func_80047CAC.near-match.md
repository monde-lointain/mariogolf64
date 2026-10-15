---
type: "Reconstruction Note"
title: "func_80047CAC — banked with shared FP scratch and an unweighted boundary"
description: "Records the verified 47-instruction interpolation stepper and the separate lifetime, scheduling, and reference-weight levers that recover it."
---

# func_80047CAC — banked with shared FP scratch and an unweighted boundary

**BANKED, 2026-10-15:** `tools/verify-rom.sh` succeeds on the integrated source,
ROM SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
Main bank, ROM `0x230AC`, VRAM `0x80047CAC`, host
`src/main/func_800453E0.c`; unchanged `main` profile with its existing fast-math
override. The function has 47 instructions / `0xBC` bytes.

## Resolution

- Keep independent old-coordinate and delta locals, but reuse one `scaled`
  scratch for the three scaled/offset coordinates. This preserves the ROM's
  `$f14` quantity rather than coalescing each multiply into its input/delta.
- Keep the raw inputs, half factor, final Z delta, conversion factor, and
  smoothing factor as separate quantities. Reusing one variable across these
  disjoint lifetimes pushes it into global allocation and permutes the FPRs.
- Put an **empty** `do { } while (0)` after the final input scaling and before
  `smoothing = 0.2f`. The loop notes constrain scheduling without weighting the
  constant assignment. Enclosing that assignment instead preserves the 47-word
  schedule but scores 105 with a register permutation; moving it outside the
  empty boundary scores zero and retains both retail `mtc1` hazard nops.
- Read/write X through `f32* position_x = &D_800DAF30`. This preserves its held
  address without changing the global's scalar declaration or sibling bodies.
- `19.6500015f` reproduces the ROM's `0x419D3334` middle constant. `19.65f`
  rounds one ULP lower to `0x419D3333`.

## Evidence

`uv run --locked python tools/decomp_loop.py --func func_80047CAC` scores zero,
47/47 matching rows. The final scalar-owner source and measurement are:

`nonmatchings/func_80047CAC/measurements/44ca27d8ccdc2bc16a0997b3de6884c48c6ec1708a41e6c5015779deeb9d127e.{c,json}`.

Object SHA-256:
`2ae66f76b334c0956519e7f070f9c0a7173061aa874f54db473a6eaf843764f7`.
The formatted host was built through `tools/rom_build.py`; its fresh
`objdump -drz` function range preserves the instructions and relocations.
`target.s` and `provisional.o` are retained in the candidate directory.

A throwaway 32-bit UBSan harness extracted the integrated body and passed
12,000 bitwise float-result checks over 4,000 input/state combinations. It is
native host evidence, not an emulator run.

The earlier three-delta source scored 2195 under the same effective profile
(`measurements/4e025b81d84f63e999adf8ad42f81f5ed96159460a7456108653f2dd71fde6bc.{c,json}`).
The historical scheduler-impossibility and permuter recommendations are superseded
by this pure-C, full-ROM-verified resolution.
