# Build and verify

* [build-and-verify](build-and-verify.md) - Clean re-extract, rebuild and SHA1 check of the whole ROM; the only command that decides a project-wide match.
* [asm-differ](asm-differ.md) - Diff a function in the built ROM against the target assembly; vendored submodule, protected.
* [data-differ](data-differ.md) - Byte-compare data symbols between the built ROM and baserom.z64; find the first mismatching symbol after a checksum failure.
* [new-worktree](new-worktree.md) - Create a sibling git worktree on a new branch with the gitignored build inputs (baserom, gcc_kmc, .venv, submodules) copied or linked from the main checkout, then build it.

# Per-function work

* [decomp-env and per-function scoring](decomp-env.md) - tools/decomp-env creates nonmatchings/<function>/ with the target assembly, a starting base.c and build.sh, which compiles an attempt with the owning file's profile and scores it.
* [permuter wrapper](permuter.md) - tools/permuter imports a candidate into decomp-permuter with the owning file's compiler profile and runs it until interrupted or matched.

# Choosing and tracking work

* [Progress report](progress.md) - tools/progress.py: code and data matched in the last build, by bytes, functions and data symbols, per category or segment; `-o` writes the objdiff report.json.
* [Selection, ranking and status tools](selection-and-status.md) - Scripts that rank unmatched functions, find similar matched code, list candidates, batch-run m2c, report nonmatching matches, and clean up per-function environments.
* [Code-quality checkers](quality-checkers.md) - Scripts that report code-quality problems in src/ and include/: pointer arithmetic, low-quality matches, redundant externs, header and struct problems, global types and oversized symbols.
* [Pipeline metrics report](pipeline-metrics.md) - tools/pipeline_metrics.py: nigel outcomes joined with omp session cost, certified attempts to match, the difficulty ledger and a progress trend.

# Automation (nigel)

* [task-runner (nigel)](task-runner.md) - nigel tasks that run an omp agent over candidate lists with verify, commit and reset handling; layout, task catalog, worktree rules.
* [verify-candidate](verify-candidate.md) - The gate nigel runs before committing a candidate: protected paths, baserom hash, optional decompiled-in-src check, new pointer arithmetic on changed lines, then build-and-verify.
* [Difficulty ledger and tiers](difficulty-ledger.md) - Shared record of each decompilation attempt's outcome, the difficulty tier that follows from a function's failure history, and the per-environment .run.json that sets the stall limit.
* [Shard integration (sync-shard, integrate-shards, merge drivers)](shard-integration.md) - Shard worktree integration: sync-shard rebases a shard onto the sync branch, integrate-shards cherry-picks shard commits behind a build check, two merge drivers resolve src and yaml conflicts.
* [list-unconsolidated-learnings](list-unconsolidated-learnings.md) - Lists per-function LEARNINGS.md and NEAR_MISS.md notes not yet merged into docs/reference/kmc-gcc/; --mark records a note in a ledger shared by all worktrees.

How these fit together, and which of them decide a match: [harness code map](/harness/code-map.md).
