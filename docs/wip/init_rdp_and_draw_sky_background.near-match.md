# init_rdp_and_draw_sky_background — 286/286, exact instruction multiset, pure order residual

> **Verdict in one line.** Every instruction, every register, every immediate and the `-0xA0` frame
> are exact: a sorted multiset of the 286 encoded words is identical to the ROM's apart from
> unresolved `%hi`/`%lo`/`jal` relocations. The whole residual is the *order* of 56 instructions in
> one window, and it reduces to a single displaced pair — the second `gDPSetScissor`'s `gfx++`
> writeback (`addiu $v0,$a1,0x60` + `sw $v0,0x80($sp)`) lands ~35 instructions early. That
> placement is **LUID order, not a scheduler decision**: it survives `-fno-schedule-insns2`. It
> comes straight from `gDPSetScissor(gfx++, ...)` evaluating `pkt` — and therefore `gfx++` — before
> the `ulx`/`uly` expressions, and the ROM requires the opposite LUID order.

Host: `src/main/func_8002A640.c`. Attempt source: `nonmatchings/init_rdp_and_draw_sky_background/base.c`.
Per-iteration log: `nonmatchings/init_rdp_and_draw_sky_background/STATUS`.

## What is settled, and must not be re-derived

**FP class (priced in iteration 1).** 47 FP mnemonics, but they are the *cheap* class: two
`gDPSetScissor` coordinate packings (`cvt.s.w` / `mul.s` by `4.0f` / `trunc.w.s` / `mfc1` /
`andi 0xFFF` / `sll 12`, four coordinates each) plus a double-precision sky-yaw wrap. The only
`swc1` are the nine that fill the `rotate`/`translate`/`scale` vec3 argument blocks. This is not
the stored-FP-scheduler class and no FP lever was needed — the FP is byte-exact.

**All 26 display-list commands are SDK macros. None is a raw word.** Decoded against
`include/libultra/PR/gbi.h` (F3DEX2):

| off | macro |
| --- | --- |
| 0x00, 0x10, 0x20, 0x28, 0x48, 0x50, 0x70, 0x78, 0x88, 0x98, 0xB8, 0xC8 | `gDPPipeSync` |
| 0x08 | `gDPSetRenderMode(0, 0)` |
| 0x18 | `gDPSetScissor(G_SC_NON_INTERLACE, 0, 0, 320, 240)` |
| 0x30 | `gDPSetCycleType(G_CYC_FILL)` |
| 0x38 | `gDPSetFillColor((GPACK_RGBA5551(0,0,0,1) << 16) \| GPACK_RGBA5551(0,0,0,1))` |
| 0x40 | `gDPFillRectangle(0, 0, 319, 239)` |
| 0x58 | `gDPSetScissor(G_SC_NON_INTERLACE, x, y, x + w, y + h)` from the viewport record |
| 0x60 | `gSPClearGeometryMode(0xFFFFFFFF)` |
| 0x68 | `gSPSetGeometryMode(G_SHADE \| G_CULL_BACK \| G_SHADING_SMOOTH)` |
| 0x80 | `gDPSetCycleType(G_CYC_1CYCLE)` |
| 0x90 | `gDPSetRenderMode(G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2)` |
| 0xA0 | `gDPSetTextureFilter(G_TF_BILERP)` |
| 0xA8 | `gDPSetPrimColor(0, 0, 255, 255, 255, 255)` |
| 0xB0 | `gDPSetCombineMode(G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM)` |
| 0xC0 | `gDPSetTexturePersp(G_TP_PERSP)` |

The render mode (`0x00552048`) and the combiner (`0xFC11FE23` / `0xFFFFF7FB`) were resolved by
*exhaustive* macro expansion, not by guessing: a generated program that evaluates every
`G_RM_*` pair and every valid `G_CC_*` pair against the target words. Both are unique up to the
`MODULATEI`/`MODULATERGB` aliasing. Reproduce with the generator idea in this doc's history if a
sibling emitter ever needs the same lookup — it is a minute of work and it is decisive.

**Local slot order is a hard constraint and it is solved.** gcc 2.7.2 assigns aggregate frame slots
**ascending in declaration order**. The ROM has `mf` at `0x10` (64 bytes), `translate` at `0x50`,
`rotate` at `0x60`, `scale` at `0x70`, so the declaration order is `mf, translate, rotate, scale`.
Getting this backwards (iteration 1) costs more than the slot numbers: with `mf` declared last, cse
keeps `&mf` in `$s0` across both calls and emits `addiu $s0,$sp,N` + two `move $a0,$s0` where the
ROM rematerialises `addiu $a0,$sp,0x10` twice — 287 instructions. Fixing the declaration order
alone took it to 286 and removed the `$s0` web. The banked sibling
`build_pin_or_cup_matrix_for_dad10` in `src/main/func_8003E400.c` **does** keep `&mf` in `$s0` and
matches, so the `move` is not a defect of the idiom — it is a consequence of where `mf` sits.

**Semantics.** `arg1` is a viewport record: `0x1C`/`0x20` origin, `0x24`/`0x28` size, `0x40` a flag
set when the yaw wraps. The yaw advance is `step = M_PI / (f64)D_800B6810`, and the half-turn nudge
is `if (yaw > M_PI_2) if (yaw < step + M_PI_2) yaw += M_PI;` — the second compare re-uses the same
`(f64)yaw` the first produced, which is why the ROM has one `cvt.d.s` feeding two `c.lt.d`.
`D_800CA200/08/10` are `M_PI`, `M_PI_2`, `2*M_PI`. The vertex loop writes `ob[0] = -+1520` on 84
`Vtx` in 42 pairs, based at `&D_800DABA0[D_800B7780 * 16]` — the same frame-stride-16 vertex bank
`emit_sky_dome_dl` draws from.

