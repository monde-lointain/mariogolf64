# Mario Golf 64 Decomp workflow

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting), KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (asm→C seed), asm-differ (library mode), decomp-permuter (escalation), Ghidra MCP bridge (live decompile + struct/symbol lookup).

## Slash commands

Two commands — the Scrum gates. Everything tactical (target selection, the per-function match loop, upstream-mirror copies, name sync) is **inline** between them (see `## Execution loop`), not a separate command. There is no MCP lock — execution is a single serial session.

- **`/sprint-plan [scope]`** — Plan gate. Ranks candidates with `tools/pick_target.py` (smallest-first), proposes one small increment (one `src/<seg>.c` → md5-candidate, or a cohesive subseg cluster) + its enablers, gets PO approval, then **performs + validates the flip** (agent edits the yaml subseg + `symbol_addrs.txt`, optionally refreshes names via `make sync-names`, runs `make extract && make` for the green ROM), and writes `SPRINT.md`. Then runs the `## Execution loop` autonomously over the committed backlog.
- **`/sprint-review`** — Review gate. Verifies the ROM-SHA-1 DoD on the increment, takes PO scope sign-off, and is the **single apply-point** for the sprint's buffered workflow-improvement edits; appends the `RETRO.md` digest + `BACKLOG.md` carry-overs.

## Execution loop (inline — runs between the gates, no command)

