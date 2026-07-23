# func_8004CDA0 — cloud-buffer blend driver (near-match, S263)

**Status:** CARRIED. 227/234 instr, structure 100% correct, tail byte-exact.
Residual = `#local-alloc-qty-permutation`: register naming + ROM's extra
register-preserving `move` copies my build collapses.

## What it does
Sky-cloud texture generator, called each frame (guarded by `flag_is_set(0xC)`).
- `D_801062C0` = `s16 [2][64][64]` double-buffered cloud height grid.
- `D_801B56F0` = `s16*` cursor to the current buffer base.
- `D_800BE6D0` = active buffer index (0/1, flipped each cycle).
- `D_800BE6D4` = frame counter (`&0x7F` wrap; `&0x3F` = blend weight).
- `D_800BE6D9` = row counter (`&0x3F`), one texture row filled per call.
- `D_800BE6D8` = one-shot init flag (read signed `lb`).
- `sky_cloud_texture_ptr` = `u8*` output 64-wide texture.

On init (`D_800BE6D8 != 0`): clear both buffers, seed 3 diamond-square corner
points (`-0xFE0`, `0x3F80`), call `func_8004C958(0,0,0)` (the recursive
diamond-square) on each buffer. Every call: blend the two buffers by `weight`
into one row of the output (`(a*(0x3F-w) + b*w) >> 13`, abs, clamp 0xFF). When
the row counter wraps, advance the frame, and on the 0x40-boundary re-clear +
re-seed the active buffer and flip `D_800BE6D0`.

## The wall
Body (in `nonmatchings/func_8004CDA0/` and the src edit reverted this sprint) is
structurally exact — the final block + re-clear loop are byte-identical, just
offset by the 7-instruction shortfall accumulated earlier. The 7 missing
instructions are all register-preserving `move`s the ROM's allocation emits and
mine does not:
- clear loops: ROM `move a0,v0` (buf copy, base in a1, buf in v0→a0) x2; mine
  keeps buf in a1 directly (no copy).
- blend inner: ROM `move a1,v0` (abs: `sra v0,a1,13` then move back) + `move
  v0,t0` (dst post-inc: advance counter early, keep old value). Mine does both
  in place.

Register NAMING also permutes (`i`: ROM t0 / mine a3; base: ROM a1 / mine t0),
cascading. Tried and did NOT move it: row-pointer hoist (`row = buf + j*0x40`),
explicit held base (`&D_801062C0[0][0][0] + i*0x1000`), compute-dst-next-before-
store (Ghidra's `sVar10=sVar9+1` order), `s8 D_800BE6D9` + `(u8)` at increment,
`*(s8*)&D_800BE6D8`. gcc-2.7.2 collapses every preserving copy my source implies.

Not permuter-eligible: 7-instruction count deficit (not a pure permutation), and
the class is `#local-alloc-qty-permutation` (0 project cracks). Re-open only with
a genuinely new regalloc lever (allocno-dump-guided steering).