## The residual, precisely

Byte-exact everywhere except object `0x274`–`0x3d8` (ROM `8002BB98`–`8002BCFC`). Same instructions,
same registers, different order. Reduce it to one fact:

* ROM: after `sw $v1,0x58($a1)` (the scissor's `w0`), the four viewport `lw`, and `addiu $a0,$sp,0x80`,
  it emits **nine consecutive** `addiu $v0,$a1,K` / `sw $v0,0x80($sp)` pairs for `K = 0x60 … 0xA0`,
  then all nine commands' word stores. The group stops at `0xA0` because `sw $v0,0x5C($a1)` (the
  scissor's `w1`) needs `$v0` — a register anti-dependence, and the build hits the same limit.
* Build: `addiu $v0,$a1,0x60` and its writeback are emitted **before** the `w0` FP chain, and the
  remaining eight bumps stay in the one-ahead `bump / w0 / w1 / writeback` rhythm that both the ROM
  and the build use for every other command in the function.

## The gate: LUID order, and it is not reachable from `gDPSetScissor(gfx++, …)`

`schedule_block` is bottom-up (`sched.c:3747-3749`, `last_scheduled_insn = insn = ready[0];` with
"The first insn scheduled becomes the new tail"). `rank_for_schedule` (`sched.c:2385`) ranks by
`INSN_PRIORITY` first (`sched.c:2394`), then by a three-way class against `last_scheduled_insn`
(`sched.c:2400`: data-dependent / anti-output-dependent / independent-or-latency-1), and **falls
back to `INSN_LUID`, the original insn order** (`sched.c:2425`). `schedule_select` then applies the
potential-hazard swap that the dump prints as `insn N has a greater potential hazard`
(`sched.c:2616`, message at `sched.c:2686`).

The decisive measurement: **compile with `-fno-schedule-insns2` and the displacement is already
there.** The pre-sched2 order is `addiu $v0,$a1,96` … `lwc1 $f2,28($s2)` … `sw $v1,88($a1)`. So the
scheduler is not what puts the bump early — it merely preserves LUID order, exactly as
`sched.c:2425` says it will when priorities and classes tie. The LUID order is fixed by the macro:
`gDPSetScissor` opens with `Gfx *_g = (Gfx *)pkt;`, so `gfx++` is evaluated and its writeback
emitted **before** the `ulx`/`uly` expressions. The ROM needs the reverse. **No spelling of
`gDPSetScissor(gfx++, …)` can produce it**, and that is the wall.

`INSN_PRIORITY` cannot be leaned on instead: in the `-dR` dump the whole window sits at priority 1
or 3, so `rank_for_schedule` never gets past the LUID tiebreak here.

## Measured nulls — do not re-run these

| # | attempt | result |
| --- | --- | --- |
| 1 | `gDPSetScissor(gfx, …); gfx++;` (move the bump after the word stores) | **284/286.** The writeback dies: with the bump after the stores, the next macro's read of `gfx` is cse-folded to a known `$a1 + K`, so `mem[sp+0x80] = $a1+0x60` has no reader before it is overwritten and DSE removes the `addiu`+`sw`. The one form that would fix the LUID order is the one form that deletes the instruction it needs to move. |
| 2 | reserve the slot: `scissor = gfx++;` … nine commands … `gDPSetScissor(scissor, …)` | 282/286, frame `-0x98`. |
| 3 | hoist `lrx`/`lry` into `s32` temps before the macro | 288/286. |
| 4 | `*(s32*)(arg1 + 0x..)` instead of a `u8* view` temp | identical output; no effect. |
| 5 | decompose permuter, `-j8`, ~40 min, `--stop-on-zero` (workspace `nonmatchings/init_rdp_and_draw_sky_background-2`) | plateaued at 1505 with one output, no zero. As predicted by `docs/levers.md`: the permuter has no move that reorders two statements *inside* a macro expansion, which is the only thing that would help. |

## What would bank it

A source form in which the second scissor's `w0` coordinate expression is emitted *before* the
`gfx` writeback, while the writeback still has a reader and so survives DSE. Everything reachable
from the stock `gDPSetScissor`/`gDPSetScissorFrac` macros has been tried (nulls 1–3). The
remaining candidates, none of them reachable without new information, are: a game-local scissor
wrapper macro that takes `Gfx**` or that computes the two words into locals before touching the
pointer; or evidence from a sibling emitter in another host that this codebase spells scissor
emission differently from the SDK macro. **Do not re-open this on size order.** It is one displaced
pair in an otherwise exact 286-instruction body, and every gate above is enumerated and refuted.

## Extern collisions with the host

`src/main/func_8002A640.c` already declares `extern s32 D_800B6810;` (line 9) — **same type**, so
the attempt's declaration is redundant, not conflicting; drop it at integration. Nothing else
collides: `sky_rotation_angle`, `D_800B7780`, `D_800DABA0`, `D_800DABB0`, `mtx_from_rts`,
`convert_and_pack_floats_to_fixed` and `M_PI_2` appear nowhere in the host, and `emit_sky_dome_dl`
appears only as its `INCLUDE_ASM` stub (line 69), so an `extern void emit_sky_dome_dl(Gfx**);` is
required and safe. The host's forward declaration at line 38 fixes the signature as
`(Gfx**, s32)`; the attempt keeps it and casts `arg1` to `u8*` internally rather than retyping the
parameter, because `setup_view_by_camera_mode` is already banked against that prototype.
