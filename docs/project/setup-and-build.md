---
type: Guide
title: "Setup and build"
description: "Clone, install dependencies and the KMC toolchain, place the ROM, build and verify; make targets."
tags: [project, build]
resource: ../../Makefile
---

This page builds the ROM from a fresh clone and lists the make targets. Agents run the scripts named here rather than `make` directly; see [ground rules](/project/ground-rules.md) §5.

# Clone

```sh
git clone --recurse-submodules -j8 https://github.com/monde-lointain/mariogolf64.git
```

Submodules: `tools/decomp-permuter`, `tools/asm-differ`.

# Dependencies

| Needed for | Packages |
|---|---|
| Build | `make`, `git`, `python3` with `venv`, `cpp`, `curl` and `tar` (for `make setup`), `perl` (provides `shasum` for `make verify`), `xxd` (for `make diff-line`), and a MIPS cross binutils (below) |
| Decomp environments | `flock` (used by `tools/decomp-env-template/build.sh`) |
| Format and lint (optional) | `clang-format-23`, `clang-tidy-23` (for `make format`, `make format-check`, `make tidy`) |
| `--by-coddog-similarity` rankings (optional) | `coddog` (`cargo install coddog`), used by `tools/score_functions.py` |

The Makefile looks for `mips-linux-gnu-`, then `mips64-linux-gnu-`, then `mips64-elf-` binutils, and stops with an error if none is installed.

Debian/Ubuntu package set, matching `docker/Dockerfile`:

```sh
sudo apt install make git python3 python3-venv cpp curl ca-certificates perl xxd binutils-mips-linux-gnu
```

The KMC toolchain download in `tools/Makefile` supports Linux and macOS.

## KMC toolchain and git hooks

```sh
make setup
```

`make setup` runs `make install-hooks`, then `make -C tools`, which downloads the KMC toolchain into `tools/gcc_kmc`. `install-hooks` links the pre-commit hook into `.git/hooks/` and registers the `decomp-c` merge driver used for `src/**/*.c`. An existing hook is backed up first.

## Python environment

```sh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

`requirements.txt` provides the Splat tooling and the packages for the permuter and asm-differ tools. `.envrc` activates `.venv` through direnv, and the Makefile uses `.venv/bin/python3` when it exists.

# Build and verify

Place the North America (revision 0) ROM in the repository root as `baserom.z64`. Its SHA1 is in the [project overview](/project/overview.md) and in `mariogolf64.sha1`. The ROM is git-ignored (`*.z64`).

```sh
./tools/build-and-verify.sh
```

The script runs `make clean`, `make extract`, then `make -j$JOBS all`. It takes no arguments. `JOBS` sets the parallel job count (default: `nproc`), and `MAKE` selects the make binary. Success prints `build/mariogolf64.z64: OK` followed by `BUILD SUCCEEDED`. See [build-and-verify](/tools/build-and-verify.md).

For a manual build outside agent sessions, run `make extract` only after changing `mariogolf64.yaml` or the ROM, because it deletes and regenerates `asm/`. Then run `make -j8`.

# Make targets

Agents cannot run most of these directly; see [ground rules](/project/ground-rules.md) §5.

| Target | Purpose |
|---|---|
| `make` / `make all` | Build `build/mariogolf64.z64`, then run `verify` unless `NON_MATCHING=1`. |
| `make verify` | `shasum --check` of the built ROM against `mariogolf64.sha1`. |
| `make nonmatching` | Build, apply the N64 CRC (`tools/n64crc.py`), and skip the SHA1 check. |
| `make extract` | Check that `baserom.z64` exists, delete `asm/`, and re-run Splat (`--make-full-disasm-for-code`). |
| `make check` | Fail unless `baserom.z64` exists. `extract` depends on it. |
| `make print-cflags SRC=<file>` | Print the effective compiler flags for a C file. See [compiler profiles](/project/compiler-profiles.md). |
| `make print-<VAR>` | Print the value of a Makefile variable, for debugging. |
| `make format` | Run clang-format-23 in place over `FORMAT_FILES`: the tracked `src/*.c` and `src/*.h`. |
| `make format-check` | Dry-run clang-format-23 over the same files; fails on any change. |
| `make tidy` | clang-tidy-23 diagnostics with `-std=gnu89 -m32`. Diagnostic only; applies no fixes. |
| `make diff-line` | Write `romdiff`, a diff of `xxd` dumps of the base and built ROMs. |
| `make install-hooks` | Run `tools/install-git-hooks.sh`: link the pre-commit hook and register the merge driver. |
| `make setup` | Run `install-hooks`, then `make -C tools`. |
| `make clean` | Remove `asm/`, `assets/`, `build/`, `mariogolf64.ld`, `mariogolf64.d`, `undefined_funcs_auto.txt` and `undefined_syms_auto.txt`. |

Internal prerequisites, not for direct use: `check-clang-format-version`, `check-clang-tidy-version` (prerequisites of `format`, `format-check` and `tidy`), and `FORCE`.

`tidy` is advisory: its clang-tidy output reports problems without changing files, and it is not a gate for a match.
