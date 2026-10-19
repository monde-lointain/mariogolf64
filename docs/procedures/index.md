# Matching code

* [decompile-function](decompile-function.md) - bootstrap an environment, match, integrate, commit, stop rules *(skill)*
* [matching-method](matching-method.md) - how to approach a function: phases, near-miss triage, large functions, data layout
* [decomp-permuter](decomp-permuter.md) - run the permuter on a 95%+ near match *(skill)*
* [decompile-microcode](decompile-microcode.md) - functions that build F3DEX2 display lists *(skill)*

# Data, segments and symbols

* [match-data-segment](match-data-segment.md) - carve a `.data`/`.rodata` range into a C file *(skill)*
* [match-bss-segment](match-bss-segment.md) - reproduce the original `.bss` layout *(skill)*
* [match-ultralib-segment](match-ultralib-segment.md) - turn a libultra ROM range into source *(skill)*
* [add-asset](add-asset.md) - replace raw PI ROM addresses and asset tables with linker symbols *(skill)*
* [rename-global](rename-global.md) - rename a function or global via `symbol_addrs.txt` *(skill)*
* [rename-file](rename-file.md) - rename a C or header file *(skill)*

# Cleanup

* [cleanup-function](cleanup-function.md) - improve a matched function's names and types without breaking the match

Skills are `.omp/skills/<name>/SKILL.md` symlinks to the file of the same name here, so the skill name always equals the file name.
