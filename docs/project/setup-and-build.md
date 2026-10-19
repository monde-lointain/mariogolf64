---
type: Guide
title: "Setup and build"
description: "Clone, install dependencies and the KMC toolchain, place the ROM, build and verify; make targets."
tags: [project, build]
resource: ../../Makefile
---

# Clone

```bash
git clone --recurse-submodules -j8 https://github.com/monde-lointain/mariogolf64.git
```

Submodules: `tools/decomp-permuter`, `tools/asm-differ`.

# Dependencies

Verified on Linux x86-64.

| Needed for | Packages |
|---|---|
| Build | `make`, `git`, `python3` with `venv`, `cpp`, `curl`, `tar`, `binutils-mips-linux-gnu`, `perl`, `xxd` (also `make diff-line`), `shasum`, `flock` |
| Format and lint (optional) | `clang-format-23`, `clang-tidy-23` |
| `--by-coddog-similarity` rankings (optional) | `coddog` (`cargo install coddog`) |

Debian/Ubuntu:

```sh
sudo apt install make git python3 python3-venv cpp curl binutils-mips-linux-gnu
```

Download KMC GCC 2.7.2 / binutils 2.6 into `tools/gcc_kmc` and install the git pre-commit hook:

```bash
make setup
```

Python environment (Splat, asm-differ, permuter, m2c). `.envrc` activates it with direnv, and the Makefile uses `.venv/bin/python3` when it exists:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

# Build and verify

Place the ROM in the repository root:

| Version | Filename | SHA1 |
|---|---|---|
| North America (rev 0) | `baserom.z64` | `e2c4e7a905b29529b49a1619a401fe699224829b` |

```bash
./tools/build-and-verify.sh
```

Success prints `build/mariogolf64.z64: OK` then `BUILD SUCCEEDED`. The script always does a clean rebuild. Details: [build-and-verify](/tools/build-and-verify.md).

For an incremental loop outside agent sessions:

```bash
make extract
make -j8
```

# Make targets

Agents cannot run most of these directly; see [ground rules](ground-rules.md) §5.

| Target | Purpose |
|---|---|
| `make setup` | Install the git hook and download the toolchain (`make -C tools`). |
| `make extract` | Delete `asm/` and re-run Splat on `baserom.z64`. |
| `make -j8` (`all`) | Build the ROM and verify its SHA1. |
| `make nonmatching` | Build without the SHA1 check and apply the N64 CRC. Add `NON_MATCHING=1` to compile `#ifdef NON_MATCHING` bodies. |
| `make -s print-cflags SRC=src/main/<file>.c` | Print the exact compiler flags for a C file. Allowed for agents. |
| `make format`, `make format-check` | clang-format 23 over project sources under `src/`. |
| `make tidy` | clang-tidy 23 diagnostics (advisory; never auto-fix, fixes can change codegen). |
| `make diff-line` | Write a byte diff of base and built ROM to `romdiff`. |
| `make clean` | Remove `asm/`, `assets/`, `build/`, the generated linker script and undefined-symbol lists. |

`make format`/`tidy` use clang, which rejects some GNU C that KMC GCC accepts, so its diagnostics are advisory only.
