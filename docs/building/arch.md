# Build on Arch Linux

This guide shows you how to build the matching ROM, a modern ROM, and the
PC port on Arch Linux.

## Before you begin

- You need a dump of the North American ROM, with the SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`.
- To build the matching ROM, you need an x86-64 PC. KMC GCC, the game's
  compiler, runs only on x86-64 Linux and on Apple silicon Macs. On another
  CPU, you can still build the modern ROM and the PC port.
- To play the PC port, you need a GPU driver with Vulkan support.

## Install the packages

Arch's official repositories have no MIPS binutils, so you build them from
the [AUR](https://aur.archlinux.org/packages/mips64-elf-binutils).

1.  Install the packages for the matching ROM, and the tools to build an
    AUR package:

    ```sh
    sudo pacman -S --needed base-devel git python
    ```

1.  Build and install `mips64-elf-binutils` from the AUR:

    ```sh
    git clone https://aur.archlinux.org/mips64-elf-binutils.git
    cd mips64-elf-binutils
    makepkg -si
    cd ..
    ```

    If you use an AUR helper, such as `yay`, you can run
    `yay -S mips64-elf-binutils` instead.

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

    Git ignores this file, so you can't commit it by mistake.

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

The build finds the AUR's `mips64-elf-` binutils by itself. `make help`
shows the prefix that it found.

## Build a modern ROM

The modern ROM is optional. It needs libdragon's GCC toolchain, which
libdragon publishes as a Debian package. You can unpack its files into your
home folder.

1.  Download the package, and unpack its toolchain into
    `~/.local/libdragon`:

    ```sh
    curl -LO https://github.com/n64brew/libdragon/releases/download/toolchain-continuous-prerelease/gcc-toolchain-mips64-x86_64.deb
    mkdir -p ~/.local/libdragon
    bsdtar -xOf gcc-toolchain-mips64-x86_64.deb 'data.tar.*' |
        bsdtar -xf - -C ~/.local/libdragon --strip-components 3 ./opt/libdragon
    ```

1.  Tell the build where the toolchain is. Add this line to `~/.bashrc`, and
    run it in your current shell:

    ```sh
    export N64_INST="$HOME/.local/libdragon"
    ```

1.  Build the ROM:

    ```sh
    make MODERN=1 -j"$(nproc)"
    ```

    The ROM is `build/modern/mariogolf64.z64`.

## Build the PC port

1.  Install the port's packages. Besides the compiler and shader tools,
    these are the libraries for the windowing, audio, and input systems
    that SDL supports:

    ```sh
    sudo pacman -S --needed cmake ninja glslang pkgconf \
        libusb systemd-libs dbus ibus alsa-lib libpulse pipewire \
        libx11 libxext libxrandr libxcursor libxfixes libxi libxss libxtst \
        libxkbcommon wayland libdecor libglvnd libdrm mesa
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
sudo pacman -S --needed clang mingw-w64-gcc wine
```

- `clang` builds the `linux-clang` presets, and provides the `clang-tidy`
  that `make lint` runs and the `clang-query` that `make check-alignment`
  runs.
- `mingw-w64-gcc` builds the `windows-mingw` preset, and `wine` runs its
  tests.

## What's next

- To change the code and run the checks, see
  [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
- To build a mod, see [Mods](../mods.md).
