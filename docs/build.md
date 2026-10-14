---
type: Guide
title: Build and verify the US retail ROM
description: Reproducible offline CMake setup, source-built MIPS tools, extraction, and the ROM acceptance gate.
---

# Build and verify the US retail ROM

The supported input is US revision 0 (`NMFE`) in big-endian `.z64` format. Supply your own
`baserom.z64` at the repository root. The authoritative SHA-1 lives in `mariogolf64.yaml`;
`tools/verify-rom.sh` is the only match gate. The ROM and extracted game assets are not distributed.

## Bootstrap prerequisites

Linux x86_64, glibc 2.28 or newer, CPython 3.12–3.14 with `venv`, CMake 3.30+, Ninja,
a host C compiler with 32-bit libc support, GNU Make, and host `cpp`. GNU Make is used only
inside the historical upstream tool builds; it is not the repository build interface.

On Ubuntu, host compilation prerequisites include `build-essential`, `gcc-multilib`,
`python3-venv`, `ninja-build`, `bison`, `flex`, and `texinfo`. Ubuntu 24.04's packaged CMake is
older than the project minimum; install a newer CMake, for example with
`uv tool install cmake==3.30.9`. The [private container](../docker/README.md) includes these
bootstrap tools. Native Windows and macOS are not supported by this pinned toolchain; use WSL
for Windows.

No cross compiler, target binutils package, submodule initialization, network download, or
source-tree Python environment is required. The checkout owns the compiler sources and the
complete locked extraction/runtime Python closure; see [dependency ownership](../third_party/README.md).

## Configure, build, and verify

```sh
cmake --preset us
tools/verify-rom.sh
```

The first build compiles KMC-compatible GCC 2.7.2, KMC binutils 2.6, and GNU MIPS binutils 2.45.1
from local source archives. It creates an isolated Python environment from the hash-locked local
wheelhouse, checks the input ROM, extracts it, compiles the configured objects, links, and produces
`build/us/mariogolf64.z64`. All generated files stay in the binary tree. A failed build returns
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
python3 tools/rom_build.py build --object src/main/vec3f_normalize.o
python3 tools/rom_build.py compile --source nonmatchings/<func>/base.c \
  --output nonmatchings/<func>/current.o --profile main --nonmatching
cmake --build build/us --target clean
```

A single-object or isolated compile is an iteration signal, not a ROM match. Run the verifier after
integrating a candidate. `sync-names` requires the optional Ghidra script configured through
`MG64_GHIDRA_SYNC_SCRIPT`; name synchronization is not part of ROM compilation.

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

`cmake --build build/us --target test-tools` runs the existing behavioral suite using the configured
Python environment. It does not require a ROM to configure or create that environment. The ROM
CTest preset selects only the authoritative ROM gate; tooling tests are a separate check.

Historical ranker/coddog snapshots have documented baseline failures; do not regenerate goldens to
conceal them. [Tool reference](tooling.md) separates optional external SDK/reference tools from the
self-contained retail-ROM build.
