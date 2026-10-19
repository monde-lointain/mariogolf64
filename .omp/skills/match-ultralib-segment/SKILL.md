---
name: match-ultralib-segment
description: Match a given segment to a libultra ROM segment. Use this when converting unidentified asm libultra ranges such as osRecvMesg to matching C source.
---

# Match an Ultralib Segment

## Core Rules

- Match libultra by compiling local source under `src/libultra`; do not patch linker scripts, generated linker scripts, or archive-member references.
- Preserve the upstream libultra subdirectory, for example `../ultralib/src/io/pfsnumfiles.c` becomes `src/libultra/io/pfsnumfiles.c`.
- Keep unidentified neighboring libultra functions as separate `asm` segments until each function is matched as source.
- Source under `src/libultra` should use real libultra names, not `func_...` or `D_...` placeholders.
- Add required callable ROM symbols to `symbol_addrs.txt` with runtime VRAM addresses. Never add libultra function aliases to a linker script.
- If a libultra source file emits `.data` or `.bss`, match those sections in `mariogolf64.yaml`; do not remove the C definitions, make them `extern`, allow multiple definitions, add linker-script aliases, or delete generated linker entries.
- When asm references a field inside a known libultra struct, annotate the owning struct symbol in `symbol_addrs.txt` with its real type and size. Do not add a linker-script alias for the field address.

## Workflow

1. Inspect the target function before trusting upstream source. Version-specific libultra code can differ from the checked-in upstream copy:

   ```sh
   python3 tools/asm-differ/diff.py --no-pager osPfsNumFiles
   ```

   Watch for local deltas such as extra initialization loops, literal stores, or changed command/status values.

2. Find the upstream source and headers in the decompals ultralib checkout next to this repository (`git clone https://github.com/decompals/ultralib ../ultralib` if it is missing):

   ```sh
   rg -n "osPfsNumFiles" ../ultralib/src ../ultralib/include
   ```

3. Copy or port the upstream file into `src/libultra`, preserving the source subtree. Project copies of the libultra headers live in `include/libultra` (`PR/`, `internal/`, `compiler/gcc/`). Verify macro values in local headers before using semantic-looking names; for example `CONT_CMD_NOP` may be `0xff`, while the target may want a literal `0`. Treat the upstream as a starting point — the compiled ROM may have been built from a different version of the source. Build early and let the diff guide iterations rather than analyzing the assembly exhaustively beforehand.

   ```text
   ../ultralib/src/os/recvmesg.c -> src/libultra/os/recvmesg.c
   ../ultralib/src/io/pfsnumfiles.c -> src/libultra/io/pfsnumfiles.c
   ```

   Hand-written libultra assembly (`.s` upstream) becomes a `hasm` subsegment with its source at `src/libultra/<subdir>/<name>.s`; the Makefile assembles those through the KMC driver.

4. Replace the matching ROM range in `mariogolf64.yaml` with a normal C segment:

   ```yaml
   - [0x4CE0, c, libultra/debug/assert]
   - [0x7E360, hasm, libultra/os/setintmask]
   ```

5. If the new source object emits `.data` or `.bss`, match those sections before trying linker workarounds. First inspect the object:

   ```sh
   mips-linux-gnu-objdump -h build/src/libultra/os/initialize.o
   mips-linux-gnu-nm -n build/src/libultra/os/initialize.o
   ```

   Then split the existing raw data/BSS ranges in `mariogolf64.yaml` so the C file owns exactly the emitted data. Keep the definitions in C when they are part of the upstream file; changing them to `extern` can alter code generation even when it looks semantically equivalent. Existing examples:

   ```yaml
   - [0xA3590, .data, libultra/io/controller]
   - [0xA35A0, .data, libultra/gu/align]
   ```

   The raw ranges before and after remain raw segments. For BSS, follow the `match-bss-segment` skill.

6. If the new source calls functions or references data already present elsewhere in the ROM, add the real ultralib symbol names to `symbol_addrs.txt`. Avoid using aliases to make the work with unlabeled symbols (those starting with D_ or func_):

   ```text
   __osDisableInt = 0x800A61B0; // type:func
   __osRestoreInt = 0x800A61D0; // type:func
   ```

   Before moving on, verify that the matched/called functions use real names instead of address-derived placeholders:

   ```sh
   rg -n "func_800[0-9A-Fa-f]+|D_800[0-9A-Fa-f]+" src/libultra
   ```

   If the command finds unrelated existing placeholders, do not rename them blindly. Fix only the symbols involved in the segment being matched.

