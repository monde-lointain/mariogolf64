# Build and verify

* [build-and-verify](build-and-verify.md) - the full clean rebuild and SHA1 check; the final arbiter of every match
* [asm-differ](asm-differ.md) - diff a function in the built ROM against the target

# Per-function work

* [decomp-env](decomp-env.md) - bootstrap `nonmatchings/<function>/` and score attempts with `build.sh`
* [permuter](permuter.md) - decomp-permuter wrapper with the owning file's compiler profile
* [data-differ](data-differ.md) - byte-compare data symbols between built and target ROM

# Choosing and tracking work

* [selection-and-status](selection-and-status.md) - progress, ranking, similarity, m2c batch, near-miss and cleanup reports
* [quality-checkers](quality-checkers.md) - pointer-arithmetic, low-quality-match, redundant-extern and header-hygiene tools
* [task-runner](task-runner.md) - nigel tasks that run agents over candidate lists

For how these fit together and which ones decide a match, see the [harness code map](/harness/code-map.md).
