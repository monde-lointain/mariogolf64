---
type: Guide
title: Build and verify the US retail ROM
description: Reproducible CMake and uv setup, source-built MIPS tools, extraction, and the ROM acceptance gate.
---

# Build and verify the US retail ROM

The supported input is US revision 0 (`NMFE`) in big-endian `.z64` format. Supply your own
`baserom.z64` at the repository root. The authoritative SHA-1 lives in `mariogolf64.yaml`;
`tools/verify-rom.sh` is the only match gate. The ROM and extracted game assets are not distributed.

## Bootstrap prerequisites

Linux x86_64, glibc 2.28 or newer, uv 0.13.1+, CMake 3.30+, Ninja,
a host C compiler with 32-bit libc support, GNU Make, and host `cpp`. uv selects or downloads
CPython 3.12–3.14 and creates the repository's `.venv`. GNU Make is used only
inside the historical upstream tool builds; it is not the repository build interface.

On Ubuntu, host compilation prerequisites include `build-essential`, `gcc-multilib`,
`ninja-build`, `bison`, `flex`, and `texinfo`. Install or update uv using its
[official installation instructions](https://docs.astral.sh/uv/getting-started/installation/).
Ubuntu 24.04's packaged CMake is older than the project minimum; install the current release with
`uv tool install --upgrade cmake`. The [private container](../docker/README.md) includes these
bootstrap tools. Native Windows and macOS are not supported by this pinned toolchain; use WSL
for Windows.

No cross compiler, target binutils package, or submodule initialization is required.
Native compiler sources remain local. Python dependencies are declared in `pyproject.toml` and
hash-locked in `uv.lock`; initial setup and dependency upgrades require network access.
Once populated, uv's cache supports `uv sync --locked --offline`. See
[dependency ownership](../third_party/README.md).

## Configure, build, and verify

```sh
uv sync --locked
cmake --preset us
tools/verify-rom.sh
```

The first build compiles KMC-compatible GCC 2.7.2, KMC binutils 2.6, and GNU MIPS binutils 2.45.1
from local source archives. uv synchronizes the shared `.venv` against `uv.lock`;
the build checks the input ROM, extracts it, compiles the configured objects, links, and produces
`build/us/mariogolf64.z64`. ROM build outputs stay in the binary tree; `.venv` is the only
source-tree environment. A failed build returns
nonzero before the verifier reads any ROM hash, so an older matching ROM cannot pass.

`cmake --workflow --preset verify` provides configure → build → CTest ROM gate. The verifier itself
configures an unconfigured binary tree, so `tools/verify-rom.sh` also works as the first command.

```sh
cmake --build --preset tools       # Host tools; no ROM needed
cmake --build --preset extract     # Extraction only
cmake --build --preset us --verbose
ctest --preset rom
```

Extraction analyzes a copied source shadow under `build/us/work/src`; it never writes to `src/`.
Missing required `INCLUDE_ASM` stubs are recovered from full-range ROM disassembly automatically.
Do not copy cached root `asm/`, `assets/`, `venv/`, or `tools/cc` into a new checkout.

## Compiler profiles and incremental work

`tools/build_profiles.json` is the single compiler-profile source. It preserves per-library
optimization, include ordering, character signedness, defines, and per-file overrides. KMC GCC
emits a file that KMC GAS assembles; modern GAS is deliberately restricted to extracted assembly
and the entry stub. KMC's encodings and section padding are part of matching behavior.

CMake declares extraction byproducts and generated objects. Compiler depfiles include C headers,
assembler includes, and embedded binary inputs. The linker consumes Splat's configured object order.
The generated `build-tools.json` publishes actual tool paths to the ranker, diff, and permuter.

```sh
uv run --locked python tools/rom_build.py build --object src/main/vec3f_normalize.o
uv run --locked python tools/rom_build.py compile --source nonmatchings/<func>/base.c \
  --output nonmatchings/<func>/current.o --profile main --nonmatching
cmake --build build/us --target clean
```

A single-object or isolated compile is an iteration signal, not a ROM match. Run the verifier after
integrating a candidate. Optional [headless Ghidra analysis](ghidra.md) has separate lifecycle
targets and a proposal-only `sync-names`; neither is part of ROM compilation.

## Nonmatching and custom binary trees

```sh
cmake --preset nonmatching
cmake --build --preset nonmatching
```

This uses `build/nonmatching` and defines `NONMATCHING`. The ROM gate rejects this configuration.
Do not reuse a matching tree for WIP C.

For another binary tree, configure it with `cmake -S . -B <binary> -G Ninja`, then set
`MG64_BUILD_DIR=<binary>` for first-party Python/shell tools and pass
`tools/verify-rom.sh --build-dir <binary>`. Keep machine-specific choices in ignored
`CMakeUserPresets.json`, not the project presets.

## Tooling checks

`uv run --locked pytest tests/tooling` runs the existing behavioral suite without a ROM or CMake.
`ctest --preset tooling` runs the same suite in the configured binary tree.
The ROM CTest preset selects only the authoritative ROM gate; tooling tests are a separate check.

Tests check behavior rather than snapshotting the changing candidate backlog or generated seed
comments. [Tool reference](tooling.md) separates optional external SDK/reference tools from the
self-contained retail-ROM build.

All scripts and analysis tools use the uv-managed `.venv`; do not install dependencies with pip,
create environments with `python -m venv`, or maintain requirements files. `uv sync --locked`
includes the `dev` dependency group (pytest, Ruff, basedpyright, Vulture, and coverage).
Use `uv run --locked ruff check tools tests/tooling`, `uv run --locked basedpyright`, and
`uv run --locked vulture` for advisory analysis.

For an existing checkout, run `deactivate` if a legacy environment is active before invoking uv.
Old root or binary-tree `venv` directories are no longer selected; uv ignores an unrelated
`VIRTUAL_ENV` and warns about it. Existing local environments are not deleted by the migration.

To pull the newest stable compatible Python releases, run `uv lock --upgrade` followed by
`uv sync --locked`, run tooling checks and `tools/verify-rom.sh`, and commit the updated lockfile.
Ordinary setup uses `--locked` rather than silently changing tool versions. Upstream dependency
constraints remain enforced, including Splat's pins on support libraries such as intervaltree and tqdm.
Historical KMC compiler/binutils versions remain fixed because their output defines matching.

Current Splat/spimdisasm can emit undefined `.L` operands for absolute jumps outside a disassembled
section. `tools/extract_rom.py` resolves those operands from the original J instruction encoding,
while retaining locally defined labels (including `jlabel` macros) and their relocations.
The assembler regression cases and full-ROM gate cover this adaptation.

