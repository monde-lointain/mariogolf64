---
type: "Reconstruction Note"
title: "func_8006C8CC — banked via shared positive-return label"
description: "Records the verified 19-instruction match and the shared-return CFG that defeats jump.c's conditional-assignment conversion."
---

# func_8006C8CC — banked via shared positive-return label

**File:** `src/main/func_8006A2C0.c` (S224 pack). 19i predicate, jal0.
**Status:** BANKED, 19/19 instructions; `tools/verify-rom.sh` succeeds.

## Verified resolution (2026-10-14)

The shared positive-return block, not another spelling of the bit comparison, resolves the carry:

```c
s32 func_8006C8CC(void* state) {
  if (D_800C4010 == 0) goto yes;
  if (D_800C4014 != 0) goto yes;
  if (D_800C4018 >= 0) goto no;
  if (*(u16*)((u8*)state + 4) & 0x8000) goto yes;
no:
  return 0;
yes:
  return 1;
}
```

Fresh baseline measurement produced 16 instructions, score 500, not the historical
18/19 claim. Its `.rtl` retains the mask and branch. The first `.jump` dump replaces
the final conditional return with `gtu`; combine then reduces that value computation
to `lshiftrt 15`. Thus `expr.c:do_store_flag` is not the observed entry point.

In the inspected GCC checkout, `jump.c:1014-1042` requires a conditional branch
followed by a register assignment, with either the branch destination immediately
after that assignment or the assignment's next simple jump targeting the same
destination. The shared `yes` block changes this relationship: the mask branch
targets the positive return, but its false-path assignment/jump reaches the common
epilogue, not `yes`. That prevents this conversion and preserves the ROM's
`lhu; andi; bnez; li 1; move 0` sequence. Multiple incoming edges alone are not the
gate; the branch/assignment/destination relationship is decisive.

An empty `do {} while (0)` between mask and test was byte-identical to baseline.
A shared *normalization* label still collapsed to a value computation (score 575);
it is not interchangeable with the shared positive return.

Reproduction and authority:

- Main bank, ROM offset `0x47CCC`, host `src/main/func_8006A2C0.c`,
  profile `main` from `tools/build_profiles.json`.
- Baseline source/measurement:
  `nonmatchings/func_8006C8CC/measurements/dc134740040f82500f21717a6438e292a1beada526b317c79cbb9928d01b7a4f.{c,json}`.
- Matching source/measurement:
  `nonmatchings/func_8006C8CC/measurements/e20c151cf4c50b72400bfa85489215e14b20d5b0715b28cae07f029326a439c3.{c,json}`.
- `uv run --locked python tools/decomp_loop.py --func func_8006C8CC`:
  score 0, 19/19 matching rows.
- Built the formatted host with `tools/rom_build.py build --object
  src/main/func_8006A2C0.o`; fresh `objdump -drz --disassemble=func_8006C8CC`
  matched the isolated function's instructions, branch offsets, and relocation targets.
- `tools/verify-rom.sh`: **CMake build OK + ROM SHA-1 MATCH**
  (`e2c4e7a905b29529b49a1619a401fe699224829b`).

## Decode (confirmed vs asm)
```c
s32 func_8006C8CC(void* arg0) {
    if (D_800C4010 == 0) return 1;
    if (D_800C4014 != 0) return 1;
    if (D_800C4018 >= 0) return 0;
    if (*(u16*)((u8*)arg0 + 4) & 0x8000) return 1;   // <-- diverges
    return 0;
}
```
Conditions 1-3 (`==0`/`!=0`/`>=0`) match byte-exact: each an early-return branch to the shared exit
`.L8006C910` with the return value in the delay slot (`beqz`/`bnez`/`bgez` + `addiu v0,1`/`move v0,zero`).

## The wall (single row)
```
TARGET                       CURRENT
lhu  v0,4(a0)                lhu  v0,4(a0)
andi v0,v0,0x8000       |    srl  v0,v0,0xf
bnez v0,.L8006C910      <
li   v0,1               <
move v0,zero            <
.L: jr ra                    jr ra
```
The ROM keeps the bit test as a BRANCH (`andi;bnez;li 1(delay);move 0` = 4 instrs). My build folds
`(flag & 0x8000) ? 1 : 0` to `srl v0,v0,0xf` (1 instr). Build is 3 instrs SHORTER -> flowing-bss /
whole-file symbol shift -> full-make SHA miss.

## Historical root-cause hypothesis (superseded)
The final condition is TERMINAL (`if(x) return 1; return 0`), so GCC collapses it to `return (x!=0)`, a
VALUE context, and `do_store_flag` picks the single-bit shift (value known narrow from `lhu`, bit 15 =
MSB -> `>> 15` yields clean 0/1). Conditions 1-3 stay branches only because each has more code after it
(not terminal). The ROM's GCC used `do_jump` (branch) for the same term -> 3 extra instrs = LESS
optimization than my build.

## Forms tried (10, all fold to `lhu;srl 0xf`)
1. flat `if(x)return 1;return 0` (attempt 1)
2. nested `if(D18<0){if(flag)return 1;}return 0`
3. accumulator `ret=0; if(D18<0){if(flag)ret=1;} return ret`
4. explicit two-BB `if(flag){ret=1;}else{ret=0;}` with `else ret=0`
5. OR-chain `if(A||B||(C&&D))return 1;return 0`
6. split `if(C&&D)return 1;return 0`
7. `==0x8000` explicit mask-eq
8. goto `if(D18>=0)goto ret0; if(flag)return 1; ret0: return 0`
9. direct bool `return A||B||(C&&D)`
10. s32-sign `(*(s32*)(a0+4) < 0)` (big-endian high-half sign) -> also `lhu;srl 0xf`

## Historical verdict (refuted by the shared-return CFG)
S231 `#value-select-if-else-vs-branch-likely` store-flag-single-bit class, but for a `(x & bit)` /
sign-bit deciding term (not the doc's `==K` equality select). Source-invariant across 10 forms; permuter
denied (< 0.97, a 3-instr length deficit not a reg permutation). ESCALATION = compiler-source dive
(gcc-2.7.2 `do_store_flag` single-bit shortcut in `expr.c` vs `do_jump` BIT_AND_EXPR path in `jump.c`):
find what makes the ROM's build keep `do_jump` for a terminal single-bit test. Until then: CARRY.
