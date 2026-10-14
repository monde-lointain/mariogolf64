---
type: Project Overview
title: Mario Golf 64 decompilation
description: Introduces the byte-exact Mario Golf 64 decompilation and routes contributors to setup and workflow documentation.
---

# Mario Golf 64 decompilation

Reconstructs the USA revision-0 Nintendo 64 game as readable C and assembly that rebuild to the
original ROM. A successful build is not sufficient evidence of a match: the project match gate is
[`tools/verify-rom.sh`](tools/verify-rom.sh), which checks build success before the ROM SHA-1.

## Start here

- [Build locally](docs/build.md) — prerequisites, extraction, verification, and WIP build isolation.
- [Documentation index](docs/index.md) — choose a task, then load only its relevant reference.
- [Agent workflow](docs/agent-workflow.md) — plan, execute, and review an approved decomp sprint.
- [Tool reference](docs/tooling.md) — candidate selection, matching signals, and maintenance commands.
- [Private CI container](docker/README.md) — maintainers only; the image contains the copyrighted ROM.

The ROM is not distributed. Supply your own `baserom.z64`; keep it and any ROM-bearing container
private. No current progress percentage is pinned here: candidate counts and matching results change
with the source tree. Use the ranker and ROM gate for current evidence.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/`, `include/` | Reconstructed game code and library sources/headers. |
| `asm/`, `assets/` | Splat extraction outputs, including reference assembly. |
| `mariogolf64.yaml`, `symbol_addrs.txt` | Segment layout and curated symbol placement. |
| `mk/`, `Makefile` | Compiler profiles and ROM build rules. |
| `tools/`, `tests/tooling/` | Decomp helpers and their characterization suite. |
| `docs/` | The project's OKF v0.2 knowledge bundle. |

Generated and low-level files have mutation restrictions; see the invariants in
[AGENTS.md](AGENTS.md#invariants) or [CLAUDE.md](CLAUDE.md#invariants) before changing them.
The [history guide](docs/history.md) distinguishes current procedures from sprint-time evidence.

## Contributing

Start with the [coding and matching style](docs/coding-style.md) for C, or
[prompt style](docs/prompt-style.md) for agent-facing documentation. Never commit a non-matching
function. Sprint scope, process changes, and outward operations follow the approval contracts in the
[sprint gates](docs/workflow/gates.md).

The project is licensed under [MIT](LICENSE). That license does not grant rights to Nintendo's ROM.
