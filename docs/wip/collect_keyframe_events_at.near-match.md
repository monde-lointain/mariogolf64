# collect_keyframe_events_at near-match (S260) — 54/54, ONE BRANCH-OFFSET BIT

`src/main/func_80054900.c`. The S213 verdict (`#base-register-vs-displacement` +
`#indexed-vs-pointer-loop-strength-reduction`: "loop.c strength_reduce builds a 2nd giv for the
tag/pad byte loads, splitting `e+=4` into two pointer bumps -> defeats reorg optimize_skip") is
RETIRED. The goto-loop lever removes loop.c from the picture entirely, and the whole body then
reproduces byte-for-byte: same registers (count=a1, sentinel=a2, q=v1, p=a3, e=a0, track=s0, out=s1,
want=s2), same frame 0x20 with 3 saved registers, both annulled skips (`bnel` + `beql` with `q++` in
the delay slot), same epilogue split across `.FD8`/`.FDC`.

## Reproduction (54/54)

See the in-file comment above the `INCLUDE_ASM` stub for the exact body. Levers, in the order they
mattered:

1. **goto loop + out-of-line `advance` handler.** Both skips `goto advance`, where `advance:` is the
   single `q++` placed after the collect block; reorg then steals `q++` into both annulled delay
   slots. Same lever as `find_keyframe_offset_by_tag` (banked this sprint) and `func_8006DF84`.
2. **`count = 0` AFTER the call.** Initialising it in the declaration makes its live range cross the
   `get_character_state` call, which promotes it to a 4th callee-saved register and grows the frame
   to 0x28.
3. **Literal `-1` for the pre-loop check, a local `sentinel` for the loop.** The ROM materialises -1
   twice (v0 then a2); a goto loop de-hoists a literal, so the loop needs its own variable.
4. **`q = e` walking copy.** Reproduces the ROM's `move v1,a0`. It only survives coalescing when the
   loop-top and loop-bottom loads go to separate locals (`v` / `w`) — with one shared local gcc
   coalesces `q` into `e` and the copy disappears.
5. **`p = out` walking copy**, and the `(u32)` int-add spelling for `off + tracks` (puts the offset
   in `rs`).

## Residual (the only diff)

The loop back edge. ROM `bne v0,a2,0x30398` (0xfff1) targets the loop-top `lh v0,0(v1)`; the build
emits `bne v0,a2,+0xfff2`, one instruction later, skipping that re-load — the bottom sentinel load
left the same value in the same register, so gcc redirects the edge over the redundant load. Both
loads are emitted; only the offset differs. Every other byte matches.

## Tried

- raw `*(s16*)q` on the top, on the bottom, and on both: no effect (whatever pass redirects the edge
  compares the post-allocation insns, and MEM_IN_STRUCT_P is not part of that identity).
- separate `v` / `w` locals for the two loads: needed for lever 4, but they still colour to `v0`.
- reading the sentinel as `q[1].val` BEFORE the increment: this DOES fix the back edge (the two loads
  become `lh v0,0(v1)` and `lh v0,4(v1)`), but the `advance` handler stops being a single
  instruction, so both annulled delay slots are lost and the loop comes out shorter.

## Escalation

The two loads must differ AFTER register allocation. A permuter run is the natural next step (the
instruction count is exact and the residual is one branch offset), or a compiler-source dive into
which `jump_optimize` pass redirects a conditional back edge over an insn identical to the one
preceding it.
