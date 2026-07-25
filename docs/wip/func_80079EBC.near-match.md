# func_80079EBC — shared-literal-pool partial-bank carry (S279)

## Verdict
Body is fully RE'd and BYTE-EXACT (S279 gccB, 148/148 in isolation). NOT bankable in the current
partial file: it needs the TU's shared FP literal pool, still owned by still-asm siblings.

## Byte-exact body (isolation-proven, literal form)
```c
extern Particle particle_array[40];
extern s32 D_800FBE64[3];   /* pool of 3 s32: x, y(+15360), z; scalar decl at file scope covers [0] */

void func_80079EBC(void) {
  s32 i;
  s32 count;
  Particle* p;

  count = 0;
  for (i = 0; i != 40; i++) {          /* i != 40 (not i < 40) -> li v0,0x28; bne */
    p = &particle_array[i];
    if (p->unk_3A == -1) {
      p->unk_00 = (f32)D_800FBE64[0] * (1.0f / 1024.0f);
      p->unk_04 = ((f32)D_800FBE64[1] - 15360.0f) * (1.0f / 1024.0f);
      p->unk_08 = (f32)D_800FBE64[2] * (1.0f / 1024.0f);
      p->unk_0C = 0;
      p->unk_10 =
          ((f32)count * 0.3f + 0.1f + (f32)(guRandom() % 10) * 0.01f) * -1.5f;
      p->unk_14 = 0;
      p->unk_28 = 255.0f;
      p->unk_2C = -1.0f;
      p->unk_30 = (f32)(guRandom() % 30) * 0.1f + 1.0f;
      p->unk_34 = (f32)(guRandom() % 10) * 0.001f + 0.04;   /* 0.04 = D_800D19E0 pool double */
      p->unk_3A = 0x10;
      p->unk_3B = 0;
      count++;
      if (count == 3) {
        break;
      }
    }
  }
}
```
Integration deltas when banked: `extern s32 D_800FBE64;` -> `[3]` and reload_scene_assets use `[0]`
(codegen-neutral, gccB `-S`-verified). `0.1f` const is held in one reg reused for both `+0.1f` and
`*0.1f` — matches naturally.

## Why it does not bank in a partial file (the blocker)
The `0.04` at the tail is a gcc FP literal-pool constant (`D_800D19E0`, `asm/data/ACD60.rodata.s`).
That pool is SHARED: `D_800D19E8` (0.2), `D_800D19F0` (0.049...), and `jtbl_800D1990` are all still
referenced by asm/53D10.s (still-asm siblings in this TU). Consequences:
- **Literal form** (`+ 0.04`): gcc appends a NEW 0.04 pool word to func_80078910.o's rodata while the
  extracted `D_800D19E0` still exists -> duplicate -> the whole following .rodata/.data/.bss flows
  +0x10 (16-byte-aligned double). Full-make SHA-1 miss on every later data symbol (particle_array,
  D_800FBE64 seen +0x10). Confirmed empirically.
- **Extern form** (`+ D_800D19E0`, plain or `const`): no duplicate, no shift, .text otherwise
  byte-identical — BUT a source-invariant sched.c coin flips: the `count++` (`addiu $s2,$s2,1`)
  hoists ONE slot ahead of the `ldc1 %lo(D_800D19E0)`, cascading the unk_3A/unk_3B/li reorder
  (22 bytes at 0x554A4-0x554C8). Driver = `CONST_DOUBLE` (unchanging, cheap, scheduled early so the
  addiu stays behind it) vs extern `MEM` (load cost, scheduled later so the ready addiu fills the gap).
  `extern const f64` did NOT restore it (gcc-2.7.2 alias flag insufficient); `++count` preinc did not
  move it. Permuter is the NON-payoff shape here (single-instruction schedule coin).

## Bank condition
Banks cleanly once the pool-owning still-asm siblings (the fns in asm/53D10.s using D_800D19E8 /
D_800D19F0 / jtbl_800D1990) are also C, so the WHOLE TU literal pool is C-emitted in one unit and the
literal form (which reproduces the ROM schedule exactly) no longer duplicates. Until then, carry.
Kin to S169 partial-one-tu (extern-ref keeps rodata in the blob) — the exception S169 does not cover is
a case where the extern ref itself moves a schedule coin.
