---
type: Guide
title: Build and verify the US retail ROM
description: Bootstrap the pinned Linux toolchain, choose a build task, and verify the supported ROM.
sources:
  - resource: ../CMakeLists.txt
    title: Host requirements and CTest targets
  - resource: ../CMakePresets.json
    title: Supported configure, build, test, and workflow presets
  - resource: ../cmake/Rom.cmake
    title: ROM extraction, compile, link, and build targets
  - resource: ../tools/clangd_config.py
    title: Editor-only compilation database generator
  - resource: ../.omp/lsp.json
    title: Repository-local Oh My Pi language server selection
  - resource: ../tools/verify-rom.sh
    title: Stable ROM verification entrypoint
---

# Build and verify the US retail ROM

The supported input is US revision 0 (`NMFE`) in big-endian `.z64` format. Supply your own
`baserom.z64` at the repository root. The authoritative SHA-1 is in `mariogolf64.yaml`;
`tools/verify-rom.sh` is the ROM match gate. The ROM and extracted game assets are not distributed.

## Choose a task

- [Bootstrap and full ROM verification](#configure-build-and-verify)
- [Build tools, extract, or build one object](#task-specific-builds)
- [Compiler profiles and incremental work](#compiler-profiles-and-incremental-work)
- [Iterate on nonmatching code](#nonmatching-and-custom-binary-trees)
- [Enable clangd navigation](#clangd-navigation)
- [Run tooling checks](#tooling-checks)

## Bootstrap prerequisites

Linux x86_64 (including WSL), glibc 2.28 or newer, uv 0.13.1+, CMake 3.30+, Ninja,
a host C compiler with 32-bit libc support, GNU Make, and host `cpp` are required. uv selects or
downloads CPython 3.12–3.14 and creates the repository's `.venv`. GNU Make is used only inside
the historical upstream tool builds; it is not the repository build interface.

On Ubuntu, host prerequisites include `build-essential`, `gcc-multilib`, `ninja-build`, `bison`,
`flex`, and `texinfo`. Install or update uv using its
[official installation instructions](https://docs.astral.sh/uv/getting-started/installation/).
Ubuntu 24.04's packaged CMake is older than the project minimum; install a current release with
`uv tool install cmake`. The [private container guide](../docker/README.md) describes the CI image.
Native Windows and macOS are not supported by this pinned toolchain; Windows users can use WSL.

The target compiler sources are local archives; no cross-compiler package or submodule setup is
required. Python dependencies are declared in `pyproject.toml` and hash-locked in `uv.lock`;
initial setup requires network access. After dependencies are cached, `uv sync --locked --offline`
is available. See [dependency ownership](../third_party/README.md).

## Configure, build, and verify

```sh
uv sync --locked
cmake --preset us
tools/verify-rom.sh
```

The first build compiles KMC-compatible GCC 2.7.2, KMC binutils 2.6, and GNU MIPS binutils 2.45.1
from local source archives. CMake synchronizes `.venv`, checks the input ROM, extracts it, compiles
configured objects, links, and produces `build/us/mariogolf64.z64`. Build outputs stay in the binary
tree; `.venv` is the only source-tree environment. A failed build returns nonzero before the verifier
checks a ROM hash, so an older matching ROM cannot pass.

`cmake --workflow --preset verify` runs configure, build, then the ROM CTest gate. The verifier
also configures an unconfigured binary tree, so `tools/verify-rom.sh` works as the first command.

## Task-specific builds

```sh
cmake --build --preset tools       # Host toolchains and Python environment; no ROM
cmake --build --preset extract     # Extraction only; needs baserom.z64
cmake --build --preset us --verbose
ctest --preset rom                 # Authoritative build-and-ROM gate
```

Extraction analyzes a copied source shadow under `build/us/work/src`; it never writes to `src/`.
Missing required `INCLUDE_ASM` stubs are recovered from full-range ROM disassembly automatically.


## Compiler profiles and incremental work

`tools/build_profiles.json` is the compiler-profile source. It preserves per-library optimization,
include ordering, character signedness, defines, and per-file overrides. KMC GCC emits assembly
for KMC GAS; modern GAS is restricted to extracted assembly and the entry stub. KMC encodings and
section padding affect matching.

CMake tracks extraction byproducts and generated objects. Depfiles include C headers, assembler
includes, and embedded binary inputs. The linker follows Splat's configured object order.
`<build-dir>/build-tools.json` (normally `build/us/build-tools.json`) publishes actual tool paths.

```sh
uv run --locked python tools/rom_build.py build --object src/main/vec3f_normalize.o
uv run --locked python tools/rom_build.py compile --source nonmatchings/<func>/base.c \
  --output nonmatchings/<func>/current.o --profile-source src/<seg>.c --nonmatching
```

For isolated compilation, `--profile-source` selects the parent translation unit's effective
profile, including per-file overrides. `--profile` instead forces a named profile. To discard
generated outputs and rebuild from scratch, use `cmake --build build/us --target clean` explicitly;
cleaning is not part of routine iteration.

A single-object or isolated compile is an iteration signal, not a ROM match; verify after
integrating a candidate. Optional [headless Ghidra analysis](ghidra.md) has separate lifecycle
targets and a proposal-only `sync-names`; neither is part of ROM compilation. The
[tool reference](tooling.md) covers Python-tool entrypoints.
The scenery billboard emitters remain assembly-backed: unfinished C candidates are retained
locally under `nonmatchings/scenery_billboard/`. Terrain cleanup experiments under
`nonmatchings/terrain_cleanup/` are likewise excluded from the matching build.


## Nonmatching and custom binary trees

```sh
cmake --preset nonmatching
cmake --build --preset nonmatching
```

This uses `build/nonmatching` and defines `NONMATCHING`; the ROM gate refuses this configuration.
Do not reuse a matching tree for WIP C.

For another binary tree, configure it with `cmake -S . -B <binary> -G Ninja`, then set
`MG64_BUILD_DIR=<binary>` for first-party Python/shell tools and pass
`tools/verify-rom.sh --build-dir <binary>`. Keep machine-specific choices in ignored
`CMakeUserPresets.json`, not the project presets.

## clangd navigation

Install `clangd-23` on `PATH`. From the repository root, generate the editor database:

```sh
uv run --locked python tools/clangd_config.py
clangd-23 --check=src/main/get_table_entry.c
```

This standalone setup needs neither a ROM nor a built KMC toolchain. `cmake --preset us`
also regenerates the root `compile_commands.json` during configuration. Rerun the generator
after adding/removing C files or changing compiler profiles; CMake tracks profile changes.
The database contains every `src/**/*.c` translation unit, not generated `build/**/work/src`
copies or isolated `nonmatchings/` candidates. Headers borrow a source command from clangd.
The database contains checkout-specific absolute paths and is ignored, as is `.cache/clangd/`.

`tools/clangd_config.py` reuses `rom_build.profile_flags()` for include ordering, defines,
signedness, optimization and file overrides. It translates KMC's architecture/ABI spellings
to Clang flags and adds the big-endian MIPS target, GNU89 and `-nostdlibinc` so host libc
headers do not replace the N64 SDK. These commands are for parsing only; ROM builds still
use KMC. Do not use `CMAKE_EXPORT_COMPILE_COMMANDS`: ROM compilation uses custom commands.
The old `compile_flags.txt` and per-library `.clangd` deltas are replaced by this one profile-derived
database. A fixed flags database can parse an opened file but cannot enumerate translation
units for background indexing.

Oh My Pi reads `.omp/lsp.json`, which selects `clangd-23` and inherits its built-in clangd
arguments, file types and root markers. Start omp at the repository root. If the session
predates setup, invoke its `lsp` tool with `{"action":"reload","file":"*"}` to refresh
discovery. Definition/reference requests start the server; workspace queries need background
indexing to finish. Other editors can launch `clangd-23` with `--background-index` against
the same root database and `.clangd`.

Clang diagnostics are advisory, not a KMC compile or ROM-match verdict. Clang does not support
GNU nested functions (for example `src/main/vec3f_normalize.c`) or KMC lvalue casts (for example
`src/libkmc/memset.c`). Those diagnostics remain visible and can limit navigation in affected
functions. Assembly-only `INCLUDE_ASM` bodies are not C definitions in the index. Do not
rewrite matching code or apply clang-tidy fixes just to quiet the language server.

## Tooling checks

`uv run --locked pytest tests/tooling` runs the behavioral suite without a ROM or CMake;
`ctest --preset tooling` runs it in the configured binary tree. The ROM CTest preset selects only
the authoritative ROM gate. Tests cover behavior, not the changing candidate backlog or generated
seed comments. Optional external SDK/reference tools are described in the
[tool reference](tooling.md).

All scripts and analysis tools use the uv-managed `.venv`; don't install dependencies with pip,
create environments with `python -m venv`, or maintain requirements files. `uv sync --locked`
includes dev dependencies (pytest, Ruff, basedpyright, Vulture, and coverage). Run
`uv run --locked ruff check tools tests/tooling`, `uv run --locked basedpyright`, and
`uv run --locked vulture` for advisory static analysis. Ordinary setup uses `--locked`;
[dependency updates](../third_party/README.md#updating-a-dependency) are a separate maintainer task.
Historical KMC compiler/binutils versions stay fixed because their output defines matching.

Current Splat/spimdisasm can emit undefined `.L` operands for absolute jumps outside a disassembled
section. `tools/extract_rom.py` resolves those operands from the original J instruction encoding
while retaining locally defined labels and relocations. Extraction regressions and the ROM gate
cover this compatibility behavior.