After `/sprint-plan` writes `SPRINT.md`, the agent works the committed backlog autonomously — one function at a time, no per-function PO stop. This is the absorbed match-loop / upstream-mirror logic; the `## Conventions` bullets below carry the technical gotchas it relies on. Ghidra MCP is used inline at seed time (assume it's running on port 8089; all MCP calls pass `program="baserom.z64"`). There is **no MCP lock** — execution is a single serial session.

For each target function (from the committed `SPRINT.md` backlog, smallest-first):

1. **Resolve** the function's subseg in `mariogolf64.yaml`. `hasm` → permanent asm, skip. The subseg was flipped to `c` at the gate.
2. **Branch — upstream-mirror** (target maps to a libultra/libkmc upstream — `tools/pick_target.py`'s `upstream` column + the `## Conventions` upstream-mirror pattern):
   - Pre-flight the upstream `.c` for a **file-scope `static`** (BSS-layout-conflict — see the convention bullet). If present, fall to the classical loop (step 3) with the `static` **dropped** in the C body; do not copy verbatim.
   - Otherwise copy the upstream `.c` **verbatim** to its mirrored `src/lib<name>/...` path, add any missing headers verbatim under `include/lib<name>/...`, and **skip clang-format**.
   - Write any referenced symbols not yet in `ghidra_symbols.txt`/`symbol_addrs.txt` (add-only, disjoint). For an **unsymbolized data global** the body references (absent from Ghidra + both name files), recover its vram **deterministically** by disassembling the *target fn itself* (MCP `disassemble_function`) and reading the `lui`/`addiu` HI/LO16 pair that materializes its address — no RE guesswork, no Ghidra symbol needed (e.g. `osCreateMesgQueue`'s `lui 0x800d`/`addiu -0x7de0` → `__osThreadTail` = 0x800C8220). Then **`make`** → ROM SHA-1 must equal the baserom. The subseg was flipped at the gate, so its asm/data is already extracted — the execution-middle finalize is just `make`; re-run `make extract` only when you split a subseg **mid-flight**. The mirror branch's **proof IS the full-`make` ROM SHA-1** (a verbatim upstream copy + green ROM = match); the byte-`cmp` spot-check is **classical-loop-only** (it guards the isolated-compile regalloc caveat, which the mirror branch doesn't hit).
3. **Branch — classical match loop** (no upstream, or `static`-dropped): seed → iterate → spot-check → finalize:
   - **Seed** `nonmatchings/<func>/base.c`: fetch the Ghidra decompile via MCP, then `venv/bin/python3 tools/seed_c.py --func <placeholder> --parent src/<seg>.c` (externs, ultra64.h types, m2c reference block). **The asm (`disassemble_function`) is ground truth, not the Ghidra decompile** — the decompiler can be silently *wrong* on classical leaves (S11: it rendered `func_800AB600`'s return as `return 0` when the asm returns `(status>>8)&1`). Use the decompile for shape/types, but translate the actual logic from the instruction listing.
   - **Iterate** ≤25 times: `venv/bin/python3 tools/decomp_loop.py --func <placeholder>` → parse JSON; `score == 0` is a candidate match; 5 consecutive `compile_ok == False` means a broken seed (stop); else read the top mismatches, edit `base.c`, re-run. Permuter (`./run-permuter.sh`) only at `percent ≥ 0.97`.
   - **Spot-check** (only at score 0): byte-level `cmp` of the in-tree compiled `.text` vs the isolated one (mnemonic diff false-positives — see the convention bullet). Mismatch ⇒ isolation-only divergence; the spot-check is the truth.
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the `INCLUDE_ASM` line, `clang-format -i` (skip under `src/libultra/` & `src/libkmc/`), then **`make`** until `build/mariogolf64.z64: OK` and SHA-1 == baserom. Guard non-16-aligned subsegs (KMC `as` auto-pad — split the yaml or add `.balign`). The `make` is what proves the match; gate the `sha1sum` on `make` succeeding (stale-`.z64` false-positive).
4. **Bank** the function: matched only at score 0 + spot-check + full-`make` SHA match. Give it its curated name if Ghidra has one (add to `symbol_addrs.txt`, rename in the body, re-`make`). `git commit`. Append a standup line to `SPRINT.md`, and **record** the run's numbered "Suggested workflow improvements" into `SPRINT.md`'s buffer — applied only at `/sprint-review`, never mid-sprint.

A function that locks `< 0.97 percent`, needs the permuter, or hits a BSS-layout/alignment wall is a **spike**: note it, carry its file to the next sprint (`BACKLOG.md ## Carry-overs`), and move on. **Never weaken the DoD to bank a spike.** The increment (the `src/<seg>.c` file) banks only when its last `INCLUDE_ASM` stub is gone.

## Scrum operating model (lightweight overlay)

The tactical match-loop runs inside a thin Scrum cadence (McConnell, *More Effective Agile*). These are the only ceremonies — keep it lightweight. Artifacts: `BACKLOG.md` (PO-owned ordering rationale + enablers + carry-overs), `SPRINT.md` (gitignored ephemeral board + resume surface), `RETRO.md` (consolidated digest). Target selection is `tools/pick_target.py` (smallest-first ranker), not a stored roadmap.

- **Roles.** *Product Owner* = the user (backlog priority, sprint-goal approval, scope sign-off, retro selection). *Development Team* = the agent (executes the gate enablers — subseg flips/splits, `symbol_addrs.txt` additions, `make extract` — after PO scope approval) + `tools/` + the KMC-GCC/asm-differ/ROM-SHA-1 oracle. *Process discipline* = the slash-command loop itself.
- **Cycle.** `/sprint-plan` (plan gate: PO approves a goal + small backlog; the agent then performs + validates the flip enablers) → autonomous execution → `/sprint-review` (review gate: DoD verify, scope sign-off, retro) → repeat.
- **Execution model.** Between the gates the agent runs the **`## Execution loop`** inline over the committed backlog — one function at a time, no per-function PO stop, no MCP lock (single serial session). `SPRINT.md` is the resume surface if the middle spans context windows.
- **Sprint = one `src/<seg>.c` file to md5-candidate (0 `INCLUDE_ASM` stubs), or one cohesive subseg cluster** when the file is part of a multi-file pack. The **function is the task** (the execution-loop unit); the **file/cluster is the increment**. **Cap small early — 1 upstream file, or ≤ ~3–4 functions** — so the middle fits a context window and to calibrate the still-unproven classical loop (every win to date was an upstream mirror). Span multiple files only when **siblings**. Review fires when every committed item is **banked or spiked/carried**.
- **Definition of Ready (gate into a sprint).** Subseg flippable (not `hasm`); coarse size known; upstream-mirror availability noted (yes/no); hazards flagged (file-scope `static` → classical loop, not the upstream mirror; BSS-layout-conflict; subseg-alignment). **Enablers** (subseg flip + `make extract`, multi-file split, `symbol_addrs.txt` additions) are performed by the **agent at the plan gate** — after the PO approves the goal/scope — and **validated there** (`make extract && make` must still produce the green baserom ROM with the new stubs) before DoR is "ok". This gate build-check is **load-bearing**, not a formality — it surfaced a missing-`mips-linux-gnu-cpp` toolchain regression on 2026-06-11 that was silently emptying every asm object; never skip it. A **name sync** (`make sync-names`) is **gate-only**, never mid-sprint (a rename breaks in-flight links until `make extract && make`). Execution is then flip-free and autonomous — all yaml/`symbol_addrs.txt` edits land at the gate, so the middle touches only `src/`.
- **Definition of Done (binary; the oracle already exists).** Per-function = the execution loop's finalization (score 0 + byte `cmp` spot-check + inlined + clang-format + full `make` → `build/mariogolf64.z64: OK`, SHA-1 == baserom + committed). Per-sprint = every fn in the file inlined (0 stubs = md5-candidate) + ROM SHA-1 matches + committed. **The ROM SHA-1 is green at every commit** (un-decompiled parts fill from extracted asm via the Makefile `md5sum -c`), so at review the `make`+SHA-1 paste is a **regression guard**; the *value* delta is the matched-count / md5-candidate-files number. Never commit a non-matching fn; the global md5 makes a separate per-unit regression check unnecessary.
- **Spike + carry-over.** A function that blocks its file's all-or-nothing DoD (stuck-far < 0.97 percent, needs permuter, BSS-layout-conflict, subseg-alignment) is timeboxed as a **spike**: note it, **carry its file/cluster to the next sprint**, count credit at the **function** level so one bad fn doesn't zero the sprint. **Never weaken DoD to bank a spike.**
- **Quality counter-metric.** Track stuck-far + permuter-escalated + carried + re-opened per sprint (from the `SPRINT.md` standup log + `git log`), reported next to the match count so the count can't be gamed by premature spiking.
- **Process changes are retro-gated.** The "Suggested workflow improvements" the execution loop emits are *recorded* into `SPRINT.md` during the sprint and applied **only** at `/sprint-review` — never mid-sprint.
- **Story points (active since 2026-06-11, the named-trigger switch).** Velocity basis = **realized points per sprint** (Fibonacci 1,2,3,5,8,13), **not** files/week — sprints are scope-boxed with no calendar cadence. Shipped in two phases: **v1 (live)** = a deterministic `pts` seed (`pick_target.py`) + the **8-point decompose gate**; **v2 (deferred)** = the realized-tier / residual / re-anchor machinery, activated by PO approval at the first classical/mixed sprint that produces real variance. See `## Story points` below and `VELOCITY.md` (the committed dashboard). Velocity is a **planning indicator, not a performance target** (McConnell Ch. 19) — reported next to the quality counter-metric, which (with per-file all-or-nothing banking) is the real anti-gaming guard.
- **Two-gate close.** Review accepts the *product* (DoD + scope); retro improves the *process* (and is the single apply-point for tooling edits) — two distinct decisions in `/sprint-review`.
- **Resume protocol (multi-session).** On a fresh mid-sprint session: read `SPRINT.md`, reconcile banked work via `git log` since the snapshot, and verify Ghidra MCP connectivity (`list_instances`, port 8089) before resuming the execution loop.

## Story points (v1 — the velocity system)

The lightweight estimation layer over the Scrum cadence (McConnell, *More Effective Agile*,
Ch. 20/19/9), ported from `../marioparty7/VELOCITY.md` and re-calibrated for MG64's regime.
`VELOCITY.md` is the committed dashboard (rules + bootstrap anchors); this section is the rule
summary. **Scale: Fibonacci 1,2,3,5,8,13.** Scope: the current phase (`regime: mirror`); the
classical/game regime gets a separate track.

- **Deterministic seed.** `tools/pick_target.py`'s **`pts`** column (`seed_points()`) is the
  a-priori seed — a pure function of `size`, `upstream` (mirror vs classical), `band`
  (warm/cold), `nfns`, and `hazards`. MG64's effort axis is **path + enabler load**, not bytes
  (every target is a tiny <256 B leaf), so the byte gates are dormant until large/classical fns
  arrive. A **cluster** seed is the **sum** of its files' seeds. `pts` is **display-only — it
  does not change the smallest-first sort.** A `blk` seed = an un-pickable needs-header
  (unindexed `-I` / system header, e.g. `<libaudio.h>`) — a DoR reject, not a cheap target.
- **The 8-point decompose gate (the primary value).** A seed of **8 or 13 must NOT run as a
  normal 1-increment sprint** — decompose it (split the subseg at the upstream-file/function
  boundary, as the existing multi-fn-pack doctrine already does) or pull a scaffolding enabler
  as the goal instead. Prevents an all-or-nothing bank stall on a too-big unit. Applied at the
  `/sprint-plan` gate (Step 5b).
- **Per-file all-or-nothing banking.** Points bank **per file**, even inside a cluster: a
  spiked/carried file scores **0 pt** (the anti-gaming guard), but a banked sibling still
  counts — so sibling-pair batching (the S4/S5 win) is never punished. This is a *separate
  ledger* from the function-level **quality counter-metric** (stuck-far / permuter / carried /
  re-opened), which is unchanged and still reported next to velocity.
- **v1 has no freeze commit.** The v1 seed is deterministic and re-derivable, so it carries
  near-zero retrofit risk: the plan gate creates **no commit**, and the committed seed is
  logged in `VELOCITY.md` in the normal `/sprint-review` retro commit (so an abandoned sprint
  leaves no orphan row). The two-pass git-freeze belongs to v2 (where the realized tier *is*
  retrofittable).
- **v2 (deferred).** The realized-tier rule + residual loop + rolling-5 + re-anchor trigger +
  the separate classical track + the two-pass freeze. **Activation trigger:** the first
  classical or mixed sprint that produces real residual variance — proposed at that review, PO
  approves (turning it on against the 5 identical verbatim sprints would only measure a point
  mass). Designed in `VELOCITY.md`; dormant until then.

## Conventions

- **One function at a time.** `tools/pick_target.py` ranks (smallest-first); you pick the target.
- **Scratch dir**: `nonmatchings/<func>/` (gitignored, shared with permuter).
- **Ghidra MCP** is assumed running via `~/development/reversing/ghidra/start-headless.sh` (port 8089). All MCP calls pass `program="baserom.z64"` (including `sync_decomp_names.py`).
- **File-level matching**: a source file matches only when every function in it matches. Splat generates `src/<seg>.c` with `INCLUDE_ASM` stubs; promoting a match means replacing one stub with the C body. Don't expect md5 wins until the last stub in a file is gone.
- **`symbol_addrs.txt`/`reloc_addrs.txt`** are sacrosanct — **never delete an entry**. The agent **may add** maintainer-override entries to `symbol_addrs.txt` (curated name → vram — a function entry is `name = 0x<vram>; // type:func`, a **data extern** is `name = 0x<vram>; // size:0x<n>` with **no** `type:func`, e.g. `__osThreadTail = 0x800C8220; // size:0x8`); it never deletes from it and never touches `reloc_addrs.txt`. `ghidra_symbols.txt` is owned by `sync_decomp_names.py`; do not hand-edit. The two name files must stay disjoint — never add a `symbol_addrs.txt` entry for an address already in `ghidra_symbols.txt` (splat errors on duplicates).
- **Subsegment flips** (`asm` → `c`), multi-file subseg **splits**, and the `make extract` that regenerates the scaffold are **agent actions**, performed at the `/sprint-plan` gate after the PO approves the increment (or inline during the execution loop when a split is needed mid-flight). The agent's `mariogolf64.yaml` authority is limited to subseg flip/split/path-qualifier lines — never the segment/memory-map structure (`mariogolf64.ld` and segment boundaries stay off-limits).
- **Decomp is authoritative for names** (per `~/development/reversing/ghidra/mariogolf64/docs/re/coordination/decomp_coordination.md`).
- **`hasm` subsegments** are handwritten asm (entry stub, `__muldi3` libkmc math module, RSP microcode entries in `extern_syms.txt`). The execution loop refuses these — they stay as raw asm forever.
- **Permuter** (`./run-permuter.sh`) only runs when asm-differ's `percent` ≥ 0.97. Below that, iterate on C or reconsider whether the subsegment should be `hasm`.
- **Display lists**: F3DEX2 microcode. Include `<PR/gbi.h>` when the target manipulates `Gfx*`. For static dlists in rodata, run `~/development/repos/n64-tools/src/gfxdis-rom/gfxdis` (build the tool with `make -C ~/development/repos/n64-tools` once). `gfxdis` only handles static dlists — dynamic builders are hand-decompiled.
- **C naming conventions**: follow the convention table at the top of this file (functions/variables/struct-members `lower_case`; types `CamelCase`; global/static const `kCamelCase`). When stitching m2c output into base.c, rename m2c's `temp_*` / `local_*` / `arg_*` synthetics before promoting.
- **Prompt authoring**: any time `/sprint-plan` or `/sprint-review` (or any other slash command in this project) is created or modified, the prompt text follows `PROMPT_GUIDELINES.md` at the project root.
- **Register-reuse nudge (classical regalloc)**: when a classical match locks just short with the *only* diff being which scratch register holds an intermediate (e.g. `srl a0,...` in the target vs `srl v1,...` in yours, same value), split the single expression into two statements over **one** lvalue to force GCC to reuse the register. S11: `bit = (status>>8)&1;` (compiler used a temp `v1`) → `bit = status>>8; bit &= 1;` (reused `a0`) flipped score 400→0 in one iteration. The value is identical; the split changes only the register-allocation pressure.
- **Isolated-compile caveat**: the execution loop compiles `nonmatchings/<func>/base.c` standalone. KMC GCC's regalloc/scheduling can differ from the in-tree compile — that's why every match runs an in-tree spot-check before being declared. If iteration mismatches cluster around extern address loads (HI/LO16 relocs against project-local symbols), those are isolation-only — the spot-check is the truth.
- **`src/%` is assembled by KMC `as` (`tools/cc/as`)**, not modern `mips-linux-gnu-as`. The original assembler emits `addu` for `move dst,zero` (encoding `0x...1021`, matching the original ROM, vs modern as's `0x...1025` from `or`) and auto-pads `.text` to its 16-byte section alignment. `asm/%` continues to use modern as because raw asm uses modern-only directives (`.set gp=64`, `.internal`). `include/macro.inc` had its `.internal _MACRO_INC_GUARD` line removed so KMC `as` parses it cleanly; the `.set _MACRO_INC_GUARD, 1` on the next line still guards the include. `permuter_settings.toml` mirrors the Makefile's src/% pipeline.
- **Spot-check via byte-level `cmp` only**: `mips-linux-gnu-objdump` renders both `0x...1021` and `0x...1025` as `move v0,zero`, so mnemonic-level diff silently false-positives. Always `cmp` raw `.text` bytes.
- **Match finalization is three steps**: inline body into `src/<seg>.c` → `clang-format -i src/<seg>.c` → full `make` until ROM SHA-1 matches baserom. Spot-check passing ≠ ROM matching; the final `make` is what proves the match. See memory `match-finalization-checklist`. **Gate the `sha1sum` on `make` actually succeeding** — confirm the `build/mariogolf64.z64: OK` line / exit 0 first. A failed link leaves the *previous* `.z64` in `build/`, so `sha1sum` on that stale ROM false-positives a "match" (this masked a broken build mid-debug on 2026-06-11).
- **Never run clang-format on library code under `src/libultra/` or `src/libkmc/`.** These files are verbatim copies from upstream (`~/development/repos/libultra_modern/`, `~/development/repos/libkmc/`); reformatting them defeats cross-referencing. Both dirs carry a local `.clang-format` with `DisableFormat: true` as a belt-and-suspenders. When the execution loop finalizes a match into one of these dirs, skip the clang-format step.
- **Upstream-mirror pattern for SDK functions**: when a function address maps to a known SDK upstream — libultra (`~/development/repos/libultra_modern/src/`), libkmc (`~/development/repos/libkmc/src/`), or libnusys (`~/development/repos/n64sdkmod/packages/libnusys/usr/src/PR/libsrc/nusys-2.07/nusys/src/`) — strongly prefer **copying the upstream source verbatim** over hand-decompiling. **Verbatim means verbatim** — keep dead `#ifdef _DEBUG` blocks and their *unconditional* companion `#include`s; they compile out because `_DEBUG` is undefined (e.g. `createmesgqueue.c`'s `__osError` block, `visetmode.c`'s `assert(modep != NULL)` block). When such a block references a header missing from the tree, **copy the companion header** rather than trimming the include (a sibling that didn't need it — `vigetcurrcontext.c` — is not the verbatim template). **The project source path mirrors the upstream path exactly**: take the upstream's path relative to its `src/` root and prepend `src/lib<name>/`. So upstream `libultra_modern/src/shared/system/afterprenmi.c` → project `src/libultra/shared/system/afterprenmi.c`; upstream `libultra_modern/src/monegi/vi/vigetcurrcontext.c` → project `src/libultra/monegi/vi/vigetcurrcontext.c`; upstream `libkmc/src/matherr.c` → project `src/libkmc/matherr.c`; upstream `nusys-2.07/nusys/src/mainlib/nugfxswapcfb.c` (libnusys src root is `…/nusys-2.07/nusys/src/`) → project `src/libnusys/mainlib/nugfxswapcfb.c`. The yaml subseg path qualifier matches: `[0x<seg>, c, libultra/shared/system/afterprenmi]`. Do NOT collapse upstream variant dirs (`shared/`, `monegi/`, `nintendo/`) into a flat `libultra/<subdir>/` — the variant carries information about which SDK branch the function came from. Companion headers go under `include/libultra/`, `include/libkmc/`, or `include/libnusys/` mirroring their upstream `include/` layout the same way; CFLAGS adds `-I include/libultra -I include/libultra/internal -I include/libkmc -I include/libnusys` so canonical `<PR/x.h>` / `"viint.h"` / `<math.h>` / `<nusys.h>` includes resolve unchanged. The libnusys band was unlocked in S15 by copying its single self-contained `nusys.h` (deps `<ultra64.h>`+`<PR/gs2dex.h>`, both already in-tree) to `include/libnusys/nusys.h` and adding the `-I include/libnusys` path — one paid-once enabler that turns the whole nuGfx*/nuCont* band into cheap cold mirrors. **Include-resolvability is a pick/DoR hazard, though**: the audio band (`alCopy`, `alHeapInit`, …) uses bare `#include <libaudio.h>` (the header sits at `include/libultra/PR/libaudio.h`), which does **not** resolve under the current `-I` set — there is no `-I include/libultra/PR`. Prefer `<PR/x.h>`-style upstreams (`monegi/`) until that `-I` path is deliberately added; flagging an audio leaf as a clean flip without it is a false-clean (the verbatim copy won't compile). `tools/pick_target.py` now surfaces this automatically as the **`needs-header:<inc>`** hazard — it greps every upstream `#include` against the project `-I` set (`-nostdinc`, so every header must come from `include/{,libultra,libultra/internal,libkmc}`) and flags any that don't resolve. The flag covers **both** companion-include cases, and the gate reads it to decide which: a **missing-but-copyable** header → copy it in the execution middle (`assert.h` for `visetmode.c`); an **unindexed `-I` path** → a deferred Makefile enabler, defer the pick unless the PO adds the path (the audio band's `<libaudio.h>` at `include/libultra/PR/`). (The grep is `#ifdef`-blind, so a dead-`_DEBUG` include can over-flag — the gate confirms against the upstream.) The Makefile's recursive `find` over `src/` automatically picks up nested paths. The execution loop's upstream-mirror branch handles this flow. **Open-band fast-path (S14)**: once a band has **≥2 banked siblings** (e.g. `monegi/thread/`, `monegi/vi/`), a candidate that `pick_target.py` reports with **no hazard** may skip the agent's *manual per-ref symbol-presence re-grep* at the gate — `pick_target.py`'s own ref-grep is authoritative, so the redundant manual check is the only thing dropped. The fast-path **never overrides a flagged hazard**: a `defines-data` / `refs-unplaced` / `needs-header` flag still routes normally even inside a warm band (e.g. `__osDequeueThread` lives in the warm thread band yet is a `defines-data` false-clean — it does **not** fast-path). The gate build-check (`make extract && make`) remains the load-bearing validation regardless.
- **File-scope `static` blocks the upstream mirror**: if the upstream `.c` declares a file-scope `static <type> <name>;` (BSS global), KMC GCC emits a section-relative `.bss` reloc instead of a symbol reloc. The linker can't place rand.o-style local BSS at the splat-side `next` slot, so the 0x10-aligned chunk inserts ahead and shifts every downstream BSS symbol — every code reloc into that range breaks (saw 10,853 ROM bytes diff for `rand`). The execution loop's upstream-mirror pre-flight catches this (**BSS-layout-conflict**) and falls to the classical loop with the `static` **dropped** in the C body — the linker then resolves to the splat-side global at the correct vram. (Function-local `static` is fine; only file-scope is the problem.)
- **Multi-function-segment splitting (at the gate)**: splat's auto-extraction often packs several functions — sometimes from different upstream files — into a single `[0x<rom>, asm]` subseg. The 0x8E620 case bundled `rand`+`srand` (upstream `libkmc/rand.c`) with `_xsincos`+`sin`+`cos`+`tan` (upstream `libkmc/sin.c`). When more than one upstream file lives in the same subseg, **split the subseg** before flipping rather than flipping the whole block — insert a new `[0x<offset>, asm]` line at the boundary between the upstream files. Two cases: (a) **upstream-mirror** on the first file — flip the first chunk to `[0x<seg>, c, lib<name>/<basename>]`, leave the second chunk as `asm` for a follow-up run; (b) **classical loop** (e.g., when statics block the mirror) — flip the first chunk to `[0x<seg>, c]` so the splat scaffold for `src/<seg>.c` only carries `INCLUDE_ASM` stubs for the functions you're actively targeting. Address splitting at function-aligned boundaries (read the first instruction's vram from the asm). Same principle applies to overlay-BSS multi-file packing.
- **libkmc compile profile is `-O`, libultra is `-O2`**: libkmc upstream was built with `gcc -O` (per `~/development/repos/libkmc/src/genn64.bat`), not `-O2`. The instruction scheduler differs between the two: at `-O` rand.c stores `next` between the two `addiu` ops; at `-O2` it gets pushed to the end. Same C, same compiler, different `-O` level → byte mismatch. The Makefile carves this via `LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))` and a `$(BUILD_DIR)/$(SRC_DIR)/libkmc/%.o` pattern rule whose specificity beats the generic `src/%` rule. libultra continues to use `-O2`. **If a libkmc match locks at ~0.9 percent across every C rewrite, the diff is almost certainly scheduler-level — verify by compiling `nonmatchings/<func>/base.c` with `-O` directly before iterating further on the C.** `decomp_loop.py` auto-applies the `-O` profile when a matching `src/libkmc/<basename>.c` exists for the target's segment.
- **Workflow retrospection**: each function the execution loop banks contributes a numbered "Suggested workflow improvements" entry. These are *recorded* into `SPRINT.md`'s suggestion buffer and applied **only at the `/sprint-review` gate** — never mid-sprint — so the sprint's matching behavior stays fixed and its measurement stays clean (see `## Scrum operating model`). The retro is the single apply-point for edits to `CLAUDE.md` / `tools/*.py` / the two command files.

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)

- **Names (Ghidra → decomp)**: `make sync-names` pulls curated function + data names into `ghidra_symbols.txt` (run gate-only, at `/sprint-plan`). The agent edits `ghidra_symbols.txt` ONLY through this target — direct hand-edits remain forbidden. `symbol_addrs.txt` stays agent-curated (add-only); the sync skips any address already in it (the two files must be disjoint).
- **Names (decomp → Ghidra)**: the agent **writes** the curated name into `symbol_addrs.txt` directly (add-only; keep disjoint from `ghidra_symbols.txt`), then runs `make extract` + `make` to confirm the ROM stays green. Propagating the name back into the Ghidra workspace — `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` — remains a cross-repo follow-up (it modifies the separate Ghidra project), surfaced in the final response.
- **Structs**: agent pulls from Ghidra MCP, cross-checks against `re_tracking/structs.yml`, appends to `include/structs.h` (created on first use; flat layout, reshape later). Discrepancies between MCP-live and the snapshot are surfaced in the final response and block the write.
- **Memory map**: read-only on both sides for the agent.
- **Forbidden agent edits**: `ghidra_symbols.txt` (hand-edit forbidden; modify only via `make sync-names`), `mariogolf64.ld`, `re_tracking/*.yml`, `reloc_addrs.txt`. **Agent-editable** (boundary moved 2026-06-11): `mariogolf64.yaml` subseg flip/split/path-qualifier lines only (never segment/memory-map structure), and `symbol_addrs.txt` add-only (never delete; keep disjoint from `ghidra_symbols.txt`).

--- 

# Coding Guidelines

## Prime directive: manage complexity

No one can hold a whole nontrivial system in their head. Every rule below exists
to **reduce how much you must think about at one time**. When choosing between
options, pick the one that leaves less to keep in mind.

- Minimize *accidental* complexity (introduced by our choices); partition the
  *essential* complexity (inherent to the problem) into brain-sized pieces.
- **Write for people first, the machine second.** Code is read far more often
  than written — optimize for the reader, not for typing speed.
- **Program *into* your language, not *in* it.** Decide what you want, then
  express it with available tools; compensate for missing features with
  conventions, not by limiting your thinking.
- **If it's hard, it's probably wrong.** "Tricky code," code that resists a
  clean name, comment, or test, is a warning sign — simplify instead of pressing on.
- **Be eclectic, not dogmatic.** Heuristics, not commandments. Hold conventions
  as tools you can drop when a better one fits.
- Make code so simple there are obviously no defects, not so complex there are no
  obvious defects.

---

## Naming

- The name should fully and accurately describe what the thing **represents**.
  If you can say in words what it is, that is usually the best name.
- Name the **problem**, not the mechanics: `employeeData`, not `inputRec`.
- Be specific. Vague names usable for anything (`x`, `temp`, `data`, `flag`,
  `handle`, `process`) signal a vague design.
- Length follows scope: short for tiny/local scope, longer for wide scope.
  ~10–16 chars is a good target for most names.
- Put computed-value qualifiers last: `totalCost`, `customerCount`, `maxLength`.
- Use `count` (a total) and `index` (one element), not the ambiguous `num`.
- Use precise opposites consistently: begin/end, first/last, min/max, next/prev,
  old/new, source/target, add/remove, get/set, open/close.
- Booleans read as true/false and are stated positively: `done`, `found`,
  `success` — never `notDone`. Avoid `flag`; name the condition.
- Loop indices `i`/`j`/`k` only in short, non-nested loops; meaningful names when
  nested, long, or used outside the loop.
- Adopt one convention and apply it everywhere — **any** convention beats none.
  It should distinguish local vs. member vs. global, and constants/types/variables.
- Avoid: misleading names, names differing by 1–2 chars or only by case,
  numerals (`data1`, `data2`), homophones, hard-to-read characters (`l`/`1`,
  `O`/`0`), and abbreviations that fail the "read it aloud over the phone" test.

```c
/* Bad: mechanics, no meaning */          /* Good: states the problem */
x = x - xx;                               balance = balance - lastPayment;
```

---

## Variables and data

- Give every variable the **smallest scope** that works. Start restrictive
  (loop → function → file → global, last resort) and widen only when forced.
- Minimize **live time** (lines from first to last use): keep references close
  together so the window where the value can be wrongly altered stays small.
- Declare each variable **near its first use** and initialize it there; never in
  a block at the top.
- **One variable, one purpose.** Don't recycle a `temp` across unrelated jobs,
  and don't overload a value with a hidden second meaning.
- Mark values that shouldn't change as `const`.
- Replace **magic numbers/strings** with named constants. Only `0` and `1`
  belong as bare literals.
- Guard every division against a zero denominator. Watch integer overflow and
  truncation, **including in intermediate results**.
- **Never compare floats for exact equality** — compare within a tolerance.
- Keep array/string indices in bounds; check first, middle, and last endpoints
  for off-by-one.
- Prefer enums over loose constants for a fixed value set; reserve the first slot
  for "invalid" and handle the unexpected case.

```c
/* Magic number → named constant */
for (i = 0; i < MAX_EMPLOYEES; i++) { ... }      /* not: i < 100 */

/* Never test floats with == ; compare within a tolerance */
static const double EPSILON = 1e-9;
int approx_equal(double a, double b) { return fabs(a - b) < EPSILON; }

/* One purpose per variable: don't reuse `temp` for two unrelated jobs */
discriminant = sqrt(b*b - 4*a*c);   /* not: temp = sqrt(...); ... temp = swap */
```

---

## Functions

- Create a function for a sound reason: to **name an abstraction**, remove
  duplication, hide a sequence or a complex test, or reduce complexity. Deep
  nesting is a signal to extract one.
- Aim for **functional cohesion**: a function does one and only one thing.
- The name describes **everything** it does, including side effects. If the name
  needs "and," remove the side effect rather than lengthening the name. Avoid
  vague verbs (`handle`, `process`, `dealWith`).
- Let length follow the logic, not an arbitrary cap — but be suspicious past a
  screenful.
- Parameters: order **input → modify → output**; keep that order consistent
  across similar functions; put status/error params last.
- Limit parameters to ~7. Needing more consistently means coupling is too tight —
  group the data.
- Use every parameter. Don't reuse an input parameter as a working variable —
  copy to a local; mark inputs `const`.
- Ensure every path returns a valid value (initialize the return value up top).
- Never return a pointer/reference to a local.

```c
/* Don't mutate input params; copy to a working local */
int scale(const int input) {
    int working = input * current_multiplier(input);
    working      = working + current_adder(working);
    return working;            /* `input` still holds the original */
}
```

---

## Modules and data abstraction

A "class" here means any **module**: data plus the functions that own it (an
Abstract Data Type). The principles apply whether or not your language has classes.

- Model each module as an **ADT**: expose operations, hide representation. Callers
  should never touch the internal data.
- **Information hiding** — for every module ask "what should I hide?" Hide two
  things: complexity, and decisions likely to change.
- Hide a decision behind a function and a type, not scattered literals:

```c
typedef int IdType;                 /* hide the representation */
IdType id = new_id();               /* hide the creation policy */
/* The body of new_id() is the ONE place that changes when IDs must become
   non-sequential, range-reserved, thread-safe, etc. Compare the leaky version:
   id = ++g_max_id;  — duplicated everywhere, impossible to evolve. */
```

- Give each module **one** consistent abstraction. If you can't name what it
  abstracts, or it does two unrelated things, split it.
- Minimize accessibility; expose data through accessor functions, never raw.
- **Prefer containment ("has-a") over inheritance ("is-a").** Use inheritance
  only for genuine specialization, and only when every subtype honors the base's
  full contract (Liskov substitution). Keep hierarchies shallow (2–3 levels).
- Replace a repeated `switch` on a type field with one dispatch point (e.g. a
  function-pointer table).
- **Avoid global data.** Make variables local first; if data must be shared,
  reach it only through access functions — they give one control point,
  validation on every access, and an easy path to a real ADT later.

---

## Control flow

- **Make the nominal path obvious.** Put the normal case right after the `if`,
  stack error/exception cases below. Don't write an empty `if` with work in the
  `else` — negate the test.
- Cover every case: give `if/else` chains a final `else`, and `switch` a
  `default`, that catches the unexpected value and reports it to the developer.
- Use **guard clauses** (early returns) to check error cases up front and keep the
  nominal code un-indented:

```c
if (!valid_name(name)) return ERR_NAME;
if (!file_open(name))  return ERR_OPEN;
if (!key_valid(key))   return ERR_KEY;
/* real work lives here, at the top indent level */
```

- Treat each loop as a **black box**: one entry, control conditions stated from
  outside, assured termination (mentally run the first, a middle, and the last
  iteration). Keep the body short enough to see at once; extract long bodies.
- **Always brace** conditional and loop bodies, even one-liners — layout must not
  be able to lie about what the body is (see Layout).
- Use `break`/`continue`/early `return` to *simplify*, sparingly; each weakens the
  black-box property. Avoid `goto`.
- For recursion, ensure a base case stops it; prefer iteration for simple cases.
- Replace complicated `if`/`switch` logic (or inheritance chains) with a
  **lookup table** — logic scales badly, data stays flat:

```c
static const int days_per_month[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
days = days_per_month[month - 1];     /* not a 12-branch if/else */
```

- Simplify boolean expressions: name complex tests with an intermediate boolean
  or a well-named predicate function; state them positively (DeMorgan); fully
  parenthesize — don't rely on precedence; write comparisons in number-line order
  (`MIN <= x && x <= MAX`).
- Flatten nesting (cap ~3 levels) and keep **cyclomatic complexity low** (~≤10:
  count 1 + each `if`/`while`/`for`/`case`/`&&`/`||`). High count → extract a
  function.

---

## Defensive programming

- **Validate all data from any external source** (user, file, network, another
  module) — range, length, format. Reject or sanitize at the boundary.
- Build a **barricade**: validate data as it crosses into trusted code. Outside
  is dirty (use error handling); inside is clean (use assertions).
- **Assertions** are for conditions that must *never* happen (a bug). **Error
  handling** is for conditions you *expect* but hope don't occur. Keep
  side-effecting code out of assertions.
- Decide **robustness vs. correctness** deliberately: keep running with a
  best-effort value (consumer software) vs. never produce a wrong result
  (safety-critical). Apply it consistently.
- Always check returned error codes / call results, even when failure "can't"
  happen. No empty catch blocks.
- Use exceptions sparingly — same bar as assertions — and throw at the
  abstraction level of the interface, with full context.
- **Fail loud in development, soft in production.** Add lavish debug-only checks;
  make bugs glaring while building so the shipped program can degrade gracefully.

---

## Comments and layout

- **Self-documenting code first.** Good names, structure, named constants, and
  simple control flow carry most of the documentation. Fix bad code; don't paper
  over it with comments.
- Comment **intent and summary ("why")**, never restate the code ("what"). A
  wrong or redundant comment is worse than none.
- A good comment is at the level you'd *name a function* doing the same thing —
  if you can name it cleanly, consider extracting that function.
- Keep comments next to their code and updated; document surprises, workarounds,
  and the reason behind any deliberate oddity (quantify perf tricks so no one
  "fixes" them).

```c
if (account_type == ACCOUNT_NEW)   /* if establishing a new account  — intent */
/* not:  if (account_flag == 0)       // if account flag is zero      — mechanics */
```

- **Layout reveals logical structure** — that is its job; looking pretty is
  secondary. **Consistency matters more than which style** you pick.
- Indent subordinate code (2–4 spaces). Separate "paragraphs" of related
  statements with blank lines. Use whitespace within expressions.
- One statement per line; one declaration per line; no multiple side effects per
  line.
- Brace single-statement bodies so layout and logic can't diverge:

```c
/* No braces: indentation says 3 lines run each pass; the compiler runs only the
   first in the loop, the other two once. The visual structure lies. */
for (i = 0; i < n; i++) {
    int tmp   = left[i];
    left[i]   = right[i];
    right[i]  = tmp;
}
```

---

## Design heuristics

- **Loose coupling**: few, small, visible, flexible connections. Pass the minimum
  a function needs through its parameter list, not via shared/global state.

```c
/* Tight: forces caller to build a whole Employee.
   Loose: depends only on what it actually uses. */
double vacation_days(Date hire_date, JobClass job_class);
```

- **Strong cohesion**: everything in a module serves one central purpose.
- **Design for change**: identify what's likely to change (business rules, I/O
  formats, platform/vendor specifics, status values) and isolate each behind one
  module so a change hits one place. Use an enum, not a bool, for status that may
  grow new states.
- Favor **high fan-in** (reuse low-level utilities) and **low-to-medium fan-out**
  (a module using >~7 others is doing too much).
- Keep the design **lean** — no speculative "might-need-it" parts. Build the code
  the requirements demand, clearly.
- **One Right Place**: one place to find a given piece of code, one place to make
  a given change.
- Design is **iterative and heuristic** — there's no single right answer. Try
  more than one decomposition. Reuse known patterns, but don't force-fit them.
- Prototype to answer a *specific* design question with throwaway code; keep it
  junk so it isn't reused.

---

## Writing, evolving, and verifying code

**Grow code from intent.** Design a non-trivial function in intent-level
pseudocode first (what, not how). Review the pseudocode — iterating is cheap
before you're invested in code. Then turn each line into a comment and fill real
code beneath it. If one line explodes into too much code, extract a function (its
name falls out of the pseudocode).

**Refactor purposefully.** Every change should leave internal quality *better*.

- Small, **behavior-preserving** steps, one at a time; recompile and retest after
  each; keep tests green.
- **Never refactor and add functionality at once.** Save the original first.
- Refactor opportunistically — when adding a feature or fixing a bug, and in the
  error-prone/high-complexity areas first.
- Treat even one-line changes as risky (>50% first-attempt error rate); review them.
- **Code smells** to act on: duplicated code; an over-long function; deep nesting;
  poor cohesion; too many parameters; related data not grouped; a function more
  interested in another module than its own; a primitive overloaded for a richer
  concept (e.g. `int` for money); comments propping up bad code; global variables;
  heavy setup/takedown around a call (the interface is wrong).

```c
/* Smell: many setters before, many getters after → wrong interface */
process_withdrawal(id, balance, amount, date);     /* fix: pass what it needs */
```

**Debug scientifically.** The goal is the *cause*, not the symptom.

- Assume the bug is yours. Reproduce it reliably; shrink to the simplest case.
- Form a hypothesis from all the data; understand the problem well enough to
  predict it before you change anything.
- **Fix the root cause, never special-case the output.** Make one change at a
  time, for a reason you understand — no superstitious "+1 until it works."
- Add a regression test that exposes the bug, then look for the same bug elsewhere
  (defects cluster).

```c
/* Symptom patch (wrong): the real defect is sum[] never initialized to 0 */
if (client == 45) sum[45] += 3.45;     /* do NOT do this — fix the init instead */
```

**Test as you go.** Testing *measures* quality; it can't create it.

- Write tests early (ideally first); keep them as a regression suite.
- Aim for branch coverage, not just line coverage. Derive the minimum cases from
  the branch count (1 + one per decision).
- Test **boundaries** (just below / at / just above each limit) and **bad data**
  (none, too much, wrong kind/size, uninitialized), not just clean inputs.
- Focus on error-prone areas — defects cluster (~80% in ~20% of routines).

**Combine quality techniques; prevention beats detection.** No single
defect-finding method catches more than ~75% — **reviews/inspections catch what
testing can't** and find defects earlier and cheaper. Review every change.
Improving quality *lowers* total cost; it doesn't trade against it.

**Performance comes last.** Build clean, correct, modular code first.

- Don't optimize as you go. Most run time lives in a small fraction of code
  (80/20).
- **Measure before tuning, and again after** — intuition about hot spots is
  usually wrong, and many "optimizations" do nothing or backfire (and vary by
  compiler/platform). Keep the clean version unless a measured need justifies the
  trade.

```c
/* The straightforward version. A hand "pointer-walk" rewrite measured ZERO gain
   here — the optimizer already does it. Don't trade clarity for an unmeasured guess. */
for (row = 0; row < rows; row++)
    for (col = 0; col < cols; col++)
        sum += matrix[row][col];
```

---

# C Naming Conventions Guide

## General Rules

| Identifier Type                   | Convention   | Example                  |
| --------------------------------- | ------------ | ------------------------ |
| Namespace-like Prefixes / Modules | `lower_case` | `audio_system`, `input_` |
| Struct                            | `CamelCase`  | `PacketHeader`           |
| Enum                              | `CamelCase`  | `ConnectionState`        |
| Enum Constant                     | `UPPER_CASE` | `CONNECTION_CONNECTED`   |
| Function                          | `lower_case` | `initialize_system()`    |
| Variable                          | `lower_case` | `frame_count`            |
| Parameter                         | `lower_case` | `host_name`              |
| Global Constant                   | `UPPER_CASE` | `MAX_CONNECTIONS`        |
| Macro Definition                  | `UPPER_CASE` | `BUFFER_SIZE`            |

---

# Constants

## Global Constants

* Use `UPPER_CASE`
* No `k` prefix

```c
static const int MAX_CONNECTIONS = 16;
static const float GRAVITY_SCALE = 9.81f;
```

## Static Constants

* Use `UPPER_CASE` when they have global/static storage duration

```c
static const int DEFAULT_PORT = 8080;
```

## Local Constants

* Use `lower_case`

```c
void retry_operation(void) {
    const int max_retries = 5;
}
```

---

# Macros

* Use `UPPER_CASE`

```c
#define BUFFER_SIZE 1024
#define MAX(a, b) ((a) > (b) ? (a) : (b))
```

---

# Struct Naming

* Struct names use `CamelCase`

```c
typedef struct PacketHeader {
    int packet_size;
    int packet_type;
} PacketHeader;
```

* Struct members use `lower_case`

```c
typedef struct AudioDevice {
    int sample_rate;
    int channel_count;
} AudioDevice;
```

---

# Enum Naming

* Enum type names use `CamelCase`
* Enumerator values use `UPPER_CASE`

```c
typedef enum ConnectionState {
    CONNECTION_DISCONNECTED,
    CONNECTION_CONNECTING,
    CONNECTION_CONNECTED
} ConnectionState;
```

---

# Functions

* Use `lower_case`
* Separate words with underscores
* Parameters use `lower_case`

```c
void initialize_system(void);
int open_connection(const char *host_name);
```

---

# Variables

## Local Variables

* Use `lower_case`

```c
int frame_count;
float delta_time;
```

## Function Parameters

* Use `lower_case`

```c
int open_connection(const char *host_name, int port_number);
```

## Global Variables

* Use `lower_case`

```c
int active_connection_count;
```

## Static Variables

* Use `lower_case`

```c
static int initialized;
```

---

# Example

```c
#include <stdbool.h>

static const int DEFAULT_CHANNELS = 2;

typedef enum PlaybackState {
    PLAYBACK_STOPPED,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
} PlaybackState;

typedef struct AudioManager {
    int channel_count;
    bool initialized;
} AudioManager;

void initialize_system(AudioManager *manager) {
    manager->channel_count = DEFAULT_CHANNELS;
    manager->initialized = true;
}
```
