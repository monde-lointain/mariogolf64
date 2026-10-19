# Matching code

* [Decompile a function](decompile-function.md) - Decompile one Mario Golf 64 assembly function into matching C, or improve an existing partial match; use when asked to decompile, match or improve a function. *(skill)*
* [Matching method](matching-method.md) - How to approach a function that will not match: reconstruct plausible source first, escape plateaus with a clean rewrite, classify near misses, handle large functions and data layout.
* [Decompile F3DEX2 display-list code](decompile-microcode.md) - Decompile a function that builds an F3DEX2 display list into gSP/gDP macros; use when a Gfx * or Gfx ** parameter or 8-byte command pairs written through a cursor show display-list construction. *(skill)*
* [Run the permuter (offline, by a person)](decomp-permuter.md) - Run the permuter offline on a 95%+ register-only near match; use when a person asks to permute a function, since the agent decompile loop never runs it. *(skill)*

# Data, segments and symbols

* [Match a data or rodata segment](match-data-segment.md) - Map a data or rodata segment to a C file and replace externs with ordered definitions; use when asked to match, add, split or fix one in mariogolf64.yaml. *(skill)*
* [Match a bss segment](match-bss-segment.md) - Replace externs with definitions in expected VRAM order to match a .bss segment; use when asked to match, add, split or fix a .bss segment in mariogolf64.yaml. *(skill)*
* [Match a libultra segment](match-ultralib-segment.md) - Match a given segment to a libultra ROM segment; use when converting unidentified asm libultra ranges such as osRecvMesg to matching C source. *(skill)*
* [Add an asset and convert ROM-address references](add-asset.md) - Replace raw ROM addresses and D_<hex> externs with <name>_ROM_START/_ROM_END symbols by adding named bin segments to mariogolf64.yaml; use when code or asm references a ROM asset by address. *(skill)*
* [Rename a global variable or function](rename-global.md) - Update a global variable or function name through its symbol_addrs.txt entry and every C reference; use when asked to rename one, since the build fails otherwise. *(skill)*
* [Rename a C file or header](rename-file.md) - Rename a C file and its header, updating Splat segments, INCLUDE_ASM paths and Makefile rules; use when asked to rename a C file or header (.h) in src/ or include/. *(skill)*

# Cleanup

* [Clean up a matched function](cleanup-function.md) - Improve names, types and structure of already-matching code without breaking the full-ROM match; shared by the cleanup-matched and fix-low-quality nigel tasks.

Entries marked *(skill)* are also omp skills: `.omp/skills/<name>/SKILL.md` is a symlink to the file of the same name here, so the skill name equals the file name.
