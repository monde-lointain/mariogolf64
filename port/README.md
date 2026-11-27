# PC port

The PC port builds the decompiled game as a native 64-bit program for
Linux, Windows, macOS, and Android. The game's code in `src/` compiles as
it is, with the few changes that a 64-bit, little-endian host needs. This
folder holds everything else that the program needs:

- The port's own versions of the libraries that the N64 build links:
  libultra, NuSystem, libmus, and KMC's C library.
- A renderer that interprets the game's display lists and draws them with
  SDL_GPU.
- The platform layer: the window, input, audio, saves, and settings.

The program reads every model, texture, sound, and song from your ROM when
it starts.

## Build the port

Your system's [build guide](../README.md#get-started) lists the packages
that the port needs and builds the default preset. In short, on Linux:

```sh
cmake --workflow --preset linux
build/pc/linux/port/src/mariogolf64 baserom.us.z64
```

The build needs nothing from the ROM: even the few textures that the game
keeps in arrays in its code are read from the ROM when the program starts.
The build needs CMake 3.30 or later and Python 3.10 or later. It
downloads SDL and builds it with the game, so that SDL supports a Wii U
GameCube controller adapter through libusb. To build against an installed
SDL 3.4 or later instead, configure with `-DMG64_USE_SYSTEM_SDL=ON`.

The game's code defines some 60 routines inside other routines, a GNU C
extension that Clang lacks. For Clang, the build compiles a copy of each
source with those routines moved out to file scope.

### Presets

Each preset builds into `build/pc/PRESET`. To list the presets for your
system, run `cmake --list-presets`.

| Preset                 | What it builds                                                     |
|------------------------|--------------------------------------------------------------------|
| `linux`                | Linux, with the default C compiler                                 |
| `linux-clang`          | Linux, with Clang                                                  |
| `linux-sanitize`       | A Linux debug build, with AddressSanitizer and UBSan               |
| `linux-clang-sanitize` | The same, with Clang, without KMC GCC's meanings, and with fuzzing |
| `linux-clang-tysan`    | An optimized Clang 23 build, with TypeSanitizer                    |
| `linux-clang-tsan`     | An optimized Clang build, with ThreadSanitizer                     |
| `windows-mingw`        | 64-bit Windows, cross-compiled with MinGW-w64                      |
| `macos`                | macOS, with Apple's Clang                                          |
| `macos-sanitize`       | A macOS debug build, with AddressSanitizer and UBSan               |
| `android-arm64`        | 64-bit Arm Android, cross-compiled with the Android NDK            |

`cmake --workflow --preset PRESET` configures, builds, and runs the tests
in one step, for every preset but `android-arm64`, which has no tests. To configure and build without the tests, run
`cmake --preset PRESET` and then `cmake --build --preset PRESET`.

### Build for Android

The `android-arm64` preset builds the app for 64-bit Arm devices with
Android 10 or later, the first version that requires Vulkan.

