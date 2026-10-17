---
type: "Reconstruction Note"
title: "mainproc near-match — 170/174, global-alloc priority tie (debug_mode vs the loop's sentinel temp)"
description: "Records the main loop's near-exact reconstruction, the dump-cited global-alloc ordering that decides the remaining s0/s2 swap, and the source probes that falsified the naive placements."
---

# mainproc near-match — 170/174, global-alloc priority tie

`void mainproc(void* unused)` (0x8002A640-range, 0x2B8) in `src/main/vec3f_normalize.c`
(`main/func_8002A640` bank). Best candidate: 174 rows, 170 matched, **score 20**.
The candidate body is `nonmatchings/mainproc/base.c`; `stopped` and `debug_mode` are
user variables and the inner `while (D_801052CC == -1)` test carries its own literal.

## Residual (corrected — it is NOT the stopped/debug_mode pair)

Four rows, all renames of **`debug_mode` and the loop's sentinel literal**, which
exchange `s0` and `s2`:

| row | ROM | build |
| --- | --- | --- |
| `addiu s1,zero,-1` (stopped), preheader | `s1` | `s1` ✓ |
| `addiu s?,zero,2` (debug_mode) | `s2` | `s0` |
| `addiu s?,zero,-1` (loop sentinel literal) | `s0` | `s2` |
| `bne v0,s?` (`D_800B67C8 == debug_mode`) | `s2` | `s0` |
| `beq v0,s?` (`D_801052CC == -1`, loop test) | `s0` | `s2` |

Instruction order, block structure, frame (0x20, three saved `s` registers) and both
annulled delay slots match. `stopped` already lands exactly where the ROM puts it.

## Responsible pass — global-alloc (dump-cited)

Command (isolated candidate, parent profile):

```sh
uv run --locked python tools/rom_build.py compile \
  --source nonmatchings/mainproc/base.c --output nonmatchings/mainproc/diagnostic.s \
  --profile-source src/main/vec3f_normalize.c --nonmatching --assembly-only \
  --extra-flag=-dg --extra-flag=-dl
```

Dumps (KMC gcc writes them at the repository root named after the input basename;
moved here): `nonmatchings/mainproc/dumps/base.c.greg`, `.../base.c.lreg`.

From `base.c.lreg`, the three stores and their pseudos:

```
(insn 281 … (set (reg/v:SI 76) (const_int -1)))   ; stopped = -1
(insn 284 … (set (reg/v:SI 75) (const_int 2)))    ; debug_mode = 2
(insn 397 … (set (reg:SI 120) (const_int -1)))    ; loop sentinel literal
```

From `base.c.greg`:

```
;; 9 regs to allocate: 115 73 74 127 83 75 76 120 101
;; Register dispositions:
73 in 16  74 in 16  75 in 16  76 in 17  101 in 18  120 in 18  127 in 17
;; Hard regs used:  2 3 4 5 6 16 17 18 31
```

MIPS 16/17/18 = `s0`/`s1`/`s2`. So `stopped` (76) → `s1` ✓ as the ROM does,
`debug_mode` (75) → `s0`, and the sentinel (120) → `s2`. In the printed allocation
order `debug_mode`'s pseudo (75, 6th) precedes the sentinel's (120, 8th), so it
takes `s0` first; the conflict/preference table then pushes the sentinel to `s2`.

**Mechanism:** both quantities are cross-block pseudos with the same two references
(set + use), so `global.c`'s `allocno_compare` (gcc 2.7.2, `global.c:587`:
`pri = floor_log2(n_refs) * n_refs / live_length`, ties broken by the *lower allocno
index*) decides by relative live length / creation order — a user variable against a
loop de-hoisted literal. Nothing in the source-level statement order moved that
relation (table below).

**Falsifiable prediction:** if the sentinel's allocno preceded `debug_mode`'s in the
`;; 9 regs to allocate:` order, the two would exchange `s0`/`s2` and the function
would match. Two probes aimed at exactly that were measured and **falsified** (they
changed the schedule/frame instead):

| probe | score |
| --- | --- |
| `stopped = -1;` pre-header (ROM placement) + `debug_mode = 2;` as the first statement inside the loop (invariant, for `loop.c` to hoist) | 320 |
| both invariants inside the loop body | 615 |

## Allocation arithmetic (tools/allocno_report.py)

`uv run --locked python tools/allocno_report.py nonmatchings/mainproc/base.c mainproc` reads the
numbers behind the ordering (`global.c:587`: `floor_log2(n_refs)*n_refs / live_length * 10000 * size`,
ties on allocno number):

| allocno | refs | len | priority | reg | identity |
| --- | --- | --- | --- | --- | --- |
| 74 | 11 | 45 | 7333 | s0 | (temp) |
| 127 | 7 | 43 | 3255 | s1 | (temp) |
| 75 | 4 | 48 | **1666** | s0 | `debug_mode` (`.lreg` insn 284) |
| 76 | 4 | 50 | **1600** | s1 | `stopped` (`.lreg` insn 281) |
| 120 | 3 | 22 | **1363** | s2 | loop sentinel literal (`.lreg` insn 397) |

So the build's order is `debug_mode > stopped > sentinel` → `s0/s1/s2`, while the ROM's registers
imply `sentinel > stopped > debug_mode` (`s0/s1/s2`; 75 and 120 conflict, so whichever comes first in
the order takes `s0`). Calculation for the flip:

- raise the sentinel's refs 3 → 4 (`floor_log2` steps to 2): priority 8/22 = 3636 > 1666 ⇒ flips; or
- lengthen `debug_mode`'s live length 48 → ≥ 59 (8/59 = 1356 < 1363) ⇒ flips.

Both are arithmetic targets rather than guesses, and neither may be reachable by statement order
alone (see the falsified probes above), which is why this residual is carried rather than ground on.

## Earlier source probes (all ≥ 20)

| preheader spelling | score |
| --- | --- |
| `do { stopped = -1; debug_mode = 2; } while (0);` | 20 (debug_mode sinks into the loop) |
| `do { debug_mode = 2; stopped = -1; } while (0);` | 40 |
| `stopped = -1; debug_mode = 2;` (plain) | 25 |
| `debug_mode = 2; stopped = -1;` (plain) | 20 |
| swap the two declaration lines | 20 |
| literals in both uses | 635 |
| `stopped` variable + literal `2` | 220 |

The prior permuter case (`nonmatchings/mainproc-2`, outputs `output-20-1..3`) is also
pinned at 20 — search does not cross this tie-break.

## Next levers

- Change the sentinel's *allocno creation order* relative to `debug_mode` without
  touching the instruction layout, or give one of them a different reference count
  (`floor_log2(n_refs)` steps only at 2→3) so the comparison stops being a tie.
- Confirm with `-dA`/`loop.c` output whether the sentinel pseudo is created by the
  loop condition's expansion (as assumed here) — if so, the only remaining lever is
  the loop's *shape* upstream of `loop.c`, not the statement order.
