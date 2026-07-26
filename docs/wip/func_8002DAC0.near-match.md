# func_8002DAC0 - GCC nested function of render_frame (carry, not a wall)

**Verdict: CARRY, coupled.** Banks as a nested function INSIDE its decompiled parent `render_frame`
(still asm, 14400B, the pack's largest fn). Per `docs/levers.md` (nested function banks the parent too) /
`docs/hazards.md#nested-function-static-chain-spill`. Same class as [[func_8002BE78]] (this pack's
other nested child, parent draw_ground_shadow_decals).

## Evidence
- Arg in `$v0`, not `$a0`: prologue `addu $s1,$v0,zero; sw $v0,0x10($sp)` (dead spill of the static
  chain; `$v0` = STATIC_CHAIN_REGNUM = `$2`).
- Single caller `render_frame @ 8002DC10` (parent immediately follows the child at 0x8002DAC0 <
  0x8002DC10 = gcc nested-child-first ordering).
- Body reads parent fields through `$s1` (the chain): `s1->0` (guard), `s1->4` (a matrix base at
  +0x180 / +0x140), builds a translate*view matrix (guTranslateF, guMtxCatF, func_80065D5C), 0xE8
  frame for the Mtx scratch.

## Re-open when
`render_frame` is decompiled. Write as a nested `static`/GCC-nested fn lexically inside render_frame
so gcc homes the static chain in `$v0`; bank both together.
