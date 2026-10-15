---
type: Workflow
title: Decomp execution details
description: Defines evidence authority, residual diagnosis, and source-integration constraints without duplicating the native workflow.
sources:
  - resource: ../../tools/decomp_loop.py
    title: Isolated compile and diff implementation
  - resource: ../../tools/verify-rom.sh
    title: ROM verification gate
  - resource: ../../tools/rom_build.py
    title: Profile-aware build implementation
---

# Execution details and every-function conventions

The [native decomp skill](../../.omp/skills/mg64-decomp/SKILL.md) owns
select/iterate/bank/review. This reference owns tool limits and technical decisions;
[tooling](../tooling.md) owns command discovery, [build](../build.md) owns setup and
profiles, and [fan-out](fan-out.md) owns delegated analysis. Read the section needed
for the current question, not every reference before starting.

## Toolchain

Splat extracts function boundaries and assembly; KMC GCC 2.7.2 and the configured
assemblers reproduce the ROM. m2c translates assembly into a seed, asm-differ
compares isolated objects, and decomp-permuter searches source variants. The
[bank-aware Ghidra database](../ghidra.md) supplies optional types and shape evidence.
Use existing tools before hand-rolling comparisons: `cmpfn.sh`, `seg_diff.py`,
`allocno_report.py`, `loop_window.py`, and the display-list decoders answer distinct
questions. Compiler profiles come from `tools/build_profiles.json` via `rom_build.py`.

## Execution loop (inline)

