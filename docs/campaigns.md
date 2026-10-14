---
type: Reference
title: Reference discovery and campaign queues
description: Explains advisory cross-checkout references, campaign queue selection, prepass limits, and deadline semantics.
sources:
  - resource: ../tools/decomp_references.py
    title: Reference discovery implementation
  - resource: ../tools/decomp_campaign.py
    title: Campaign queue implementation
  - resource: ../tools/decomp_budget.py
    title: Deadline enforcement implementation
---

# Reference discovery and campaign queues

Use this reference for queue preparation and its constraints; the [tool catalog](tooling.md#references-and-native-campaigns)
provides command entrypoints. The [native `mg64-decomp` skill](../.omp/skills/mg64-decomp/SKILL.md#campaigns)
owns candidate execution. Queue sharding divides analysis, not execution ownership: serialize shared
build-tree, candidate-directory, Ghidra MCP, and source operations under the native session's parent.

## Advisory C-reference discovery

`tools/decomp_references.py --func <fn> [-n N] [--reference-root <checkout> ...]` returns JSON
with target provenance, ranked references, `best_similarity`, coverage, and the explicit authority
`advisory; not a bank verdict`. `-n` defaults to 3 and must be positive. Roots are additive to the
active checkout. Each referenced checkout must declare the same ROM SHA-1 and provide extracted
assembly in `build/us/work/asm` (the active checkout uses `MG64_BUILD_DIR` when set). Discovery
reads current files on each run. It reports source/target hashes and ROM/bank/offset identity;
missing coverage is reported rather than converted into a zero score. Ambiguous labels are rejected.

Similarity combines original-ROM instruction shape, opcode distribution, and shared callees. It is
not a matching percentage, difficulty estimate, or trust signal. A source-present result is
conservative discovery, not proof the checkout passes the ROM verifier. Inspect its C, conditional
compilation, dependencies and hashes; remeasure reused code in the current checkout.

Safe read-only examples:

```sh
uv run --locked python tools/decomp_references.py --func get_table_entry -n 3
uv run --locked python tools/decomp_campaign.py --segment main --pool-size 60 -n 12 --by-similarity
```

Queue preparation does not compile or integrate code unless `--prepass` is supplied.

## Queue selection

`tools/decomp_campaign.py` defaults to `--segment main`, `-n 12`, `--pool-size 60`, one shard
(`--shard 1/1`), and a per-candidate operation budget of 1800 seconds (`--seconds`). Segment and
explicit-label selection are mutually exclusive. Repeated `--func` selects exact labels and retains
risk/coupling tags; default selection asks `pick_target.py --loose-stubs <segment> -n <pool-size>
--json`, so it starts from that ranker's eligible (fresh) shortlist. The pool-size bounds the
shortlist before reference ranking or similarity sorting; final `-n` limits the queue after shard
assignment and optional `--by-similarity` sorting. Set `--pool-size >= -n > 0`.

Agent queues must obey the skill's [main-completion gate](../.omp/skills/mg64-decomp/SKILL.md#main-before-overlays):
validate YAML `main` membership for every target, including explicit labels and resumed queues,
before execution. Do not replace blocked main work with overlays. The default `--loose-stubs main`
pool includes every C host in the YAML bank, including linked libraries, but not unflipped
assembly packs. An empty eligible pool is not main completion. Inspect carried packs with
`pick_target.py --segment main --include-stuck -n 12`, retained C-host stubs with
`pick_target.py --loose-stubs main --all -n 12`, and queue eligible pack members explicitly.

`--reference-root` may be repeated. `--shard i/N` uses stable ROM SHA-1 + bank + ROM offset identity,
not mutable labels or similarity, and partitions analysis queues only. JSON includes each target's
provenance, references, budget, shard, and `banked: false`; queue authority remains unbanked. A
`provisional-zero` is only a candidate for ordinary source review, integration and full-ROM
verification.

## Optional m2c prepass

`--prepass` opts into serial seed/compile measurements and does not edit matching source or claim a
bank. It uses a nonblocking lock to exclude another prepass, not manual builds or Ghidra activity;
parent ownership must still serialize those resources. It skips coupled/nested or intrinsic targets,
known library source trees, already-C functions, and any existing candidate directory. It preserves
pre-existing carries and never integrates or commits. Choose upstream source first where available.
The result and any generated artifacts are provisional iteration evidence, not integration or bank
proof.

## Candidate deadlines

A direct `tools/decomp_budget.py --seconds S [--reserve R] -- <command> <args>...` bounds one command
and observed descendants. `--seconds` is required; reserve defaults to zero. Expiration prints an
`EXPIRED` diagnostic, retains artifacts, returns status 124, and gives no bank verdict. Otherwise it
returns the wrapped command's status (signals are translated to shell-style status). Descendant
cleanup uses procps-compatible `ps`; nested guards get graceful termination followed by bounded hard
escalation and reaping. Cleanup can extend return beyond the execution deadline. This is lifecycle
containment, not a sandbox for hostile daemonizing commands.

For a campaign, start one absolute deadline when the candidate acquires execution ownership and pass
`MG64_DEADLINE_UNIX` to bounded operations, preserving time for review. For example, establish a
30-minute deadline in the persistent shell:

```sh
export MG64_DEADLINE_UNIX=$(( $(date +%s) + 1800 ))
uv run --locked python tools/decomp_budget.py --seconds 600 --reserve 120 -- \
  uv run --locked python tools/decomp_loop.py --func <fn>
unset MG64_DEADLINE_UNIX
```

The effective execution window is the lesser of operation seconds and remaining absolute time minus
reserve; successive commands cannot extend the candidate's deadline. Replace or unset the value
before the next candidate. `run-permuter.sh` has the same guard, defaults to 600 seconds, and accepts
numeric `MG64_PERMUTER_SECONDS`; when a deadline is inherited it reserves 120 seconds. Expiry is a
resource boundary, not a retry policy or evidence that the source cannot match.
