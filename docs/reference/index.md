# Compiler behavior

* [KMC GCC learnings](kmc-gcc/) - generic KMC GCC 2.7.2 behavior by symptom: control flow, instruction selection, registers, types, data placement
* [Decompilation artefacts](decompilation-artefacts.md) - Patterns m2c and permuters leave in C (do-while loops, gotos, duplicated temporaries, shifts, false returns) and the GCC 2.7.2 codegen patterns not to fight.

# Configuration formats

* [symbol_addrs.txt format](symbol-addrs-format.md) - How symbols are named and annotated for Splat: line syntax, the options this project uses, and rules for duplicates, overlays and absolutes.
* [mariogolf64.yaml segments](splat-yaml.md) - How the Splat configuration maps ROM ranges to C, assembly, data, rodata, bss and binary assets; the entry forms this project uses and the rules that keep the build matching.

# Hardware

* [F3DEX2 display list commands](f3dex2-commands.md) - Third-party byte-level description of every F3DEX2 command (opcode, field layout, parameters); use gbi.h macros in source and this file to identify raw words.
