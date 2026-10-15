---
name: mg64-decomp
description: Select, decompile, iterate, bank, and review Mario Golf 64 functions while preserving matching, source-edit, profile, and shared-resource constraints.
---

# Mario Golf 64 decomp workflow

Use this workflow for function selection, classical decompilation, upstream mirrors, bank decisions, and review. The ROM SHA-1 against `baserom.z64` is the only bank authority: `tools/verify-rom.sh` must succeed. All other comparisons are iteration signals.

Use the configured Python interpreter `uv run --locked python`; the default binary tree is `build/us`.

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

1. Rank whole main packs with `uv run --locked python tools/pick_target.py --segment main -n 12`
   and individual C-host leaves with `--loose-stubs main -n 12`. Loose-stub and `--file-close main`
   discovery follow YAML bank membership, including linked libraries; neither inventories unflipped
   assembly packs. `--lib` is a substring filter, not bank scope.
   Inspect assembly and source comments before pricing. Size leaves from their assembly headers,
   not adjacent VRAM deltas. Check individual carried/nested status before selecting pack members;
   a carried leader does not classify every sibling (`--carried-check`, `--nested-check`).
   Read only the selected target's WIP/backlog evidence. Reopen a carry for a named new source lever
   or changed evidence, not its age or a file-closure reward; rederive its signature and residual.
   If the fresh shortlist is empty, use `--segment main --include-stuck -n 12` and
   `--loose-stubs main --all -n 12`. An empty shortlist is not completion.
   Prefer demonstrated sibling/upstream leverage. Use [reference discovery](../../../docs/tooling.md#references-and-native-campaigns)
   when it can supply a source shape; inspect C, assembly, types/callees, and hashes before reuse.
   Similarity and source presence do not establish a bank.
2. Route hazards through [the hazard index](../../../docs/hazard-index.md), then read the selected playbook. Consult [C coding style](../../../docs/coding-style.md) for source conventions.
3. Resolve the source subsegment in `mariogolf64.yaml`. Change only explicitly required subsegment flip/split/path lines; never edit generated extraction outputs by hand. Preserve `INCLUDE_ASM` for unbanked functions. Do not change C, headers, or YAML unless the task requires that source change.
   A compiler jump table couples source and rodata: carve it only when the C switch regenerates its symbol.
4. Keep candidate experiments isolated under `nonmatchings/<func>/`; do not run concurrent builds. The build tree and Ghidra MCP are shared resources: serialize access to both.
   For delegated analysis, use the [fan-out ownership and evidence contract](../../../docs/workflow/fan-out.md).

## Campaigns

For a campaign request, prepare a bounded queue with the [campaign CLI](../../../docs/tooling.md#references-and-native-campaigns).
Keep the native session as the integration owner; execute targets serially and use native `task`
only for independent reasoning. Shards divide analysis queues, not permission to run shared builds.

Apply the [main-completion gate](#main-before-overlays) to every queue entry, including explicit
`--func` labels and resumed queues. Validate YAML bank membership before execution; a campaign's
default `--loose-stubs main` pool includes library C hosts but not unflipped assembly packs.

- Prefer upstream mirrors first. An opt-in m2c prepass can seed and measure untouched candidates
  before spending agent effort; it never overwrites carries or edits matching source.
- Treat each queue identity as ROM SHA-1 + bank + ROM offset. Revalidate the live stub and target
  before starting; changed reference lists, names, or ranking are not completion.
- Start the candidate's absolute deadline when it acquires execution ownership, not while queued.
  Pass `MG64_DEADLINE_UNIX` to bounded commands and retain inspection time. Stop launching
  expensive experiments when the reserve is exhausted; deadline expiry is not a source-wall verdict.
- Review provisional zeroes using the ordinary integration and bank gates. If an attempt expires
  or fails, retain its isolated artifacts and measured residual; never reset unrelated work or commit
  automatically. Continue to the next independent target only after shared resources are released.

## Iterate

- For an upstream mirror, copy the pinned upstream source and required headers, then validate with `tools/verify-rom.sh`. Check names, calls, data references, and routed hazards before treating it as a clean mirror.
- For classical work, read extracted assembly first. Translate a small clear leaf directly into
  `nonmatchings/<func>/base.c`; do not generate a seed or request Ghidra merely to fill the directory.
  When body/context recovery helps, use `uv run --locked python tools/seed_c.py --func <func> --parent src/<seg>.c`.
  Apply the [source-recovery gate](../../../docs/workflow/loop.md#source-recovery) to generated
  control flow, call contracts, data accesses, and display-list packets before chasing a score.
  Add `--ghidra` only for an unresolved context question.
  Measure with `uv run --locked python tools/decomp_loop.py --func <func>`.
- Fix diagnostics before retrying. Classify the residual as structure/count, registers, or ordering;
  change one named [source lever](../../../docs/levers.md) and remeasure. Use the
  [permuter eligibility rule](../../../docs/workflow/loop.md#permuter-eligibility), not score alone.
  A wall verdict belongs to its measured base, not permanently to the function.
- Score zero starts provisional integration: inline the body, remove its stub, format with an available clang-format binary (versioned names are valid), build the host object, and spot-check the target function against the isolated object. Then run the full ROM verifier. Compare the same function, not whole multi-function `.text` sections; [oracle limits](../../../docs/workflow/loop.md#oracles) explain relocation and normalization caveats. If integration fails, restore the matching stub/source and keep the candidate isolated.
- Preserve profile fidelity. Isolated compilation must use the same per-source profile and flags as the ROM build; use `tools/rom_build.py` and the authoritative `tools/build_profiles.json` rather than hand-assembled compiler commands. Never infer a matching result from an isolated compile alone.
  The loop retains content-addressed measurement/source pairs under the candidate's `measurements/`
  directory. Cite these from existing WIP notes rather than transcribing scores or creating a second
  ledger. Changes to source, profile, headers, or context require a fresh measurement.

## Bank

A function is banked only after `tools/verify-rom.sh` exits successfully. Do not substitute a direct hash, object comparison, or incremental diff for this full-ROM oracle. Before renaming a function, check committed C callers and update them with the rename. Stage source inputs only; generated extraction artifacts stay ignored in the binary tree. A file is complete only when all its `INCLUDE_ASM` stubs are gone. Keep unfinished work isolated and describe measured residuals in `docs/wip/<func>.near-match.md`.

## Review and resume

Review the banked source and verifier result, and distinguish verified matches from carried candidates. Count functions as well as remaining stubs: extraction may already emit empty leaves as C. Preserve backlog carry-overs and technical evidence; update task-specific notes as required. On resume, reconcile the current task state with committed changes and the tracked backlog before continuing.

Persist only enough state to reproduce: target ROM/bank/host, exact candidate and profile, measurement command and residual, verifier outcome or unbanked status, rejected hypotheses tied to that base, and the next experiment. Workers' native artifacts serve session communication; existing WIP/backlog notes serve durable evidence. Do not create parallel resume ledgers or reread complete sprint history.

## Domain references

- [Execution details and tool authority](../../../docs/workflow/loop.md)
- [Candidate and symptom routing](../../../docs/index.md)
- [Subagent isolation and fan-out](../../../docs/workflow/fan-out.md)
- [Build profiles and verification](../../../docs/build.md)