1.  Install the [Android SDK](https://developer.android.com/studio) with
    an NDK, and a Java Development Kit, version 17 to 23.
1.  Set `ANDROID_NDK_HOME` to the NDK's folder, such as:

    ```sh
    export ANDROID_NDK_HOME="$HOME/Android/Sdk/ndk/30.0.16248370"
    ```

1.  Configure the preset, with `MG64_JAVA_HOME` set to the Java that Gradle
    runs on. It defaults to `JAVA_HOME`.

    ```sh
    cmake --preset android-arm64 -DMG64_JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
    ```

1.  Build the app, and install it on a device:

    ```sh
    cmake --build --preset android-arm64 --target mg64_apk
    adb install build/pc/android-arm64/port/android/gradle/app/outputs/apk/debug/app-debug.apk
    ```

`mg64_apk` packs the program into a debug APK with Gradle, using SDL's own
Android project and Java sources. Gradle downloads its Android plugin on
its first run.

When the app first starts, it asks for the ROM with the system's file
picker, and keeps a copy among its own files. To copy the ROM to a debug
build without the picker:

```sh
adb push baserom.us.z64 /data/local/tmp/
adb shell run-as org.mariogolf64 sh -c "'mkdir -p files && cp /data/local/tmp/baserom.us.z64 files/'"
adb shell rm /data/local/tmp/baserom.us.z64
```

## Run the game

```sh
mariogolf64 [OPTIONS] [ROM]
```

If you don't name a ROM, the program plays the one that the `rom` setting
names. Without one, it looks for `baserom.us.z64` beside itself, then in
the current folder, and then in your data folder. If it
finds none, it asks for the ROM with the system's file dialog, and keeps a
copy in your data folder for the next start. The ROM can be in any byte
order: `.z64`, `.v64`, or `.n64`.

Your data folder holds your save and your settings:

| System  | Data folder                                              |
|---------|----------------------------------------------------------|
| Linux   | `~/.local/share/mariogolf64/mariogolf64/`                |
| Windows | `%APPDATA%\mariogolf64\mariogolf64\`                     |
| macOS   | `~/Library/Application Support/mariogolf64/mariogolf64/` |

### Settings

The game keeps its settings in `config.ini` in your data folder, and
writes the file, with a comment on each setting, when there is none. The
settings include the ROM to play, the scale to draw at, full screen, the present mode
(`mailbox`, `vsync`, or `immediate`), anti-aliasing (`off`, `2x`, `4x`,
or `8x` samples a pixel; `4x` by default, and `off` on Android), the window's size and place, whether
to pause when another window has the focus, the volume, and the keys and
gamepad controls for each N64 control.

If a line has a problem, the game logs the line's number and ignores it.
When the game ends, it saves the window's size and place in the file,
unless the file has a problem. This rewrites the file, so comments of your
own are lost.

Options on the command line take precedence over the file:

| Option                | Effect                                                                                                 |
|-----------------------|--------------------------------------------------------------------------------------------------------|
| `--scale N`           | Draws at N times the N64's 320 by 240 pixels, from 1 to 8.                                             |
| `--fullscreen`        | Starts in full screen.                                                                                 |
| `--config PATH`       | Reads and writes the settings in another file.                                                         |
| `--save PATH`         | Uses another save file.                                                                                |
| `--headless`          | Runs without a window. It reads no settings without `--config`.                                        |
| `--record PATH`       | Records the first controller's input. See [Record and replay a session](#record-and-replay-a-session). |
| `--input-script PATH` | Plays a recording into the first controller.                                                           |
| `--frames N`          | Ends the run after N video retraces.                                                                   |
| `--game-frames N`     | Ends the run at game frame N.                                                                          |
| `--screenshot PATH`   | Saves the last frame as a PNG image.                                                                   |
| `--gpu-debug`         | Turns on SDL_GPU's debug mode, with the driver's validation.                                           |

An option that the program doesn't know makes it print the list of
options and exit.

### Use a GameCube controller adapter on Linux

To use a Wii U GameCube controller adapter on Linux, give your user access
to its USB device. Create a udev rule, such as
`/etc/udev/rules.d/51-gamecube-adapter.rules`, with this line:

```text
SUBSYSTEM=="usb", ATTRS{idVendor}=="057e", ATTRS{idProduct}=="0337", TAG+="uaccess"
```

Then reconnect the adapter.

## Record and replay a session

A recording is an input script: a text file with a line for each game frame
at which the first controller's state changes. The game reads the
controller once a frame, however long the frame takes, so a session that
you play in a window replays the same way headless.

The recording's header names the save that the session started from, by
its CRC-32, and a replay must start from the same save:

1.  Keep a copy of the save that you start from, and record a session:

    ```sh
    cp my.sra session-start.sra
    mariogolf64 --save my.sra --record session.txt baserom.us.z64
    ```

1.  Replay the session headless from a copy of that save, to the frame on
    the recording's last line:

    ```sh
    cp session-start.sra replay.sra
    mariogolf64 --headless --game-frames N --save replay.sra \
        --input-script session.txt baserom.us.z64
    ```

1.  Compare the saves:

    ```sh
    cmp replay.sra my.sra
    ```

`--record` never overwrites a file. It writes each line as soon as it
makes it, so a session that crashes keeps everything up to the crash.

A replay that stops before frame N, because the window closed or the
system ended the program, logs how far it got and exits with a failure.
With `--screenshot`, it saves the frame where it stopped.

A replay runs in lockstep with the game, so it can't show a stall. When you
play in a window, the game logs a stall instead: a retrace that came 100 ms
or more after the last, or a frame that the game took that long to reach.
The log line gives the game frame, so you can find the spot in your
recording.

The recorded sessions in [`playthroughs/`](playthroughs/README.md) replay
the game's main modes.

## Test the port

The port's tests are in `tests/`, and CTest runs them. Every preset's
workflow runs them, and none of them needs the ROM: the tests that read
data from a ROM build small ROMs of their own.

```sh
cmake --workflow --preset linux
```

Without a GPU, the GPU's tests skip themselves. To run the
`windows-mingw` tests on Linux, install Wine.

### Sanitizers

`-DMG64_SANITIZERS=LIST` builds the game, SDL, and the tests with the
sanitizers that `LIST` names, as `-fsanitize` names them. For example,
`linux-sanitize` sets `address;undefined`. The configure step rejects
combinations that can't work, such as `address;thread`. The sanitizers
read their options from the environment when the program runs, such as
`ASAN_OPTIONS`.

The game was written for KMC GCC, which gave some undefined behavior a
meaning that modern compilers don't have to, such as wrapping on signed
overflow. By default, the port keeps those meanings with flags such as
`-fwrapv`, which also hide those cases from UBSan.
`-DMG64_KMC_SEMANTICS=OFF`, which the `linux-clang-sanitize` preset sets,
leaves the flags out, so that UBSan finds what the port must fix before it
can drop them.

The other sanitizer presets each look for one kind of problem:

- `linux-clang-tysan` uses TypeSanitizer to find code that reads or stores
  an object as a type that strict aliasing doesn't allow. It checks only
  the project's code, not SDL's, and needs Clang 23. It builds optimized,
  because the checks make a debug build too slow to replay a session.
- `linux-clang-tsan` uses ThreadSanitizer to find data races between the
  game's threads, the threads that raise the N64's interrupts, and the
  renderer. It draws with Mesa's lavapipe
  (`VK_LOADER_DRIVERS_SELECT=*lvp*`), because NVIDIA's driver crashes under
  ThreadSanitizer. `src/sanitizers/tsan_suppressions.txt` ignores what it
  reports in SDL, libdbus, and Mesa. lavapipe starts a thread for each CPU,
  so set `LP_NUM_THREADS` low when you run several replays at once.

### Fuzzing

The fuzz targets in `tests/` feed generated input to the port's parsers of
outside data: `input_script_fuzz` to the input script parser,
`config_fuzz` to the settings file's reader, and `rom_image_fuzz` to the
ROM's byte-order detection. Each target checks
properties that must hold for any input, such as that a script reads back
the same after it's written. Every preset's tests run each target over its
saved inputs in `tests/fuzz_corpus/TARGET`.

`linux-clang-sanitize` builds the targets with libFuzzer. To fuzz a target,
give it a folder of its own, seeded with the saved inputs:

```sh
mkdir -p /tmp/fuzz
cp port/tests/fuzz_corpus/input_script_fuzz/* /tmp/fuzz
build/pc/linux-clang-sanitize/port/tests/input_script_fuzz -max_total_time=300 /tmp/fuzz
```

libFuzzer writes an input that fails to `crash-HASH` in the current folder.
After you fix the bug, add the input to the corpus, with a name that says
what it holds.

## Layout

| Folder          | Contents                                                           |
|-----------------|--------------------------------------------------------------------|
| `cmake/`        | Toolchain files, and the scripts that write the generated headers  |
| `include/`      | Headers that replace libultra's where a 64-bit host differs        |
| `src/os/`       | libultra: threads, message queues, timers, ROM reads, controllers  |
| `src/nusys/`    | NuSystem: the frame loop, graphics tasks, controllers, EEPROM      |
| `src/mus/`      | libmus: the sequencer and the synthesizer behind the sound calls   |
| `src/gu/`       | libultra's gu library: matrices, vectors, sine, and cosine         |
| `src/kmc/`      | KMC's C library: its random numbers and CORDIC trigonometry        |
| `src/gfx/`      | The display list interpreter, its SDL_GPU backend, and its shaders |
| `src/platform/` | `main`, the window, input, audio output, saves, and the ROM file   |
| `android/`      | The Android app's Gradle project and manifest                      |
| `tests/`        | Unit tests and fuzz targets, run by CTest                          |
| `playthroughs/` | Recorded sessions, and the notes made while recording them         |

## Design

This section explains the decisions that shape the port. The source files'
own comments cover the details.

### Pointers and data from the ROM

The game's code runs natively, with 64-bit pointers. Each display list
command grows to two pointer-sized words, so that a command can hold any
host address, as `gbi.h`'s commands do on the N64. The build writes the
port's copy of `gbi.h` from libultra's when it configures, with the words
and casts widened.

The renderer tells a host's address from an N64 address by its size: N64
addresses fit in 32 bits, and the host's don't. The build makes the
program position-independent, so that the system loads it above 4 GiB.

Data that only the renderer reads stays in the ROM's big-endian byte order:
textures, and the display lists, vertices, and matrices inside character
models. The renderer reads memory that the port registered as big-endian
in that order. Data that the game's code reads itself is converted to the
host's byte order when it loads.

### Threads

Each libultra thread runs on a host thread, but only one runs at a time:
the thread of highest priority that isn't waiting, as on the N64's single
CPU. A thread gives up the CPU only when it waits, so the game sees the
same order of events as on the console, without locks.

### Graphics

The interpreter walks each graphics task's display lists on the game's
thread. It transforms and lights vertices as the F3DEX2 microcode did, and
records the triangles and rectangles with the RDP state that they need.
The backend then draws a frame's primitives with as few draw calls as the
blend and depth modes allow. Every texture that a frame uses goes into one
atlas, and each triangle carries an index into a table of combiner
settings, so a change of texture or combiner doesn't end a batch.

The game draws into textures and reads its own frame buffer. So every
color image that a task sets becomes a render target, and a texture loaded
from a render target's memory samples that target. The finished frame and
its depth are copied back into the memory where the game reads them.

The render targets hold 8 bits for each color channel, where the N64's
frame buffer held 5. The port leaves out the RDP's color dither, which
only hid that loss: NuSystem turns on the video interface's dither filter,
which rebuilds each pixel's lost bits from its neighbors, so the console
shows close to what the port draws. The port keeps the alpha dither,
which with the alpha compare makes a primitive partly transparent.

The shaders are GLSL, compiled to SPIR-V for Vulkan. On Apple's systems,
the build also translates them to Metal's shading language with
SPIRV-Cross.

### Audio

The port's libmus reads the game's song, effect, and wave banks from the
ROM, decodes their VADPCM samples, and mixes them into the host's audio
stream.

### Overlays

Every overlay is linked into the program. Loading an overlay resets its
variables to their values at start-up, as reading it from the ROM did.

### Arithmetic

The port gives the game the same numbers on every host, so a recorded
session plays the same way on each. The build forbids fusing a multiply and
an add, which some compilers do by default. The game's random numbers and
its `sin`, `cos`, `tan`, `atan`, and `atan2` are KMC's, as on the N64
(`src/kmc/`). Its `sinf` and `cosf`, and the gu library's transforms, give
the float nearest to the true value (`src/gu/sine.c`), because the hosts'
C libraries differ in the last bit. The rest of its math is its own code,
or square roots, which IEEE arithmetic rounds the same everywhere.
