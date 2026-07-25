# detect_terrain_collision — register-pressure / spill-pattern carry (S280)

## Verdict
Body is FULLY RE'd and BEHAVIORALLY COMPLETE (every instruction's semantics reconstructed). First
build is a clean compile but 252 instrs vs the ROM's 283 (0x46C) — 31 instructions SHORT because the
ROM operates under much HEAVIER register pressure: it spills the three params (x, z, out) to their
home slots and re-reads them via a materialized arg pointer (`s7 = sp + 0x68`), and spills `row_frac`
to the stack (0x3C) and the three vertex pointers to 0x18/0x1C/0x20, reloading each per iteration.
gcc kept those values in registers for my reconstruction (s0-s7 available), producing shorter code.
Matching the ROM's exact spill/reload set is a local-alloc coloring problem, same wall class that
walled S280 item-1 `get_interpolated_terrain_height`, but larger. asm-differ score ~19974 (dominated
by the length gap + the spill divergence). NOT permuter-eligible (not exact-count; a spill-pattern
divergence, not a register permutation).

## What the function does (fully RE'd)
`s32 detect_terrain_collision(s32 x, s32 z, s16* out)` — locates the terrain triangle under world
point (x,z) and fills `out` (the `s16 tri[11]` buffer that `get_interpolated_terrain_height` reads:
out[2..4]=x[3], out[5..7]=y[3], out[8..10]=z[3]). Returns 0 on hit (out filled), -1 on out-of-range
or miss.

1. Range check (mode = `g_terrain_vtx_xform_mode`, s8): mode!=0 -> x,z in [0,0xFFFFF] else return -1;
   mode==0 -> x in [0,0x3FFFFF], z in [0,0x7FFFFF] else return -1. (mode is RE-READ for step 2, matching
   the ROM's two `lb g_terrain_vtx_xform_mode` loads.)
2. Grid cell base + fractional offset:
   - mode!=0: col_base=(x>>17)<<2, row_base=(z>>16)<<2, col_frac=(x>>15)&3, row_frac=(z>>14)&3
   - mode==0: col_base=(x>>19)<<2, row_base=(z>>19)<<2, col_frac=(x>>17)&3, row_frac=(z>>17)&3
3. Nested 4x4 scan (`for ri in 0..3, for ci in 0..3`), scanning cells in the order given by the LUT
   `D_800BB0EC = {0,1,3,2}` (s8[4]):
   - row = row_base + ((row_frac + D_800BB0EC[ri]) & 3); col = col_base + ((col_frac + D_800BB0EC[ci]) & 3)
   - Four grid vertices: va=gtvp(col,row), vb=gtvp(col+1,row), vc=gtvp(col,row+1),
     vd=gtvp(col+1,row+1) where gtvp = `get_terrain_vertex_pointer(col,row)` (returns s16* vertex,
     v[0]=x, v[1]=y/height, v[2]=z).
   - Quad split into two triangles; 2D point-in-triangle via 3 CCW edge cross products, each
     `(B.x-A.x)*(z - A.z<<10) - (B.z-A.z)*(x - A.x<<10) >= 0`:
     - tri1 = (va,vb,vc) traversed va->vb->vc->va. If all three cross >= 0: fill out with
       (va,vb,vc) x/y/z, return 0.
     - tri2 = (vb,vd,vc) traversed vb->vd->vc->vb (4th vertex vd computed only if tri1 misses). If
       inside: fill out with (vb,vd,vc), return 0.
4. All 16 cells miss -> `check_and_print_grid(D_800CAB9C /* "GetHeightInfo" */, 0xA, 0xB)` then return -1.

The reconstruction (see S280 git history / the reverted diff) is behaviorally faithful and compiles
clean; the divergence is purely codegen (spill pattern), not logic.

## The wall
- 31-instruction deficit: the ROM spills x/z/out to home slots + arg-pointer (`s7=sp+0x68`, read
  x=s7[0], z=s7[4], out=s7[8] per triangle test), spills row_frac to 0x3C, and spills va/vb/vc to
  0x18/0x1C/0x20. My build keeps these in s-registers (8 callee-saved available), so it re-reads far
  less and is shorter. The ROM's allocator ran out of registers (all s0-s7 consumed by
  col_base/row_base/col_frac/col/row/ci/ri + argptr) and spilled the params; mine did not.
- Also (secondary): the ROM's fill uses the SIGNED already-loaded value for out[2] (first x, saved in
  t8) but re-loads out[3..]/y[]/z[] via `lhu` (unsigned) — a per-slot lh-vs-lhu asymmetry driven by
  which vertex fields are still register-resident, i.e. also coloring-dependent.

Class: `#pervasive-regalloc-classical-main` (register-pressure / spill-set coloring), kin to the S280
item-1 wall and the get_terrain_vertex_pointer multi-reg-perm carry (S274). NOT exact-count so the
permuter is off-payoff.

## Re-open lead
Force the ROM's higher register pressure so gcc spills x/z/out to home slots and materializes the arg
pointer: the tell is `s7 = &args` / reading x,z,out via a frame-pointer + row_frac on the stack. A
gcc-2.7.2 local-alloc / reload dive on WHY the ROM spills the three params (does the source keep an
extra long-lived value in a saved reg that mine doesn't? does taking a param's address force it to
memory?) is the crack lead. Reproducing the exact spill SET (params + row_frac + 3 vertex ptrs) is the
whole match; the logic is done.
