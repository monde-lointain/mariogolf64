---
type: Playbook
title: Local build and verification
description: Sets up the local toolchain, extracts the supported ROM, and distinguishes matching builds from WIP artifacts.
sources:
  - resource: ../Makefile
    title: Build targets and toolchain configuration
  - resource: ../tools/Makefile
    title: Legacy compiler downloads
  - resource: ../tools/requirements.txt
    title: Runtime and extraction dependencies
  - resource: ../tools/verify-rom.sh
    title: Build-success and ROM-SHA-1 gate
---

# Local build and verification

Run commands from the repository root. The build supports the USA revision-0 ROM; the authoritative
expected SHA-1 is the top-level `sha1:` in [mariogolf64.yaml](../mariogolf64.yaml).
Supply that ROM as `baserom.z64`. It is not included in the repository.

## Prerequisites

The Linux build uses host `cpp` and system MIPS binutils for extracted assembly, plus the downloaded
KMC GCC 2.7.2 and KMC binutils 2.6 for C. On Ubuntu/Debian:

```sh
sudo apt-get update
sudo apt-get install make git build-essential wget ca-certificates \
    python3 python3-venv python3-pip binutils-mips-linux-gnu cpp
```

Initialize the recorded tool submodules:

```sh
git submodule update --init --recursive
```

`clang-format-23` is a separate source-authoring dependency, not a prerequisite for
rebuilding existing sources. See [coding style](coding-style.md#comments-and-layout).
Native Windows is rejected by the toolchain download Makefile; use WSL. The commands here describe
Linux, not a verified macOS setup.

## Setup, extract, verify

Place `baserom.z64` before any root `make` invocation: the Makefile checks for it at parse time,
even for `help`, `setup`, and `test-tools`.

```sh
sha1sum baserom.z64
make setup
make extract
tools/verify-rom.sh
```

Compare the input hash with `mariogolf64.yaml` before extraction. `make setup` creates `venv`, installs
[tools/requirements.txt](../tools/requirements.txt), and downloads the legacy compiler. Splat and its
assembly-generation dependencies are pinned there; do not upgrade them incidentally.

The gate exits 0 only after `make` succeeds, its final line reports `build/mariogolf64.z64: OK`, and
the built ROM SHA-1 equals the YAML hash. It prints `make OK + ROM SHA-1 MATCH` on success. A stale
ROM left by a failed build is not a passing result. Stop on a failed gate; use the error output and
[build hazard lookup](hazard-index.md#flag-to-section) to diagnose it.

After an allowed layout change, `tools/verify-rom.sh --extract` combines extraction and the same gate.
Extraction regenerates assembly and linker scaffolding; do not hand-edit those outputs.

### uv-managed environment

If the host uses uv rather than the system venv installer, replace `make setup` with:

```sh
uv venv --seed venv
uv pip install --python venv/bin/python3 -r tools/requirements.txt
make -C tools
```

Extraction and verification commands remain unchanged.

## Matching versus WIP builds

| Command or path | Meaning |
| --- | --- |
| `tools/verify-rom.sh` | Matching-build gate; builds and checks `build/mariogolf64.z64`. |
| `make V=1` | Matching build with full command lines, useful for inspecting actual flags. |
| `make NONMATCHING=1` | Compiles WIP C with `-DNONMATCHING`, skips the Makefile MD5 check. |
| `build/nonmatching/` | Separate WIP output tree; not evidence of a matching ROM. |
| `nonmatchings/<func>/` | Per-function seed, compile, diff, and permuter scratch. |

`make clean` removes the selected build tree; it is not needed for normal incremental builds.
`make distclean` also deletes extraction outputs and `symbol_addrs.txt`, which contains curated
placements. Do not use it as a routine troubleshooting step.

## Checks and CI

- `make test-tools` runs the tooling suite; it does not prove ROM matching.
- `make prompt-lint` checks prompt contracts and prints the advisory report.
- `make setup-dev` installs optional Python analysis tools; `make check-all` adds format, lint,
  type, and dead-code checks to the tooling tests.
- The [CI workflow](../.github/workflows/build.yml) requires the private ROM-bearing container for
  its build job. Fork PRs skip that job; Python static-analysis steps are advisory and ROM-free.
  The current workflow does not run `make test-tools`.

Container maintenance and its local end-to-end recipe live in the
[private CI container guide](../docker/README.md).
