# Mario Golf 64

A matching decompilation/build of Mario Golf 64 (N64, North America — NMFE, rev 0). The build
reproduces the original ROM byte-for-byte: undecompiled code is re-assembled from baserom-extracted
assembly, and decompiled libraries are compiled from C/asm in `src/`. Libraries land on `master` one
commit at a time.

## Requirements

- The original ROM at `baserom.z64` in the project root (SHA-1
  `e2c4e7a905b29529b49a1619a401fe699224829b`). It is gitignored and never committed.
- `mips-linux-gnu` binutils (supplies `cpp`, `as`, `ld`, `objcopy`, `strip`).
- `python3` with `venv`.
- `wget` (to download the KMC GCC 2.7.2 + binutils 2.6 toolchain during setup).

## Build

```
make setup     # create venv (splat) + download the KMC toolchain into tools/cc
make extract   # split the baserom into asm/ + scaffold with splat
make           # build build/mariogolf64.z64 and verify it matches the baserom
```

`tools/verify-rom.sh` runs `make` and asserts the `OK` line before trusting the ROM SHA-1 (the
build's stale-ROM guard). CI (`.github/workflows/build.yml`) runs the same setup -> extract -> verify
flow in the container defined under `docker/`.

## Layout

- `mariogolf64.yaml` — splat configuration (ROM layout + per-file subsegments).
- `src/` — decompiled C and vendored hand-asm. `include/` — headers.
- `mk/` — per-tree compile rules included by the top `Makefile`.
- `symbol_addrs.txt`, `reloc_addrs.txt`, `hardware_regs.txt`, `extern_syms.txt` — splat/link inputs.
