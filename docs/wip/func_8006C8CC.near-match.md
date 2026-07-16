# func_8006C8CC near-match (CARRY, S239)

**File:** `src/main/func_8006A2C0.c` (S224 pack). 19i predicate, jal0.
**Status:** 18/19 instrs byte-exact; TERMINAL store-flag-single-bit fold on the last condition.

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

## Root cause
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

## Verdict
S231 `#value-select-if-else-vs-branch-likely` store-flag-single-bit class, but for a `(x & bit)` /
sign-bit deciding term (not the doc's `==K` equality select). Source-invariant across 10 forms; permuter
denied (< 0.97, a 3-instr length deficit not a reg permutation). ESCALATION = compiler-source dive
(gcc-2.7.2 `do_store_flag` single-bit shortcut in `expr.c` vs `do_jump` BIT_AND_EXPR path in `jump.c`):
find what makes the ROM's build keep `do_jump` for a terminal single-bit test. Until then: CARRY.
