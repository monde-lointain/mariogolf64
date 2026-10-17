---
type: Project Overview
title: Mario Golf 64 decompilation
description: Byte-exact US retail reconstruction with an owned toolchain, CMake build, and Oh My Pi workflow.
---

# Mario Golf 64 decompilation

Reconstructs the US revision-0 Nintendo 64 game as C and assembly that rebuild to the original ROM.
The repository owns its pinned MIPS compiler/binutils sources and vendored decompilation tools.
Supply your own big-endian US revision-0 `baserom.z64`; the ROM is not distributed.

## Start here

- [Build and verify](docs/build.md#bootstrap-prerequisites) — Host prerequisites, locked Python setup, CMake presets, and the authoritative ROM acceptance gate.
- [Documentation index](docs/index.md) — Choose an operational task, matching diagnosis, or reconstruction note before reading its detail.
- [Decomp workflow](docs/workflow/index.md) — Native Oh My Pi skill, execution decisions, and independent analysis.

## Maintainer references

- [Dependency ownership](third_party/README.md) — Pinned native sources, Python lockfile, origins, and licenses.
- [Private CI container](docker/README.md) — ROM-bearing image and CI maintenance; never publish it.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Reconstructed game and library source/header inputs. |
| `mariogolf64.yaml`, `symbol_addrs.txt` | Segment layout and curated symbol placement. |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | Build graph and reproducible configure/build/test workflows. |
| `third_party/` | Pinned native source archives and vendored-tool provenance. |
| `tools/`, `tests/tooling/` | Ordinary vendored decomp tools, first-party helpers, and behavioral tests. |
| `.omp/` | Native skill, thin user command, and `RULES.md` project invariants (sticky, forwarded to subagents). |
| `build/us/work/`, `build/us/obj/` | Generated extraction/source shadow and objects; never source inputs. |
| `docs/`, `BACKLOG.md`, `RETRO.md`, `VELOCITY.md` | Current guidance and retained historical evidence. |

## Contributing

Read [the invariants](.omp/RULES.md) before changing generated or low-level inputs.
Never commit a non-matching function. Candidate experiments stay isolated until the full-ROM gate
passes. Use [C coding and matching style](docs/coding-style.md) for source work and
[prompt style](docs/prompt-style.md) for agent instructions. The [history guide](docs/history.md)
distinguishes current procedures from sprint-time records.

The ROM is not distributed. Keep it, generated game assets, and any ROM-bearing container private.
The root [MIT license](LICENSE) does not grant rights to Nintendo's ROM or replace
[third-party licenses](third_party/README.md#licenses).
