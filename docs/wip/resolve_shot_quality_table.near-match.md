# resolve_shot_quality_table — near-match (S185, structurally complete, 3-instr irreducible residual)

`src/main/func_80043C20.c` @ 0x80044AC8 (109 insns, 0x1B4). **STRUCTURALLY COMPLETE (109/109 insns);
only a 2-instruction schedule swap + the mult operand order differ** (v0/v1). Full near-match body in
`docs/wip/func_80043C20.resolve_shot_quality.near-match.c.txt`. Deep GCC-2.7.2-source-proven irreducible
(sched load-latency + regalloc fork). **Permuter will NOT help** (no C mutation changes the 4-node DAG).

## The `.rodata` carve — SOLVED (the retry's main enabler; replay mechanically)
The 23 per-terrain digit tables `D_800CC590..6F0` (16B each) + `jtbl_800CC700` (24-word switch table)
are interleaved with `jtbl_800CC530` (resolve_club_terrain_mask, already banked) in the ROM:
`[jtbl530 @0xA7930][digit tables @0xA7990][jtbl700 @0xA7B00]`.
- **Digit tables MUST be TU-owned INDIVIDUAL `static const char q00..q22[16]`** (local statics inside
  the fn). NOT a 2D array (`tables[k]`/`tables[k+1]` lets GCC fold `p2 = p1+16` → 1 insn short/case,
  10 insns short total). NOT extern (then the .o emits only the two jtbls contiguous 0x60 apart, not
  the ROM's 0x1D0 — whole-ROM 0x30 shift). With individual statics GCC emits
  `.o(.rodata) = [jtbl530 0x60][q00..q22 0x170][jtbl700 0x60] = 0x230`, matching the ROM exactly.
- Carve = extend the existing func_80043C20 `.rodata` sibling to the full 0x230:
  `[0xA7930, .rodata, main/func_80043C20]` then `[0xA7B60, rodata]` (removing the interim
  `[0xA7990, rodata]`). vram 0x800CC000 = ROM 0xA7400. Digit strings (13 chars, 3-null pad each) are
  transcribed in the saved near-match `.c.txt`.

## Levers landed (KEEP in the retry)
1. Individual static const `q00..q22[16]` (fixed the 2D-array `p2=p1+16` fold).
2. **ADDRESS-SELECT** `const char* addr = (arg2==2) ? &p2[club_index] : &p1[club_index];` then
   `(u8)*addr` — fixed the a0/a1 register swap (p1 in a1 not a0, club_index in a0 not a1) AND the
   category branch (`bne cat,2`+address-in-delay, not `beql`+pointer-move). 22 diffs → 3.
3. Result-var + shared-tail (`if (club_index != 0xD) {...; if (v<6) return v; v=5;} else v=0; return v;`)
   — matches the ROM's C60/C64 shared `v0=v1` return tail (same class as the switch-funnel fix banked
   in resolve_club_terrain_mask).
4. `s8 g_terrain_vtx_xform_mode` already in-tree (unrelated to this fn).

## The residual (irreducible — carry blocker)
ROM: `lbu v0; addiu v1,v0,-0x30 (d→v1, ACCEPTS load-use interlock); lh v0,0x14(s0) (power→v0 reuse);
mult v1,v0`.
Build: `lbu v0; lh v1,0x14(s0) (power→v1 FRESH, hoisted into the lbu load shadow); addiu v0,v0,-0x30
(d→v0); mult v0,v1`.
- Root cause (GCC 2.7.2 source, focused subagent S185): the load-latency-3 model (`mips.md:153-159`)
  makes the bottom-up list scheduler (`sched.c:2578-2606` queue-by-latency) hoist the independent
  `power` load out of the `mult` and into the earlier `lbu`'s load shadow. That hoist lengthens
  `power`'s pre-reload live range (`sched.c:4946`), so the allocator gives it a FRESH `$v1`. The ROM's
  build instead put `power` on `$v0` (reusing dead `digit`), whose WAR anti-dep (`sched.c:1710-1716`)
  then FORBIDS the hoist — a self-consistent fixed point. The fork is register allocation, gated by the
  schedule; no faithful C rewrite of this dataflow reaches it (proven: explicit temp, split statement,
  mult-operand swap, power-loaded-first — all DAG-identical, all inert).
- Both `-O2` scheduler passes are on and correct for the profile (matches ~15 sibling fns). Not a flag
  issue. The only faithful lever would be a surrounding live-range/pressure difference — but the fn is
  byte-identical everywhere except these 3 insns, so no such difference exists → irreducible.

## Retry checklist (near-free once a lever/permuter path emerges)
1. Restore the saved near-match body + the 0x230 carve (both in this dir / the `.c.txt`).
2. All refs placed (get_table_entry, the q## are TU-local). No recover-externs. No symbol adds.
3. Only path to close: a NEW faithful idiom that makes GCC allocate `power`→`$v0` (keep `d` in the
   fresh reg), OR a compiler-config confirmation. Permuter ruled out by the source proof.

## decomp.me scratch (S185)
Clean scratch (gcc2.7.2kmc, `--main` -O2/F3DEX2 profile, near-match source + get_table_entry context):
https://decomp.me/scratch/dZACn/claim?token=eyJzbHVnIjoiZFpBQ24ifQ.Es5YK7DG4cgVmfDIO3kYxlmtYBM
Created via `import.py --settings permuter_settings_main.toml <standalone.c> <target.s> --decompme`
(a standalone one-fn file, NOT the multi-fn func_80043C20.c — the GCC nested fn in that file breaks
pycparser's context/source split). NOTE: the scratch omits the .rodata carve (decomp.me diffs .text
only); the individual-static-const digit tables are inline in the source and match the ROM's layout.
