# find_keyframe_offset_by_tag near-match (base-register caching + regalloc perm)

src/main/func_80054900.c. Keyframe-by-tag search. Logic 100% decoded; residual is
`#base-register-vs-displacement` + a callee-saved coloring permutation.

## Logic
```c
typedef struct { s16 val; u8 tag; u8 pad; } KfEntry;      // 4B stride
typedef struct { KfEntry *entries; s16 base_offset; } KfTrack;  // 12B stride
s32 find_keyframe_offset_by_tag(s32 id, s32 arg1, s32 arg2) {
    u8 *cs = get_character_state(id);
    s32 result = -1;
    if (cs != NULL && arg1 < *(s32 *)(cs + 0x1C)) {
        s32 off = arg1 * 12;   // base = (cs->0)->0x14; entries = *(base+off); base_off = *(s16*)(base+off+4)
        KfEntry *e = *(KfEntry **)(*(u8 **)(*(u8 **)cs + 0x14) + off);
        if (e != NULL && e->val != -1) {
            for (;;) {
                if (e->tag == arg2) { result = e->val - *(s16*)(*(u8**)(*(u8**)cs+0x14)+off+4); break; }
                e++;
                if (e->val == -1) break;
            }
        }
    }
    return result;
}
```
NOTE: reconciled the decl `find_keyframe_offset_by_tag(s32 id, s32 arg1, s32 arg2)`
(was `void *base` -- arg0 is the get_character_state id) + func_800550B8 `s32 id`.

## Residual
TARGET holds arg1->s0, result->s1, arg2->s2 (callee-saved, cross the get_character_state
call), keeps `arg1*12` in a1 across the loop, and RE-DERIVES base `(cs->0)->0x14` at the
found-branch base_offset access. BUILD (both struct-array `tracks[arg1]` and raw-inline
forms) caches the base pointer in a caller-saved reg (a3/t0) -> `lh v0,4(a3)` and rotates
the whole allocation onto a-regs. `#base-register-vs-displacement` (no reliable source
lever, S210). Escalation: corpus-mining the KMC-2.7.2 search-loop shape, or the permuter.

## SIBLING: collect_keyframe_events_at (same wall)
Same keyframe list-walk (`e=*(base+arg1*12); walk e+=4 while e->val!=-1`), collecting
(e->tag, e[3]) pairs where `e->val==arg2 && count<0x10` into an out buffer, returns count.
Nested-if gets the two skip branches, but BUILD emits plain `bne`/`beqz`+nop where TARGET
uses branch-LIKELY `bnel`/`beql` annulling `e+=4` into the delay slots (reorg replicates
the increment), + `move v1,a0` walking-pointer copy (BUILD walks e in a0 directly).
Same `#base-register-vs-displacement`/`#indexed-vs-pointer-loop-strength-reduction` class.
