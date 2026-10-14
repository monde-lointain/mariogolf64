---
type: Project Overview
title: Mario Golf 64 decompilation
description: Byte-exact US retail reconstruction with an owned toolchain, CMake build, and Oh My Pi workflow.
---

# Mario Golf 64 decompilation

Reconstructs the US revision-0 Nintendo 64 game as C and assembly that rebuild to the original ROM.
This Git monorepo contains the pinned MIPS compiler/binutils sources and vendored decompilation
tools. CMake 3.30+ owns the build; uv 0.13.1+ owns Python dependencies and the root `.venv`.
No submodules or target-tool downloads are needed; initial Python setup downloads from PyPI.

Supply your own big-endian `baserom.z64`, then:

```sh
uv sync --locked
cmake --preset us
tools/verify-rom.sh
```

The verifier requires a successful matching build before reading the authoritative ROM SHA-1.
Generated native tools, extraction outputs, objects, and the ROM live under `build/us/`.
The uv-managed Python environment lives at `.venv/` and is shared by all binary trees.
See [build prerequisites and commands](docs/build.md), including isolated nonmatching builds.

## Start here

- [Build and verify](docs/build.md) — source-built tools, uv setup, presets, and ROM gate.
- [Dependency ownership](third_party/README.md) — immutable sources, Python lockfile, origins, and licenses.
- [Native decomp skill](.omp/skills/mg64-decomp/SKILL.md) — Oh My Pi `/decomp` selects, iterates, banks, and reviews.
- [Documentation index](docs/index.md) — on-demand matching hazards, code-generation levers, and evidence.
- [Tool reference](docs/tooling.md) — candidate selection and isolated matching signals.
- [Headless Ghidra](docs/ghidra.md) — persistent bank-aware analysis, scoped MCP, and live seed context.
- [Private CI container](docker/README.md) — maintainers only; it contains the copyrighted ROM.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Reconstructed game and library source/header inputs. |
| `mariogolf64.yaml`, `symbol_addrs.txt` | Segment layout and curated symbol placement. |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | Build graph and reproducible configure/build/test workflows. |
| `third_party/` | Pinned native source archives and vendored-tool provenance. |
| `tools/`, `tests/tooling/` | Ordinary vendored decomp tools, first-party helpers, and behavioral tests. |
| `.omp/` | Native skill and thin user command; root `AGENTS.md` holds safety context. |
| `build/us/work/`, `build/us/obj/` | Generated extraction/source shadow and objects; never source inputs. |
| `docs/`, `BACKLOG.md`, `RETRO.md`, `VELOCITY.md` | Current guidance and retained historical evidence. |

## Contributing

Read [the invariants](AGENTS.md#invariants) before changing generated or low-level inputs.
Never commit a non-matching function. Candidate experiments stay isolated until the full-ROM gate
passes. Use [C coding and matching style](docs/coding-style.md) for source work and
[prompt style](docs/prompt-style.md) for agent instructions. The [history guide](docs/history.md)
distinguishes current procedures from sprint-time records.

The ROM is not distributed. Keep it, generated game assets, and any ROM-bearing container private.
The root [MIT license](LICENSE) does not grant rights to Nintendo's ROM or replace
[third-party licenses](third_party/README.md#licenses).
