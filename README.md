# Mario Golf (N64) decompilation

A matching decompilation of *Mario Golf* for the Nintendo 64 (North America).

It builds the following ROM:

| File | SHA-1 |
| --- | --- |
| `mariogolf64.z64` | `e2c4e7a905b29529b49a1619a401fe699224829b` |

## Building

You need the original ROM; it is **not** included. Place it at the root of the
repository as `baserom.us.z64`.

Requirements: `make`, `wget`, Python 3.10+, and the `mips-linux-gnu` binutils.

```sh
git submodule update --init
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
make setup    # download the KMC GCC 2.7.2 toolchain and split the ROM
make -j       # build and verify build/mariogolf64.z64
```

## Formatting

The sources follow the Google style through clang-format 23. Run
`make format` before committing. The SDK sources in `lib/` keep their
own formatting.

The sources are UTF-8. The build converts each C file to Shift-JIS
with iconv before KMC GCC compiles it, so the game's Japanese strings
can be written as they read. GCC 2.7 would take a second byte of 0x5C
for the start of an escape, so characters such as 能 stay as octal
escapes.

`make lint` checks the names against the naming scheme in
`.clang-tidy` with clang-tidy 23.

The Python tools in `tools/` are formatted with ruff, which `make format`
also runs. `make pycheck` lints them with ruff, type-checks them with
basedpyright and looks for dead code with vulture, using the settings in
`pyproject.toml`.

## Layout

| Path | Contents |
| --- | --- |
| `mariogolf64.yaml` | [splat](https://github.com/ethteck/splat) configuration describing the ROM |
| `symbol_addrs.txt` | Names of functions and variables |
| `src/` | Decompiled C sources |
| `include/` | Headers |
| `lib/ultralib` | Decompiled libultra, the parts the game uses |

The game was compiled with KMC GCC 2.7.2 at `-O2`. Code lives in a main
segment loaded at `0x80025C00` and in overlays (“module sets”) that the
game's overlay manager loads on demand; see `mariogolf64.yaml`.
