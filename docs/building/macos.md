# Build on macOS

This guide shows you how to build the matching ROM, a modern ROM, and the
PC port on macOS, with [Homebrew](https://brew.sh).

## Before you begin

- You need a dump of the North American ROM, with the SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`.
- To build the matching ROM, you need a Mac with Apple silicon. KMC GCC,
  the game's compiler, has no build for Intel Macs. On an Intel Mac, you
  can still build the modern ROM and the PC port.
- Install Apple's Command Line Tools, which provide Clang and Git:

  ```sh
  xcode-select --install
  ```

- Install [Homebrew](https://brew.sh).

## Install the packages

Install the packages for the matching ROM:

```sh
brew install make python mips-linux-gnu-binutils
```

macOS comes with GNU Make 3.81, which is too old for the build. Homebrew's
`make` package installs GNU Make 4 as `gmake`, so this guide runs `gmake`.

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
    gmake setup
    ```

    This downloads KMC GCC into `tools/gcc_kmc`, creates a Python
    environment in `.venv`, and extracts the game's assets from your ROM
    into `assets/`. You run it once.

1.  Build the ROM:

    ```sh
    gmake -j"$(sysctl -n hw.ncpu)"
    ```

    The build ends with this line, which confirms that your ROM is
    identical to the original:

    ```text
    build/mariogolf64.z64: OK
    ```

## Build a modern ROM

The modern ROM is optional. It needs libdragon's GCC toolchain, which has
no package for macOS, so you build it from source. The build takes about an
hour, and its script installs the libraries that it needs with Homebrew.

1.  Create the folder for the toolchain:

    ```sh
    sudo mkdir -p /opt/libdragon
    sudo chown "$USER" /opt/libdragon
    ```

1.  Build the toolchain into that folder:

    ```sh
    git clone https://github.com/n64brew/libdragon.git ~/libdragon
    N64_INST=/opt/libdragon ~/libdragon/tools/build-toolchain.sh
    ```

1.  Build the ROM:

    ```sh
    gmake MODERN=1 -j"$(sysctl -n hw.ncpu)"
    ```

    The ROM is `build/modern/mariogolf64.z64`.

## Build the PC port

On macOS, the port draws with Metal.

1.  Install the port's packages:

    ```sh
    brew install cmake ninja glslang spirv-cross libusb
    ```

    `spirv-cross` translates the port's shaders into Metal's shading
    language.

1.  Configure, build, and test the port:

    ```sh
    cmake --workflow --preset macos
    ```

    The first build downloads and builds SDL, so it takes a few minutes.

1.  Run the game:

    ```sh
    build/pc/macos/port/src/mariogolf64 baserom.us.z64
    ```

To learn about the port's settings and its other presets, see
[`port/README.md`](../../port/README.md).

## Install the contributor tools

These packages are optional. Install them to run every check that
[`CONTRIBUTING.md`](../../CONTRIBUTING.md#checks) lists:

```sh
brew install llvm
```

Homebrew's `llvm` provides the `clang-tidy` that `gmake lint` runs and the
`clang-query` that `gmake check-alignment` runs. Homebrew doesn't add them
to your `PATH`, so add them when you run those checks:

```sh
PATH="$(brew --prefix llvm)/bin:$PATH" gmake lint
PATH="$(brew --prefix llvm)/bin:$PATH" gmake check-alignment
```

`gmake check-alignment` also needs libdragon's toolchain, as
[Build a modern ROM](#build-a-modern-rom) describes.

## What's next

- To change the code and run the checks, see
  [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
- To build a mod, see [Mods](../mods.md).
