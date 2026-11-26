# Build on Ubuntu

This guide shows you how to build the matching ROM, a modern ROM, and the
PC port on Ubuntu 26.04 LTS. For Ubuntu 24.04 LTS, see
[Ubuntu 24.04 LTS](#ubuntu-2404-lts).

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
sudo apt update
sudo apt install build-essential git python3 python3-venv binutils-mips-linux-gnu
```

If apt reports `Unable to locate package binutils-mips-linux-gnu`, your
system installs the packages built for newer CPUs, called amd64v3, as
Ubuntu's cloud images and GitHub's runners do. Apt then doesn't list
Ubuntu's MIPS binutils. Install them from the baseline packages:

```sh
sudo apt -o APT::Architecture-Variants= update
sudo apt -o APT::Architecture-Variants= install binutils-mips-linux-gnu
```

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

## Build a modern ROM

The modern ROM is optional. It needs libdragon's GCC toolchain.

1.  Download and install the toolchain's package:

    ```sh
    sudo apt install curl
    curl -LO https://github.com/n64brew/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-x86_64.deb
    sudo apt install ./gcc-toolchain-mips64-x86_64.deb
    ```

    The package installs into `/opt/libdragon`, where the build looks for
    it. On an Arm CPU, download `gcc-toolchain-mips64-arm64.deb` instead.

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
    sudo apt install cmake ninja-build glslang-tools pkg-config \
        libusb-1.0-0-dev libudev-dev libdbus-1-dev libibus-1.0-dev \
        libasound2-dev libpulse-dev libpipewire-0.3-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
        libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
        libwayland-dev libdecor-0-dev libegl-dev libdrm-dev libgbm-dev
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
sudo apt install clang clang-tidy clang-tools mingw-w64 wine
```

- `clang-tidy` runs `make lint`, and `clang-tools` provides the
  `clang-query` that `make check-alignment` runs.
- `clang` builds the `linux-clang` presets.
- `mingw-w64` builds the `windows-mingw` preset, and `wine` runs its tests.

## Ubuntu 24.04 LTS

The same steps work on Ubuntu 24.04 LTS, except that its CMake, version
3.28, is older than the 3.30 that the port needs. Install a newer CMake
from PyPI into an environment of its own, and use it to build the port:

```sh
python3 -m venv ~/.local/share/cmake
~/.local/share/cmake/bin/pip install cmake
~/.local/share/cmake/bin/cmake --workflow --preset linux
```

## What's next

- To change the code and run the checks, see
  [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
- To build a mod, see [Mods](../mods.md).
