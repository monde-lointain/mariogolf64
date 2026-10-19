---
type: Procedure
name: match-ultralib-segment
title: "Match a libultra segment"
description: "Match a given segment to a libultra ROM segment; use when converting unidentified asm libultra ranges such as osRecvMesg to matching C source."
tags: [libultra, segments, splat]
---

# Rules

* Compile local source under `src/libultra`, preserving the upstream subtree: `../ultralib/src/io/pfsnumfiles.c` becomes `src/libultra/io/pfsnumfiles.c`. Source there uses real libultra names, never `func_...` or `D_...` placeholders.
* Keep unidentified neighboring libultra functions as separate `hasm` entries until each is matched as source.
* Add callable ROM symbols to `symbol_addrs.txt` at their runtime VRAM, never as linker-script aliases. Do not patch, generate or post-process linker scripts, and do not add `--allow-multiple-definition`.
* If a libultra source file emits `.data` or `.bss`, match those sections in `mariogolf64.yaml`. Do not change C definitions to `extern`, delete them, or alias them away; that can change code generation even when it looks equivalent.
* When asm references a field inside a known struct, annotate the **owning** symbol in `symbol_addrs.txt` with `type:` and `size:` so the output is `%hi(__osPiDevMgr + 0x8)`. Never create a standalone `D_...` or linker alias for a field address.

# Workflow

1. **Inspect the target before trusting upstream.** This ROM's libultra can differ from the checked-in upstream copy (extra init loops, literal stores, changed command or status values).

   ```sh
   python3 tools/asm-differ/diff.py --no-pager osPfsNumFiles
   ```

2. **Find upstream source and headers** in the decompals ultralib checkout next to this repo (`git clone https://github.com/decompals/ultralib ../ultralib` if missing). Search `../ultralib/src` and `../ultralib/include` for the function name.

3. **Port the file** into `src/libultra`, preserving the subtree. Project copies of the headers are in `include/libultra` (`PR/`, `internal/`, `compiler/gcc/`). Verify macro values locally before using semantic names (for example `CONT_CMD_NOP` may be `0xff` while the target wants a literal `0`). Treat upstream as a starting point; build early and let the diff guide you. Hand-written upstream assembly (`.s`) becomes a `hasm` subsegment with its source at `src/libultra/<subdir>/<name>.s`, assembled through the KMC driver.

4. **Replace the ROM range** in `mariogolf64.yaml` with a C segment ([format](/reference/splat-yaml.md)):

   ```yaml
   - [0x4CE0, c, libultra/debug/assert]
   - [0x7E360, hasm, libultra/os/setintmask]
   ```

   Adjust start and end offsets and the segment name if the range does not match the upstream file.

5. **Match emitted `.data`, `.rodata` and `.bss`.** First check whether the object emits any. KMC objects always carry empty `.data`/`.bss` plus `.reginfo`, `.comment` and `.note`, so check section sizes, not names:

   ```sh
   mips-linux-gnu-objdump -h build/src/libultra/os/initialize.o
   mips-linux-gnu-nm -n build/src/libultra/os/initialize.o
   ```

   If it does, split the broad raw segments around the emitted range so the C file owns exactly that data; the raw ranges before and after stay raw:

   ```yaml
   - [0xA3590, .data, libultra/io/controller]
   - [0xA35A0, .data, libultra/gu/align]
   - [<rom>, .rodata, libultra/gu/sinf]
   ```

   Use [match-data-segment](/procedures/match-data-segment.md) for `.data` and `.rodata`, and [match-bss-segment](/procedures/match-bss-segment.md) for `.bss`. If the object has only `.text`, keep matching instructions first.

6. **Name real symbols.** If the new source calls functions or data elsewhere in the ROM, instead of aliasing unlabeled `D_`/`func_` symbols, add the real ultralib names to `symbol_addrs.txt` ([format](/reference/symbol-addrs-format.md)). Entries use this form; add a line only when the name is absent:

   ```text
   __osDisableInt = 0x800B0500; // type:func
   __osPiDevMgr = 0x800C7E70; // size:0x1C
   ```

   Check that nothing in the segment still uses placeholders: search `src/libultra` for `func_800<hex>` and `D_800<hex>` names. Fix only the symbols involved; do not rename unrelated ones blindly.

7. **Build and verify**, then diff every function the new source emits before looking at linker or data issues:

   ```sh
   ./tools/build-and-verify.sh
   python3 tools/asm-differ/diff.py --no-pager osPfsNumFiles
   ```

8. **Record learnings** only if they would help match an unrelated libultra function tomorrow ([editing rules](/reference/kmc-gcc/editing-rules.md)).

# Build flags and VERSION_J

`src/libultra` builds with the libultra profile; [compiler profiles](/project/compiler-profiles.md) lists its flags. Print the effective flags with `make -s print-cflags SRC=src/libultra/io/pfsnumfiles.c`. If one file needs different flags, add a file-specific override as that page describes (`$(BUILD_DIR)/src/libultra/io/pfsnumfiles.%: OPT_FLAGS = -O2`).

When porting upstream:

* Keep only the `BUILD_VERSION >= VERSION_J` side of version conditionals.
* `char` is signed (`-fsigned-char` in the profile). Compare the emitted loads and shifts against the assembly before changing types.
* Trust the assembly over upstream: if upstream has a call the target lacks, omit it.
