# coddog cross-ref (S71)

`coddog compare2` fingerprints MG64's functions against ultralib VERSION_J (reloc-masked
instruction hashes), revealing each remaining `func_<addr>`'s ultralib source file. This turns a
candidate `pick_target.py` mis-classifies as `upstream none` / classical (because its functions are
un-named) into a known verbatim-mirror target. Sprint 71's `crc.c` was mis-seeded **pts-13
classical**; coddog proved it a trivial **verbatim 2-fn mirror**.

## Run

    make coddog-sweep        # or: tools/coddog_sweep.sh

Needs a fresh `make` (the MG64 ELF), a built ultralib (`~/development/repos/ultralib/build/J`), and
the coddog binary. Env overrides: `CODDOG`, `ULTRALIB`, `ULVER`, `ULLIB`, `THRESH` (see the script
header). Writes `coddog_map.tsv` here (gitignored — depends on local builds).

## Effect

`pick_target.py` reads `coddog_map.tsv` if present (`build_coddog_index`): a candidate whose lead fn
matches an ultralib fn gets a `coddog-mirror:<file>@<pct>` hazard; a ≥99% non-audio hit is re-priced
as a `libultra` mirror (drops off the `classical and pack` → 13 seed path). Absent map → ranking
unchanged. Full recipe + the `ld -r` combined-ELF build is in `docs/hazards.md#coddog-cross-ref`.

## NuSystem variant

    make coddog-sweep-nusys   # or: tools/build_nusys_ref.sh && tools/nusys_sweep.sh

The libnusys analog. `build_nusys_ref.sh` compiles the nusys-2.07 `mainlib` with the project's KMC
GCC + `LIBNUSYS_CFLAGS` (the in-tree mirror recipe) into `build/nusys-ref/` and merges it to one
relocatable ELF (nusys has no prebuilt object tree at the project profile, unlike ultralib);
`nusys_sweep.sh` runs `compare2` against it and writes `nusys_map.tsv` here (gitignored). `pick_target.py`
(`build_coddog_nusys_index`) reads it as a separate additive pass that flags
`coddog-mirror:mainlib/<file>.c@<pct>` and re-prices `upstream libnusys`; an absent `nusys_map.tsv`
leaves libultra ranking byte-identical. See `docs/hazards.md#coddog-cross-ref` (the *Nusys sweep* note).
