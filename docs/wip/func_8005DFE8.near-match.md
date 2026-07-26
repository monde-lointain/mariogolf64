# func_8005DFE8 — CARRIED (SRAM load/verify; near-match, S224-class allocno + reorg residual)

Size 0x198 (102 instr), jal=6, fp=0. `src/main/func_80059BA0.c` pack. SRAM LOAD/verify counterpart to
the S265-banked `func_8005E180` (SRAM SAVE); shares the `D_800C2BE0/BE4/BE8/BEC` bank-address globals
(0x10-strided, 2 banks) + `crc16_ccitt` + `nuPiReadWriteSram`. Characterized S266. Near-match C in
`nonmatchings/func_8005DFE8/base.c` (compiles, 102/102 instructions, correct structure).

## Fully decoded behaviour (re-derived from .s per S258 DoR)
```c
s32 func_8005DFE8(u8* buf, s32 flag_arg) {
  s32 status6[2];              // s6 = sp+0xE0  (func_80028204 out header)
  u8 raw[0xD0];               // aligned scratch for status3
  s32* status3 = (s32*)(((u32)raw + 0xF) & ~0xF);   // s3 = sp+0xA0 (16-aligned)
  s32 off=0, tries=0, magicBad=0, size=0x2A78, flag=flag_arg&0xFF;
  do {
    if (flag == 0) {                                  // controller-pak read path (func_80028204)
      if (func_80028204(*(char**)((u8*)&D_800C2BE0+off), status6) != 0) { tries++; goto loopend; }
      if (status6[0] != size)                        { tries++; goto loopend; }   // size held in fp
      if (func_80028204(*(char**)((u8*)&D_800C2BE4+off), status6) != 0) { tries++; goto loopend; }
      if (status6[0] != 4)                           { tries++; goto loopend; }
    } else {                                          // SRAM read path (nuPiReadWriteSram dir=0)
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BE8+off), buf,     0x2A78, 0);
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BEC+off), status3, 4,      0);
    }
    if (*(s32*)buf != 0x12345678) { magicBad++; tries++; goto loopend; }   // magic check
    { u16* h = (u16*)status3;
      u16 x = h[1] ^ h[0];                            // stored_crc ^ status word (xor in crc's delay slot)
      u16 crc = crc16_ccitt(buf, 0x2A78);
      if ((x & 0xFFFF) == (crc & 0xFFFF)) break;      // success -> exit loop
      tries++; }
  loopend:
    off += 0x10;
  } while (tries < 2);                                // 2 banks
  if (tries == 0) return 1;                           // bank 0 ok
  if (tries == 1) return 1;                           // bank 1 ok  (keep TWO == tests, not `<2`)
  func_8005AF80();                                    // both failed
  return (magicBad != 2) ? -1 : -2;                   // -2 if both banks had bad magic
}
```
`tries` == index of the successful bank (0/1), or 2 if all failed. `magicBad` (s5) counts bad-magic banks.

## CRACKED levers (all in base.c; these are SOLVED, keep them)
- **`s32 size = 0x2A78` used ONLY in the `status6[0] != size` in-loop compare, LITERAL `0x2A78` for the
  nuPi/crc call args.** This hoists 0x2A78 into a callee-saved reg for the compare (the ROM holds it in
  `fp`) while the call args `li` it fresh — matching the ROM exactly. Using `size` for the args too
  emits `move a2,size` where the ROM has `li a2,0x2a78`.
- **Declare `size` BEFORE `flag`** -> flag=s7, size=s8/fp (matches ROM). Reverse order swaps them.
- **`raw[0xD0]`** (not 0xC0): places status6 at sp+0xE0 and frame at 0x110, matching the ROM stack
  exactly (linear: raw 0xC8->s6 0xD8/frame 0x108; 0xD0->0xE0/0x110; 0xE0->0xF0/0x120).
- **Two separate `if(tries==0)return 1; if(tries==1)return 1;`** (NOT `if(tries==0||tries==1)`): defeats
  the `docs/levers.md` (gcc272 fold range test slti merge) `<2` fold; the ROM keeps two equality tests. This also
  brought the instruction count from 98 to 102 (exact).

## Residual (why carried) — ~10 rows, S224-class, NOT source-steerable here
1. **s1<->s2 allocno role.** ROM: off=s1, tries=s2. base.c: off=s2, tries=s1. The
   `docs/levers.md` (global allocno compare livelength biv order) priority (floor_log2(nref)*nref/live_length) puts
   `tries` (more refs, longer live to the tail) at higher priority -> s1. Declaration-order swaps
   (off-first, tries-first) do NOT flip it (verified both = 24 rows). Would need an nref/live-length
   nudge that has no faithful source form.
2. **4th check `bnel v1,v0`+`s2++` vs base.c `beq v1,v0,verify`.** The `status6[0] != 4` check (last in
   the flag==0 block, before the shared magic-check merge): the ROM keeps the fail-path `bnel`+explicit
   `j verify`; gcc folds base.c's to the inverted `beq ==4 -> verify`. reorg/layout coin.
3. **Prologue init/save order** (`andi s7` vs `move s5` first) + tail return-value materialization
   (`li v0,2` placement). Scheduling/reorg coins.

Permuter is BLIND here: the isolated compile scores 0.1 (the `D_800C2BE0..EC` + callee %hi/%lo relocs
pervade), so asm-differ can't see the ~10-row in-tree near-match through the reloc noise (kin to
`docs/levers.md` (permuter blind to internal branch target) / the CF78 jtbl isolation). In-tree `diff.py` is the only
truth; ~10 rows, all register-role/reorg coins at exact instruction count.

Route: a dedicated allocno-crack slice (try the `docs/levers.md` (global allocno compare livelength biv order) 8th-ref
/ live-length nudge on off vs tries) OR corpus-mining. NOT a fresh smallest-first leaf. base.c is the
warm start (only s1/s2 + 3 reorg coins from byte-exact).
