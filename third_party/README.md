# Owned build dependencies

This Git monorepo owns the target toolchain and extraction tools. Normal configure/build does not
fetch source, follow a branch, use a mutable release URL, or initialize submodules.

| Input | Ownership and pin |
| --- | --- |
| KMC-compatible GCC 2.7.2 | Local source archive, revision `43d1cdb67ed135879869b5266f01efaaada5e35a`. |
| KMC binutils 2.6 | Local source archive, revision `54514ded39ceb32165a125ddba04ca5b551773a2`. |
| GNU MIPS binutils 2.45.1 | Local GNU release source archive, SHA-256 checked by CMake. |
| Splat 0.37.1, spimdisasm 1.39.3, rabbitizer 1.14.3 | Complete hash-locked Python closure in `python/requirements.lock`; source distributions in `python/sources/`. |
| asm-differ, decomp-permuter, m2c | Ordinary source files under `tools/`, not Git links. Imported upstream revisions are recorded in `sources.json`. |

`sources.json` and `python/sources.json` record origins, versions, immutable revisions, and archive
hashes. CMake verifies native source archives before extraction. Pip verifies selected wheels and
installs with `--no-index --require-hashes`. The wheelhouse contains Linux x86_64 wheels for CPython
3.12, 3.13, and 3.14; the original source distributions accompany native and pure-Python packages.
`intervaltree` and `pylibyaml` pure-Python wheels were built from their recorded source distributions.

Large upstream source trees remain compressed to avoid adding generated build machinery and
hundreds of unrelated target backends as loose files. CMake's `ExternalProject` gives each historical
Autoconf/Make tool its own source/build trees under the selected binary directory. This is build
isolation, not external dependency acquisition. First-party ROM compilation uses output-tracked
custom commands because a single contemporary C compiler target cannot model KMC GCC → KMC GAS
and the separate modern-GAS path faithfully.

The host OS, libc, C compiler, `cpp`, Python, GNU Make, Ninja, and CMake are bootstrap prerequisites.
Optional Ghidra workspaces, SDK reference trees, IDO compilers, and coddog are research inputs, not
requirements for reproducing the retail ROM.

## Updating a dependency

Update the recorded source archive/revision, retain its upstream license and corresponding source,
update CMake's archive hash or the Python wheel lock, then build in a fresh binary tree and run
`tools/verify-rom.sh`. Splat and disassembler updates can change labels and linker output without
any C change; do not update them casually. Preserve local modifications to ordinary vendored tools
when importing a new upstream snapshot.

## Licenses

The root MIT license covers first-party work, not third-party dependencies or Nintendo's ROM.
Upstream `COPYING`, `COPYING.LIB`, `LICENSE`, and other notices remain inside the source archives,
Python distributions, and ordinary vendored tool directories. GCC/KMC binutils are GPL-2.0-or-later;
modern binutils contains GPL-3.0-or-later and LGPL components; m2c is GPL-3.0; asm-differ and
permuter retain their own license files. Redistributing built tools requires honoring their upstream
licenses and retaining their corresponding source. Never include a ROM or generated game assets in
a public source or tool package.
