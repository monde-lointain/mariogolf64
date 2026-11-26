# Mario Golf 64 decompilation

This project is a matching decompilation of *Mario Golf* for the Nintendo 64,
North American release. Apart from a few short routines in assembly, the
game's code is C source that compiles back to the original code, byte for
byte. The same source also builds a
native PC port for Linux, Windows, macOS, and Android.

The repository holds no data from the game. To build anything that runs, you
need your own copy of the ROM:

| File             | SHA-1                                      |
|------------------|--------------------------------------------|
| `baserom.us.z64` | `e2c4e7a905b29529b49a1619a401fe699224829b` |

## What you can build

From the one source tree, you can build the following:

- **The matching ROM.** KMC GCC 2.7.2, the compiler that the game shipped
  with, builds `build/mariogolf64.z64`, which is identical to the original.
  The build checks this every time.
- **A modern ROM.** A current GCC from libdragon builds
  `build/modern/mariogolf64.z64`, which plays the same game on the console
  and in emulators. Its code is optimized across each segment's files.
- **A mod.** A mod is a folder of its own, outside this repository, that
  holds only the files that it changes. See [Mods](docs/mods.md).
- **The PC port.** A native program that reads the game's data from your
  ROM when it starts. See [`port/README.md`](port/README.md).

## Get started

Follow the guide for your system. Each one takes you from a fresh install to
a matching ROM, and then to the PC port:

- [Ubuntu](docs/building/ubuntu.md)
- [Fedora](docs/building/fedora.md)
- [Arch Linux](docs/building/arch.md)
- [macOS](docs/building/macos.md)
- [Windows, with WSL](docs/building/windows-wsl.md)

In short, once the packages that your guide lists are installed, these
commands build and check the matching ROM:

```sh
git clone https://github.com/monde-lointain/mariogolf64.git
cd mariogolf64
cp /path/to/your/rom.z64 baserom.us.z64
make setup
make -j"$(nproc)"
```

`make setup` downloads KMC GCC, creates a Python environment in `.venv`, and
extracts the game's assets from your ROM into `assets/`. You run it once. The
last line of a successful build is `build/mariogolf64.z64: OK`.

To see every goal and setting, run `make help`.

## How the build works

The build has two halves that share the game's source:

- **GNU Make builds the ROMs.** The `Makefile` compiles the game with KMC GCC,
  or with modern GCC when you set `MODERN=1`. It links the code with the
  linker scripts that `tools/layout.py` writes from `layout.txt`, and then
  inserts the assets that `make setup` extracted.
- **CMake builds the PC port.** `CMakeLists.txt` and `CMakePresets.json`
  build the game's code natively, with the port's own versions of the
  N64 libraries in `port/`.

The matching ROM depends on details that the C source can't show, such as
the order of the files in each segment. `layout.txt` records that order, so
the source and `layout.txt` together describe the whole ROM.

### Build settings

You pass settings to `make` on the command line, for example
`make MODERN=1`:

| Setting          | Effect                                                                                                                                        |
|------------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `NON_MATCHING=1` | Builds `build/nonmatching/mariogolf64.z64` with KMC GCC, without the original's undefined behavior, in a layout that lets code and data move. |
| `MODERN=1`       | Builds with libdragon's GCC, which `N64_INST` names. The default is `/opt/libdragon`.                                                         |
| `LTO=0`          | With `MODERN=1`, links each object as it was compiled, without link-time optimization. Add `-B` when you change it.                           |
| `BUILD_DATE=...` | With `MODERN=1`, sets the build date that the game shows and stamps into saves.                                                               |
| `CROSS=PREFIX`   | Selects the MIPS binutils, such as `CROSS=mips-linux-gnu-`. The build finds one if you don't.                                                 |

A matching ROM must keep every address of the original. When you add or
grow code, build with `NON_MATCHING=1` or `MODERN=1` instead.

A modern ROM takes the date of the last commit as its build date, so the
same commit always builds the same ROM.

## Repository map

