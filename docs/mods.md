# Mods

A mod changes the game without a fork of this repository. It lives in a
folder of its own and holds only the files that it changes. When you build
in that folder, the build takes each file from the mod if the mod has it,
and from this repository if not.

## Before you begin

Build the matching ROM once, as your [build guide](../README.md#get-started)
describes. Mods use this repository's tools, Python environment, and
extracted assets, so `make setup` must have run here.

## Create a mod

1.  Create a folder beside this repository:

    ```sh
    mkdir my-mod
    cd my-mod
    ```

1.  Create a file named `Makefile` that includes this repository's:

    ```make
    include ../mariogolf64/Makefile
    ```

1.  Copy a file that you want to change, at the same path, and edit it.
    For example:

    ```sh
    mkdir -p src/shot
    cp ../mariogolf64/src/shot/club.c src/shot/
    ```

1.  Build the mod:

    ```sh
    make -j"$(nproc)"
    ```

    The ROM is `build/nonmatching/mariogolf64.z64`, in the mod's folder. A
    mod's ROM never matches, so the build doesn't check it.

The mod's own `build/` holds everything that the build makes, so a mod
never uses this repository's objects.

## What a mod can replace

| Path in the mod         | Effect                                                       |
|-------------------------|--------------------------------------------------------------|
| `src/`, `include/`      | A source or header replaces the original's at the same path. |
| `assets/`               | An asset replaces the extracted asset at the same path.      |
| `assets/asset_archive/` | An archive asset replaces the original's of the same ID.     |
| `layout.txt`            | The mod's layout replaces the ROM's. Copy it to add a file.  |

To add a source file, copy `layout.txt` into the mod and list the new file
after the file that it follows, in the segment that it loads in:

```text
segment ovl_03 0x8024D800
    src/round/computer_golfer.c
    src/round/new_file.c
```

## Share files between mods

`MODS` names more folders that are laid out the same way, such as code
that several mods share. The build looks in the mod's folder first, then
in each folder of `MODS` in order, and then in this repository:

```sh
make MODS=../shared-code
```

## Add rules and settings

A mod, or a folder in `MODS`, can have a file named `mod.mk`. The build
reads it before its own rules, so a `mod.mk` can add rules that write
files for the build, and settings that choose files.

Add each setting that chooses which files the build takes to
`MOD_SETTINGS`, as `NAME=value`. No file's date shows such a choice, so the
build records `MOD_SETTINGS` and rebuilds everything when it changes:

```make
CHARACTERS ?= characters
MOD_SETTINGS += CHARACTERS=$(abspath $(CHARACTERS))
```

To give the ROM another name, set `TARGET`, either in `mod.mk` or on the
command line.
