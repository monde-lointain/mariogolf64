# Contributing

This guide describes how to change the code, check your change, and write
the commit that holds it. It assumes that you can already build the
matching ROM, as your [build guide](README.md#get-started) describes.

## Ground rules

- **Never commit data from the game.** Don't commit a ROM, in whole or in
  part, or the game's models, textures, sounds, music, or text. Don't commit
  disassembly that a tool dumped from the ROM. `.gitignore` keeps out
  `*.z64`, `*.n64`, `*.v64`, `asm/`, and `assets/`, but check what you stage.
- **Keep the matching ROM matching.** After a change to the game's code,
  `make` must still end with `build/mariogolf64.z64: OK`. A fix for the
  original's undefined behavior belongs behind `#ifdef AVOID_UB`, which
  only the non-matching and modern builds define.
- **Make the code explain itself.** Prefer a clear name to a comment. Use a
  comment to say what the code can't: why it does what it does, and what
  it assumes. Put a comment on the line above the code that it describes,
  not at the end of the line, except on a declaration.

## Make a change

1.  Create a branch from `main`.
1.  Make one logical change: one fix, one renamed concept, or one feature.
1.  Format the change:

    ```sh
    make format
    ```

1.  Run the checks that your change touches. See [Checks](#checks).
1.  Commit, with a message as [Commit messages](#commit-messages)
    describes.
1.  Open a pull request. Continuous integration runs the checks that don't
    need the ROM.

## Checks

Each check is a `make` goal. The ones that build a ROM need `baserom.us.z64`
and the assets that `make setup` extracted.

| Goal                   | What it checks                                                                      | Needs                       |
|------------------------|-------------------------------------------------------------------------------------|-----------------------------|
| `make`                 | The ROM builds, and the matching ROM's SHA-1 equals the original's.                 | ROM                         |
| `make objects`         | Everything that needs no file from the ROM compiles. CI runs this.                  |                             |
| `make check-shiftable` | The code links with its code and data moved, so no reference depends on an address. | ROM                         |
| `make check-format`    | The C sources are formatted as `make format` leaves them.                           |                             |
| `make lint`            | The names follow the naming scheme in `.clang-tidy`.                                | clang-tidy                  |
| `make check-alignment` | Each buffer that the RCP or a DMA reads asks for 8-byte alignment.                  | clang-query, libdragon      |
| `make pycheck`         | The Python tools pass ruff, basedpyright, and vulture.                              |                             |
| `make tool-tests`      | The Python tools' unit tests pass.                                                  |                             |
| `make test`            | The unit tests in `tests/` pass on both toolchains' code.                           | ROM, libdragon, n64-harness |
| `make equivalence`     | The matching and modern ROMs reach the same game state, frame by frame.             | ROM, libdragon, n64-harness |
| `make sanitize`        | A modern ROM with undefined-behavior checks runs without a failed check.            | ROM, libdragon, n64-harness |

The tools that the last column names are optional. Your build guide shows
how to install clang-tidy, clang-query, and libdragon's toolchain. To build
n64-harness, see [Run the tests on an emulated console](#run-the-tests-on-an-emulated-console).

For the PC port's checks, see [`port/README.md`](port/README.md#test-the-port).

### Run the tests on an emulated console

The unit tests in `tests/` run on the N64's CPU. `make test` links them,
once with the matching build's code and once with the modern build's, into
test ROMs that boot into the tests. It then runs each test ROM in
n64-harness, a headless N64 emulator built from a fork of
[ares](https://ares-emu.net) with commands to read the game's memory and
control its frames.

To build n64-harness on Linux:

1.  Install the packages that ares needs. On Ubuntu:

    ```sh
    sudo apt install cmake ninja-build g++ pkg-config libgtk-3-dev \
        libsdl2-dev libgl-dev libasound2-dev libpulse-dev libao-dev libudev-dev
    ```

1.  Clone the `mariogolf-harness` branch of the fork and build the
    `n64-harness` target:

    ```sh
    git clone --branch mariogolf-harness https://github.com/monde-lointain/ares.git
    cmake -S ares -B ares/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DARES_CORES=n64 -DARES_BUILD_OPTIONAL_TARGETS=ON
    cmake --build ares/build --target n64-harness
    ```

1.  Set `N64_HARNESS` to the program, for example in `~/.bashrc`:

    ```sh
    export N64_HARNESS="$HOME/ares/build/tools/n64-harness/n64-harness"
    ```

1.  Run the tests:

    ```sh
    make test
    ```

To add a test group, create `tests/NAME_tests.c` with
[Unity](https://github.com/ThrowTheSwitch/Unity)'s fixture macros, and add
its `RUN_TEST_GROUP` to `tests/target/runner.c`. The test ROM needs an
Expansion Pak, whose memory holds the tests. `tests/target/segment.ld`
places them there.

### Compare and sanitize whole runs

`make equivalence` and `make sanitize` run the game for thousands of frames
without input. They check the whole game at once, so they are coarse checks
that you run from time to time, not after each change.

- `make equivalence` runs the matching ROM and the modern ROM side by side
  and compares the game's state at each frame. Use it after a change that
  could alter behavior, such as replacing undefined behavior. For its
  options, such as `--modern DIR` to compare another modern build, see
  `tools/equivalence.py`.
- `make sanitize` builds `build/sanitize/mariogolf64.z64` with GCC's
  undefined-behavior sanitizer. It reports each place that indexes past an
  array, shifts by the width or more, divides by zero, or converts a float
  to an integer that can't hold it, with the values of the first failure.
  See `tools/sanitize.py`.
- `tools/find_races.py` reports each store by the CPU into data that a
  queued frame has yet to draw with. Such a race makes a frame depend on
  how far the RDP falls behind. It can press buttons from a file, such as
  `tools/inputs/tournament_tee_shot.txt`, to reach gameplay.

## Code style

### C

`.clang-format` sets the format, based on WebKit's style: a 4-space indent,
braces at the end of the line that opens them, and a 100-column limit.
`make format` applies it with the clang-format that `requirements.txt`
pins, because each clang-format version formats a little differently. The
libraries in `lib/` keep their own format.

`.clang-tidy` describes the naming scheme. In short:

| Kind                       | Style          | Example               |
|----------------------------|----------------|-----------------------|
| Functions                  | `PascalCase`   | `GetGroundPoint`      |
| Global variables           | `g_PascalCase` | `g_HolePoints`        |
| File-local data            | `f_PascalCase` | `f_CellCursorRow`     |
| Types                      | `PascalCase`   | `HoleTriangle`        |
| Fields, parameters, locals | `camelCase`    | `cupPositions`        |
| Macros and enum constants  | `UPPER_CASE`   | `HOLE_TRIANGLE_COUNT` |

A name that isn't known yet keeps the original's address, such as
`func_80400FBC`, until someone finds its meaning.

The sources are UTF-8. Before KMC GCC compiles a C file, the build converts
it to Shift-JIS with iconv, so the game's Japanese text reads as it is.
KMC GCC treats a second byte of `0x5C` as the start of an escape, so a
character such as 能, whose Shift-JIS code ends in that byte, stays an
octal escape.

### Python

The tools in `tools/` support Python 3.10 and later. `pyproject.toml` holds
their settings: ruff with every rule, basedpyright in its strictest mode,
and vulture. `make format` formats them with ruff, and `make pycheck`
checks them. Use only the standard library in a tool that runs before
`make setup` creates the Python environment, such as
`tools/install_kmc.py`.

## Commit messages

Write each message for someone who reads it months later without your
context:

- **One logical change per commit.** If you can't summarize the change in
  a few words, split it. Don't mix a whitespace or reformatting change with
  a code change.
- **A summary line of 50 characters or fewer**, in the imperative mood,
  such as `Fix the ball's spin on slopes`.
- **A body that explains the change.** Say why it's needed, how it works at
  a high level, and what effects it has. The diff already shows the code.
- **A last paragraph that says how you checked it.** Say whether the
  matching ROM still matches and which tests pass.

For example, from the project's history:

```text
Stop reading past a path's last point

When StepAlongPath's target finishes the line to a path's last point, it
loads the next line, whose end is the point after the last, read from
past the caller's points. The next call ends the path without reading
that line, so the load does nothing but read out of bounds, which is
undefined.

Under AVOID_UB the end of the next line is loaded only when there is a
next line. The start, which is the last point, is still loaded, and the
matching build is unchanged.

The curve tests, which cover the path's end and starting over after it,
pass against both builds' code. The wind's Fly Guy is the only caller,
and neither equivalence.py's run without input nor the tournament tee
shot's inputs step a path, so the equivalence run, which had no
unexpected differences, does not reach the change.
```

## Continuous integration

`.github/workflows/ci.yml` runs on every push and pull request. It has no
ROM, so it runs the checks that don't need one: the format and Python
checks, `make objects` with both toolchains, and the PC port's builds and
tests on Linux, Windows, and macOS. Run `make`, `make check-shiftable`, and
`make test` yourself before you push.
