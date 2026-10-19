Mario Golf 64
=============

A matching decompilation of [Mario Golf](https://en.wikipedia.org/wiki/Mario_Golf_(video_game)) for Nintendo 64 (North America, revision 0).

**This repository does not contain any game assets or assembly whatsoever. An existing copy of the game is required. This project is not a port, to PC or any other platform. It takes a Nintendo 64 rom, extracts assets from it, and combines them with C code we reverse-engineered from the rom, to produce an identical rom. It is explicitly a non-goal for this project to be used for any commercial purpose.**

## Cloning

Clone this repository, including its submodules:

```
git clone --recurse-submodules -j8 https://github.com/monde-lointain/mariogolf64.git
```

## Dependencies

The build is verified on Linux (x86-64).

System packages:

* make
* git
* python3 (with venv)
* cpp (the host C preprocessor)
* curl and tar (toolchain download)
* binutils-mips-linux-gnu
* clang-format-23 / clang-tidy-23 (optional, for `make format` and `make tidy`)

On Debian/Ubuntu:
```sh
sudo apt install make git python3 python3-venv cpp curl binutils-mips-linux-gnu
```

Download the KMC GCC 2.7.2 / binutils 2.6 toolchain into `tools/gcc_kmc` and install the git
pre-commit hook:

```bash
make setup
```

Install the Python dependencies (Splat, asm-differ, the permuter, m2c) in a virtual environment:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

`.envrc` activates the environment automatically with direnv. The Makefile also uses
`.venv/bin/python3` when it exists.

## Building

Place your ROM in the repository root as `baserom.z64`:

| Version | Filename | SHA1 |
| --- | --- | --- |
| North America (rev 0) | `baserom.z64` | `e2c4e7a905b29529b49a1619a401fe699224829b` |

Build and verify the ROM:

```bash
./tools/build-and-verify.sh
```

The output is `build/mariogolf64.z64`. A successful build prints the output path followed by `OK`,
confirming that its SHA1 matches the original ROM.

For incremental builds, extract once, then run make:

```bash
make extract
make -j8
```

Other targets:

| Target | Purpose |
| --- | --- |
| `make nonmatching` | Build without the SHA1 check (applies the N64 CRC). Add `NON_MATCHING=1` to compile `#ifdef NON_MATCHING` bodies. |
| `make -s print-cflags SRC=src/main/<file>.c` | Print the exact compiler profile used for a C file. |
| `make format` / `make format-check` | Format project sources under `src/` with clang-format 23. |
| `make tidy` | Run clang-tidy 23 diagnostics (advisory). |
| `make diff-line` | Write a byte diff of the built and base ROM to `romdiff`. |
| `make clean` | Remove `build/` and all extracted output. |

## Decompiling

The agent-driven workflow follows the Snowboard Kids decompilation projects:

- [AGENTS.md](AGENTS.md) holds the project instructions for agents. `.omp/` holds the omp project
  config (`config.yml`), skills (`skills/`), and the policy hook (`hooks/pre/mg64-policy.ts`).
- `./tools/decomp-env --bootstrap-only <function>` creates `nonmatchings/<function>/` with the target
  assembly, an m2c starting point, and `build.sh`, which compiles a candidate with the owning file's
  compiler profile and scores it against the target. Without `--bootstrap-only` it launches
  `omp --thinking xhigh` inside the environment.
- `timeout 300s ./tools/permuter --source-file nonmatchings/<function>/base_N.c <function>` runs
  [decomp-permuter](https://github.com/simonlindholm/decomp-permuter) with the same profile.
- `python3 tools/asm-differ/diff.py --no-pager <function>` diffs a function in the built ROM.
- `python3 tools/score_functions.py asm/nonmatchings` ranks unmatched functions;
  `python3 tools/project_status.py` reports progress.
- [task-runner/](task-runner) holds [nigel](https://github.com/cdlewis/nigel) tasks
  (`nigel --list`, `nigel decompile-similar`). Nigel resets the checkout (`git clean -f; git reset --hard HEAD`)
  at startup and after every failed candidate, so run it in a dedicated worktree, never in a checkout with
  uncommitted work. A worktree needs its own ignored inputs: `git worktree add ../mg64-a`, then in `../mg64-a`
  run `git submodule update --init`, `make setup`, create `.venv` as above, and copy `baserom.z64`.

Generic KMC GCC matching knowledge lives in [DECOMPILATION_LEARNINGS.md](DECOMPILATION_LEARNINGS.md).

## License

The [MIT license](LICENSE) covers first-party work, not Nintendo's ROM or the third-party tools in
`tools/`.
