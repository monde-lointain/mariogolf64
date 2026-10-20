# Build and verify

* [build-and-verify](build-and-verify.md) - Clean re-extract, rebuild and SHA1 check of the whole ROM; the only command that decides a project-wide match.
* [asm-differ](asm-differ.md) - Diff a function in the built ROM against the target assembly; vendored submodule, protected.
* [data-differ](data-differ.md) - Byte-compare data symbols between the built ROM and baserom.z64; find the first mismatching symbol after a checksum failure.

# Per-function work

* [decomp-env and per-function scoring](decomp-env.md) - tools/decomp-env creates nonmatchings/<function>/ with the target assembly, a starting base.c and build.sh, which compiles an attempt with the owning file's profile and scores it.
* [permuter wrapper](permuter.md) - tools/permuter imports a candidate into decomp-permuter with the owning file's compiler profile and runs it until interrupted or matched.

# Choosing and tracking work

* [Progress report](progress.md) - tools/progress.py: code and data matched in the last build, by bytes and by functions or data symbols, per category or segment; writes the objdiff report.json that decomp.dev reads.
* [Selection, ranking and status tools](selection-and-status.md) - Scripts that report progress, rank unmatched functions, find similar matched code, batch-run m2c, and report or clean per-function environments.
* [Code-quality checkers](quality-checkers.md) - Scripts that find pointer arithmetic, low-quality matches, redundant externs, header-ownership problems, struct unions and oversized symbols.

# Automation (nigel)

* [task-runner (nigel)](task-runner.md) - nigel tasks that run an omp agent over candidate lists with verify, commit and reset handling; layout, task catalog, worktree rules.
* [verify-candidate](verify-candidate.md) - The gate nigel runs before committing a candidate: protected paths, baserom hash, optional decompiled-in-src check, new pointer arithmetic on changed lines, then build-and-verify.
* [Difficulty ledger and tiers](difficulty-ledger.md) - Shared record of each decompilation attempt's outcome, the difficulty tier that follows from a function's failure history, and the per-environment .run.json that sets the stall limit.
* [new-worktree](new-worktree.md) - Create a sibling git worktree on a new branch with the gitignored build inputs (baserom, gcc_kmc, .venv, submodules) copied or linked from the main checkout, then build it.
* [Shard integration (sync-shard, integrate-shards, merge drivers)](shard-integration.md) - Shard worktrees: sync-shard rebases a shard before each candidate, integrate-shards cherry-picks shard commits behind a build gate, merge-decomp-c and merge-splat-yaml resolve src and yaml conflicts.
* [Pipeline metrics report](pipeline-metrics.md) - tools/pipeline_metrics.py: nigel outcomes joined with omp session cost, certified attempts to match, the difficulty ledger and a progress trend.
* [list-unconsolidated-learnings](list-unconsolidated-learnings.md) - Lists per-function LEARNINGS.md and NEAR_MISS.md notes that have not been merged into docs/reference/kmc-gcc/, and records consolidated notes in a ledger shared by all worktrees.

How these fit together, and which of them decide a match: [harness code map](/harness/code-map.md).
