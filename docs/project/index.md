# Orientation

* [Project overview](overview.md) - Mario Golf 64 (N64, US revision 0) matching decompilation; goal and non-goals.
* [Ground rules](ground-rules.md) - Non-negotiable project constraints: matches are measured, protected files, harness fixes, commits.
* [Repository layout](repository-layout.md) - Directories and root files, which are hand-written, which are generated, and where each kind of source lives.
* [Setup and build](setup-and-build.md) - Clone, install dependencies and the KMC toolchain, place the ROM, build and verify; make targets.

# Standards

* [Code-quality standards](code-quality.md) - What counts as an acceptable match: no inline asm or raw display-list words, typed structs not pointer math, no redundant externs, file structure, validation checklist.
* [Compiler profiles](compiler-profiles.md) - KMC GCC 2.7.2 flags per source directory and per file, and how to print the effective flags.
