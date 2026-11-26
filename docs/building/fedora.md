# Build on Fedora

This guide shows you how to build the matching ROM, a modern ROM, and the
PC port on Fedora 45.

## Before you begin

- You need a dump of the North American ROM, with the SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`.
- To build the matching ROM, you need an x86-64 PC. KMC GCC, the game's
  compiler, runs only on x86-64 Linux and on Apple silicon Macs. On another
  CPU, you can still build the modern ROM and the PC port.
- To play the PC port, you need a GPU driver with Vulkan support.

## Install the packages

Install the packages for the matching ROM:

```sh
sudo dnf install make gcc git python3 binutils-mips64-linux-gnu glibc-gconv-extra
```

`glibc-gconv-extra` holds iconv's Shift-JIS converter, which the build uses
to convert the game's Japanese text for KMC GCC.

## Get the source

1.  Clone the repository:

    ```sh
    git clone https://github.com/monde-lointain/mariogolf64.git
    cd mariogolf64
    ```

1.  Copy your ROM into the repository as `baserom.us.z64`:

    ```sh
    cp /path/to/your/rom.z64 baserom.us.z64
    ```

## Build the matching ROM

1.  Set up the build:

    ```sh
    make setup
    ```

    This downloads KMC GCC into `tools/gcc_kmc`, creates a Python
    environment in `.venv`, and extracts the game's assets from your ROM
    into `assets/`. You run it once.

1.  Build the ROM:

    ```sh
    make -j"$(nproc)"
    ```

    The build ends with this line, which confirms that your ROM is
    identical to the original:

    ```text
    build/mariogolf64.z64: OK
    ```

The build finds Fedora's `mips64-linux-gnu-` binutils by itself. `make help`
shows the prefix that it found.

## Build a modern ROM

The modern ROM is optional. It needs libdragon's GCC toolchain.

1.  Download and install the toolchain's package:

    ```sh
    curl -LO https://github.com/n64brew/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-x86_64.rpm
    sudo dnf install ./gcc-toolchain-mips64-x86_64.rpm
    ```

    The package installs into `/opt/libdragon`, where the build looks for
    it.

1.  Build the ROM:

    ```sh
    make MODERN=1 -j"$(nproc)"
    ```

    The ROM is `build/modern/mariogolf64.z64`.

## Build the PC port

1.  Install the port's packages. Besides the compiler and shader tools,
    these are the headers for the windowing, audio, and input systems that
    SDL supports:

    ```sh
    sudo dnf install cmake ninja-build glslang pkgconf-pkg-config \
        libusb1-devel systemd-devel dbus-devel ibus-devel \
        alsa-lib-devel pulseaudio-libs-devel pipewire-devel \
        libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXfixes-devel \
        libXi-devel libXScrnSaver-devel libXtst-devel libxkbcommon-devel \
        wayland-devel libdecor-devel mesa-libEGL-devel libdrm-devel mesa-libgbm-devel
    ```

1.  Configure, build, and test the port:

    ```sh
    cmake --workflow --preset linux
    ```

    The first build downloads and builds SDL, so it takes a few minutes.

1.  Run the game:

    ```sh
    build/pc/linux/port/src/mariogolf64 baserom.us.z64
    ```

To learn about the port's settings and its other presets, see
[`port/README.md`](../../port/README.md).

## Install the contributor tools

These packages are optional. Install them to run every check that
[`CONTRIBUTING.md`](../../CONTRIBUTING.md#checks) lists:

```sh
sudo dnf install clang clang-tools-extra mingw64-gcc mingw64-gcc-c++ wine
```

- `clang-tools-extra` provides the `clang-tidy` that `make lint` runs and
  the `clang-query` that `make check-alignment` runs.
- `clang` builds the `linux-clang` presets.
- `mingw64-gcc` and `mingw64-gcc-c++` build the `windows-mingw` preset,
  whose SDL has some C++, and `wine` runs its tests.

## What's next

- To change the code and run the checks, see
  [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
- To build a mod, see [Mods](../mods.md).
