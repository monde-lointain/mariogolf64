---
name: mg64-decomp
description: Mario Golf 64 decompilation — select, match, permute, bank, and review functions.
---

# Mario Golf 64 decomp workflow

Use this workflow for function selection, classical decompilation, upstream mirrors, bank decisions,
and review. The ROM SHA-1 against `baserom.z64` is the only bank authority: `tools/verify-rom.sh`
must succeed. All other comparisons are iteration signals. The invariants in
[.omp/RULES.md](../../RULES.md) apply throughout.

Run Python tools as `uv run --locked python tools/<tool>.py`; the default binary tree is `build/us`.

## Main before overlays

Complete the `main` code segment in `mariogolf64.yaml` before selecting, queuing, dispatching,
or integrating overlay decompilation. Scope by YAML bank membership, not a `src/main/` path or
function-name prefix: linked library code in `main` is part of this milestone. Preserve existing
overlay carries without advancing them. Read-only overlay references may inform a main target;
they do not make the overlay a work target.

Main is complete only when its YAML subsegments contain no remaining decompilation `asm`
subsegments, all of its C hosts (including library paths) have zero `INCLUDE_ASM` stubs, and
`tools/verify-rom.sh` succeeds on that integrated source state. Intentional `hasm` sources and
raw data subsegments are not C-decompilation backlog. Audit YAML and live sources before
declaring completion; ranker output is not a completeness oracle.

An empty fresh shortlist, deadline, carried wall, nested coupling, or pool/rodata blocker does
not permit switching to overlays. Expand main discovery to carried and coupled work, inspect
whole-subsegment and library hosts, and retain measured blockers in existing WIP/backlog notes.
If no main target is actionable, report the specific blocker rather than filling the queue
with overlays. Once the gate passes, select overlay work using exact bank and ROM provenance.

## Select

1. Rank with the [discovery commands](../../../docs/tooling.md#discover-an-increment): whole main
   packs (`pick_target.py --segment main -n 12`) and C-host leaves (`--loose-stubs main -n 12`).
   An empty fresh shortlist is not completion: rerun with `--include-stuck` and `--loose-stubs main --all`.
   Start from live ranker rows, not the WIP index: confirm the named `INCLUDE_ASM` and YAML host
   before opening a note. Size leaves from assembly headers, not VRAM deltas.
   Check every selected member with `--carried-check <fn> --nested-check <fn>`. `CARRIED-WALL`
   names a function-specific WIP note, not impossibility; reopen a carry only for a named new lever
   or changed evidence, then [refresh its measurement](../../../docs/workflow/loop.md#measurement).
   Prefer demonstrated sibling/upstream leverage ([reference discovery](../../../docs/campaigns.md));
   similarity and source presence do not establish a bank.
2. Route hazards through [the hazard index](../../../docs/hazard-index.md), then read only the selected
   playbook section. Consult [C coding style](../../../docs/coding-style.md) for source conventions.
3. Resolve the source subsegment in `mariogolf64.yaml`. Change only required flip/split/path lines.
   Preserve `INCLUDE_ASM` for unbanked functions. A compiler jump table couples source and rodata:
   carve it only when the C switch regenerates its symbol, and only where the host object's
   `.rodata` (`objdump -h`; KMC pads it to its 8-byte alignment) ends exactly at the carve boundary —
   a tail pad that overlaps the next assembly-owned constant shifts the whole image
   ([rodata alignment split](../../../docs/hazards.md#decomposed-one-tu-rodata-alignment-split-a-counter-case-to-the-8-point-decompose-gate)).
4. Keep candidate experiments isolated under `nonmatchings/<func>/`. For delegated analysis, use the
   [fan-out ownership and evidence contract](../../../docs/workflow/fan-out.md).

## Campaigns

For a campaign request, prepare a bounded queue with the [campaign CLI](../../../docs/campaigns.md)
and keep this session as the integration owner: execute targets serially, apply the main-completion
gate to every queue entry, and use native `task` only for independent reasoning. Queue identity,
deadline, and prepass rules live in that reference.

## Iterate

- For an upstream mirror, copy the pinned upstream source and required headers, then validate with
  `tools/verify-rom.sh`. Check names, calls, data references, and routed hazards first.
- For classical work, read extracted assembly first. Translate a small clear leaf directly into
  `nonmatchings/<func>/base.c`. When body/context recovery helps, use
  `tools/seed_c.py --func <func> --parent src/<seg>.c` and apply the
  [source-recovery gate](../../../docs/workflow/loop.md#source-recovery) before chasing a score;
  add `--ghidra` only for an unresolved context question. Measure with `tools/decomp_loop.py --func <func>`.
- Fix diagnostics before retrying. Classify the residual as structure/count, registers, or ordering
  and change one named [source lever](../../../docs/levers.md). For a register-only residual, read the
  allocator's numbers first (`tools/allocno_report.py nonmatchings/<func>/base.c <func>`) and name the
  reference count or live length that must move. Use the
  [permuter eligibility rule](../../../docs/workflow/loop.md#permuter-eligibility), not score alone;
  `run-permuter.sh` drives the newest imported case. A wall verdict belongs to its measured base.
- Score zero, or exact code generation with only confirmed linker-alias residuals, starts provisional
  integration: inline the body, remove its stub, format the changed range with clang-format, build the
  host object, and [spot-check the same function ranges](../../../docs/workflow/loop.md#oracles)
  before the full ROM gate. If integration fails, restore the stub and keep the candidate isolated.
- Use `tools/rom_build.py` and `tools/build_profiles.json` for the parent TU's exact profile.
  [Measurement records](../../../docs/workflow/loop.md#measurement) retain source/diagnostic pairs;
  source, profile, header, or context changes require a fresh measurement.

## Bank

A function is banked only after `tools/verify-rom.sh` exits successfully. It prints one verdict line
and logs the build to `build/us/verify-rom.log`; a mismatch names the first differing ROM offset.
An offset far from the candidate (the entry stub or another translation unit) with a clean function
diff means a layout shift: compare section sizes in `build/us/mariogolf64.map` before touching code.
Before renaming a function, update committed C callers. Stage source inputs only. A file is complete
only when all its `INCLUDE_ASM` stubs are gone. Keep unfinished work isolated and describe measured
residuals in `docs/wip/<func>.near-match.md`.

## Review and resume

Review the banked source and verifier result, and distinguish verified matches from carried
candidates. Count functions as well as remaining stubs: extraction may already emit empty leaves as C.
On resume, reconcile the current task state with committed changes and the tracked backlog first.

Persist only enough state to reproduce: target ROM/bank/host, exact candidate and profile,
measurement command and residual, verifier outcome or unbanked status, rejected hypotheses tied to
that base, and the next experiment. Existing WIP/backlog notes serve durable evidence; record the
current state, not a session narrative. Do not create parallel resume ledgers or reread complete
sprint history.

## Domain references

- [Execution details and tool authority](../../../docs/workflow/loop.md)
- [Candidate and symptom routing](../../../docs/index.md)
- [Build profiles and verification](../../../docs/build.md)
