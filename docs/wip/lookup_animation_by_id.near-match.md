# lookup_animation_by_id (0x80055FFC) near-match — loop-optimizer/reorg BB-layout wall

S211. `src/main/func_80054900.c` animation pack. Linear search over a list of
0xC-byte entries: return the index of the first entry whose byte at +0x8 == the
target id; list terminates when entry->0x0 == 0; returns 0 if cs null / not found.

## Root cause FIXED (frame/regalloc) — reusable lever

Initial seed put `s32 result = 0;` BEFORE the `get_character_state()` call, so
`result` was live ACROSS the call -> GCC pinned it to a callee-saved reg (`s1`)
-> extra saved reg -> frame `0x20` vs the target's `0x18`, cascading every
downstream regalloc + forcing `beqzl`. **Fix: init `result = 0` AFTER the call**
(the target sets `a1=0` in the null-check `beqz` delay slot). result then lives
only post-call -> caller-saved `a1`, frame `0x18`, prologue/epilogue/setup all
byte-match. GENERAL LEVER for the get_character_state family (and any
`default-then-conditionally-overwrite` return): a sentinel/default that does not
need to survive a call must be assigned AFTER the call to stay caller-saved.

## Residual WALL (the loop only)

Target loop (byte ground truth):
```
     move a0,zero            ; idx=0  (before loop, in the list-empty beqz delay)
L2C: lbu  v0,8(v1)
     bnel v0,s0,L40          ; branch-LIKELY: if != , advance; annul e+=0xC if ==
     addiu v1,v1,0xc         ;   delay (annulled on ==)
     j    L4C                ; == found
     move a1,a0              ;   delay: result=idx
L40: lw   v0,0(v1)
     bnez v0,L2C             ; conditional BACK-EDGE
     addiu a0,a0,1           ;   delay: idx++
```
Target = un-rotated head (compare at loop top, `idx=0` hoisted before) + a
conditional branch-likely back-edge (`bnez`), with the found block INLINE
between compare and advance.

Forms tried (all with the frame fix in place), NONE match the loop:
- `do{ if(==)break; e+=0xC; idx++; }while(e->0!=0)` — ROTATES (peels first
  compare, `idx=0` lands inside, `move v0,a1` return-materialization artifacts);
  DOES get a `bnel` back-edge.
- `for(;;){...}` and `while(e->8!=target){...}`+goto — same rotation as do-while.
- goto-plain (`loop: if(==){result=idx;goto done;} e+=0xC; if(e->0==0)goto done;
  idx++; goto loop;`) — head CORRECT (no rotation, `idx=0` before loop) but the
  back-edge is `beqz v0,END; idx++; j loop; nop` (unconditional j, no
  branch-likely) because the exit is the conditional and the loop-back is
  unconditional.
- goto-inverted (`if(!=)goto check_end; ...`) — GCC jump-opt canonicalizes it
  right back to the goto-plain shape.

No single C loop idiom yields BOTH the un-rotated head (goto-only) AND the
conditional branch-likely back-edge (do-while-only). Structured loops emit
NOTE_INSN_LOOP_BEG -> loop.c rotation; gotos avoid rotation but reorg fills the
unconditional back-jump with nop instead of an annulled conditional. Score stuck
1340 (down from 2588 seed / 1745 after frame fix). Class:
`#goto-loop-vs-structured-loop-codegen` / `#top-tested-loop-goto-local-hoist`.

## Escalation
Corpus-mine a banked KMC-2.7.2 sibling with this exact search-loop shape
(bnel-compare + bnez-back-edge + inline-found), or the permuter once seeded past
the frame fix (structural, so uncertain). Do NOT use explicit register alloc.
Best near-match C (goto-plain, head-correct) preserved below.

```c
s32 lookup_animation_by_id(s32 id, s32 target) {
    u8 *cs = get_character_state(id);
    s32 result = 0;
    if (cs != NULL) {
        u8 *e = *(u8 **)(*(u8 **)cs + 0x14);
        if (*(s32 *)e != 0) {
            s32 idx = 0;
        loop:
            if (*(u8 *)(e + 0x8) == target) {
                result = idx;
                goto done;
            }
            e += 0xC;
            if (*(s32 *)e == 0) {
                goto done;
            }
            idx++;
            goto loop;
        }
    }
done:
    return result;
}
```