| Path           | Contents                                                                                    |
|----------------|---------------------------------------------------------------------------------------------|
| `src/`         | The game's source, grouped by the part of the game that it serves                           |
| `include/`     | The game's headers, in the same folders as `src/`                                           |
| `lib/`         | The libraries that the game links: libultra, NuSystem, libmus, n_audio, and KMC's C library |
| `layout.txt`   | The ROM's contents, in order: each segment's objects, and the asset files                   |
| `assets.txt`   | The asset files that `make extract` cuts from the ROM                                       |
| `textures.txt` | The textures that C files hold, which the build makes from PNG files                        |
| `tools/`       | Python tools for the build, the checks, and the tests                                       |
| `tests/`       | Unit tests that run on the console, or in an emulator                                       |
| `port/`        | The PC port                                                                                 |
| `docs/`        | Build guides and the guide to mods                                                          |

Each source file and tool starts with a comment that describes its purpose,
so the files are the place to look for details.

The game's code loads in segments. The `main` segment stays in memory at
`0x80025C00`, and overlays load on demand at fixed addresses. The source is
grouped by purpose, not by segment, so `layout.txt` is where you find which
segment a file is linked into.

| Folder in `src/` | Contents                                                                |
|------------------|-------------------------------------------------------------------------|
| `system/`        | Entry point, boot, threads, heap, overlays, asset archive, crash screen |
| `controllers/`   | Controllers, the Rumble Pak, and the Transfer Pak                       |
| `math/`          | Angles, random numbers, curves, matrices, and the C math library        |
| `screen/`        | RCP setup, display lists, screen fades and wipes, and the sky           |
| `camera/`        | Cameras, framing, and projection                                        |
| `text/`          | Messages and fonts                                                      |
| `audio/`         | Sound effects and music                                                 |
| `characters/`    | Character models, animation, voices, and reactions                      |
| `course/`        | Courses and holes: loading, terrain, collision, pins, and billboards    |
| `shot/`          | Clubs, the ball's flight, and the shot's effects                        |
| `round/`         | Turns, players, scoring, the round's HUD and results, and tournaments   |
| `save/`          | Saved data, SRAM, awards, and records                                   |
| `menus/`         | The title screen and the menus outside of play                          |
| `cinematics/`    | Boot logos, the opening, and the staff roll                             |
| `debug/`         | Debug text, flags, the performance meter, and the hole editor           |
| `game/`          | The game loop and its scenes                                            |

### Assets

`make extract`, which `make setup` runs, cuts the files that the game reads
from its ROM into `assets/`. Git ignores this folder. Each run overwrites
your edits, so you only run it again after you change `assets.txt`.

| Folder in `assets/` | Contents                                                                           |
|---------------------|------------------------------------------------------------------------------------|
| `sound/`            | The sound pointer, wave, and effect banks                                          |
| `songs/`            | One song for each value of `enum Song`                                             |
| `asset_archive/`    | The asset archive's 2449 assets, named by ID. `tools/asset_archive.py` packs them. |
| `text/`             | Messages and the staff roll as text. `tools/messages.py` encodes them.             |
| `character_models/` | Each character's models, and the patches for its other palettes                    |
| `microcode/`        | The RSP's boot, graphics, and audio microcode, as the ROM holds it                 |
| Other folders       | The textures that `textures.txt` lists, as PNG files, under their C file's path    |

The code finds each file through the linker script, so a file can change
size. About half of the archive's assets are images in a run-length
encoding, which `tools/decode_asset.py` reads. They stay encoded in
`assets/`, because the original's encoder split its runs in ways that the
decoded image doesn't determine. Encoding a decoded image again gives a
different file.

The ROM's boot code (IPL3) and the RSP microcode are copied from your ROM as
they are. The PC port reads everything from the ROM at start-up, so it
needs no extracted assets.

## Contributing

To change the code, run the checks, and write commit messages that fit the
project, see [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

This project's own code is under the [MIT License](LICENSE). That code
includes the decompilation of the game and of KMC's C library in
`lib/libkmc`. The other libraries in `lib/` come from elsewhere and keep
their authors' notices: libultra from the
[ultralib](https://github.com/decompals/ultralib) decompilation, and
n_audio, NuSystem, and libmus from the N64 developer kits. *Mario Golf* and its
data belong to Nintendo and Camelot. This repository holds none of the
game's data: no ROM, models, textures, sounds, or music.
