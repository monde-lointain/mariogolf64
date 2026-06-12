# Mario Golf 64 decomp sprint velocity (story points)

The story-point dashboard (activated 2026-06-11; PO directive at the Sprint-5 review — the
"phase 2" named trigger in `CLAUDE.md`, fired at the ≈5-sprint mark). Scale = **Fibonacci
1, 2, 3, 5, 8, 13**. Scope = the **current phase: the libultra/libkmc upstream-mirror band**
(`regime: mirror`). The classical/game-code regime gets a **separate track** when it starts —
never compare velocity across regimes (McConnell, *More Effective Agile*, Ch. 19).

**Ported from `../marioparty7/VELOCITY.md`** (23-sprint sibling system), but **shipped in two
phases** because MG64 differs from MP7 (its cost axis is deterministic and its functions are
tiny/uniform — see `CLAUDE.md ## Story points`):

- **v1 (now): deterministic seed + the 8-point decompose gate.** This is the part with signal
  today. The seed is a pure function of `tools/pick_target.py`'s columns, so it is
  **re-derivable** — it needs no anti-retrofit freeze commit, and the plan gate creates no
  commit. The committed seed is logged here in the normal review/retro commit.
- **v2 (deferred): the realized-tier + residual + rolling/re-anchor machinery.** Dormant until
  the **first classical or mixed sprint** produces real residual variance to calibrate against
  (turning it on against the current 5 identical verbatim sprints would only measure a point
  mass). Designed in `CLAUDE.md`; activated by PO approval at the review that hits the trigger.

## Purpose — what the v1 number actually changes
1. **8-point decompose gate (primary).** A seed of **8 or 13 means do NOT commit it as a normal
   1-increment sprint** — decompose it (split the subseg at the upstream-file/function boundary)
   or pull a scaffolding enabler as the sprint goal. Catches an all-or-nothing bank stall
   *before* a sprint sinks into a too-big unit. (Most remaining classical units seed 8/13 — see
   Regime — so this gate will fire on nearly every sprint once the mirror band is mined out.)
2. **Regime indicator (primary).** A sustained seed/velocity shift flags the mirror→classical
   transition so the PO re-plans cadence and triggers v2.
3. **Forecasting — omitted in v1.** The backlog is overwhelmingly un-pointed classical work, so
   a remaining-points / velocity number would be a false deadline. Added with the classical
   track in v2, and even then "tertiary, noisy, not a deadline" (book Ch. 20).

## Seed rubric (deterministic; `tools/pick_target.py seed_points()`, the `pts` column)
Pure function of `size, upstream, band, nfns, hazards` + a copyable-vs-blocked header classify.
**Display-only — does NOT change the smallest-first sort.** A **cluster** seed = the **sum** of
its files' seeds; points bank **per file** (a spiked sibling scores 0 for that file only, not
the whole cluster — so sibling-pair batching is never punished).

```
blk  : a needs-header that can't be fixed by a companion-copy (unindexed -I / system header,
       e.g. <libaudio.h>, guint.h) → un-pickable, a DoR reject (not a cheap seed)
13   : size>=1536, or any classical multi-fn pack                  → must decompose
8    : nfns>=4 (large pack, any path), or a classical fn that is big/pack/drop/needs-copy
5    : a small single classical fn (no upstream — unproven, high variance)
mirror (has upstream): base = 1 if warm else 2;  +1 each for {static/data-drop, header-copy,
       pack-or-big};  snap to Fibonacci (4→5)
```
`drop` = `file-static`/`defines-data` (→ classical fallback, defs dropped). `warm` = the mirror
dir already holds a banked sibling (enabler-free). `big` = size≥768 B, `huge` = ≥1536 B — the
byte gates are **dormant** in the current <256 B regime and only bite for large/classical fns.

## Realized tier + residual loop — DEFERRED to v2
Scored at review from the RETRO facts (start at the seed band; +1 per
{stuck-far / permuter / re-attempt / novel bank-gotcha / mid-sprint split / carry-or-reopen};
−1 verbatim first-try ≤1 fix-iteration; per-file then summed). With it come the two-pass
**freeze** (plan-time seed committed before `src/`; realized in a second commit). **Caveat
(do not over-credit the freeze):** the freeze locks only the deterministic *seed*; the realized
tier is **agent-scored and subjective**, so the real anti-gaming guards are the **per-file
all-or-nothing bank + the quality counter-metric** (stuck-far / permuter / carried / re-opened),
not the freeze. None of this is active in v1.

## Bootstrap anchors (retro-pointed S1–5; the calibration loop, pre-run)
All `regime: mirror`, all tiny verbatim leaves (<256 B). `seed` = the rubric (a-priori, **live
in v1**). `(real)` is the v2 realized rule, shown *italic/illustrative* only — **not** logged
until v2. These rows double as the **reference stories** for the plan-time ±1 adjust.

