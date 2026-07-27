# func_8005DFE8 — SRAM load/verify, CARRIED (S292 re-open; 102/102, 36 diff rows)

`src/main/func_80059BA0.c`, 0x198 / 102 instructions, jal=6, fp=0. Counterpart to the S265-banked
`func_8005E180` (SRAM save); shares the `D_800C2BE0/BE4/BE8/BEC` bank-address globals (0x10-strided,
2 banks) plus `crc16_ccitt` and `nuPiReadWriteSram`. Characterized S266, advanced S292.

**Read this first.** The S266 version of this doc was wrong in three ways, each of which cost S292
time: it claimed a compiling 102/102 body (the fresh `--refresh-residual` build read **104**, of which
2 rows were the isolated object's trailing padding nops, so the true count was 102 all along and
neither number was the doc's); it stated the residual as an `s1<->s2` allocno role swap, which is real
but is a *consequence* of a source-structure error rather than a colouring problem; and it had the
return values wrong. Re-derive before trusting anything here, including what follows.

## Semantics corrected (S292)

The tail returns **1 for bank 0 and 2 for bank 1**, not 1 for both:

```
.L8005E124:
  beqz  $s2, .L8005E150      # tries == 0
   addiu $v0, $zero, 0x1     # delay slot: return value 1
  beq   $s2, $v0, .L8005E150 # tries == 1  (reuses the 1 just materialised)
   addiu $v0, $zero, 0x2     # delay slot: return value 2
```

The `li $v0,1` does double duty as the first return value and as the compare operand of the second
test, and the second delay slot carries the second return value. `return 1` twice leaves that slot a
`nop` and cannot match.

## The structural error S266 recorded as a register swap

The ROM increments `tries` in **four annulled `bnel` delay slots** plus one standalone insn at
`.L8005E114`. That is *one* source-level `tries++` at a `fail:` label, which `reorg` copies into each
branch-likely slot and redirects the branch past (`#out-of-line-handler-block-branch-likely`).
Writing `tries++; goto loopend;` at each of the seven failure sites — what the S266 body did — gives
`tries` 29 `REG_N_REFS` against `off`'s 13, so `tries` wins `$s1` and `off` takes `$s2`. That is the
"allocno role swap", and no allocno knob fixes it; the single-increment source shape does, and it
fixes the branch forms at the same time. 76 diff rows to 36.

## Current body (102/102, 36 diff rows, frame `-0x110` matches)

```c
extern u32 D_800C2BE0;
extern u32 D_800C2BE4;
extern u32 D_800C2BE8;
extern u32 D_800C2BEC;
extern s32 func_80028204(char* str, s32* out);

s32 func_8005DFE8(u8* buf, s32 flag_arg) {
  u8 raw[0xD0];
  s32 status6[2];
  s32* status3 = (s32*)(((u32)raw + 0xF) & ~0xF);
  s32 magicBad = 0;
  s32 tries = 0;
  s32 flag = flag_arg & 0xFF;
  s32 off = 0;

  do {
    if (flag == 0) {
      if (func_80028204(*(char**)((u8*)&D_800C2BE0 + off), status6) != 0) {
        goto fail;
      }
      if (status6[0] != 0x2A78) {
        goto fail;
      }
      if (func_80028204(*(char**)((u8*)&D_800C2BE4 + off), status6) != 0) {
        goto fail;
      }
      if (status6[0] != 4) {
        goto fail;
      }
    } else {
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BE8 + off), buf, 0x2A78, 0);
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BEC + off), status3, 4, 0);
    }
    if (*(s32*)buf != 0x12345678) {
      magicBad++;
      goto fail;
    }
    {
      u16* s3h = (u16*)status3;
      u16 x = s3h[1] ^ s3h[0];
      u16 crc = crc16_ccitt(buf, 0x2A78);
      if ((x & 0xFFFF) == (crc & 0xFFFF)) {
        break;
      }
    }
  fail:
    tries++;
    off += 0x10;
  } while (tries < 2);

  if (tries == 0) {
    return 1;
  }
  if (tries == 1) {
    return 2;
  }
  func_8005AF80();
  return (magicBad == 2) ? -2 : -1;
}
```

Solved levers, all of which must be kept:

1. **One `tries++` at `fail:`; every failure site is a bare `goto fail;`.**
2. **`raw[0xD0]`** for the exact stack layout (`status3` at `sp+0xA0`, `status6` at `sp+0xE0`, frame
   `-0x110`).
3. **Literal `0x2A78` in the compare, never a `size` variable.** Every source-variable spelling of
   that constant makes `move_movables` also hoist the `0x12345678` magic into a callee-saved
   register, which evicts the size constant and costs about 90 rows. Measured over four spellings.
4. **Two separate `if (tries == 0) return 1; if (tries == 1) return 2;`** — defeats the
   `fold_range_test` `< 2` merge, and `return 2` fills the second delay slot.
5. **`return (magicBad == 2) ? -2 : -1;`**, not the `!= 2 ? -1 : -2` spelling. Same value-select
   sequence, but the `==` form emits `li $v1,-2` *after* `xori`/`sltu`/`negu` as the ROM does, where
   the `!=` form front-loads it into the post-call slot. That change also shortened `magicBad`'s live
   range by one, which flipped `magicBad`/`&status6` from `$s6`/`$s5` to the ROM's `$s5`/`$s6` — a
   2142-vs-2173 margin in `allocno_compare` terms.

## Residual: two clusters, both measured with `tools/allocno_report.py`

**(a) Which hoisted constant keeps `$fp`.** `loop.c` hoists both compare constants (`0x2A78` at check
2, `4` at check 4); only one can hold the last callee-saved register and reload rematerialises the
other inline. The ROM keeps `0x2A78` (`bnel $v0,$fp`) and rematerialises the 4 (`li $v0,4; bnel`);
this build does the reverse. The two allocnos are **340 vs 333** — refs 3 each, live lengths 88 and
90. That 2-unit gap is the preheader distance between the two hoists, and hoist order is loop-body
order, so the constant used *first* hoists *first*, always has the longer range, and always loses.
Matching the ROM needs the later-used constant hoisted first, which no faithful ordering of the four
checks provides.

**(b) Where `off = 0` is emitted.** ROM: `[magicBad, tries, flag] [&status6, const] [off]`. Mine:
`[magicBad, tries, flag, off] [&status6, const]`. Hoisted movables are appended to the preheader, so a
source-level `off = 0` can never land after them — meaning the ROM's `move $s1,$zero` is
compiler-generated (a `loop.c` induction-variable init), not a source statement.

Probes that changed nothing, all measured: `off = 0 * flag_arg`; `off` assigned as its own statement
immediately before the loop; `off` declared first or last; and a `do {} while (0)` reweight around
either constant's compare (101 and 42 rows, both worse). Dropping `off` for the giv spelling
`tries * 0x10` gives 106 instructions and frame `-0x118`: gcc emits a `sll` per access rather than
building the giv.

## Permuter

66k iterations, base 185, best 110, no zero. Its best candidate deletes `off`'s initialiser outright
(leaving a dead `off = 0;` after a `break`), which scores better only because a missing init beats a
misplaced one. Read as a knob it confirms cluster (b) is the dominant residual and offers no
legitimate fix. (The S266 claim that the permuter is blind here because of reloc noise did not
reproduce: it scores the function fine.)

## Verdict

CARRY, not terminal. The body is at exact count with corrected semantics, correct frame, correct
branch-likely forms and correct callee-saved roles, and both residual clusters are named with the gcc
pass responsible (`loop.c move_movables` hoist order; `loop.c` preheader placement).

Next attempt: the four address sites are `%hi`-relative byte offsets off four *different* symbols,
which is what defeats giv formation. Test whether `D_800C2BE0`/`BE4`/`BE8`/`BEC` are really one
0x10-strided array indexable as `arr[bank]` — that is the one source shape that would make gcc build
the induction variable the ROM has, and it addresses cluster (b) directly.