Follow the [skill](../../.omp/skills/mg64-decomp/SKILL.md#iterate), using the
[CLI catalog](../tooling.md#iterate-a-function). The artifact contract is:

| Artifact under `nonmatchings/<fn>/` | Role |
| --- | --- |
| `base.c` | Editable candidate, with declarations and seed body; not a banked source. |
| `target.s`, `target.rodata.s` | Assembly and optional data evidence; read separately, not duplicated in `base.c`. |
| `m2c.c`, `ctx.c`, `ctx_ghidra.c` | Generated seed/context inputs; regenerate rather than hand-edit. |
| `ghidra.c`, `ghidra_ctx.c`, `ghidra.json` | Optional pseudocode, transitive types, and ROM/bank provenance. |
| `current.o` | Fresh isolated compile; iteration evidence only. |
| `measurements/<hash>.json`, `measurements/<hash>.c` | Immutable observed score/diagnostic paired with its measured source; includes target-dump, profile and toolchain identities. Unbanked. |

### Source recovery

`seed_c.py` reports `body_source` as `m2c` or `ghidra` when a translator supplies a body.
If neither does, it exits nonzero, retains assembly/diagnostics, and does not write `base.c`.
Success means recovered text, not valid C or matching behavior. Reconcile the affected region
against assembly before interpreting its score:

- Preserve branch and switch destinations; check stack-output buffers and variadic argument
  binding. Fix translation errors rather than compiling an empty or simplified substitute.
- Prefer a banked callee definition. Otherwise trace incoming argument registers through the
  whole callee before clobber, including forwarded arguments, and corroborate explicit caller
  setup/delay slots and return uses. A prologue or stale callsite register is insufficient.
  An unused formal can still require a caller argument; report unresolved types instead of
  inventing `(void)` or a return type.
- Check load/store width, signedness, byte offsets, and stride before introducing typed
  pointers/aggregates. A decompiler's mutable global cursor is not an immutable packet base.
  Generated externs and source-local partial types remain hypotheses.
- For display lists, decode packet order and recover the SDK composite macros first. Execute
  a throwaway host harness against this repository's F3DEX2 `gbi.h`, comparing emitted words
  **and cursor advancement** with ROM-derived packets, including relevant clipping/branch
  boundaries. A better isolated score cannot validate a macro's semantics.

Use the smallest correct parent/type context. Fetch [live Ghidra context](../ghidra.md#agent-access-and-context)
only to answer an unresolved question; a small asm-first leaf needs neither Ghidra nor m2c.
Keep pseudocode and assembly in their own artifacts; do not overwrite a measured carry to reseed it.

### Measurement

On a compile failure, read and fix the diagnostic before another compile. On a residual,
change one evidence-backed source lever and measure it against the same profile. Do not
repeat unchanged failures or stop solely because an arbitrary retry count was reached.
At score zero, integrate provisionally, build the host object, and spot-check the target
function against the isolated candidate before the full-ROM gate; the in-tree candidate
cannot be compared before it has been integrated. Do not compare an entire multi-function
TU's `.text` with a one-function seed. Failed integration remains unbanked.

The loop emits `measurement_path` for successful and failed compiles. Old source/score pairs
are not overwritten; edits detected during measurement invalidate attribution. Header/context
inputs are not archived, so the record's limitations require regeneration and a fresh measurement
on resume. Use it as evidence in the existing WIP note, not a second task ledger.

For a carry, confirm the live stub, parent TU, and cited source before compiling. If `base.c`
is missing or differs from the cited measurement, recover its immutable `.c` snapshot and
regenerate the required context; do not overwrite a different live candidate. If no snapshot
survives, reconstruct the documented source against assembly and establish a new baseline.
Refresh with `uv run --locked python tools/decomp_loop.py --func <fn>`; a note's score or
compiler-wall verdict does not transfer to a different base.

For bounded queues, cross-checkout references, an isolated m2c prepass, and deadlines, use the
[campaign guide](../campaigns.md). Its output remains iteration evidence; the native skill owns
execution and the parent retains shared-resource ownership.

## Candidate pricing

Ranker tags classify evidence, not difficulty or bank probability. `fresh` is a ceiling;
FP scheduling, value selection, and allocation walls may lack assembly-detectable tells.
Prefer demonstrated leverage: a banked sibling's shape/types, a pinned upstream mirror,
or a measured carry for which a new structural transformation targets the cited pass.
Check carried and nested evidence before choosing hand-mined leaves.

- A `dl-emitter` is a reconstruction task, not automatically a wall. Equal command-word
  multisets (`dl-twin`) suggest useful bank order, not identical source.
- Price floats by their consumer (`fp-coord`, `fp-sched`, `fp-mixed`), not register-name
  grep or raw FP count. Inspect constants for fdlibm provenance before pricing a leaf
  as novel FP logic.
- A carveable jump table can be cheap; alignment, pool cohorts, and intervening
  asm-owned data can block integration even when the isolated body matches.
- A single-callee wrapper may reuse a banked helper despite a large call count.
- `hasm` is permanent assembly. A pressure-looking leaf may instead be a nested child
  that must bank with its parent; use `--nested-check`.

Historical bank rates and pool counts belong in [sprint evidence](../history.md), not
in a forecast printed on every selection. `-n` bounds candidate output; `--all` includes
otherwise hidden classes, and `--json` gives records without prose. File-closure mode
uses `-n` as its maximum stubs per host, not a candidate-row limit.

## Oracles

**Only `tools/verify-rom.sh` exiting zero authorizes banking.**

| Tool/evidence | Use and limit |
| --- | --- |
| `verify-rom.sh [--extract] [--build-dir <path>]` | Successful configured build plus ROM SHA-1. Requires build success before hashing, so a stale ROM cannot pass after build failure. |
| `decomp_loop.py` score/percent | Isolated compile signal. Zero is a candidate, not proof; normalized labels/addresses can hide byte differences. `--score-only` deliberately omits mismatch details. |
| `cmpfn.sh <fn> [<obj>]` | Instruction stream/counts while extracted target assembly exists; extraction may remove it after integration. Normalization is not byte proof. |
| Fresh `objdump -drz --disassemble=<fn> <obj>` and `objdump -r` | Use on both isolated and integrated objects after the host build. Keep `z` for nops; interpret actual relocation targets/addends, not raw unlinked call fields. |
| Linked asm-differ view | Relocation-resolved iteration evidence after a fresh full build. Object-only builds do not refresh link maps. |
| Function-byte spot-check | Isolated versus integrated code-generation diagnostic at score zero. Compare the same function/range, not whole TUs; relocation addends and assembler encodings require interpretation. Full-ROM verification resolves link/layout effects. |
| decomp-permuter | Search, not acceptance. Apply [source/residual eligibility](#permuter-eligibility) before import. Internal branch targets can normalize to a false zero; inspect real encodings. |
| GCC `-dL` (`.loop`) | Loop hoist, induction, strength-reduction decisions. Read measured savings/lifetime/counts rather than estimate them; filter the whole-file dump. |
| GCC `-dg` (`.greg`), `-dl` (`.lreg`), `allocno_report.py` | Allocation order, conflicts, preferences, dispositions, and priority arithmetic. Interpret only after structural/count deficits are fixed. |
| GCC `-dS` (`.sched`), `-dR` (`.sched2`), `-dd` (`.dbr`) | Identify the first pass where ordering differs; inspect dependency links and ready lists, not just final assembly. |
| Ghidra pseudocode/types | Revisable shape/type hypotheses. Select the exact bank plus ROM provenance because overlay VRAM repeats; FP ABI and source-local partial types may be wrong. |
| `dl_decode.py`, `rdp_word.py`, `combine_word.py`, `gbi_match.py` | Packet/list order and SDK macro fields. Instruction order can differ from packet order; runtime words remain symbolic. Composite macros must be checked with `p++`, not `p`, because they reevaluate `pkt`. |
| Assembly header size and mnemonic histograms | Header bytes divided by four give ROM instruction count; adjacent VRAM deltas do not. Normalize aliases. Histograms identify structural deltas but cannot locate them alone. |

A nonzero score with no displayed mismatch is not proof of a source wall: check whether
`--score-only` suppressed detail, inspect fresh instruction counts and real operands,
and distinguish isolation/relocation differences from code-generation differences.

### Permuter eligibility

Start from faithful, integration-eligible C under the parent TU's profile. Diagnostic-only
asm barriers, explicit register bindings, undefined behavior, or omitted calls are not search
bases, regardless of score. Remove diagnostic scaffolding and remeasure the clean source first.

Then inspect the actual residual. `percent >= 0.97`, or exact instruction count with a
single-operand residual, is a search-suitability signal—not an automatic launch instruction.
Prefer a source lever for structural deficits or pervasive register/FP-schedule permutations.
For displaced scheduling blocks, inspect raw instructions/counts rather than trusting normalized
alignment or percent alone. A bounded search can narrow a residual; expiry cannot prove a wall.

The run wrapper uses the permuter's normal exploration; pass `--best-only` explicitly when
monotonic search is justified. Setup/run/score synchronize the locked environment once; compiler,
assembler, and objdump callbacks reuse that interpreter without resolving dependencies per candidate.

## Residual diagnosis

Localize by region/loop before interpreting alignment hunks. Opposite count deltas in
adjacent regions can be one moved sequence. Use `seg_diff.py` or mnemonic comparisons
before reading a large permutation diff.

1. **Count or shape differs:** fix control flow, types, prototypes, induction variables,
   and value lifetimes first. An extra saved register alone does not establish pressure.
2. **Exact count, registers differ:** inspect allocation order with `allocno_report.py`.
   A priority gap suggests reference-count/live-length steering; a tie suggests allocno
   order/scope. Separate a pointer or index spanning two loops before blaming coloring.
3. **Exact count, order differs:** inspect scheduling and the relevant
   [residual playbook](../hazard-symptoms.md), not arbitrary register permutations.

Use [source levers](../levers.md) for measured transformations. A compiler-pass citation
explains a hypothesis, not impossibility: ask what its experiment did not measure.
A new structure can bypass the previously cited pass. Re-measure after structural
changes before reusing a rejected lever or an old permuter-eligibility verdict.

### Compiler-source escalation

Escalate the measured residual, not the whole function. Supply a reproducing source/profile,
the differing instruction window, and a question for the responsible pass: allocation,
scheduling/reorg, or assembler encoding/hazards. Parallel read-only slices use the
[fan-out contract](fan-out.md); the parent produces any missing diagnostics serially.

Use the existing profile-aware compiler interface for dumps, choosing only the relevant flags:

```sh
uv run --locked python tools/rom_build.py compile \
  --source nonmatchings/<fn>/base.c --output nonmatchings/<fn>/diagnostic.s \
  --profile-source src/<host>.c --nonmatching --assembly-only --extra-flag=-dSRd
```

KMC GCC writes dumps named after the input basename in the repository root, not beside
`--output`. Preserve the relevant before/after evidence or a reproducible command in the
existing WIP note, then remove only the temporary files you created. A compiler-source
explanation needs the corresponding dump evidence before it becomes a measured mechanism;
failure of one source form does not establish impossibility for every equivalent form.


## Conventions (every-sprint)

[AGENTS.md](../../AGENTS.md) owns immutable source/generated-file boundaries.
[Build profiles](../build.md#compiler-profiles-and-incremental-work) own flags and
paths; [C style](../coding-style.md) owns naming and formatting. In addition:

- Non-library source paths use `main/<stem>` or `overlay_<N>/<stem>`. SDK-template
  code belongs in its library tree; game-region mirrors can have the game profile,
  not the library profile. Preserve per-file overrides and include order.
- Mirror pinned source logic and names. Public SDK headers go under `include/<lib>/`;
  private headers mirror the vendor's `src/` layout under `src/<lib>/`. Keep
  `SortIncludes: Never`; formatting does not authorize changing mirror logic.
- A compiler-generated jump table couples C and rodata: carve only when the switch
  regenerates its symbol. Check emitted pools, actual relocations, duplicate data,
  and nested-function coupling before integration.
- Curated renames migrate C callers explicitly; extraction only rewrites assembly.
  Use `--extract` for metadata/layout changes. Header-only changes need the ROM
  verifier, not routine clean builds; stale dependents indicate a dependency defect.
- A file is complete when its last `INCLUDE_ASM` is gone. Empty auto-C leaves can make
  function count exceed initial stub count; count both when reporting progress.
- Keep matching sources on stubs when carrying unfinished candidates. Use the skill's
  [review/resume contract](../../.omp/skills/mg64-decomp/SKILL.md#review-and-resume) for durable
  evidence and the [permuter rules](#permuter-eligibility) before importing a candidate.