| Sp | increment | nfns | path / band / hazard | seed | *(real)* | note |
|----|-----------|------|----------------------|------|----------|------|
| 1 | dp.c | 1 | mirror / cold / — | 2 | *2* | verbatim vs cpp-toolchain gotcha |
| 2 | createmesgqueue.c | 1 | mirror / cold / — | 2 | *2* | verbatim vs `__osThreadTail` asm-recovery |
| 3 | visetmode.c | 1 | mirror / warm / needs-copy | 2 | *1* | trivial `assert.h` copy |
| 4 | sirawread + sirawwrite | 1+1 | mirror / warm / — | 1+1=2 | *2* | both verbatim, warm floor 1 each |
| 5 | viswapbuf + visetevent | 1+1 | mirror / warm / — | 1+1=2 | *2* | both verbatim, warm floor 1 each |
| 6 | viblack | 1 | mirror / warm / — | 1 | *1* | first live-logged v1 sprint; verbatim, zero-enabler, banked 1pt |
| 7 | virtualtophysical | 1 | mirror / cold / — | 2 | *2* | opens convert band (vi exhausted); verbatim, zero-enabler, banked 2pt |
| 8 | stopthread | 1 | mirror / warm / — | 1 | *1* | opens thread band; verbatim, zero-enabler, banked 1pt |
| 9 | func_80099490 | 1 | **classical** / — / — | 5 | *—* | **first classical-track row** — no-upstream wrapper `nuPiInitSram()`; first-pass clean (score 0, 0 iter), banked 5pt; classical loop PROVEN. Kept OUT of the mirror seed-velocity (separate regime) |

**Seed-velocity = 2.0 pt/sprint** (bootstrap anchors S1–5, sum seed 10 / 5 sprints). With the
three live-logged sprints S6 (seed 1) + S7 (seed 2) + S8 (seed 1), the running mirror-regime
seed-velocity is 14 pt / 8 sprints = **1.75 pt/sprint** — the predicted downward drift as the
warm singleton pool depletes (S6 last clean vi leaf; S7 opened the *cold* convert band at the
2 pt floor; S8 the warm-1 thread leaf). **S9 is the first classical-track row** (seed 5,
banked 5, first-pass clean) — logged separately, NOT folded into the mirror 1.75 average
(never compare regimes). Classical track: n=1, seed-velocity 5 pt/sprint (one point, no signal
yet). It proved the classical loop mechanically but produced **zero residual variance** (clean
verbatim-shaped wrapper), so the PO **deferred v2** at the S9 review — a non-trivial classical
leaf (arithmetic/branches/locals) is scheduled next to generate the variance v2 needs.
Three honest caveats:
- **Fitted, not validated.** The bands were chosen so this history reads ~2 pt/sprint; the
  first out-of-sample sprint is the real test. The byte gates (768/1536) and the classical
  5/8/13 tiers have **zero** supporting data — provisional, corrected by v2's residual loop.
- **Non-stationary.** The recent ~2 came from *warm* sibling pairs, but of the ~56 remaining
  mirror candidates only ~18 are warm and ~38 are cold-band (the 2–3 pt tier) — expect velocity
  to **drift down within the mirror regime** as the warm pool depletes, then a discontinuity at
  the classical wall.
- **Commitment-shaped, not pure throughput.** Sprints are PO-scope-boxed, so points/sprint
  partly measures how much was committed, not raw output — read it as a **regime indicator**,
  not a productivity score.
- **S0 excluded.** Sprint 0 (7 files, pre-Scrum) had no DoD discipline and was batch-extracted
  before the overlay; its 7-files/sprint outlier would corrupt the per-sprint baseline. It
  stays in `BACKLOG.md` as historical record, out of this calibration.

## Velocity (v1 headline)
- **Seed-velocity: 2.0 pt/sprint** (mirror regime, n=5, all verbatim). Realized-velocity,
  rolling windows, residual, and the re-anchor trigger are **v2** (need variance to exist).
- **Regime:** `mirror` (~22 % of ranked candidates: 56 mirror vs 194 classical; 18 warm /
  38 cold). The mirror band is a depleting minority — the first classical/mixed sprint trips
  the v2 activation trigger.
- **S7 note:** vi band exhausted of clean leaves (S6), so S7 opened the **cold convert band**
  (`virtualtophysical`, seed 2 = the cold-mirror floor). The S7 retro added a **`refs-unplaced`
  hazard** to `pick_target.py` (a referenced data extern absent from both name files — the dual
  of `defines-data`); it re-priced `osYieldThread` 1→2 and `osEPiLinkHandle` 2→3, and confirmed
  `osStopThread` is the smallest *clean* remaining leaf (seed 1, warm, no unplaced ref). Still
  7-straight first-pass-clean mirrors → zero residual variance → v2 trigger remains unmet.
