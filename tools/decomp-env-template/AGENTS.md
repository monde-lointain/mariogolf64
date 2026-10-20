# Decompile $functionName

Write C for `$functionName` that KMC GCC 2.7.2 (C89) compiles to exactly the bytes of `target.s`. The ROM checksum is the only acceptance test, so a near match does not count. The C must read like the code a programmer wrote: real types, struct fields and control flow, as `$root/docs/project/code-quality.md` describes.

Your environment is `$env`, inside the repository `$root`. Everything you need to start is below, so work directly in this session rather than delegating.

$context

# Files

* `target.s`: the target assembly. `base.c`: m2c's draft; its compile result is in `<baseline>` above.
* `./build.sh <file>.c`: compiles with the owning file's profile, scores the result, writes `<file>_diff`, and logs `base_<N>.c` scores to `match_log.txt`.
* `./diff.sh <file>.o`, `./objdump.py <file>.o`, `./map_asm_to_c.py <file>.o <line>`: normalized diff, normalized disassembly, assembly line to C line.

# Attempt loop

1. Work out what the function does from its callers and the references above. Then copy `base.c` to `base_1.c` and make it compile, taking types and prototypes from the callers, declarations and references: m2c often gets argument counts and types wrong. `base_<N>.c` may include only `"common.h"`, so define anything else it needs inline.
2. For each later attempt, read the last `_diff`, decide on one change by asking what the original programmer wrote, copy the best attempt to the next number (`cp base_3.c base_4.c`), edit the copy and run `./build.sh base_4.c`. Leave scored files unchanged, because `match_log.txt` refers to them.
3. Keep attempting until `build.sh` prints `NEXT:` (a match) or `STOP:` (the stall limit). A register, scheduling or frame difference means trying another shape of the code. When stuck, read `$root/docs/procedures/matching-method.md`, and `$root/docs/reference/kmc-gcc/index.md` for the compiler behavior behind a diff. In those docs a link starting with `/` is relative to `$root/docs/`.

# Integrate a match

After `NEXT:`, from `$root`:

1. Replace the function's `INCLUDE_ASM(...)` line in the C file the `NEXT:` line names with the matching code, named exactly `$functionName` even when a caller uses another name.
2. Use the argument and return types the assembly shows. A prototype or a caller's call that disagrees (often a guessed `(void)`) is wrong: correct it. Reuse the declarations and structs in `src/` and `include/`, and update the header that declares the function rather than adding a second declaration.
3. Run `./tools/build-and-verify.sh`. If the checksum fails, diff the functions in the changed files with `python3 tools/asm-differ/diff.py --no-pager <function>`, starting with those that use structs you changed.

`BUILD SUCCEEDED` ends the run: stop and leave the changes uncommitted. nigel then checks the candidate (new pointer arithmetic is rejected), commits it and records the match, so further checks, commits or ledger records from you only cost time.

# Finish without a match

Stop at the first of these that applies:

* **`STOP:` printed:** if your best score beats every score in `<prior_attempts>` (or there are none), write `NEAR_MISS.md` here: the best file and score, the remaining differences, and what to try next.
* **Integration cannot pass `build-and-verify.sh`:** restore the C file with `git checkout -- <file>`, then from `$root` run `python3 tools/difficulty_ledger.py record $functionName --outcome integration_failed --reason "<one line>"`.
* **The only change left is one the code standards forbid:** inline assembly, or an unused or `volatile` local other than a nested function's static-chain stand-in or unused frame space (`$root/docs/reference/kmc-gcc/registers-and-scheduling.md`, "Stack frames"). Record it the same way with `--outcome gave_up`.

With `Time left` under 5 min, write `NEAR_MISS.md`: the run is killed at the deadline.

# Reply

End with one line: `matched`, or the outcome with the best score and file.
