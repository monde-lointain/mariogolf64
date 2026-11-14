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

## Progress

Every routine matches the original's. `make progress` also builds the ROM
that needn't match (`NON_MATCHING=1`) and writes the progress images and page to
`build/progress`: a treemap of the code by source file (`progress.svg`),
every routine by state (`functions.svg`), the matched code over time
(`history.svg`, `day.svg`, `daily.svg`) and all of them on one page
(`index.html`). Each run adds the measured commit to `history.json`.
`tools/progress_report.py` measures the build into objdiff's
`report.json` format, scoring each draft as objdiff does, and lists the
drafts that differ in register numbers only. `tools/progress_map.py`, a
copy of a tool kept elsewhere with this project's settings, draws them.

## Assets

`make extract` cuts the files that the game reads from ROM out of the
original, into `assets/`, which is never committed:

| Path | Contents |
| --- | --- |
| `sound/` | The sound pointer, wave and effect banks |
| `songs/` | A song for each value of `enum Song` |
| `asset_archive/` | The asset archive's 2449 assets, named by ID; `tools/asset_archive.py` packs them |
| `text/` | The messages and the staff roll's lines as text, which `tools/messages.py` encodes |
| `character_models/` | Each character's models, and the files that patch them to its other palettes |

The code takes each file's ROM address from the linker script, so a
file can change size. About half of the archive's assets are images
encoded as runs, which `tools/decode_asset.py` reads. They stay encoded:
the original's runs split where the decoded bytes give no reason to, as
if chosen from the image before its colors were reduced, so encoding
the decoded bytes again does not give the original's.

IPL3 and the RSP's boot, audio and graphics microcode are Nintendo's
own binaries, cut from the original as they are, and `textures.txt`
lists the textures that C files hold, which are built from PNGs.

## Formatting

The sources follow the style in `.clang-format`, based on WebKit's, through
clang-format 23: a 4-space indent, braces that end the line they open on,
no alignment to the end of a previous line, and a 100-column limit. Run
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

The sources are grouped by the part of the game they serve, not by the
segment they load in; `mariogolf64.yaml` says which segment each file is
linked into. `include/` has the same directories as `src/`, and a header
is included by its directory, as in `"course/terrain.h"`.

| Directory | Contents |
| --- | --- |
| `system/` | Entry point, boot, threads, heap, overlays, asset archive, crash screen |
| `controllers/` | Controllers, the Rumble Pak and the Transfer Pak |
| `math/` | Angles, random numbers, curves, matrices and the C math library |
| `screen/` | RCP setup, display lists, screen fades and wipes, the sky |
| `camera/` | The cameras, framing and projection |
| `text/` | Messages and fonts |
| `audio/` | Sound effects and music |
| `characters/` | Character models, animation, voices and reactions |
| `course/` | Courses and holes: loading, terrain, collision, pins, billboards |
| `shot/` | Clubs, the ball's flight and the shot's effects |
| `round/` | Turns, players, scoring, the round's HUD and results, tournaments |
| `save/` | Saved data, SRAM, awards and records |
| `menus/` | The title screen and the menus outside of play |
| `cinematics/` | Boot logos, the opening and the staff roll |
| `debug/` | Debug text, flags, the performance meter and the hole editor |
| `game/` | The game loop and its scenes |
