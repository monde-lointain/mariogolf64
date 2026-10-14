---
type: Reference
title: Owned build dependencies
description: Records the pinned target toolchain, vendored tools, Python dependencies, and their ownership.
sources:
  - resource: ../CMakeLists.txt
    title: Project host requirements
  - resource: CMakeLists.txt
    title: Native toolchain build definitions
  - resource: sources.json
    title: Native archive and vendored-tool provenance
  - resource: ../pyproject.toml
    title: Python dependency declarations
  - resource: ../uv.lock
    title: Resolved Python dependency origins and hashes
---

# Owned build dependencies

This repository owns the target toolchain sources and vendored decompilation tools. Native builds
use immutable local archives; Python setup uses uv and the PyPI releases recorded in `uv.lock`.
For supported host requirements and build commands, see the [build guide](../docs/build.md).

## Choose a task

- [Review dependency inputs](#inputs-and-pins)
- [Understand build isolation](#build-isolation)
- [Update a dependency](#updating-a-dependency)
- [Review licensing and redistribution](#licenses)

## Inputs and pins

| Input | Ownership and pin |
| --- | --- |
| KMC-compatible GCC 2.7.2 | Local source archive, revision `43d1cdb67ed135879869b5266f01efaaada5e35a`. |
| KMC binutils 2.6 | Local source archive, revision `54514ded39ceb32165a125ddba04ca5b551773a2`. |
| GNU MIPS binutils 2.45.1 | Local GNU release source archive, SHA-256 checked by CMake. |
| Splat, spimdisasm, rabbitizer, Python analysis/test tools | Declared in root `pyproject.toml`; `uv.lock` records resolved versions, origins, and distribution hashes. |
| asm-differ, decomp-permuter, m2c | Ordinary source files under `tools/`, not Git links. Imported upstream revisions are recorded in `sources.json`. |

`sources.json` records native origins, immutable revisions, archive hashes, and vendored-tool
patches. CMake verifies native source archives before extraction. uv manages the shared root `.venv`,
verifies locked distributions, and downloads from PyPI during initial setup. Supported interpreters
are CPython 3.12–3.14.

## Build isolation

Large upstream source trees remain compressed to avoid adding generated build machinery and
hundreds of unrelated target backends as loose files. CMake's `ExternalProject` gives each historical
Autoconf/Make tool its own source/build trees under the selected binary directory. This is build
isolation, not external dependency acquisition. First-party ROM compilation uses output-tracked
custom commands because a contemporary C compiler target cannot model KMC GCC → KMC GAS and the
separate modern-GAS path faithfully.

The host OS, libc, C compiler, `cpp`, uv, GNU Make, Ninja, and CMake are bootstrap prerequisites.
Optional Ghidra workspaces, SDK reference trees, IDO compilers, and coddog are research inputs, not
requirements for reproducing the retail ROM.

## Updating a dependency

Python dependency updates follow this guide; [tooling checks](../docs/build.md#tooling-checks)
and [ROM verification](../docs/build.md#configure-build-and-verify) keep their canonical commands
in the build guide. Update constraints in `pyproject.toml` as needed, run `uv lock --upgrade` and
`uv sync --locked`, then run both checks. Commit `pyproject.toml` and `uv.lock` together when
constraints change. Do not bypass upstream compatibility constraints to
force newer support-library versions. Splat and disassembler updates can change labels and linker
output without any C change.

For vendored tools, import a reviewed upstream revision, retain licenses, and update `sources.json`.
Preserve local modifications, especially the permuter's CMake/assembly adaptation. Historical target
compiler versions are not part of Python-tool upgrades.

## Licenses

The root MIT license covers first-party work, not third-party dependencies or Nintendo's ROM. Native
distributions and ordinary vendored tool directories retain upstream license notices. Python
distribution origins and hashes are in `uv.lock`. GCC/KMC binutils are GPL-2.0-or-later; modern
binutils contains GPL-3.0-or-later and LGPL components; m2c is GPL-3.0; asm-differ and permuter
retain their own license files. Redistributing built tools requires honoring their upstream licenses
and retaining their corresponding source. Never include a ROM or generated game assets in a public
source or tool package.
