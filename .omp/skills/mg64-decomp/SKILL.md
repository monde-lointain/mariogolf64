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

1. Rank main candidates with `uv run --locked python tools/pick_target.py --segment main -n 12`; combine `--lib` with `--segment main` for library scope. Use `--loose-stubs main` and `--file-close main` for game-host leaves and closure, not a whole-bank inventory. Read only the selected candidate's backlog/WIP evidence. Check carried walls and nested children before pricing hand-selected leaves (`--carried-check`, `--nested-check`). Inspect its assembly and source comments; ranker signals are not proof of tractability.
   `--lib` filters substrings, not segment ranges. Size leaves from their assembly headers, not adjacent VRAM deltas. Re-derive carried signatures, declarations, instruction counts, and residuals from current evidence; a prior compiler-pass verdict is a hypothesis.
   Bound discovery with `-n`; loose-stub mode also supports `--json`. Prefer a demonstrated sibling/upstream/source lever over a historical bank-rate forecast.
   If fresh work runs out, use `--segment main --include-stuck -n 12` and
   `--loose-stubs main --all -n 12`, then inspect remaining YAML main hosts, including libraries.
   `--loose-stubs main` and `--file-close main` scan `src/main/`, not every host in the main bank.
   Use [reference discovery](../../../docs/tooling.md#references-and-native-campaigns) when a sibling
   implementation could supply a source shape. Read its C and assembly, compare types/callees,
   and recheck its hashes before reuse. Similarity and source presence do not establish a bank.
2. Route hazards through [the hazard index](../../../docs/hazard-index.md), then read the selected playbook. Consult [C coding style](../../../docs/coding-style.md) for source conventions.
3. Resolve the source subsegment in `mariogolf64.yaml`. Change only explicitly required subsegment flip/split/path lines; never edit generated extraction outputs by hand. Preserve `INCLUDE_ASM` for unbanked functions. Do not change C, headers, or YAML unless the task requires that source change.
   A compiler jump table couples source and rodata: carve it only when the C switch regenerates its symbol.
4. Keep candidate experiments isolated under `nonmatchings/<func>/`; do not run concurrent builds. The build tree and Ghidra MCP are shared resources: serialize access to both.
   Work inline by default. Delegate only independent analysis that justifies separate contexts; the parent serializes candidate compiles, builds, MCP, and source integration. Use the native `task` contract and [fan-out guidance](../../../docs/workflow/fan-out.md), not a copied dispatch prompt.

## Campaigns

For a campaign request, prepare a bounded queue with the [campaign CLI](../../../docs/tooling.md#references-and-native-campaigns).
Keep the native session as the integration owner; execute targets serially and use native `task`
only for independent reasoning. Shards divide analysis queues, not permission to run shared builds.

Apply the [main-completion gate](#main-before-overlays) to every queue entry, including explicit
`--func` labels and resumed queues. Validate YAML bank membership before execution; a campaign's
default `--loose-stubs main` pool covers game-host leaves, not the complete main-bank backlog.

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
- For classical work, read the function's extracted assembly as ground truth. Use Ghidra decompilation and types only as shape/context evidence. Seed `nonmatchings/<func>/base.c` with `uv run --locked python tools/seed_c.py --func <func> --parent src/<seg>.c`, then iterate with `uv run --locked python tools/decomp_loop.py --func <func>`. Small asm-first leaves can be translated directly into the isolated candidate.
  Read `target.s` and optional `ghidra.c` separately; the editable seed does not duplicate them. Check `body_source` and refine fallback bodies, prototypes, and types against assembly. Add `--ghidra` only when fresh [bank-aware context](../../../docs/ghidra.md#agent-access-and-context) resolves a question; omit `--parent` when its partial types conflict.
- Fix compile diagnostics before retrying. Classify a fresh residual as structure/count, registers, or ordering; change a named [source lever](../../../docs/levers.md) and re-measure. Escalate to the permuter at `percent >= 0.97`, or exact instruction count with a single-operand residual. A wall verdict belongs to its measured base, not permanently to the function.
- Score zero starts provisional integration: inline the body, remove its stub, format, build the host object, and spot-check the target function against the isolated object. Then run the full ROM verifier. Compare the same function, not whole multi-function `.text` sections; [oracle limits](../../../docs/workflow/loop.md#oracles) explain relocation and normalization caveats. If integration fails, restore the matching stub/source and keep the candidate isolated.
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
