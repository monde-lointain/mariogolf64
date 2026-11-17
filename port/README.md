# Native PC port

This folder holds what it takes to build the decompiled game as a native
64-bit program for Windows, macOS, Linux and Android: our own implementations
of the libraries that the N64 build links (libultra, NuSystem, libmus), a
renderer for the game's display lists on SDL_GPU, and the platform layer
around them. The game's own code in `src/` is compiled as it is, with the
few changes that a 64-bit, little-endian host needs made in place.

Nothing here holds data from the game. The program reads the ROM that you
supply at start-up and takes every model, texture, sound and song from it.

## Building

CMake 3.30 or newer, Python 3.10 or newer, and GCC or Clang are required.
The game's code defines some 60 routines inside others, as the original
did, a GNU C extension that Clang lacks, so the build compiles a copy of
each source with them moved to file scope. SDL is downloaded and built
with the game, so that it has libusb, which a Wii U GameCube controller
adapter needs (libusb's headers must be installed for that).
`-DMG64_USE_SYSTEM_SDL=ON` builds against an installed SDL 3.4 or newer
instead. On Linux, the adapter's USB device must be open to the player,
as a udev rule like this one, in `/etc/udev/rules.d/`, makes it:

    SUBSYSTEM=="usb", ATTRS{idVendor}=="057e", ATTRS{idProduct}=="0337", TAG+="uaccess"

The build takes the textures that the game keeps in its code from the
PNGs that `make extract` cuts from the ROM (see the top-level README).

    cmake --preset linux
    cmake --build --preset linux
    ./build/pc/linux/port/src/mariogolf64 baserom.us.z64

`cmake --list-presets` shows the others:

| Preset           | What it builds                                               |
|------------------|--------------------------------------------------------------|
| `linux`          | Linux, with the default C compiler                           |
| `linux-clang`    | Linux, with Clang                                            |
| `linux-sanitize` | Linux, a debug build with AddressSanitizer and UBSan         |
| `windows-mingw`  | 64-bit Windows, cross-compiled with MinGW-w64                |

Each preset builds into `build/pc/<preset>`. `cmake --workflow --preset
<preset>` configures, builds and runs the tests in one step, as CI does;
`windows-mingw` runs its tests through Wine where it is installed.

`-DMG64_SANITIZERS=<list>` builds the game, SDL and the tests with the
sanitizers named, as `-fsanitize` names them: `address;undefined` is what
`linux-sanitize` sets, and `thread` builds one with ThreadSanitizer. The
configure step rejects the combinations that can't work, such as address
and thread together. The sanitizers' options come from the environment
when the program runs (`ASAN_OPTIONS` and the like).

CI (`.github/workflows/port.yml`) checks the Python tools and the
nested-function hoister on every push. The builds need the assets cut from
the ROM, so they run each preset's workflow only where the repository's
`BASEROM_URL` secret names a private copy of the ROM, which is checked
against `mariogolf64.sha1` before use and deleted after.

## Recording and replaying a session

`--record PATH` writes what the game reads from the first controller to a
new file, as an input script: a line for each game frame at which the
controller changes. `--input-script PATH` plays such a file back into the
first controller. The game reads the controller once a frame, however long
the frame takes, so a session played in a window replays the same way
headless. The recording's header names the save that the session started
from (its CRC-32), and a replay must start from the same save:

    cp my.sra session-start.sra
    mariogolf64 --save my.sra --record session.txt baserom.us.z64
    mariogolf64 --headless --game-frames N --save session-start.sra \
        --input-script session.txt baserom.us.z64

where N is the frame that the recording ends at, as its last line says.

`--record` won't overwrite a file, so a recording can't be lost to a
reused name. Each line is written as soon as it is made, so a session cut
short by a crash keeps what came before.

## Layout

| Folder           | What it holds                                                     |
|------------------|-------------------------------------------------------------------|
| `cmake/`         | Toolchain files and the scripts that write the generated headers  |
| `include/`       | Headers that stand in for libultra's where a 64-bit host differs  |
| `src/os/`        | libultra: threads, message queues, timers, ROM reads, controllers |
| `src/nusys/`     | NuSystem: the frame loop, graphics tasks, controllers, EEPROM     |
| `src/mus/`       | libmus: the sequencer and the synthesizer behind the sound calls  |
| `src/gu/`        | The matrix and vector routines of libultra's gu library           |
| `src/kmc/`       | KMC's C library: its random numbers and CORDIC trigonometry       |
| `src/gfx/`       | The display list interpreter, its SDL_GPU backend and its shaders |
| `src/platform/`  | `main`, the window, input, audio output, saves and the ROM file   |
| `tests/`         | Unit tests of the port's libraries and tools, run by CTest        |

## Design

### Pointers and data from ROM

The game's code runs natively, with 64-bit pointers. Display list commands
grow to two pointer-sized words so that a command can hold any address, as
`gbi.h`'s commands do on the N64. The port writes its copy of `gbi.h` from
libultra's at configure time, widening the words and the casts.

Data that the game reads from ROM stays in the ROM's big-endian byte order
where only the renderer reads it: textures, and the display lists, vertices
and matrices inside character models. The renderer reads memory registered as
big-endian in the ROM's order. Data that the game's code reads itself is
converted to the host's layout as it is loaded.

### Threads

Each libultra thread runs on a thread of the host, but only one of them runs
at a time: the one of highest priority that isn't waiting, as on the N64's
single CPU. A thread gives up the CPU only when it waits, so the game sees
the same ordering that it did on the console, without needing locks.

### Graphics

The interpreter walks each graphics task's display lists on the game's
thread, transforming and lighting vertices as F3DEX2 did, and records the
triangles and rectangles with the RDP state that they need. The backend then
draws a frame's worth of them with as few draw calls as the blending and
depth modes allow: every texture that a frame uses goes into one atlas, and
each triangle carries an index into a table of combiner settings, so that
changes of texture or combiner don't break a batch.

The game draws into textures and reads its own framebuffer, so every color
image that a task sets becomes a render target, and a texture loaded from a
render target's memory samples it. The finished frame and its depth are
copied back into the memory that the game reads them from.

### Audio

The libmus replacement reads the game's song, effect and wave banks from ROM
and mixes them into the audio stream of the host, decoding the VADPCM
samples itself.

### Overlays

Every overlay is linked into the program. Loading one resets its variables
to their initial values, as reading the overlay from ROM did.
