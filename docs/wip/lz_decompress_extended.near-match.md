# lz_decompress_extended — near-match (S166 -> S317, CARRIED)

`src/main/lz_decompress_simple.c`, 0x468 / 282 instructions, the file's last `INCLUDE_ASM` stub.
Banking it takes the host to md5-candidate (`lz_decompress_dma` S165 `20ff76d`,
`lz_decompress_simple` S166 `60ecb9a`).

## Measured (rows as of S317)

Replaying `nonmatchings/lz_decompress_extended/base.c` verbatim into the host (its `St` struct and
`inline static expand` helper, return type corrected to `s32`) gives, on the first build:

- **282 of 282 instructions at the ROM's exact `-0x18` frame.** No structural deficit in count.
- **194 `cmpfn` rows** (S317 tooling), which is a whole-function register permutation: nearly every
  row differs by register name only.
- **The mnemonic multiset does NOT match**, and that is new information the S166 verdict did not
  have: `andi` rom=23 mine=20, `lhu` rom=35 mine=37, `beqz` 11 vs 9, `bnez` 7 vs 8, `beqzl` 0 vs 1,
  `sw` 13 vs 14. So three `andi` masks, two loads and three branch forms are *structural*, not
  allocation. Located but not yet fixed: the missing `andi ...,0xFFFF` is in the second `expand`
  loop's `q->run != 0` test, and the two `0x7FF` masks around `ro = ridx & 0xffff` /
  `ridx = (ridx + 1) & 0x7ff` differ in operand source.
- Deleting the `rc = (int) param_1;` copy (using `param_1` for every field access and returning the
  expression directly) takes 194 -> 193 rows and is otherwise inert; the ROM's entry
  `addu $t6,$a0,$zero` still does not appear first.

## Attributed

- The S166 carry-over calls this a **greg-proven raw-185 register-permutation floor** — two coupled
  near-tied 3-cycles, Cycle A `{ridx, src, dist}` and Cycle B `{param, end, ring}`. That verdict was
  measured before the mnemonic-histogram oracle existed, and the histogram says the measured body
  still carries structural differences. Per the S259 rule (a percent/score verdict belongs to the
  body that was measured), the coloring conclusion cannot be inherited until the multiset matches.
- The body is otherwise a faithful model of the ROM: the local `St st` at `sp+0x0..0xF` with the
  frame at `-0x18`, `q = &st` materialised as `addu $a3,$sp,$zero` at the decode entry, the
  `marker = 0x8000` in `$t9`, and the `lbu $v0,0x9($a3)` low-byte reload of `hidx & 0xff` are all
  reproduced.

## Re-open checklist

1. Replay the base (one edit: paste `base.c`'s `typedef`/`expand`/function over the stub, return
   type `s32`). Confirm 282/282 at `-0x18`.
2. Close the mnemonic multiset first — the three `andi`, two `lhu`, and the `beqzl` — before reading
   any register verdict. Only then re-derive with `tools/allocno_report.py`.
3. The host's banked sibling `lz_decompress_simple` is the style and type reference; its
   `LzDecompressState` covers this function's fields too (`0x18` ring, `0x1C` u16 ring index, `0x1E`
   u16 run, `0x20` u16 history index, `0x24` history base), so the rewrite should use it rather than
   `int *param_1` indexing.
4. Enablers: none. Split already done (`[0x43810, c, main/lz_decompress_simple]`), all callees
   placed, no rodata/data carve, the name is pre-curated.
5. Spent: the safe-passes permuter (S166, plateau 230 -> 208) and the S166 lever sweep. Do not
   re-grind either before step 2.
