# Build on Windows with WSL

This guide shows you how to build the matching ROM, a modern ROM, and the
PC port on Windows. The build runs in Ubuntu, under the Windows Subsystem
for Linux (WSL), and the PC port that it builds is a native Windows
program.

## Before you begin

- You need a dump of the North American ROM, with the SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`.
- You need 64-bit Windows 10, version 2004 or later, or Windows 11, on an
  x86-64 PC.

## Install WSL and Ubuntu

1.  Open PowerShell as an administrator, and install WSL with Ubuntu:

    ```powershell
    wsl --install
    ```

1.  Restart Windows when the installer asks you to.
1.  Open **Ubuntu** from the **Start** menu, and create your Linux user
    name and password.

Run the rest of this guide's commands in the Ubuntu terminal.

## Build the ROMs

Follow the [Ubuntu guide](ubuntu.md) through
[Build a modern ROM](ubuntu.md#build-a-modern-rom), with these
differences:

- Clone the repository into your Linux home folder, such as
  `~/mariogolf64`, not into a Windows folder under `/mnt/c`. The build
  reads thousands of files, which is many times slower across the
  boundary between the two systems.
- Copy your ROM from its Windows folder, which WSL shows under `/mnt/c`.
  For example, if your Windows user name is `alex` and the ROM is in your
  **Downloads** folder:

  ```sh
  cp /mnt/c/Users/alex/Downloads/rom.z64 baserom.us.z64
  ```

## Build the PC port for Windows

The `windows-mingw` preset cross-compiles the port into a Windows program
with MinGW-w64.

1.  Install the packages for the Windows build. It needs none of the Linux
    windowing and audio libraries:

    ```sh
    sudo apt install cmake ninja-build glslang-tools mingw-w64
    ```

1.  Configure and build the port:

    ```sh
    cmake --preset windows-mingw
    cmake --build --preset windows-mingw
    ```

    The first build downloads and builds SDL, so it takes a few minutes.

1.  Copy the program and your ROM into a Windows folder. For example, for
    the user `alex`:

    ```sh
    mkdir -p /mnt/c/Users/alex/Games/mariogolf64
    cp build/pc/windows-mingw/port/src/mariogolf64.exe baserom.us.z64 \
        /mnt/c/Users/alex/Games/mariogolf64/
    ```

1.  In File Explorer, open that folder and double-click
    **mariogolf64.exe**. The program plays the `baserom.us.z64` beside it.

`mariogolf64.exe` needs no other files: it links SDL statically, and uses
only the DLLs that come with Windows.

To learn about the port's settings, see
[`port/README.md`](../../port/README.md).

## Install the contributor tools

To run every check that [`CONTRIBUTING.md`](../../CONTRIBUTING.md#checks)
lists, install the tools that the Ubuntu guide's
[Install the contributor tools](ubuntu.md#install-the-contributor-tools)
lists. With `wine` installed, `cmake --workflow --preset windows-mingw`
also runs the Windows build's tests.

## What's next

- To change the code and run the checks, see
  [`CONTRIBUTING.md`](../../CONTRIBUTING.md).
- To build a mod, see [Mods](../mods.md).
