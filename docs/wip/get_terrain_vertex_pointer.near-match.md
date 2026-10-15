---
type: "Reconstruction Note"
title: "get_terrain_vertex_pointer — banked via shared scratch chains"
description: "Records the verified match and register-reuse lever for get_terrain_vertex_pointer."
---

# get_terrain_vertex_pointer — banked via shared scratch chains

## Verified state — 2026-10-14

- ROM: US revision 0, SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
- Bank: YAML `main`; ROM offset `0x1C924`, VRAM `0x80041524`.
- Host: `src/main/get_tile_attribute.c`; the assembly stub is replaced by C.
- Size: `0xA0`, 40 instructions; the parent's configured `main` profile is unchanged.
- Candidate: `nonmatchings/get_terrain_vertex_pointer/base.c`.
- Measurement: `measurements/010f910df4fc8221fd0d60403ec42ff3e0f6d6485bcbe0427c187e558c059590.json`
  under that candidate directory, score zero and 40/40 aligned rows.
- Commands: `uv run --locked python tools/decomp_loop.py --func get_terrain_vertex_pointer`,
  `uv run --locked python tools/rom_build.py build --object src/main/get_tile_attribute.o`,
  then function-scoped `objdump -drz` on both objects and `tools/verify-rom.sh`.
- The integrated object reproduced all instruction/register/branch offsets after accounting
  for section-relative jump relocations and the same-TU callee relocation.
- `tools/verify-rom.sh` succeeded: build OK and full-ROM SHA-1 match.

## Semantics

Even column/row coordinates delegate to `get_direct_grid_vertex(col >> 1, row >> 1)`.
Odd coordinates select an 8-column tile grid with 4-cell subdivisions. Row `0x40`
uses tile row 15 and subdivision row 4; column `0x20` uses tile column 7 and
subdivision column 4. The byte lookup at `D_800BA9B0` selects a 16-byte vertex
within the 256-byte tile at `D_80185220`.

## Lever and retired carry verdict

The reconstructed separate-temporary baseline compiled to score 820, not the historical
score 3060. It had the target's 40 instructions but used `a3` for `col_hi` and
`a0` for the tile offset, with a plain `bne` instead of the target's `bnel`.

The allocation dumps showed local scratch quantities taking `v0` before the global
coordinate quantities; declaration-order changes could not remove those conflicts.
The fix is reuse, not a compiler-profile change or a scheduling barrier:

1. Reuse `col_hi` for the lookup result. This alone reached score 345 and restored
   the column-select register and branch-likely instruction.
2. Split tile arithmetic into `row_hi <<= 3; row_hi += col_hi; row_hi <<= 8`.
3. Reuse `col` for both table base addresses, `row_hi` for the tile address, and
   `col_hi` for the lookup-address, byte-load and vertex-offset chain.

The complete scratch chains eliminated the remaining local/global register conflicts
and reached score zero. Sharing just the comparison constants, extending just the tile
offset lifetime, and wrapping only the tile calculation in `do {} while (0)` did not
match. No permuter or Ghidra mutation was needed.

The earlier exact-count register-permutation carry and permuter-only recommendation
are retired. The banked source retains unsigned shifts, endpoint selects, full table
base materialization, and the ROM's addition grouping.
