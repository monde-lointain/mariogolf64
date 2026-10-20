# Compiler behavior

* [KMC GCC learnings](kmc-gcc/) - Generic KMC GCC 2.7.2 behavior by symptom: control flow, instruction selection, registers, types and data placement.
* [Decompilation artefacts](decompilation-artefacts.md) - Patterns m2c and permuters leave in C (do-while loops, gotos, duplicated temporaries, shifts, false returns); open when a decompiled C shape may be the cause of a mismatch.

# Configuration formats

* [symbol_addrs.txt format](symbol-addrs-format.md) - Line syntax of symbol_addrs.txt as Splat parses it, the options and types this repo uses, and rules for duplicates, overlay labels and absolutes; open before editing symbols.
* [mariogolf64.yaml segments](splat-yaml.md) - Segment and subsegment forms in mariogolf64.yaml, bss sizing, data ownership and INCLUDE_ASM rules; open this before editing the Splat configuration.

# Hardware

* [F3DEX2 display list commands](f3dex2-commands.md) - Third-party byte-level description of the F3DEX2 display list commands (opcode, field layout, parameters); use gbi.h macros in source and this file to identify raw words.
