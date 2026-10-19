# Docs Update Log

## 2026-10-18
* **Initialization**: Converted `AGENTS.md`, `README.md`, `DECOMPILATION_LEARNINGS.md`, `tools/HARNESS.md`, `docker/README.md`, `tools/data-differ/README.md` and the omp skills into this bundle. `AGENTS.md` and `README.md` remain as short entry points that link here; the other files were removed and the skills became symlinks into `procedures/`.
* **Update**: Env scaffolding no longer links `DECOMPILATION_LEARNINGS.md` into `nonmatchings/<function>/`; the env brief reaches this bundle through `../../docs/`.
* **Update**: The policy hook now allows the read-only `make -s print-cflags` query that the docs and skills already told agents to run ([policy hook](/harness/policy-hook.md)).
* **Update**: Added coverage that was missing: `src/mgu` and overlays' profile, optional dependencies (`coddog`, `flock`, `xxd`), `decomp-env` flags and generated files, `data_diff.py` size resolution, nigel layout and task catalog, `.bss` status.
* **Update**: Merged the identical `match-data-file` and `match-data-segment` skills, folded `convert-asset-array` into [add-asset](/procedures/add-asset.md), and made skill names equal their directory names.
* **Update**: Reconciled [KMC GCC learnings](/reference/kmc-gcc/index.md) with the code-quality rules: inline assembly, empty barriers and fixed-register locals are prohibited, not "last resort".
* **Update**: Aligned stop rules with `tools/decomp-env-template/build.sh` (it warns after 20 attempts without improvement; it never issues a hard stop).
