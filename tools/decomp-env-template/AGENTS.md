# $functionName

Decompile `$functionName` from Mario Golf 64 into C that compiles to the target assembly exactly. This directory, `$env`, is its environment; run the commands below from here. The repository root is `$root`. Follow `$root/docs/procedures/decompile-function.md` (the decompile-function skill): it says how to integrate a match and when to stop.

The compiler is KMC GCC 2.7.2 (C89). `./build.sh` compiles with the profile of the C file that owns the function (named in `.source_file`). `base.c` is m2c's draft; `target.s` is the target assembly.

## Foundation

Delegate these steps to one omp `task` subagent, so the exploration stays out of your context:

<subagent-instructions>
1. Read `$root/docs/reference/kmc-gcc/index.md` and pick the concepts that apply to this function, given `$env/base.c` and `$env/target.s`. Write them to `$env/LEARNINGS.md`, one line each: the concept, why it applies, and the technique to try.
2. Find how `$functionName` is used: grep `$root/src` and `$root/include` for it and read its callers. Add what it does to `LEARNINGS.md`.
3. Make the smallest change that lets `./build.sh base.c` compile in `$env`, so its score is a true baseline. Reuse types and prototypes from `$root/src` and `$root/include`; `base.c` may include only `"common.h"`, so define anything else it needs inline.
4. Check the argument count, argument types and return type of every signature `base.c` relies on; m2c often gets them wrong.
5. Check that the C does what the function's purpose requires, and note logical errors.
6. Report in at most 20 lines: build status, baseline score, the LEARNINGS.md digest, and the signature and logic findings.
</subagent-instructions>

## Attempt loop

1. Read `<attempt>_diff` to find the differing instructions; open `target.s` or the object dumps only when you need the surrounding code.
2. Decide on one change, thinking about what the original programmer wrote.
3. Copy the best attempt so far to the next number and edit the copy, e.g. `cp base_3.c base_4.c` and then edit `base_4.c` (the first attempt copies `base.c` to `base_1.c`). Leave scored files unchanged: `match_log.txt` keeps their scores.
4. Run `./build.sh base_<n>.c`, and note findings worth keeping in `LEARNINGS.md`.

`build.sh` prints `Verified exact match: yes` and a `NEXT:` line on a match, a `STOP:` line once the stall limit is reached, and `Time left` when the run has a deadline. Only a `NEXT:` or `STOP:` line ends the loop; with `Time left` under 5 min, write `NEAR_MISS.md` (the run is killed at the deadline).

Other helpers: `./diff.sh <file>.o` (normalized diff), `./objdump.py <file>.o` (normalized MIPS), `./map_asm_to_c.py <file>.o <line>` (assembly line to C).

## Writing the C

- Write the body in C. `INCLUDE_ASM`, `INCLUDE_RODATA`, inline assembly, register bindings and macros that hide struct access do not count as a decompilation.
- Prefer struct fields and array indexing to pointer arithmetic, and put declarations at the start of each block (C89). Build display lists with the F3DEX2 macros from `PR/gbi.h` (decompile-microcode skill).
- A symbol-only difference such as `%lo(D_800B67A5)` against `%lo(array+1)` is a naming difference in the extracted assembly; note it and move on.
- Read `$root/docs/procedures/matching-method.md` after two attempts without improvement, `$root/docs/reference/decompilation-artefacts.md` for m2c artefacts, and `$root/docs/reference/symbol-addrs-format.md` when the function's symbols look like one array or struct split into many.
- Put KMC GCC findings in `LEARNINGS.md`, not in `$root/docs/`; a separate task verifies and merges them.