7. If asm references an address inside a known libultra struct, fix the owning symbol metadata instead of creating a standalone `D_...` symbol. The desired output is an addend reference such as `%hi(__osPiDevMgr + 0x8)`, not a generated field label and not a linker-script alias. Annotate the struct with the size from the header layout and the observed next symbol:

   ```text
   __osPiDevMgr = 0x800DF2F0; // type:OSDevMgr size:0x1C
   ```

8. Build and verify:

   ```sh
   ./tools/build-and-verify.sh
   ```

9. If verification fails, diff every function emitted by the new source before looking for linker or data issues:

   ```sh
   python3 tools/asm-differ/diff.py --no-pager osPfsNumFiles
   ```

10. Document Learnings:

Update the DECOMPILATION_LEARNINGS.md file in the root of the project with any additional learnings that you gathered as part of matching this segment. Only add **generic, broadly applicable lessons** — things that would help when matching a *different* function in the future. Do not add per-file details such as specific VRAM/ROM addresses, individual symbol names, exact segment split locations, or step-by-step accounts of what happened for one particular function. Ask: "would this lesson help me match an unrelated ultra function tomorrow?" If not, leave it out.

## Build Flags

Everything under `src/libultra` uses the libultra profile from the Makefile's COMPILER PROFILES section: KMC GCC 2.7.2 at `-O3` with `-DBUILD_VERSION=VERSION_J -DF3DEX_GBI_2 -fsigned-char`. Check the effective flags with:

```sh
make -s print-cflags SRC=src/libultra/io/pfsnumfiles.c
```

If one file needs different flags, add a file-specific override next to the libultra profile, for example:

```make
$(BUILD_DIR)/src/libultra/io/pfsnumfiles.%: OPT_FLAGS = -O2
```

## VERSION_J Specifics

This project uses `BUILD_VERSION=VERSION_J`. When porting upstream ultralib source:

- Keep only the `#if BUILD_VERSION >= VERSION_J` side (or equivalent) of version conditionals.
- This ROM's libultra was built with signed `char`: `strchr`/`strlen`-style code sign-extends bytes (`lb` + shifts, not `lbu`/`andi`).
- The upstream source may contain calls that were not present in the actual build. Trust the assembly over the upstream — if a call appears in upstream but not in the target assembly, omit it.

## Incorrect Segments

Be mindful that you may need to adjust the start/end points of segments as well as the segment name to match it to the correct ultralib c file.

## Data, Rodata, and BSS Matching

Ultralib segments may contain data, rodata, and/or bss which must be similarly matched. These are likely unlabelled. You will need to find the appropriate data/rodata/bss segment and link it back to your matched ultralib segment, e.g:

```
- [0xA3590, .data, libultra/io/controller]
- [<rom>, .rodata, libultra/gu/sinf]
- { type: .bss, vram: <vram>, name: libultra/os/initialize }
```

First check whether the compiled object actually emits these sections:

```sh
mips-linux-gnu-objdump -h build/src/libultra/io/pfsnumfiles.o
```

If the object has only `.text` with content (KMC objects always carry empty `.data`/`.bss` plus `.reginfo`, `.comment`, and `.note`), continue matching instruction generation before adding data or rodata segments. Check section sizes, not just names.

If the object emits data/BSS that already exists in a broad raw segment, split the broad segment around the emitted range. Do not use linker scripts to alias the symbols or Makefile commands to strip sections out of the generated linker script. Keep the C definitions and let the corresponding `.data`/`.bss` YAML entries own those sections.

You can use the match-data-file skill to help match the correct segment. Note that it might not be properly annotated (yet) in mariogolf64.yaml.

## Avoid

- Do not mutate linker scripts with `perl -0pi`, `perl -pe`, or similar build hacks.
- Do not edit the Makefile to `sed` generated linker scripts or add `--allow-multiple-definition` to hide duplicate data.
- Do not add libultra function or data aliases to a linker script; define the symbol in `symbol_addrs.txt` at its runtime `0x800...` address and rename the real labels/references.
- Do not add linker-script aliases for struct field addresses. Annotate the owning symbol with `type:` and `size:` in `symbol_addrs.txt` so asm can reference `symbol + offset`.
- Do not change real libultra data definitions to `extern` just to avoid duplicate storage; it can change code generation. Match the emitted data/BSS segments instead.
- Do not leave `func_...` or `D_...` references in `src/libultra`.
