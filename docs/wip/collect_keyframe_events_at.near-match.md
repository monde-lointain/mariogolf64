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

## S261 — ROOT-CAUSED to a gcc first-load PEEL; permuter-BLIND; terminal for source

The residual is NOT an assembler artifact and NOT permuter-reachable. Three findings:

1. **It is a gcc first-load peel, visible in `gcc -S`.** gcc places the loop label `.L14:` AFTER the
   top `lh $2,0($3)` (the `v = q->val` load), so the back edge `bne $2,$6,.L14` re-enters at the
   `bnel` and never re-executes the top load. On iterations 2+, gcc reuses the value the BOTTOM load
   (`.L13: lh $2,0($3)`, the `w = q->val` sentinel read) left in `$2` — the two reads are the same
   field `q->val` at offset 0 with `q` unchanged across the back edge, so gcc treats the top load as
   redundant and peels it to the first iteration. Both assemblers (KMC `tools/cc/as` and modern GAS)
   assemble this faithfully to `fff2`; the peel is entirely in gcc's output. The ROM's gcc did the
   opposite (label before the top load, `fff1`, re-loads each iteration) — a pass-ordering coin, same
   source either way.

2. **Why find_keyframe_offset_by_tag matched and this does not.** find_keyframe's loop tests
   `e->tag` (offset 2) at the top and `e->val` (offset 0) at the bottom — DIFFERENT fields, so there
   is no redundant load to peel. collect_keyframe inherently tests `q->val` at BOTH the match-test
   (top, `== want`) and the end-sentinel-test (bottom, `== -1`); both are `val`, so the field-split
   that saved find_keyframe is unavailable here without changing semantics.

3. **The permuter cannot see this residual — asm-differ is blind to internal branch TARGETS.**
   Importing the exact 54/54 base.c and running the permuter reports `base score = 0` and "Found zero
   score!", yet the real object (`objdump`) is `fff2` and the ROM is `fff1`. The permuter's target.o
   IS correct (`fff1`); asm-differ normalises a branch to a local label and does not distinguish
   `bne …,<label@0x7c>` from `bne …,<label@0x80>`, so it scores the one differing bit as matched.
   This is the SAME blind spot S260 fixed in `cmpfn.sh` (branch-target normalisation). So the
   permuter is not just "unlikely" here, it is structurally incapable of scoring the defect, and any
   permuter "0" on a pure-internal-branch-target residual is a false positive — gate on
   `tools/verify-rom.sh`/`objdump`, never on the permuter score.

Source levers tried and rejected (extends the S260 list): `volatile s16` top load — DEFEATS the peel
(the back edge re-enters at the top load, `fff1`) but forces the top read to `lhu` (the ROM has `lh`)
and grows the loop by disturbing scheduling. The peek-before-increment (`q[1].val`) still costs the
annulled delay slots (S260). No source form yields {top load kept as a plain `lh` + q++ as the single
foldable advance + label before the top load} together.

**Verdict: TERMINAL for source (a gcc first-load-peel coin), and permuter-unreachable (asm-differ
blind to the internal branch target).** Carry the 54/54 body.
