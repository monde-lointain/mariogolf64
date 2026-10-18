---
name: match-data-segment
description: Match a data segment by mapping it to an existing C file and replacing extern declarations with ordered definitions. Use when asked to match, add, split, or fix a .data segment in mariogolf64.yaml.
---

# Match a Data Segment

## Update mariogolf64.yaml

Code, data, rodata and bss are all defined as separate segments in `mariogolf64.yaml`. The first step of matching a new data segment is to update the appropriate data segment entry to mark is as defined in the existing C file (code segment).

Most game data still lives in large unsplit blobs such as `- [0x91310, data, main_data]`. To match the data of a C file, carve its range out of the blob and leave the remainder as raw data. For example, to give `src/main/func_80025D30.c` the data that starts at ROM `0x91358` and ends at `<end>`:

```
- [0x91310, data, main_data]
```

Should be changed to:

```
- [0x91310, data, main_data]
- [0x91358, .data, main/func_80025D30]
- [<end>, data, main_data_func_80025D30_tail]
```

Existing examples of matched data are entries such as `- [0xA2ED0, .data, libmus/aud_sched]`. Take the start and end ROM offsets from the `/* ROM VRAM */` comments in the data assembly and from `symbol_addrs.txt`.

## Rebuild the Project (Build Fails)

Rebuild the project by running:

```
./tools/build-and-verify.sh
```

The build will now fail because we have not yet added the expected data. But these error messages can be instructive for telling us what data is needed, etc.

## Update C File

Examine the assembly code for the data segment, this is located in `asm/data/`. Before the split, our example's data is in `asm/data/main_data.data.s`; after re-extraction the carved range is no longer emitted there. Remember that the N64 rom is big endian.

Take the labeled data from the data file and add the appropriate variable definitions to the C file (`src/main/func_80025D30.c` in our example).

Note that the C file will have missing data definitions already but they will be `externs`. You should add the variable definitions in the order in which they appear in the data file (do not follow the ordering of the externs).

## Build and Fix Matching Issues

Rebuild the project by running:

```
./tools/build-and-verify.sh
```

You will likely encounter build errors or checksum failures. Work through the errors and resolve any checksum mismatches.
