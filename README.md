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

`make MODERN=1` builds `build/modern/mariogolf64.z64` with a modern GCC,
libdragon's `mips64-elf-gcc`, which `N64_INST` names the directory of. It
leaves out what nothing uses (`--gc-sections`) and compiles each segment's
code together, with link-time optimization, so that routines inline
across the segment's files while each overlay keeps its own code (see
`tools/link_segments.py`). `LTO=0` links the objects as each compiled,
such as to find whether LTO brings a difference about.

The modern ROM carries a build date of its own, which the game shows on a
debug screen and stamps into each save: the last commit's, in UTC, so that
a commit always builds the same ROM. `BUILD_DATE` sets another, in the
original's format, such as `make MODERN=1 BUILD_DATE='Tue Jun 15 18:36:11
1999'` for the original's.

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

## Tests

`make test` builds the matching ROM, checks that it matches, then runs the
unit tests in `tests/` on the console against both toolchains' code: the
matching build's KMC GCC objects and the modern build's (`MODERN=1`). Each
build links the same tests, compiled with modern GCC, into a test ROM that
boots into them instead of the game, and runs it in
[ares](https://ares-emu.net)'s `n64-harness`, from the project's fork of
ares, which `N64_HARNESS` names. Both ROMs build and run in a few
seconds. `make target-tests` runs one build's tests.

The tests use [Unity](https://github.com/ThrowTheSwitch/Unity)'s fixture.
A group goes in a file of its own, `tests/NAME_tests.c`, and its
`RUN_TEST_GROUP` in `tests/target/runner.c`. The test ROM needs an
Expansion Pak, whose free memory holds the tests (see
`tests/target/segment.ld`).

`make equivalence` runs the matching and modern ROMs side by side for
thousands of frames and compares the game's state at each. Worker
processes compare each frame while the emulators run the next, so on a
machine with many cores it takes under a minute, but it checks the whole
game at once, so it is a coarse, occasional check (see
`tools/equivalence.py`). It holds each finished hole load until both
builds' have ended, so builds of any speed compare: `--modern DIR` names
another modern build, such as one with other optimizations, and
`--reference build/nonmatching` compares the compilers alone.

`make sanitize` builds a modern ROM, `build/sanitize/mariogolf64.z64`,
whose code checks itself with GCC's undefined behavior sanitizer, and runs
it for thousands of frames. It reports each place that indexes past an
array, shifts by the width or more, divides by zero, or turns a float into
an integer too small for it, by source line, with the values of its first
failure and how often it failed. These are what C leaves undefined, so
that the layout or the compiler decides them, and each such place may run
differently in another build. The checks' handlers note each failure and
return, so one run finds them all (see `src/debug/sanitizer.c` and
`tools/sanitize.py`).

`tools/find_races.py` runs one build and reports the CPU's stores into data
that a queued frame has yet to draw with, which make the frame depend on
how far the RDP falls behind. It holds each graphics task back, as an RDP
that falls behind would, and can press buttons from a file, such as
`tools/inputs/tournament_tee_shot.txt`, to reach gameplay.

`make check-alignment`, which `make test` runs too, finds each static
array or structure whose address goes to the RCP or to a DMA, and fails
on any that doesn't ask for 8-byte alignment with `RCP_ALIGNED` or through
its type. The original's linker placed such buffers on 8-byte boundaries
mostly by chance, so a buffer that doesn't ask can land 4 bytes off in
another layout and be drawn or read from the wrong place. It reads the
code with `clang-query`, and the alignment from the modern build's
objects (see `tools/check_alignment.py`).

## Assets

`make extract` cuts the files that the game reads from ROM out of the
original, into `assets/`, which is never committed. It writes them afresh,
over any edits, so it is needed only once:

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

## Custom characters

A custom character, modeled in Blender with the add-on in
`tools/blender`, takes the place of one of the game's characters and
plays its animations. `make CHARACTERS=PACKAGE` builds
`build/nonmatching/custom/mariogolf64.z64` with it; the original's build
is left alone. Every segment after main starts on a 16-byte boundary, as
in the original, so model files of any size fit. The character's name
replaces the original's in the menus and text, and its face pictures,
drawn from its model by `tools/face_sprites.py`, replace the original's in
the menus and scorecards. See
[`tools/blender/README.md`](tools/blender/README.md) for the workflow, and
`make tool-tests` and `make blender-tests` for the tools' tests.

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

To add a file, list it in `mariogolf64.yaml` among the subsegments of the
segment it loads in, after the file it follows, by its name alone:

```yaml
      - { type: c, name: shot/new_file }
```

`make` makes the linker script again from the yaml and builds the file.
The matching ROM no longer matches, so build with `MODERN=1` or
`NON_MATCHING=1`.

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
